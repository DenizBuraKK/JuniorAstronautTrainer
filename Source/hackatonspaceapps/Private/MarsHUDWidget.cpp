// MarsHUDWidget.cpp

#include "MarsHUDWidget.h"
#include "MarsColonySimulation.h"
#include "MarsMainComputer.h"
#include "MarsStationConsole.h"
#include "MarsUIHelpers.h"

#include "EngineUtils.h"

#include "Components/ScrollBox.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	enum EMeter : int32 { Meter_Battery, Meter_Oxygen, Meter_Shield, Meter_Health, Meter_Dose, Meter_Research, Meter_Count };

	// "\u00B7" = middle dot, "\u00B0" = degree sign (written as escapes so the source file stays plain ASCII)
	const TCHAR* HudDot = TEXT("  \u00B7  ");
}

namespace HudSideTasks
{

	/** Settings of the first station console in the level (for the help texts). */
	void GetSideTaskNumbers(const UObject* WorldContext, int32& OutSeconds, int32& OutReward, int32& OutUses)
	{
		OutSeconds = 10;
		OutReward = 5;
		OutUses = 2;
		if (const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr)
		{
			for (TActorIterator<AMarsStationConsole> It(World); It; ++It)
			{
				OutSeconds = FMath::RoundToInt32(It->HoldSeconds);
				OutReward = FMath::RoundToInt32(It->RewardAmount);
				OutUses = It->UsesPerDay;
				break;
			}
		}
	}
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

// ==================================================================== Layout

void UMarsHUDWidget::BuildLayout()
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);   // clicks pass through to the game
	Tree->RootWidget = Root;

	// ---------------------------------------------------------------- Top-left: mission clock
	{
		UBorder* P = Panel(Tree, PanelBg(), FMargin(18.f, 14.f));
		P->SetVisibility(ESlateVisibility::HitTestInvisible);
		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		P->SetContent(Box);

		UHorizontalBox* Top = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		DayText = Text(Tree, TEXT("DAY -"), 26, White(), false, true, 40);
		AddH(Top, DayText, 1.f, FMargin(0.f));
		UBorder* SolPill = Panel(Tree, WithAlpha(Accent(), 0.15f), FMargin(10.f, 3.f), 10.f, WithAlpha(Accent(), 0.55f), 1.f);
		SolText = Text(Tree, TEXT("SOL -"), 11, Accent(), false, true, 120);
		SolPill->SetContent(SolText);
		AddH(Top, SolPill, 0.f, FMargin(0.f));
		AddV(Box, Top);

		DateText = Text(Tree, TEXT(""), 13, Muted(), false);
		AddV(Box, DateText, FMargin(0.f, 0.f, 0.f, 10.f));

		UProgressBar* DB = nullptr;
		AddV(Box, Bar(Tree, Accent(), 6.f, DB));
		DayBar = DB;

		UHorizontalBox* ClockRow = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AddH(ClockRow, Caption(Tree, TEXT("TODAY"), Dim(), 10), 1.f, FMargin(0.f));
		ClockText = Text(Tree, TEXT(""), 12, Muted(), false);
		AddH(ClockRow, ClockText, 0.f, FMargin(0.f));
		AddV(Box, ClockRow, FMargin(0.f, 5.f, 0.f, 10.f));

		AddV(Box, Divider(Tree), FMargin(0.f, 0.f, 0.f, 10.f));
		WeatherText = Caption(Tree, TEXT("WEATHER"), Warn(), 11);
		AddV(Box, WeatherText);
		TempText = Text(Tree, TEXT(""), 13, White(), true);
		AddV(Box, TempText);

		UCanvasPanelSlot* CS = Place(Root, FixedWidth(Tree, P, 370.f), FVector2D(0.f, 0.f), FVector2D(0.f, 0.f), FVector2D(24.f, 24.f));
		CS->SetAutoSize(true);
	}

	// ---------------------------------------------------------------- Top-right: base status
	{
		UBorder* P = Panel(Tree, PanelBg(), FMargin(18.f, 14.f));
		P->SetVisibility(ESlateVisibility::HitTestInvisible);
		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		P->SetContent(Box);
		AddV(Box, Caption(Tree, TEXT("BASE STATUS")), FMargin(0.f, 0.f, 0.f, 8.f));

		const FLinearColor Colors[Meter_Count] = { BatteryColor(), OxygenColor(), ShieldColor(), HealthColor(), DoseColor(), ResearchColor() };
		const TCHAR* Names[Meter_Count] = { TEXT("BATTERY"), TEXT("OXYGEN"), TEXT("SHIELD"), TEXT("CREW HEALTH"), TEXT("RADIATION DOSE"), TEXT("RESEARCH") };
		MeterValues.Reset();
		MeterRates.Reset();
		MeterBars.Reset();
		for (int32 i = 0; i < Meter_Count; ++i)
		{
			UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

			USizeBox* DotBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
			DotBox->SetWidthOverride(8.f);
			DotBox->SetHeightOverride(8.f);
			DotBox->AddChild(Panel(Tree, Colors[i], FMargin(0.f), 4.f, FLinearColor::Transparent, 0.f));
			AddH(Row, DotBox, 0.f, FMargin(0.f, 0.f, 8.f, 0.f));

			AddH(Row, Text(Tree, Names[i], 11, Muted(), false, true, 80), 1.f, FMargin(0.f));
			UTextBlock* Rate = Text(Tree, TEXT(""), 11, Dim(), false);
			AddH(Row, Rate, 0.f, FMargin(0.f, 0.f, 10.f, 0.f));
			UTextBlock* Value = Text(Tree, TEXT(""), 15, White(), false, true);
			AddH(Row, Value, 0.f, FMargin(0.f));

			AddV(Box, Row, FMargin(0.f, i == 0 ? 0.f : 9.f, 0.f, 4.f));
			UProgressBar* B = nullptr;
			AddV(Box, Bar(Tree, Colors[i], 6.f, B));

			MeterValues.Add(Value);
			MeterRates.Add(Rate);
			MeterBars.Add(B);
		}

		Place(Root, FixedWidth(Tree, P, 330.f), FVector2D(1.f, 0.f), FVector2D(1.f, 0.f), FVector2D(-24.f, 24.f));
	}

	// ---------------------------------------------------------------- Top-center: flashing red alert
	{
		AlertPanel = Panel(Tree, AlertBg(), FMargin(16.f, 12.f), 12.f, AlertLine(), 1.5f);
		AlertPanel->SetVisibility(ESlateVisibility::Collapsed);
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		AlertPanel->SetContent(Row);

		USizeBox* Badge = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Badge->SetWidthOverride(30.f);
		Badge->SetHeightOverride(30.f);
		UBorder* BadgeBg = Panel(Tree, Bad(), FMargin(0.f), 15.f, FLinearColor::Transparent, 0.f);
		BadgeBg->SetHorizontalAlignment(HAlign_Center);
		BadgeBg->SetVerticalAlignment(VAlign_Center);
		BadgeBg->SetContent(Text(Tree, TEXT("!"), 18, Ink(), false, true));
		Badge->AddChild(BadgeBg);
		AddH(Row, Badge, 0.f, FMargin(0.f, 0.f, 14.f, 0.f))->SetVerticalAlignment(VAlign_Top);

		UVerticalBox* Lines = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(Lines, Caption(Tree, TEXT("RED ALERT"), Bad(), 11));
		AlertText = Text(Tree, TEXT(""), 14, White(), true);
		AddV(Lines, AlertText);
		AddH(Row, Lines, 1.f, FMargin(0.f));

		Place(Root, FixedWidth(Tree, AlertPanel, 540.f), FVector2D(0.5f, 0.f), FVector2D(0.5f, 0.f), FVector2D(0.f, 24.f));
	}

	// ---------------------------------------------------------------- Bottom-center: interaction prompt
	{
		PromptPanel = Panel(Tree, PanelBg(), FMargin(10.f, 8.f, 18.f, 8.f), 22.f, WithAlpha(Accent(), 0.6f), 1.f);
		PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UBorder* Key = Panel(Tree, White(), FMargin(10.f, 3.f), 6.f, FLinearColor::Transparent, 0.f);
		PromptKeyText = Text(Tree, TEXT("E"), 14, Ink(), false, true);
		PromptKeyText->SetShadowColorAndOpacity(FLinearColor::Transparent);
		Key->SetContent(PromptKeyText);
		AddH(Row, Key, 0.f, FMargin(0.f, 0.f, 12.f, 0.f));
		PromptLabelText = Text(Tree, TEXT("Use the Main Computer"), 15, White(), false, true);
		AddH(Row, PromptLabelText, 0.f, FMargin(0.f));
		PromptPanel->SetContent(Row);
		Place(Root, PromptPanel, FVector2D(0.5f, 1.f), FVector2D(0.5f, 1.f), FVector2D(0.f, -110.f));
	}

	// ---------------------------------------------------------------- Bottom-left: last message
	{
		MessagePanel = Panel(Tree, PanelBg(), FMargin(0.f, 0.f, 16.f, 0.f), 10.f);
		MessagePanel->SetVisibility(ESlateVisibility::Collapsed);
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		USizeBox* Stripe = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Stripe->SetWidthOverride(4.f);
		Stripe->AddChild(Panel(Tree, Accent(), FMargin(0.f), 2.f, FLinearColor::Transparent, 0.f));
		UHorizontalBoxSlot* StripeSlot = AddH(Row, Stripe, 0.f, FMargin(0.f, 0.f, 14.f, 0.f));
		StripeSlot->SetVerticalAlignment(VAlign_Fill);
		MessageText = Text(Tree, TEXT(""), 13, White(), true);
		AddH(Row, MessageText, 1.f, FMargin(0.f, 10.f));
		MessagePanel->SetContent(Row);
		Place(Root, FixedWidth(Tree, MessagePanel, 500.f), FVector2D(0.f, 1.f), FVector2D(0.f, 1.f), FVector2D(24.f, -24.f));
	}

	// ---------------------------------------------------------------- Bottom-right: INFO button
	{
		InfoButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		StyleButton(InfoButton, PanelBg(), Hex(TEXT("1A2436F0")), Hex(TEXT("0B111CFF")), WithAlpha(Accent(), 0.6f), 22.f, FMargin(8.f, 7.f, 14.f, 7.f));
		InfoButton->SetToolTipText(FText::FromString(TEXT("How to play")));
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		USizeBox* Circle = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Circle->SetWidthOverride(26.f);
		Circle->SetHeightOverride(26.f);
		UBorder* CircleBg = Panel(Tree, Accent(), FMargin(0.f), 13.f, FLinearColor::Transparent, 0.f);
		CircleBg->SetHorizontalAlignment(HAlign_Center);
		CircleBg->SetVerticalAlignment(VAlign_Center);
		CircleBg->SetContent(Text(Tree, TEXT("?"), 15, Ink(), false, true));
		Circle->AddChild(CircleBg);
		AddH(Row, Circle, 0.f, FMargin(0.f, 0.f, 10.f, 0.f));
		AddH(Row, Text(Tree, TEXT("HOW TO PLAY"), 12, White(), false, true, 100), 0.f, FMargin(0.f, 0.f, 10.f, 0.f));
		const AMarsMainComputer* C = Computer.Get();
		AddH(Row, KeyCap(Tree, C ? C->InfoKey.GetDisplayName().ToString() : FString(TEXT("I")), 11), 0.f, FMargin(0.f));
		InfoButton->AddChild(Row);
		InfoButton->OnClicked.AddDynamic(this, &UMarsHUDWidget::HandleInfoClicked);
		Place(Root, InfoButton, FVector2D(1.f, 1.f), FVector2D(1.f, 1.f), FVector2D(-24.f, -24.f));
	}

	// ---------------------------------------------------------------- Overlay: How to play
	{
		InfoOverlay = BuildInfoPanel();
		InfoOverlay->SetVisibility(ESlateVisibility::Collapsed);
		FillCanvas(Root, InfoOverlay);
	}
}

