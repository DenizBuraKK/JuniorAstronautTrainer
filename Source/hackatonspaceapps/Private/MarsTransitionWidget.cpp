// MarsTransitionWidget.cpp

#include "MarsTransitionWidget.h"
#include "MarsColonySimulation.h"
#include "MarsMainComputer.h"
#include "MarsUIHelpers.h"

#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const TCHAR* FadeDot = TEXT("  \u00B7  ");
	constexpr float UpdateInterval = 1.f / 60.f;
	constexpr float TitleFadeSeconds = 0.4f;
	constexpr float EndContentFadeSeconds = 0.8f;
	constexpr float MaxBlackSpeed = 4.f;

	float Smooth(float X)
	{
		return FMath::SmoothStep(0.f, 1.f, FMath::Clamp(X, 0.f, 1.f));
	}
}

void UMarsTransitionWidget::Init(AMarsMainComputer* InComputer)
{
	Computer = InComputer;
}

TSharedRef<SWidget> UMarsTransitionWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UMarsTransitionWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UWorld* World = GetWorld())
	{
		LastUpdateTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(UpdateTimer,
			FTimerDelegate::CreateUObject(this, &UMarsTransitionWidget::UpdateTransition), UpdateInterval, true);
	}
	// Start black so the first frame of the game doesn't flash before the "DAY 1" intro.
	const AMarsMainComputer* C = Computer.Get();
	SetBlack(C && C->bDayTransitions && C->IsDataLoaded() ? 1.f : 0.f);
}

void UMarsTransitionWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(UpdateTimer);
	}
	Super::NativeDestruct();
}

// ==================================================================== Layout

void UMarsTransitionWidget::BuildLayout()
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	Tree->RootWidget = Root;

	// Black layer (never blocks clicks)
	BlackLayer = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BlackLayer->SetBrushColor(FLinearColor::Black);
	BlackLayer->SetVisibility(ESlateVisibility::Collapsed);
	FillCanvas(Root, BlackLayer);

	// Big day title
	{
		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		TitleCaption = Caption(Tree, TEXT(""), Accent(), 14);
		TitleCaption->SetJustification(ETextJustify::Center);
		AddV(Box, TitleCaption)->SetHorizontalAlignment(HAlign_Center);
		TitleDay = Text(Tree, TEXT(""), 110, White(), false, true, 250);
		TitleDay->SetJustification(ETextJustify::Center);
		AddV(Box, TitleDay, FMargin(0.f, 0.f, 0.f, 0.f))->SetHorizontalAlignment(HAlign_Center);
		TitleSub = Text(Tree, TEXT(""), 18, Muted(), false, true, 200);
		TitleSub->SetJustification(ETextJustify::Center);
		AddV(Box, TitleSub)->SetHorizontalAlignment(HAlign_Center);
		TitleHint = Text(Tree, TEXT(""), 15, White(), false);
		TitleHint->SetJustification(ETextJustify::Center);
		AddV(Box, TitleHint, FMargin(0.f, 18.f, 0.f, 0.f))->SetHorizontalAlignment(HAlign_Center);

		Box->SetVisibility(ESlateVisibility::Collapsed);
		TitleBox = Box;
		Place(Root, Box, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, -20.f));
	}

	// End screen (on top of full black). Blocks clicks so the player can't walk behind it.
	{
		UBorder* Holder = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Holder->SetBrushColor(FLinearColor::Transparent);
		Holder->SetHorizontalAlignment(HAlign_Center);
		Holder->SetVerticalAlignment(VAlign_Center);
		Holder->SetVisibility(ESlateVisibility::Collapsed);

		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

		UTextBlock* Cap = Caption(Tree, TEXT("MISSION REPORT"), Muted(), 13);
		Cap->SetJustification(ETextJustify::Center);
		AddV(Box, Cap);
		EndTitle = Text(Tree, TEXT(""), 56, White(), false, true, 120);
		EndTitle->SetJustification(ETextJustify::Center);
		AddV(Box, EndTitle, FMargin(0.f, 6.f, 0.f, 10.f));
		EndReason = Text(Tree, TEXT(""), 17, White(), true);
		EndReason->SetJustification(ETextJustify::Center);
		AddV(Box, EndReason, FMargin(0.f, 0.f, 0.f, 16.f));
		EndStats = Text(Tree, TEXT(""), 14, Muted(), true);
		EndStats->SetJustification(ETextJustify::Center);
		AddV(Box, EndStats, FMargin(0.f, 0.f, 0.f, 22.f));

		UButton* PlayAgain = MakeButton(Tree, TEXT("PLAY AGAIN"), EButtonKind::Primary, 15);
		PlayAgain->OnClicked.AddDynamic(this, &UMarsTransitionWidget::HandlePlayAgainClicked);
		AddV(Box, PlayAgain, FMargin(0.f, 0.f, 0.f, 30.f))->SetHorizontalAlignment(HAlign_Center);

		// Credits (text comes from BP_MainComputer > Mars|UI > Credits Text)
		UVerticalBox* Credits = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(Credits, Divider(Tree), FMargin(80.f, 0.f, 80.f, 16.f));
		UTextBlock* CreditsCap = Caption(Tree, TEXT("CREDITS"), Accent(), 12);
		CreditsCap->SetJustification(ETextJustify::Center);
		AddV(Credits, CreditsCap, FMargin(0.f, 0.f, 0.f, 8.f));
		CreditsBody = Text(Tree, TEXT(""), 14, Muted(), true);
		CreditsBody->SetJustification(ETextJustify::Center);
		UScrollBox* CreditsScroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		CreditsScroll->AddChild(CreditsBody);
		USizeBox* CreditsSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		CreditsSize->SetMaxDesiredHeight(260.f);
		CreditsSize->AddChild(CreditsScroll);
		AddV(Credits, CreditsSize);
		CreditsBox = Credits;
		AddV(Box, Credits);

		Holder->SetContent(FixedWidth(Tree, Box, 760.f));
		EndBox = Holder;
		FillCanvas(Root, Holder);
	}
}

