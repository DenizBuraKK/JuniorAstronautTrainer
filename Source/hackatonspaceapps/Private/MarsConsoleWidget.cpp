// MarsConsoleWidget.cpp

#include "MarsConsoleWidget.h"
#include "MarsColonySimulation.h"
#include "MarsMainComputer.h"
#include "MarsStationConsole.h"
#include "MarsUIHelpers.h"

#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	constexpr float ConsoleUpdateInterval = 1.f / 60.f;
	constexpr float ReleaseDrainMultiplier = 2.f;   // letting go empties the bar twice as fast as it fills

	FLinearColor TaskColor(EMarsSideTask Task)
	{
		switch (Task)
		{
		case EMarsSideTask::Energy:      return MarsUI::BatteryColor();
		case EMarsSideTask::LifeSupport: return MarsUI::HealthColor();
		case EMarsSideTask::Research:
		default:                         return MarsUI::ResearchColor();
		}
	}
}

void UMarsConsoleWidget::Init(AMarsStationConsole* InConsole)
{
	Console = InConsole;
}

TSharedRef<SWidget> UMarsConsoleWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UMarsConsoleWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWorld* World = GetWorld())
	{
		LastUpdateTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(UpdateTimer,
			FTimerDelegate::CreateUObject(this, &UMarsConsoleWidget::UpdateHold), ConsoleUpdateInterval, true);
	}
}

void UMarsConsoleWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	bHolding = false;
	Super::NativeDestruct();
}

void UMarsConsoleWidget::OnOpened()
{
	Progress = 0.f;
	bHolding = false;
	bNeedsRelease = false;
	LastResult = FText::GetEmpty();
	if (UWorld* World = GetWorld())
	{
		LastUpdateTime = World->GetTimeSeconds();
	}
	UpdateHold();
}

// ==================================================================== Layout

void UMarsConsoleWidget::BuildLayout()
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;
	const AMarsStationConsole* C = Console.Get();
	const EMarsSideTask Task = C ? C->Task : EMarsSideTask::Research;
	const FLinearColor Color = TaskColor(Task);

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Tree->RootWidget = Root;

	UBorder* DimLayer = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DimLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
	DimLayer->SetHorizontalAlignment(HAlign_Center);
	DimLayer->SetVerticalAlignment(VAlign_Center);
	FillCanvas(Root, DimLayer);

	UBorder* Window = Panel(Tree, WindowBg(), FMargin(26.f, 22.f), 16.f, WithAlpha(Color, 0.5f), 1.f);
	DimLayer->SetContent(FixedWidth(Tree, Window, 640.f));
	UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Window->SetContent(Box);

	// Header
	{
		UHorizontalBox* Head = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Titles = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(Titles, Caption(Tree, TEXT("SIDE TASK"), Color));
		TitleText = Text(Tree, TEXT(""), 26, White(), false, true);
		AddV(Titles, TitleText);
		AddH(Head, Titles, 1.f, FMargin(0.f));
		UButton* Close = MakeButton(Tree, FString::Printf(TEXT("CLOSE  [%s]"),
			C ? *C->InteractionKey.GetDisplayName().ToString() : TEXT("E")), EButtonKind::Ghost, 13);
		Close->OnClicked.AddDynamic(this, &UMarsConsoleWidget::HandleCloseClicked);
		AddH(Head, Close, 0.f, FMargin(0.f))->SetVerticalAlignment(VAlign_Top);
		AddV(Box, Head, FMargin(0.f, 0.f, 0.f, 10.f));
	}

	DescriptionText = Text(Tree, TEXT(""), 14, Muted(), true);
	AddV(Box, DescriptionText, FMargin(0.f, 0.f, 0.f, 14.f));

	ValueText = Text(Tree, TEXT(""), 13, White(), false, true);
	AddV(Box, ValueText, FMargin(0.f, 0.f, 0.f, 8.f));

	// Bar + HOLD button side by side
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* BarBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UProgressBar* B = nullptr;
		AddV(BarBox, Bar(Tree, Color, 18.f, B), FMargin(0.f));
		HoldBar = B;
		PercentText = Text(Tree, TEXT("0%"), 12, Muted(), false);
		AddV(BarBox, PercentText, FMargin(0.f, 4.f, 0.f, 0.f));
		AddH(Row, BarBox, 1.f, FMargin(0.f, 0.f, 16.f, 0.f));

		HoldButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		StyleButton(HoldButton, Color, WithAlpha(Color, 0.85f), WithAlpha(Color, 0.65f), FLinearColor::Transparent, 10.f, FMargin(26.f, 12.f));
		HoldButtonText = Text(Tree, TEXT("HOLD"), 16, Ink(), false, true, 150);
		HoldButtonText->SetShadowColorAndOpacity(FLinearColor::Transparent);
		HoldButton->AddChild(HoldButtonText);
		AlignButtonContent(HoldButton, HAlign_Center);
		HoldButton->OnPressed.AddDynamic(this, &UMarsConsoleWidget::HandleHoldPressed);
		HoldButton->OnReleased.AddDynamic(this, &UMarsConsoleWidget::HandleHoldReleased);
		AddH(Row, FixedWidth(Tree, HoldButton, 170.f), 0.f, FMargin(0.f))->SetVerticalAlignment(VAlign_Top);
		AddV(Box, Row, FMargin(0.f, 0.f, 0.f, 14.f));
	}

	StatusText = Text(Tree, TEXT(""), 13, White(), true);
	AddV(Box, StatusText, FMargin(0.f, 0.f, 0.f, 6.f));
	UsesText = Text(Tree, TEXT(""), 12, Dim(), true);
	AddV(Box, UsesText);
}