void UMarsHUDWidget::AddInfoSection(UVerticalBox* Box, const FString& Number, const FString& Title, const FString& Body)
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	UBorder* SectionCard = Panel(Tree, Card(), FMargin(16.f, 12.f), 10.f, FLinearColor::Transparent, 0.f);
	UVerticalBox* Inner = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SectionCard->SetContent(Inner);

	UHorizontalBox* Head = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	if (!Number.IsEmpty())
	{
		UBorder* Badge = Panel(Tree, Accent(), FMargin(7.f, 1.f), 9.f, FLinearColor::Transparent, 0.f);
		UTextBlock* N = Text(Tree, Number, 11, Ink(), false, true);
		N->SetShadowColorAndOpacity(FLinearColor::Transparent);
		Badge->SetContent(N);
		AddH(Head, Badge, 0.f, FMargin(0.f, 0.f, 10.f, 0.f));
	}
	AddH(Head, Caption(Tree, Title, White(), 12), 1.f, FMargin(0.f));
	AddV(Inner, Head, FMargin(0.f, 0.f, 0.f, 6.f));
	AddV(Inner, Text(Tree, Body, 13, Muted(), true));

	AddV(Box, SectionCard, FMargin(0.f, 0.f, 0.f, 10.f));
}

UWidget* UMarsHUDWidget::BuildInfoPanel()
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	const int32 Days = (C && C->GetDayCount() > 0) ? C->GetDayCount() : 7;
	const int32 Goal = Sim ? FMath::RoundToInt32(Sim->ResearchGoal) : 200;
	const int32 DoseMax = Sim ? FMath::RoundToInt32(Sim->DoseLimit) : 100;
	const int32 LowO2 = Sim ? FMath::RoundToInt32(Sim->OxygenLowThreshold) : 30;
	const int32 MaxAlloc = Sim ? FMath::RoundToInt32(Sim->MaxTotalAllocation * 100.f) : 150;
	const int32 PenR = Sim ? FMath::RoundToInt32(Sim->IgnoredAlertResearchPenalty) : 20;
	const int32 PenH = Sim ? FMath::RoundToInt32(Sim->IgnoredAlertHealthPenalty) : 8;
	const FString UseKey = C ? C->InteractionKey.GetDisplayName().ToString() : FString(TEXT("E"));
	const FString HelpKey = C ? C->InfoKey.GetDisplayName().ToString() : FString(TEXT("I"));
	const bool bPauses = C ? C->bPauseDayWhileInfoOpen : true;

	// Full-screen dim layer that also blocks clicks to the game
	UBorder* DimLayer = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DimLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.65f));
	DimLayer->SetHorizontalAlignment(HAlign_Center);
	DimLayer->SetVerticalAlignment(VAlign_Center);

	USizeBox* WindowSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	WindowSize->SetWidthOverride(1040.f);
	WindowSize->SetHeightOverride(720.f);
	DimLayer->SetContent(WindowSize);

	UBorder* Window = Panel(Tree, WindowBg(), FMargin(28.f, 22.f), 16.f, WithAlpha(Accent(), 0.35f), 1.f);
	WindowSize->AddChild(Window);
	UVerticalBox* Outer = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Window->SetContent(Outer);

	// Header
	{
		UHorizontalBox* Head = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Titles = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(Titles, Caption(Tree, TEXT("MARS BASE FIELD GUIDE"), Accent()));
		AddV(Titles, Text(Tree, TEXT("How to play"), 28, White(), false, true));
		AddH(Head, Titles, 1.f, FMargin(0.f));
		UButton* Close = MakeButton(Tree, FString::Printf(TEXT("GOT IT  [%s]"), *HelpKey), EButtonKind::Primary, 13);
		Close->OnClicked.AddDynamic(this, &UMarsHUDWidget::HandleInfoCloseClicked);
		AddH(Head, Close, 0.f, FMargin(0.f));
		AddV(Outer, Head, FMargin(0.f, 0.f, 0.f, 14.f));
		AddV(Outer, Divider(Tree), FMargin(0.f, 0.f, 0.f, 14.f));
	}

	UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
	AddV(Outer, Scroll, FMargin(0.f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UVerticalBox* Content = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Scroll->AddChild(Content);

	// Goal + little screen map side by side
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		UVerticalBox* GoalBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddInfoSection(GoalBox, FString(), TEXT("YOUR MISSION"), FString::Printf(
			TEXT("You are the commander of a small science base on Mars for %d days. The days are real: Mars weather measured by NASA's Curiosity rover and space-weather events recorded by NASA between 14 and 20 May 2024, a very active week for the Sun.\n\n")
			TEXT("WIN: collect %d research points by the end of the last day.\n")
			TEXT("LOSE: if oxygen runs out, crew health reaches 0, or the crew's radiation dose reaches %d."),
			Days, Goal, DoseMax));
		AddH(Row, GoalBox, 1.f, FMargin(0.f, 0.f, 16.f, 0.f))->SetVerticalAlignment(VAlign_Top);

		// Screen map: numbered boxes match the numbered sections below
		USizeBox* MapSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		MapSize->SetWidthOverride(320.f);
		MapSize->SetHeightOverride(190.f);
		UBorder* MapBg = Panel(Tree, Hex(TEXT("FFFFFF06")), FMargin(0.f), 10.f, Line(), 1.f);
		MapSize->AddChild(MapBg);
		UCanvasPanel* Map = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
		MapBg->SetContent(Map);

		auto Spot = [Tree, Map](const TCHAR* Num, const TCHAR* Label, FVector2D Anchor, FVector2D Offset)
		{
			UBorder* B = Panel(Tree, WithAlpha(Accent(), 0.18f), FMargin(7.f, 4.f), 6.f, WithAlpha(Accent(), 0.7f), 1.f);
			UHorizontalBox* H = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			AddH(H, Text(Tree, Num, 11, Accent(), false, true), 0.f, FMargin(0.f, 0.f, 5.f, 0.f));
			AddH(H, Text(Tree, Label, 10, White(), false), 0.f, FMargin(0.f));
			B->SetContent(H);
			Place(Map, B, Anchor, Anchor, Offset);
		};
		Spot(TEXT("1"), TEXT("Clock"),  FVector2D(0.f, 0.f),   FVector2D(8.f, 8.f));
		Spot(TEXT("3"), TEXT("Alert"),  FVector2D(0.5f, 0.f),  FVector2D(0.f, 8.f));
		Spot(TEXT("2"), TEXT("Status"), FVector2D(1.f, 0.f),   FVector2D(-8.f, 8.f));
		Spot(TEXT("4"), TEXT("Press E"), FVector2D(0.5f, 1.f), FVector2D(0.f, -34.f));
		Spot(TEXT("6"), TEXT("Messages"), FVector2D(0.f, 1.f), FVector2D(8.f, -8.f));
		Spot(TEXT("5"), TEXT("Help"),   FVector2D(1.f, 1.f),   FVector2D(-8.f, -8.f));
		UTextBlock* Screen = Text(Tree, TEXT("YOUR SCREEN"), 10, Dim(), false, true, 150);
		Place(Map, Screen, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, -4.f));

		AddH(Row, MapSize, 0.f, FMargin(0.f))->SetVerticalAlignment(VAlign_Top);
		AddV(Content, Row, FMargin(0.f, 0.f, 0.f, 0.f));
	}

	AddInfoSection(Content, TEXT("1"), TEXT("TOP LEFT  -  MISSION CLOCK"),
		TEXT("Shows which day it is, the sol number (a sol is one Mars day, about 24 hours 40 minutes) and the Earth date. ")
		TEXT("The orange bar fills up as the day goes by. When it is full, the next day begins.\n")
		TEXT("Below it is today's real weather in Gale Crater, measured by the REMS weather station on NASA's Curiosity rover."));

	AddInfoSection(Content, TEXT("2"), TEXT("TOP RIGHT  -  BASE STATUS"), FString::Printf(
		TEXT("\u2022 Battery: stores solar power. It charges when you use less than 100%% of your power and drains when you use more.\n")
		TEXT("\u2022 Oxygen: made by life support. Below %d%% the crew starts to get hurt.\n")
		TEXT("\u2022 Shield: blocks radiation. It wears down every day, and much faster during a solar storm.\n")
		TEXT("\u2022 Crew health: drops from radiation and low oxygen, slowly heals on good days.\n")
		TEXT("\u2022 Radiation dose: all the radiation the crew has absorbed. It never goes down, so protect your crew!\n")
		TEXT("\u2022 Research: your science points. Reach %d to win.\n\n")
		TEXT("The small number next to each meter is the change per day: green means getting better, red means getting worse."),
		LowO2, Goal));

	AddInfoSection(Content, TEXT("3"), TEXT("TOP CENTER  -  RED ALERT"), FString::Printf(
		TEXT("A flashing red box means the base needs you: a new event, a solar storm coming or happening, low oxygen, an empty battery... ")
		TEXT("Walk to the Main Computer and deal with it.\n")
		TEXT("If you don't answer an event before the day ends, the base picks the riskiest answer for you, and you also lose %d research and %d crew health."),
		PenR, PenH));

	AddInfoSection(Content, TEXT("4"), TEXT("BOTTOM CENTER  -  MAIN COMPUTER SIGN"), FString::Printf(
		TEXT("Walk close to the Main Computer and this sign appears. Press [%s] to open the computer. Press [%s] again or CLOSE to walk away."),
		*UseKey, *UseKey));

	AddInfoSection(Content, TEXT("PC"), TEXT("INSIDE THE MAIN COMPUTER"), FString::Printf(
		TEXT("LEFT  -  TODAY'S EVENT: read what is happening and pick one answer. Every answer shows what it will change. ")
		TEXT("After you decide, look at \"What really happened\" to see the real NASA records behind the event.\n\n")
		TEXT("RIGHT  -  POWER SPLIT: your solar panels make a fixed amount of power each day. Use the three sliders to share it between ")
		TEXT("Life support (oxygen), Shield (radiation protection) and Research (science). Giving more to one system leaves less for the others. ")
		TEXT("You can go up to %d%%, but everything above 100%% comes out of the battery.\n")
		TEXT("The EFFECT PER DAY box shows what your current split will do, before time runs on."),
		MaxAlloc));

	AddInfoSection(Content, TEXT("5"), TEXT("BOTTOM RIGHT  -  HOW TO PLAY"), FString::Printf(
		TEXT("Opens this guide. You can also press [%s].%s"),
		*HelpKey, bPauses ? TEXT(" The day clock stops while the guide is open.") : TEXT("")));

	AddInfoSection(Content, TEXT("6"), TEXT("BOTTOM LEFT  -  MESSAGES"),
		TEXT("Tells you what just happened, for example what your last decision did."));

	{
		int32 Seconds = 10, Reward = 5, Uses = 2;
		HudSideTasks::GetSideTaskNumbers(this, Seconds, Reward, Uses);
		AddInfoSection(Content, TEXT("+"), TEXT("SIDE TASKS  -  EXTRA POINTS"), FString::Printf(
			TEXT("Besides the Main Computer, the base has three small station computers. Walk up to one and press [%s], ")
			TEXT("then press and HOLD the button for %d seconds until the bar is full (letting go empties it).\n")
			TEXT("\u2022 Research Lab: +%d research points\n")
			TEXT("\u2022 Power Reactor: +%d battery\n")
			TEXT("\u2022 Life Support Bay: +%d crew health\n")
			TEXT("If the battery or crew health is already full, you gain nothing. %s ")
			TEXT("Doing side tasks while you wait for the next day helps you collect more points!"),
			*UseKey, Seconds, Reward, Reward, Reward,
			Uses > 0 ? *FString::Printf(TEXT("Each station can be used %d times per day."), Uses) : TEXT("")));
	}

	AddInfoSection(Content, FString(), TEXT("COMMANDER TIPS"),
		TEXT("\u2022 When a storm warning appears, move power to the shield BEFORE the storm hits.\n")
		TEXT("\u2022 Calm days are for research and for charging the battery.\n")
		TEXT("\u2022 Keep an eye on oxygen. Running out is the fastest way to lose.\n")
		TEXT("\u2022 Don't put everything into research: one big storm can end the mission.\n")
		TEXT("\u2022 Waiting for the next day? Do the side tasks at the station computers."));

	AddInfoSection(Content, FString(), TEXT("CONTROLS"), FString::Printf(
		TEXT("Move: your normal movement controls%s%s: Main Computer / station computers%s%s: this guide%sMouse: buttons, sliders, hold buttons"),
		HudDot, *UseKey, HudDot, *HelpKey, HudDot));

	AddInfoSection(Content, FString(), TEXT("WHERE THE DATA COMES FROM"),
		TEXT("Space weather: NASA DONKI (Space Weather Database Of Notifications, Knowledge, Information). ")
		TEXT("Mars weather: Curiosity rover REMS station (mars.nasa.gov). Events: NASA mission news from May 2024. ")
		TEXT("The data is archived, not live. Power, oxygen and research numbers are simplified so the game is fair and fun."));

	return DimLayer;
}

