// MarsHUDWidget.cpp

#include "MarsHUDWidget.h"
#include "MarsColonySimulation.h"
#include "MarsMainComputer.h"
#include "MarsUIHelpers.h"

#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	enum EMeter : int32 { Meter_Battery, Meter_Oxygen, Meter_Shield, Meter_Health, Meter_Dose, Meter_Research, Meter_Count };
}

void UMarsHUDWidget::Init(AMarsMainComputer* InComputer)
{
	Computer = InComputer;
	if (UMarsColonySimulation* Sim = InComputer ? InComputer->GetSimulation() : nullptr)
	{
		SimChangedHandle = Sim->OnStateChangedNative.AddUObject(this, &UMarsHUDWidget::HandleSimChanged);
		GameOverHandle = Sim->OnGameOverNative.AddUObject(this, &UMarsHUDWidget::HandleGameOver);
	}
}

TSharedRef<SWidget> UMarsHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	TSharedRef<SWidget> Result = Super::RebuildWidget();
	Refresh();
	return Result;
}

void UMarsHUDWidget::NativeDestruct()
{
	if (AMarsMainComputer* C = Computer.Get())
	{
		if (UMarsColonySimulation* Sim = C->GetSimulation())
		{
			Sim->OnStateChangedNative.Remove(SimChangedHandle);
			Sim->OnGameOverNative.Remove(GameOverHandle);
		}
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FlashTimer);
	}
	Super::NativeDestruct();
}

void UMarsHUDWidget::BuildLayout()
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);   // clicks pass through to the game
	Tree->RootWidget = Root;

	// ---- Top-left: day, clock, weather
	{
		UBorder* P = Panel(Tree, PanelBg(0.70f));
		P->SetVisibility(ESlateVisibility::HitTestInvisible);
		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		DayText = Text(Tree, TEXT("DAY - / -"), 20, White(), false);
		ClockText = Text(Tree, TEXT(""), 14, Grey(), false);
		DayBar = Bar(Tree, Yellow());
		WeatherText = Text(Tree, TEXT(""), 13, Grey());
		AddV(Box, DayText);
		AddV(Box, ClockText);
		AddV(Box, DayBar, FMargin(0.f, 4.f));
		AddV(Box, WeatherText);
		P->SetContent(Box);
		UCanvasPanelSlot* CS = Place(Root, P, FVector2D(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(20.f, 20.f));
		CS->SetAutoSize(false);
		CS->SetSize(FVector2D(430.f, 150.f));
	}

	// ---- Top-right: resource bars
	{
		UBorder* P = Panel(Tree, PanelBg(0.70f));
		P->SetVisibility(ESlateVisibility::HitTestInvisible);
		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		const FLinearColor Colors[Meter_Count] = { Yellow(), Cyan(), Blue(), Green(), Orange(), Purple() };
		MeterLabels.Reset();
		MeterBars.Reset();
		for (int32 i = 0; i < Meter_Count; ++i)
		{
			UTextBlock* Label = Text(Tree, TEXT(""), 13, White(), false);
			UProgressBar* B = Bar(Tree, Colors[i]);
			AddV(Box, Label, FMargin(0.f, i == 0 ? 0.f : 6.f, 0.f, 1.f));
			AddV(Box, B);
			MeterLabels.Add(Label);
			MeterBars.Add(B);
		}
		P->SetContent(Box);
		UCanvasPanelSlot* CS = Place(Root, P, FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-20.f, 20.f));
		CS->SetAutoSize(false);
		CS->SetSize(FVector2D(360.f, 300.f));
	}

	// ---- Top-center: flashing red alert
	{
		AlertPanel = Panel(Tree, FLinearColor(0.55f, 0.02f, 0.02f, 0.90f), FMargin(18.f, 10.f));
		AlertPanel->SetVisibility(ESlateVisibility::Collapsed);
		AlertText = Text(Tree, TEXT(""), 18, White());
		AlertText->SetJustification(ETextJustify::Center);
		AlertPanel->SetContent(AlertText);
		UCanvasPanelSlot* CS = Place(Root, AlertPanel, FVector2D(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 20.f));
		CS->SetAutoSize(false);
		CS->SetSize(FVector2D(620.f, 110.f));
	}

	// ---- Bottom-center: interaction prompt
	{
		PromptPanel = Panel(Tree, PanelBg(0.85f), FMargin(16.f, 8.f));
		PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
		PromptText = Text(Tree, TEXT("Press [E] to use the Main Computer"), 18, Yellow(), false);
		PromptPanel->SetContent(PromptText);
		Place(Root, PromptPanel, FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -120.f));
	}

	// ---- Bottom-left: last message
	{
		UBorder* P = Panel(Tree, PanelBg(0.60f), FMargin(12.f, 8.f));
		P->SetVisibility(ESlateVisibility::HitTestInvisible);
		MessageText = Text(Tree, TEXT(""), 14, White());
		P->SetContent(MessageText);
		UCanvasPanelSlot* CS = Place(Root, P, FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(20.f, -20.f));
		CS->SetAutoSize(false);
		CS->SetSize(FVector2D(560.f, 70.f));
	}

	// ---- Bottom-right: data credits (challenge asks to attribute sources in the tool)
	{
		UTextBlock* Credits = Text(Tree, TEXT("Real data: NASA DONKI space weather + Curiosity REMS weather, 14-20 May 2024 (archived, not live)"), 11, Grey(), false);
		Credits->SetVisibility(ESlateVisibility::HitTestInvisible);
		Place(Root, Credits, FVector2D(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-20.f, -8.f));
	}

	// ---- Center: end of game
	{
		EndPanel = Panel(Tree, PanelBg(0.95f), FMargin(30.f));
		EndPanel->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		EndTitle = Text(Tree, TEXT(""), 32, White(), false);
		EndTitle->SetJustification(ETextJustify::Center);
		EndReason = Text(Tree, TEXT(""), 16, White());
		EndReason->SetJustification(ETextJustify::Center);
		RestartButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		RestartButton->AddChild(Text(Tree, TEXT("RESTART"), 18, FLinearColor::Black, false));
		RestartButton->OnClicked.AddDynamic(this, &UMarsHUDWidget::HandleRestartClicked);
		AddV(Box, EndTitle, FMargin(0.f, 0.f, 0.f, 12.f));
		AddV(Box, EndReason, FMargin(0.f, 0.f, 0.f, 20.f));
		AddV(Box, RestartButton)->SetHorizontalAlignment(HAlign_Center);
		EndPanel->SetContent(Box);
		UCanvasPanelSlot* CS = Place(Root, EndPanel, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, 0.f));
		CS->SetAutoSize(false);
		CS->SetSize(FVector2D(640.f, 300.f));
	}
}