// ==================================================================== Update (60 times per second, read-only)

void UMarsTransitionWidget::SetBlack(float Alpha)
{
	BlackAlpha = FMath::Clamp(Alpha, 0.f, 1.f);
	if (BlackLayer)
	{
		BlackLayer->SetRenderOpacity(BlackAlpha);
		BlackLayer->SetVisibility(BlackAlpha > 0.001f ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMarsTransitionWidget::UpdateTransition()
{
	const UWorld* World = GetWorld();
	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!World || !Sim || !BlackLayer)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	const float Dt = FMath::Clamp(static_cast<float>(Now - LastUpdateTime), 0.f, 0.1f);
	LastUpdateTime = Now;

	const FMarsColonyState& S = Sim->GetStateRef();

	// ---- End of the game: fade fully to black, then show the report
	if (S.bGameOver)
	{
		if (EndTime < 0.f)
		{
			EndTime = 0.f;
			FadeInTime = -1.f;
		}
		EndTime += Dt;
		const float FadeSeconds = FMath::Max(0.05f, C->EndFadeSeconds);
		SetBlack(FMath::Max(BlackAlpha, Smooth(EndTime / FadeSeconds)));

		// Let a running day title fade away
		if (TitleTime >= 0.f && TitleBox)
		{
			TitleTime += Dt;
			const float Out = 1.f - Smooth(TitleTime / TitleFadeSeconds);
			TitleBox->SetRenderOpacity(FMath::Min(TitleBox->GetRenderOpacity(), Out));
			if (Out <= 0.f)
			{
				TitleBox->SetVisibility(ESlateVisibility::Collapsed);
				TitleTime = -1.f;
			}
		}
		if (BlackAlpha >= 0.999f)
		{
			if (EndContentTime < 0.f)
			{
				ShowEndScreen();
				EndContentTime = 0.f;
			}
			EndContentTime += Dt;
			EndBox->SetRenderOpacity(Smooth(EndContentTime / EndContentFadeSeconds));
		}
		return;
	}

	if (!C->bDayTransitions || S.Day <= 0)
	{
		SetBlack(0.f);
		return;
	}

	// ---- New day: start the fade-in and the big title
	if (S.Day != LastDay)
	{
		LastDay = S.Day;
		FadeInTime = 0.f;
		ShowDayTitle(S.Day);
	}

	// ---- Fade-out during the last seconds of the day
	float FadeOut = 0.f;
	if (C->IsDayTimerRunning())
	{
		const float Duration = C->GetDayDuration();
		const float FadeOutSeconds = FMath::Min(C->DayFadeOutSeconds, Duration * 0.4f);   // short test days still work
		const float Remaining = C->GetDayTimeRemaining();
		if (FadeOutSeconds > 0.f && Remaining < FadeOutSeconds)
		{
			FadeOut = Smooth(1.f - Remaining / FadeOutSeconds);
		}
	}

	// ---- Fade-in at the start of a day (short black hold first)
	float FadeIn = 0.f;
	if (FadeInTime >= 0.f)
	{
		FadeInTime += Dt;
		const float Hold = FMath::Max(0.f, C->DayBlackHoldSeconds);
		const float InSeconds = FMath::Max(0.05f, C->DayFadeInSeconds);
		FadeIn = 1.f - Smooth((FadeInTime - Hold) / InSeconds);
		if (FadeInTime >= Hold + InSeconds)
		{
			FadeInTime = -1.f;
			FadeIn = 0.f;
		}
	}
	// Move at most 4 units/s: normal fades are slower than that, so this only softens sudden changes
	// (for example the help panel pausing the clock in the middle of a fade-out).
	SetBlack(FMath::FInterpConstantTo(BlackAlpha, FMath::Max(FadeOut, FadeIn), Dt, MaxBlackSpeed));

	// ---- Title: fade in, stay, fade out
	if (TitleTime >= 0.f && TitleBox)
	{
		TitleTime += Dt;
		const float Total = FMath::Max(TitleFadeSeconds * 2.f, C->DayTitleSeconds);
		const float In = Smooth(TitleTime / TitleFadeSeconds);
		const float Out = 1.f - Smooth((TitleTime - (Total - TitleFadeSeconds)) / TitleFadeSeconds);
		TitleBox->SetRenderOpacity(FMath::Min(In, Out));
		if (TitleTime >= Total)
		{
			TitleBox->SetVisibility(ESlateVisibility::Collapsed);
			TitleTime = -1.f;
		}
	}
}

void UMarsTransitionWidget::ShowDayTitle(int32 Day)
{
	using namespace MarsUI;
	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!C || !Sim || !TitleBox)
	{
		return;
	}
	const FMarsColonyState& S = Sim->GetStateRef();
	const FMarsComputerDay* D = C->FindDay(Day);

	TitleCaption->SetText(FText::FromString(D
		? FString::Printf(TEXT("SOL %d%s%s"), D->Sol, FadeDot, *D->Date)
		: FString()));
	TitleDay->SetText(FText::FromString(FString::Printf(TEXT("DAY %d"), Day)));
	TitleSub->SetText(FText::FromString(Day >= S.DayCount
		? FString(TEXT("FINAL DAY"))
		: FString::Printf(TEXT("OF %d"), S.DayCount)));
	SetTextColor(TitleSub, Day >= S.DayCount ? Accent() : Muted());

	if (D && D->bHasEvent)
	{
		TitleHint->SetText(FText::FromString(FString::Printf(TEXT("Incoming: %s"), *D->EventTitle.ToString())));
		SetTextColor(TitleHint, Bad());
	}
	else
	{
		TitleHint->SetText(FText::FromString(TEXT("No incidents expected. A good day for research.")));
		SetTextColor(TitleHint, Good());
	}

	TitleTime = 0.f;
	TitleBox->SetRenderOpacity(0.f);
	TitleBox->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UMarsTransitionWidget::ShowEndScreen()
{
	using namespace MarsUI;
	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!C || !Sim || !EndBox)
	{
		return;
	}
	const FMarsColonyState& S = Sim->GetStateRef();

	EndTitle->SetText(FText::FromString(S.bWon ? TEXT("MISSION COMPLETE") : TEXT("MISSION FAILED")));
	SetTextColor(EndTitle, S.bWon ? Good() : Bad());
	EndReason->SetText(S.GameOverReason);
	EndStats->SetText(FText::FromString(FString::Printf(
		TEXT("Research %d / %d%sRadiation dose %d / %d%sCrew health %d%%%sReached day %d of %d"),
		FMath::RoundToInt32(S.Research), FMath::RoundToInt32(S.ResearchGoal), FadeDot,
		FMath::RoundToInt32(S.Dose), FMath::RoundToInt32(S.DoseLimit), FadeDot,
		FMath::RoundToInt32(S.CrewHealth), FadeDot, FMath::Max(1, S.Day), S.DayCount)));

	CreditsBody->SetText(C->CreditsText);
	CreditsBox->SetVisibility(C->CreditsText.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

	EndBox->SetRenderOpacity(0.f);
	EndBox->SetVisibility(ESlateVisibility::Visible);
}

void UMarsTransitionWidget::HandlePlayAgainClicked()
{
	if (AMarsMainComputer* C = Computer.Get())
	{
		C->RestartLevel();
	}
}