// ==================================================================== Updates

void UMarsHUDWidget::SetInteractPromptVisible(bool bVisible, const FKey& Key, const FText& Label)
{
	if (PromptPanel)
	{
		PromptKeyText->SetText(Key.GetDisplayName());
		PromptLabelText->SetText(Label.IsEmpty() ? FText::FromString(TEXT("Use the Main Computer")) : Label);
		PromptPanel->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UMarsHUDWidget::SetInfoVisible(bool bVisible)
{
	if (InfoOverlay)
	{
		InfoOverlay->SetVisibility(bVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (InfoButton)
	{
		InfoButton->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	}
}

void UMarsHUDWidget::HandleSimChanged()
{
	Refresh();
}

void UMarsHUDWidget::Refresh()
{
	using namespace MarsUI;
	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!Sim || !DayText || MeterBars.Num() != Meter_Count)
	{
		return;
	}
	const FMarsColonyState& S = Sim->GetStateRef();
	const FMarsComputerDay* D = C->FindDay(S.Day);

	// Mission clock
	DayText->SetText(FText::FromString(FString::Printf(TEXT("DAY %d / %d"), FMath::Max(1, S.Day), S.DayCount)));
	if (D)
	{
		SolText->SetText(FText::FromString(FString::Printf(TEXT("SOL %d"), D->Sol)));
		DateText->SetText(FText::FromString(FString::Printf(TEXT("%s%sGale Crater, Mars"), *D->Date, HudDot)));
		WeatherText->SetText(FText::FromString(D->WeatherCondition.ToString().ToUpper()));
		TempText->SetText(FText::FromString(FString::Printf(
			TEXT("Air %d\u00B0 to %d\u00B0C%s%d Pa%sUV %s"),
			D->AirTempMinC, D->AirTempMaxC, HudDot, D->PressurePa, HudDot, *D->UVIndex)));
	}
	ClockText->SetText(FText::FromString(S.bGameOver ? FString(TEXT("Mission over"))
		: FString::Printf(TEXT("Next day in %s"), *Clock(S.DayTimeRemaining))));
	DayBar->SetPercent(S.DayProgress);

	// Base status
	auto SetMeter = [this](int32 Index, const FString& Value, float Rate, bool bHigherIsBad, float Percent)
	{
		MeterValues[Index]->SetText(FText::FromString(Value));
		MeterRates[Index]->SetText(FText::FromString(FString::Printf(TEXT("%s/day"), *Signed(Rate))));
		SetTextColor(MeterRates[Index], RateColor(Rate, bHigherIsBad));
		MeterBars[Index]->SetPercent(FMath::Clamp(Percent, 0.f, 1.f));
	};
	auto Num = [](float V) { return FMath::RoundToInt32(V); };

	SetMeter(Meter_Battery, FString::Printf(TEXT("%d/%d"), Num(S.Battery), Num(S.BatteryMax)), S.BatteryRate, false,
		S.BatteryMax > 0.f ? S.Battery / S.BatteryMax : 0.f);
	SetMeter(Meter_Oxygen, FString::Printf(TEXT("%d%%"), Num(S.Oxygen)), S.OxygenRate, false, S.Oxygen / 100.f);
	SetMeter(Meter_Shield, FString::Printf(TEXT("%d%%"), Num(S.Shield)), S.ShieldRate, false, S.Shield / 100.f);
	SetMeter(Meter_Health, FString::Printf(TEXT("%d%%"), Num(S.CrewHealth)), S.HealthRate, false, S.CrewHealth / 100.f);
	SetMeter(Meter_Dose, FString::Printf(TEXT("%d/%d"), Num(S.Dose), Num(S.DoseLimit)), S.DoseRate, true,
		S.DoseLimit > 0.f ? S.Dose / S.DoseLimit : 0.f);
	SetMeter(Meter_Research, FString::Printf(TEXT("%d/%d"), Num(S.Research), Num(S.ResearchGoal)), S.ResearchRate, false,
		S.ResearchGoal > 0.f ? S.Research / S.ResearchGoal : 0.f);

	// Danger colors: bar and value turn red
	const bool bDanger[Meter_Count] = {
		S.Battery <= 0.5f,
		S.Oxygen < Sim->OxygenLowThreshold,
		S.Shield < 15.f,
		S.CrewHealth < 35.f,
		S.Dose >= S.DoseLimit * 0.75f,
		false };
	const FLinearColor Normal[Meter_Count] = { BatteryColor(), OxygenColor(), ShieldColor(), HealthColor(), DoseColor(), ResearchColor() };
	for (int32 i = 0; i < Meter_Count; ++i)
	{
		MeterBars[i]->SetFillColorAndOpacity(bDanger[i] ? Bad() : Normal[i]);
		SetTextColor(MeterValues[i], bDanger[i] ? Bad() : White());
	}
	if (S.Research >= S.ResearchGoal)
	{
		SetTextColor(MeterValues[Meter_Research], Good());
	}

	// Alerts (flashing)
	if (S.Alerts.Num() > 0 && !S.bGameOver)
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
				World->GetTimerManager().SetTimer(FlashTimer, FTimerDelegate::CreateUObject(this, &UMarsHUDWidget::ToggleAlertFlash), 0.6f, true);
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

	// Last message
	MessageText->SetText(S.LastMessage);
	MessagePanel->SetVisibility(S.LastMessage.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UMarsHUDWidget::ToggleAlertFlash()
{
	bFlashOn = !bFlashOn;
	if (AlertPanel)
	{
		AlertPanel->SetRenderOpacity(bFlashOn ? 1.f : 0.55f);
	}
}

void UMarsHUDWidget::HandleGameOver(bool bWon, const FText& Reason)
{
	// The black end screen (result, stats, credits, PLAY AGAIN) is drawn by MarsTransitionWidget.
	Refresh();
	if (PromptPanel)
	{
		PromptPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (InfoButton)
	{
		InfoButton->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (AlertPanel)
	{
		AlertPanel->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UMarsHUDWidget::HandleInfoClicked()
{
	if (AMarsMainComputer* C = Computer.Get())
	{
		C->OpenInfo();
	}
}

void UMarsHUDWidget::HandleInfoCloseClicked()
{
	if (AMarsMainComputer* C = Computer.Get())
	{
		C->CloseInfo();
	}
}