void UMarsHUDWidget::SetInteractPromptVisible(bool bVisible, const FKey& Key)
{
	if (PromptPanel)
	{
		PromptText->SetText(FText::FromString(FString::Printf(TEXT("Press [%s] to use the Main Computer"), *Key.GetDisplayName().ToString())));
		PromptPanel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMarsHUDWidget::HandleSimChanged()
{
	Refresh();
}

void UMarsHUDWidget::Refresh()
{
	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!Sim || !DayText || MeterBars.Num() != Meter_Count)
	{
		return;
	}
	const FMarsColonyState& S = Sim->GetStateRef();
	const FMarsComputerDay* D = C->FindDay(S.Day);

	// Day + clock + weather
	if (D)
	{
		DayText->SetText(FText::FromString(FString::Printf(TEXT("DAY %d / %d   |   SOL %d   |   %s"), S.Day, S.DayCount, D->Sol, *D->Date)));
		WeatherText->SetText(FText::FromString(FString::Printf(
			TEXT("%s  -  Air %d / %d C  -  Ground %d / %d C  -  %d Pa  -  UV %s  -  Sunrise %s, sunset %s"),
			*D->WeatherCondition.ToString(), D->AirTempMinC, D->AirTempMaxC, D->GroundTempMinC, D->GroundTempMaxC,
			D->PressurePa, *D->UVIndex, *D->Sunrise, *D->Sunset)));
	}
	ClockText->SetText(FText::FromString(S.bGameOver ? FString(TEXT("Mission over"))
		: FString::Printf(TEXT("Next day in %s"), *MarsUI::Clock(S.DayTimeRemaining))));
	DayBar->SetPercent(S.DayProgress);

	// Resource bars: label shows value and change per day
	auto SetMeter = [this](int32 Index, const FString& Label, float Percent)
	{
		MeterLabels[Index]->SetText(FText::FromString(Label));
		MeterBars[Index]->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	};
	SetMeter(Meter_Battery, FString::Printf(TEXT("Battery  %d / %d   (%s/day)"), FMath::RoundToInt32(S.Battery), FMath::RoundToInt32(S.BatteryMax), *MarsUI::Signed(S.BatteryRate)),
		S.BatteryMax > 0.f ? S.Battery / S.BatteryMax : 0.f);
	SetMeter(Meter_Oxygen, FString::Printf(TEXT("Oxygen  %d%%   (%s/day)"), FMath::RoundToInt32(S.Oxygen), *MarsUI::Signed(S.OxygenRate)), S.Oxygen / 100.f);
	SetMeter(Meter_Shield, FString::Printf(TEXT("Shield  %d%%   (%s/day)"), FMath::RoundToInt32(S.Shield), *MarsUI::Signed(S.ShieldRate)), S.Shield / 100.f);
	SetMeter(Meter_Health, FString::Printf(TEXT("Crew health  %d%%   (%s/day)"), FMath::RoundToInt32(S.CrewHealth), *MarsUI::Signed(S.HealthRate)), S.CrewHealth / 100.f);
	SetMeter(Meter_Dose, FString::Printf(TEXT("Radiation dose  %d / %d   (%s/day)"), FMath::RoundToInt32(S.Dose), FMath::RoundToInt32(S.DoseLimit), *MarsUI::Signed(S.DoseRate)),
		S.DoseLimit > 0.f ? S.Dose / S.DoseLimit : 0.f);
	SetMeter(Meter_Research, FString::Printf(TEXT("Research  %d / %d   (%s/day)"), FMath::RoundToInt32(S.Research), FMath::RoundToInt32(S.ResearchGoal), *MarsUI::Signed(S.ResearchRate)),
		S.ResearchGoal > 0.f ? S.Research / S.ResearchGoal : 0.f);

	// Danger colors
	MeterBars[Meter_Health]->SetFillColorAndOpacity(S.CrewHealth < 35.f ? MarsUI::Red() : MarsUI::Green());
	MeterBars[Meter_Oxygen]->SetFillColorAndOpacity(S.Oxygen < 30.f ? MarsUI::Red() : MarsUI::Cyan());
	MeterBars[Meter_Dose]->SetFillColorAndOpacity(S.Dose >= S.DoseLimit * 0.75f ? MarsUI::Red() : MarsUI::Orange());

	// Alerts (flashing)
	if (S.Alerts.Num() > 0)
	{
		FString Joined;
		for (const FText& A : S.Alerts)
		{
			if (!Joined.IsEmpty())
			{
				Joined += TEXT("\n");
			}
			Joined += A.ToString();
		}
		AlertText->SetText(FText::FromString(Joined));
		if (AlertPanel->GetVisibility() == ESlateVisibility::Collapsed)
		{
			AlertPanel->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UWorld* World = GetWorld())
			{
				World->GetTimerManager().SetTimer(FlashTimer, FTimerDelegate::CreateUObject(this, &UMarsHUDWidget::ToggleAlertFlash), 0.5f, true);
			}
		}
	}
	else if (AlertPanel->GetVisibility() != ESlateVisibility::Collapsed)
	{
		AlertPanel->SetVisibility(ESlateVisibility::Collapsed);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(FlashTimer);
		}
	}

	MessageText->SetText(S.LastMessage);
}

void UMarsHUDWidget::ToggleAlertFlash()
{
	bFlashOn = !bFlashOn;
	if (AlertPanel)
	{
		AlertPanel->SetRenderOpacity(bFlashOn ? 1.f : 0.45f);
	}
}

void UMarsHUDWidget::HandleGameOver(bool bWon, const FText& Reason)
{
	Refresh();
	if (!EndPanel)
	{
		return;
	}
	EndTitle->SetText(FText::FromString(bWon ? TEXT("MISSION COMPLETE") : TEXT("MISSION FAILED")));
	MarsUI::SetTextColor(EndTitle, bWon ? MarsUI::Green() : MarsUI::Red());
	EndReason->SetText(Reason);
	EndPanel->SetVisibility(ESlateVisibility::Visible);
	if (PromptPanel)
	{
		PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMarsHUDWidget::HandleRestartClicked()
{
	if (AMarsMainComputer* C = Computer.Get())
	{
		C->RestartLevel();
	}
}