// ==================================================================== Update

void UMarsConsoleWidget::UpdateHold()
{
	using namespace MarsUI;
	AMarsStationConsole* C = Console.Get();
	const UWorld* World = GetWorld();
	if (!C || !World || !HoldBar)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float Dt = FMath::Clamp(static_cast<float>(Now - LastUpdateTime), 0.f, 0.1f);
	LastUpdateTime = Now;

	FText Reason;
	const bool bCanDo = C->CanDoTask(Reason);
	const float FillPerSecond = 1.f / FMath::Max(0.5f, C->HoldSeconds);

	if (!bCanDo)
	{
		bHolding = false;   // a disabled button may never send "released"
	}
	if (bCanDo && bHolding && !bNeedsRelease)
	{
		Progress += Dt * FillPerSecond;
		if (Progress >= 1.f)
		{
			Progress = 0.f;
			bNeedsRelease = true;
			C->CompleteTask(LastResult);
		}
	}
	else
	{
		Progress = FMath::Max(0.f, Progress - Dt * FillPerSecond * ReleaseDrainMultiplier);
	}

	HoldBar->SetPercent(Progress);
	PercentText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(Progress * 100.f))));
	RefreshTexts(C->CanDoTask(Reason), Reason);
}

void UMarsConsoleWidget::RefreshTexts(bool bCanDo, const FText& Reason)
{
	using namespace MarsUI;
	const AMarsStationConsole* C = Console.Get();
	const AMarsMainComputer* Computer = C ? C->GetComputer() : nullptr;
	const UMarsColonySimulation* Sim = Computer ? Computer->GetSimulation() : nullptr;
	if (!C || !Sim)
	{
		return;
	}
	const FMarsColonyState& S = Sim->GetStateRef();
	const int32 Reward = FMath::RoundToInt32(C->RewardAmount);
	const int32 Seconds = FMath::RoundToInt32(C->HoldSeconds);

	TitleText->SetText(C->GetStationName());
	switch (C->Task)
	{
	case EMarsSideTask::Energy:
		DescriptionText->SetText(FText::FromString(FString::Printf(
			TEXT("Recalibrate the power reactor to squeeze out extra energy. Hold the button for %d seconds to add +%d to the battery."), Seconds, Reward)));
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("BATTERY  %d / %d"), FMath::RoundToInt32(S.Battery), FMath::RoundToInt32(S.BatteryMax))));
		break;
	case EMarsSideTask::LifeSupport:
		DescriptionText->SetText(FText::FromString(FString::Printf(
			TEXT("Clean the air filters and run the crew health check. Hold the button for %d seconds to give the crew +%d health."), Seconds, Reward)));
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("CREW HEALTH  %d%%"), FMath::RoundToInt32(S.CrewHealth))));
		break;
	case EMarsSideTask::Research:
	default:
		DescriptionText->SetText(FText::FromString(FString::Printf(
			TEXT("Analyse the rock samples collected in Gale Crater. Hold the button for %d seconds to earn +%d research points."), Seconds, Reward)));
		ValueText->SetText(FText::FromString(FString::Printf(TEXT("RESEARCH  %d / %d"), FMath::RoundToInt32(S.Research), FMath::RoundToInt32(S.ResearchGoal))));
		break;
	}

	HoldButton->SetIsEnabled(bCanDo);
	HoldButtonText->SetText(FText::FromString(bHolding && bCanDo && !bNeedsRelease ? TEXT("HOLDING...") : TEXT("HOLD")));

	if (!bCanDo)
	{
		StatusText->SetText(Reason);
		SetTextColor(StatusText, Warn());
	}
	else if (!LastResult.IsEmpty())
	{
		StatusText->SetText(LastResult);
		SetTextColor(StatusText, Good());
	}
	else
	{
		StatusText->SetText(FText::FromString(TEXT("Press and hold the button until the bar is full. Letting go empties the bar.")));
		SetTextColor(StatusText, Muted());
	}

	const int32 Left = C->GetUsesLeftToday();
	UsesText->SetText(FText::FromString(Left < 0
		? FString(TEXT("You can do this task as often as you like."))
		: FString::Printf(TEXT("Uses left today: %d / %d"), Left, C->UsesPerDay)));
}

void UMarsConsoleWidget::HandleHoldPressed()
{
	bHolding = true;
}

void UMarsConsoleWidget::HandleHoldReleased()
{
	bHolding = false;
	bNeedsRelease = false;
}

void UMarsConsoleWidget::HandleCloseClicked()
{
	if (AMarsStationConsole* C = Console.Get())
	{
		C->CloseConsole();
	}
}
