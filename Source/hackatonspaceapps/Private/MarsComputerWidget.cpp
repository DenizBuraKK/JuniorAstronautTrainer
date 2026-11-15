// MarsComputerWidget.cpp

#include "MarsComputerWidget.h"
#include "MarsColonySimulation.h"
#include "MarsIndexedButton.h"
#include "MarsMainComputer.h"
#include "MarsUIHelpers.h"

#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Engine/World.h"
#include "TimerManager.h"

void UMarsComputerWidget::Init(AMarsMainComputer* InComputer)
{
	Computer = InComputer;
	if (UMarsColonySimulation* Sim = InComputer ? InComputer->GetSimulation() : nullptr)
	{
		SimChangedHandle = Sim->OnStateChangedNative.AddUObject(this, &UMarsComputerWidget::HandleSimChanged);
	}
}

TSharedRef<SWidget> UMarsComputerWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UMarsComputerWidget::BeginDestroy()
{
	// The same widget is reused every time the screen opens, so unbind only when it is destroyed.
	if (AMarsMainComputer* C = Computer.Get())
	{
		if (UMarsColonySimulation* Sim = C->GetSimulation())
		{
			Sim->OnStateChangedNative.Remove(SimChangedHandle);
		}
	}
	Super::BeginDestroy();
}

void UMarsComputerWidget::OnOpened()
{
	BuiltForDay = -1;   // force a fresh event section
	if (const AMarsMainComputer* C = Computer.Get())
	{
		if (const UMarsColonySimulation* Sim = C->GetSimulation())
		{
			const FMarsColonyState& S = Sim->GetStateRef();
			if (LifeSlider)
			{
				LifeSlider->SetValue(S.AllocLifeSupport);
				ShieldSlider->SetValue(S.AllocShield);
				ResearchSlider->SetValue(S.AllocResearch);
			}
		}
	}
	Refresh();
}

namespace
{
	const TCHAR* PcDot = TEXT("  \u00B7  ");
	enum EPreview : int32 { Prev_Oxygen, Prev_Shield, Prev_Battery, Prev_Dose, Prev_Health, Prev_Research, Prev_Count };
}

UWidget* UMarsComputerWidget::MakeSliderBlock(const FString& Title, const FString& Hint, const FLinearColor& Color,
	USlider*& OutSlider, UTextBlock*& OutValue)
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	UBorder* Block = Panel(Tree, Card(), FMargin(14.f, 10.f), 10.f, FLinearColor::Transparent, 0.f);
	UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Block->SetContent(Box);

	UHorizontalBox* Head = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	USizeBox* DotBox = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	DotBox->SetWidthOverride(10.f);
	DotBox->SetHeightOverride(10.f);
	DotBox->AddChild(Panel(Tree, Color, FMargin(0.f), 5.f, FLinearColor::Transparent, 0.f));
	AddH(Head, DotBox, 0.f, FMargin(0.f, 0.f, 10.f, 0.f));
	AddH(Head, Text(Tree, Title, 13, White(), false, true, 80), 1.f, FMargin(0.f));
	OutValue = Text(Tree, TEXT(""), 13, Color, false, true);
	AddH(Head, OutValue, 0.f, FMargin(0.f));
	AddV(Box, Head);

	USlider* Slider = Tree->ConstructWidget<USlider>(USlider::StaticClass());
	Slider->SetMinValue(0.f);
	Slider->SetMaxValue(1.f);
	Slider->SetStepSize(0.05f);
	Slider->SetSliderBarColor(Track());
	Slider->SetSliderHandleColor(Color);
	AddV(Box, Slider, FMargin(0.f, 6.f));
	AddV(Box, Text(Tree, Hint, 11, Dim(), true));

	OutSlider = Slider;
	return Block;
}

void UMarsComputerWidget::BuildLayout()
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Tree->RootWidget = Root;

	// Dim the whole screen, window in the middle
	UBorder* DimLayer = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	DimLayer->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	DimLayer->SetHorizontalAlignment(HAlign_Center);
	DimLayer->SetVerticalAlignment(VAlign_Center);
	FillCanvas(Root, DimLayer);

	USizeBox* Window = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Window->SetWidthOverride(1200.f);
	Window->SetHeightOverride(740.f);
	DimLayer->SetContent(Window);
	UBorder* Frame = Panel(Tree, WindowBg(), FMargin(26.f, 22.f), 16.f, WithAlpha(Accent(), 0.35f), 1.f);
	Window->AddChild(Frame);

	UVerticalBox* Outer = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->SetContent(Outer);

	// ---- Header: title, clock, close
	{
		UHorizontalBox* Head = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UVerticalBox* Titles = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(Titles, Caption(Tree, TEXT("MAIN COMPUTER  \u00B7  GALE CRATER BASE"), Accent()));
		HeaderText = Text(Tree, TEXT(""), 24, White(), false, true);
		AddV(Titles, HeaderText);
		WeatherText = Text(Tree, TEXT(""), 12, Muted(), true);
		AddV(Titles, WeatherText);
		AddH(Head, Titles, 1.f, FMargin(0.f));

		UBorder* ClockPill = Panel(Tree, Card(), FMargin(14.f, 6.f), 16.f, Line(), 1.f);
		ClockText = Text(Tree, TEXT(""), 13, White(), false, true);
		ClockPill->SetContent(ClockText);
		AddH(Head, ClockPill, 0.f, FMargin(0.f, 0.f, 12.f, 0.f));

		const AMarsMainComputer* C = Computer.Get();
		CloseButton = MakeButton(Tree, FString::Printf(TEXT("CLOSE  [%s]"),
			C ? *C->InteractionKey.GetDisplayName().ToString() : TEXT("E")), EButtonKind::Ghost, 13);
		CloseButton->OnClicked.AddDynamic(this, &UMarsComputerWidget::HandleCloseClicked);
		AddH(Head, CloseButton, 0.f, FMargin(0.f));
		AddV(Outer, Head, FMargin(0.f, 0.f, 0.f, 12.f));
	}

	// ---- Alerts
	{
		AlertPanel = Panel(Tree, AlertBg(), FMargin(14.f, 8.f), 10.f, AlertLine(), 1.f);
		AlertText = Text(Tree, TEXT(""), 13, White(), true);
		AlertPanel->SetContent(AlertText);
		AlertPanel->SetVisibility(ESlateVisibility::Collapsed);
		AddV(Outer, AlertPanel, FMargin(0.f, 0.f, 0.f, 12.f));
	}

	UHorizontalBox* Columns = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddV(Outer, Columns, FMargin(0.f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// ---- Left column: today's event (scrollable)
	{
		UBorder* LeftPanel = Panel(Tree, Hex(TEXT("FFFFFF06")), FMargin(18.f, 14.f), 12.f, Line(), 1.f);
		UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		EventBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(EventBox);
		LeftPanel->SetContent(Scroll);
		AddH(Columns, LeftPanel, 1.2f, FMargin(0.f, 0.f, 10.f, 0.f))->SetVerticalAlignment(VAlign_Fill);
	}

	// ---- Right column: power split
	{
		UBorder* RightPanel = Panel(Tree, Hex(TEXT("FFFFFF06")), FMargin(18.f, 14.f), 12.f, Line(), 1.f);
		UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(Box);
		RightPanel->SetContent(Scroll);
		AddH(Columns, RightPanel, 1.f, FMargin(10.f, 0.f, 0.f, 0.f))->SetVerticalAlignment(VAlign_Fill);

		AddV(Box, Caption(Tree, TEXT("POWER SPLIT"), Accent()));
		GenerationText = Text(Tree, TEXT(""), 12, Muted(), true);
		AddV(Box, GenerationText, FMargin(0.f, 2.f, 0.f, 10.f));

		USlider* L = nullptr;
		USlider* Sh = nullptr;
		USlider* R = nullptr;
		UTextBlock* LV = nullptr;
		UTextBlock* SV = nullptr;
		UTextBlock* RV = nullptr;
		AddV(Box, MakeSliderBlock(TEXT("LIFE SUPPORT"),
			TEXT("Makes oxygen. Like NASA's MOXIE on Perseverance, turning CO2 into oxygen needs electricity."),
			OxygenColor(), L, LV), FMargin(0.f, 0.f, 0.f, 8.f));
		AddV(Box, MakeSliderBlock(TEXT("RADIATION SHIELD"),
			TEXT("Blocks radiation. Mars' thin air can't stop the particles from solar storms."),
			ShieldColor(), Sh, SV), FMargin(0.f, 0.f, 0.f, 8.f));
		AddV(Box, MakeSliderBlock(TEXT("RESEARCH"),
			TEXT("Science is the reason you are here. Reach the research goal by the end of the week."),
			ResearchColor(), R, RV), FMargin(0.f, 0.f, 0.f, 8.f));
		LifeSlider = L;
		ShieldSlider = Sh;
		ResearchSlider = R;
		LifeValue = LV;
		ShieldValue = SV;
		ResearchValue = RV;
		LifeSlider->OnValueChanged.AddDynamic(this, &UMarsComputerWidget::HandleLifeChanged);
		ShieldSlider->OnValueChanged.AddDynamic(this, &UMarsComputerWidget::HandleShieldChanged);
		ResearchSlider->OnValueChanged.AddDynamic(this, &UMarsComputerWidget::HandleResearchChanged);

		// Total used
		UProgressBar* TB = nullptr;
		AddV(Box, Bar(Tree, Good(), 6.f, TB), FMargin(0.f, 4.f, 0.f, 4.f));
		TotalBar = TB;
		TotalText = Text(Tree, TEXT(""), 12, White(), true);
		AddV(Box, TotalText, FMargin(0.f, 0.f, 0.f, 12.f));

		// Effect per day preview (2 rows x 3 cells)
		AddV(Box, Caption(Tree, TEXT("EFFECT PER DAY"), Muted()), FMargin(0.f, 0.f, 0.f, 6.f));
		const TCHAR* Labels[Prev_Count] = { TEXT("OXYGEN"), TEXT("SHIELD"), TEXT("BATTERY"), TEXT("RADIATION DOSE"), TEXT("CREW HEALTH"), TEXT("RESEARCH") };
		PreviewValues.Reset();
		for (int32 RowIndex = 0; RowIndex < 2; ++RowIndex)
		{
			UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
			for (int32 Col = 0; Col < 3; ++Col)
			{
				const int32 Index = RowIndex * 3 + Col;
				UBorder* Cell = Panel(Tree, Card(), FMargin(10.f, 6.f), 8.f, FLinearColor::Transparent, 0.f);
				UVerticalBox* CellBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
				AddV(CellBox, Text(Tree, Labels[Index], 9, Dim(), false, true, 80), FMargin(0.f));
				UTextBlock* V = Text(Tree, TEXT(""), 16, White(), false, true);
				AddV(CellBox, V, FMargin(0.f));
				Cell->SetContent(CellBox);
				AddH(Row, Cell, 1.f, FMargin(Col == 0 ? 0.f : 3.f, 0.f, Col == 2 ? 0.f : 3.f, 0.f));
				PreviewValues.Add(V);
			}
			AddV(Box, Row, FMargin(0.f, 0.f, 0.f, 6.f));
		}
		StormText = Text(Tree, TEXT(""), 12, Muted(), true);
		AddV(Box, StormText, FMargin(0.f, 2.f, 0.f, 10.f));

		AddV(Box, Divider(Tree), FMargin(0.f, 0.f, 0.f, 8.f));
		GoalText = Text(Tree, TEXT(""), 12, Muted(), true);
		AddV(Box, GoalText);
	}
}

void UMarsComputerWidget::HandleSimChanged()
{
	if (IsInViewport())
	{
		Refresh();
	}
}

void UMarsComputerWidget::Refresh()
{
	using namespace MarsUI;
	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!Sim || !HeaderText || PreviewValues.Num() != Prev_Count)
	{
		return;
	}
	const FMarsColonyState& S = Sim->GetStateRef();
	const FMarsComputerDay* D = C->FindDay(S.Day);

	if (D)
	{
		HeaderText->SetText(FText::FromString(FString::Printf(TEXT("Day %d of %d%sSol %d%s%s"),
			S.Day, S.DayCount, PcDot, D->Sol, PcDot, *D->Date)));
		WeatherText->SetText(FText::FromString(FString::Printf(
			TEXT("Today's weather (Curiosity REMS): %s, air %d\u00B0 to %d\u00B0C, ground %d\u00B0 to %d\u00B0C, %d Pa, UV %s"),
			*D->WeatherCondition.ToString(), D->AirTempMinC, D->AirTempMaxC, D->GroundTempMinC, D->GroundTempMaxC, D->PressurePa, *D->UVIndex)));
	}
	ClockText->SetText(FText::FromString(FString::Printf(TEXT("Next day in %s"), *Clock(S.DayTimeRemaining))));

	// Alerts
	FString Alerts;
	for (const FText& A : S.Alerts)
	{
		Alerts += (Alerts.IsEmpty() ? TEXT("") : TEXT("\n")) + A.ToString();
	}
	AlertText->SetText(FText::FromString(Alerts));
	AlertPanel->SetVisibility(Alerts.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	// Power split
	const float G = S.PowerGeneration;
	GenerationText->SetText(FText::FromString(FString::Printf(
		TEXT("Your solar panels make %d power units per day. Share them between the three systems. Battery: %d / %d."),
		FMath::RoundToInt32(G), FMath::RoundToInt32(S.Battery), FMath::RoundToInt32(S.BatteryMax))));
	auto PowerLabel = [G](float Alloc)
	{
		return FText::FromString(FString::Printf(TEXT("%d%%  \u00B7  %d/day"), FMath::RoundToInt32(Alloc * 100.f), FMath::RoundToInt32(Alloc * G)));
	};
	LifeValue->SetText(PowerLabel(S.AllocLifeSupport));
	ShieldValue->SetText(PowerLabel(S.AllocShield));
	ResearchValue->SetText(PowerLabel(S.AllocResearch));

	const float TotalAlloc = S.AllocLifeSupport + S.AllocShield + S.AllocResearch;
	const int32 TotalPct = FMath::RoundToInt32(TotalAlloc * 100.f);
	FString Total = FString::Printf(TEXT("Using %d%% of today's solar power."), TotalPct);
	FLinearColor TotalColor = Good();
	if (S.bPowerShortage)
	{
		Total += TEXT(" Battery is EMPTY: every system gets less power!");
		TotalColor = Bad();
	}
	else if (S.bOverdrive)
	{
		Total += TEXT(" Over 100%: the battery is draining.");
		TotalColor = Warn();
	}
	else if (TotalPct < 100)
	{
		Total += TEXT(" The rest charges the battery.");
	}
	TotalText->SetText(FText::FromString(Total));
	SetTextColor(TotalText, TotalColor == Good() ? Muted() : TotalColor);
	TotalBar->SetPercent(Sim->MaxTotalAllocation > 0.f ? FMath::Clamp(TotalAlloc / Sim->MaxTotalAllocation, 0.f, 1.f) : 0.f);
	TotalBar->SetFillColorAndOpacity(TotalColor);

	// Effect per day
	const float Rates[Prev_Count] = { S.OxygenRate, S.ShieldRate, S.BatteryRate, S.DoseRate, S.HealthRate, S.ResearchRate };
	for (int32 i = 0; i < Prev_Count; ++i)
	{
		PreviewValues[i]->SetText(FText::FromString(Signed(Rates[i])));
		SetTextColor(PreviewValues[i], RateColor(Rates[i], i == Prev_Dose));
	}
	StormText->SetText(FText::FromString(FString::Printf(TEXT("Radiation level right now: %.1fx normal%s"),
		S.RadiationMultiplier, S.bStormActive ? TEXT("  \u00B7  SOLAR STORM ACTIVE") : (S.bStormIncoming ? TEXT("  \u00B7  storm coming later today") : TEXT("")))));
	SetTextColor(StormText, S.bStormActive ? Bad() : (S.bStormIncoming ? Warn() : Muted()));

	GoalText->SetText(FText::FromString(FString::Printf(TEXT("Research %d / %d%sCrew dose %d / %d%sCrew health %d%%"),
		FMath::RoundToInt32(S.Research), FMath::RoundToInt32(S.ResearchGoal), PcDot,
		FMath::RoundToInt32(S.Dose), FMath::RoundToInt32(S.DoseLimit), PcDot, FMath::RoundToInt32(S.CrewHealth))));

	// Rebuild the event section only when the day or its pending state changed.
	if (BuiltForDay != S.Day || bBuiltWithPending != S.bEventPending)
	{
		RebuildEventSection();
	}
}

void UMarsComputerWidget::RebuildEventSection()
{
	using namespace MarsUI;
	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!Sim || !EventBox)
	{
		return;
	}
	const FMarsColonyState& S = Sim->GetStateRef();
	const FMarsComputerDay* D = C->FindDay(S.Day);
	BuiltForDay = S.Day;
	bBuiltWithPending = S.bEventPending;
	EventBox->ClearChildren();
	UWidgetTree* Tree = WidgetTree;

	if (!D || !D->bHasEvent)
	{
		AddV(EventBox, Caption(Tree, TEXT("ALL CLEAR"), Good()));
		AddV(EventBox, Text(Tree, TEXT("No incidents today"), 22, White(), true, true), FMargin(0.f, 2.f, 0.f, 8.f));
		AddV(EventBox, Text(Tree, TEXT("A calm day on Mars. Use it to charge the battery and the shield, or push your research forward."), 14, Muted()));
		if (!S.LastMessage.IsEmpty())
		{
			AddV(EventBox, Text(Tree, S.LastMessage.ToString(), 12, Dim()), FMargin(0.f, 14.f, 0.f, 0.f));
		}
		return;
	}

	AddV(EventBox, Caption(Tree, TEXT("TODAY'S EVENT"), Bad()));
	AddV(EventBox, Text(Tree, D->EventTitle.ToString(), 22, White(), true, true), FMargin(0.f, 2.f, 0.f, 6.f));
	AddV(EventBox, Text(Tree, D->EventDescription.ToString(), 14, Muted()), FMargin(0.f, 0.f, 0.f, 14.f));

	if (S.bEventPending)
	{
		AddV(EventBox, Caption(Tree, TEXT("CHOOSE ONE"), Warn()), FMargin(0.f, 0.f, 0.f, 4.f));
		for (int32 i = 0; i < D->Choices.Num(); ++i)
		{
			UMarsIndexedButton* Button = Tree->ConstructWidget<UMarsIndexedButton>(UMarsIndexedButton::StaticClass());
			StyleButtonKind(Button, EButtonKind::Choice);
			Button->Setup(i);
			Button->OnIndexedClicked.AddUObject(this, &UMarsComputerWidget::HandleChoiceClicked);

			UVerticalBox* Content = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			AddV(Content, Text(Tree, D->Choices[i].Text.ToString(), 14, White(), true, true), FMargin(0.f));
			AddV(Content, Text(Tree, Sim->DescribeChoiceEffects(S.Day, i).ToString(), 12, Muted(), true), FMargin(0.f, 3.f, 0.f, 0.f));
			Button->AddChild(Content);
			AlignButtonContent(Button, HAlign_Fill);
			AddV(EventBox, Button, FMargin(0.f, 4.f));
		}
	}
	else
	{
		UBorder* Done = Panel(Tree, WithAlpha(Good(), 0.10f), FMargin(14.f, 10.f), 10.f, WithAlpha(Good(), 0.45f), 1.f);
		UVerticalBox* DoneBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(DoneBox, Caption(Tree, TEXT("DECISION MADE"), Good()));
		AddV(DoneBox, Text(Tree, S.LastMessage.ToString(), 13, White()));
		Done->SetContent(DoneBox);
		AddV(EventBox, Done, FMargin(0.f, 0.f, 0.f, 4.f));
	}

	// Official sources (real NASA records behind this event)
	if (D->RecordInfos.Num() > 0)
	{
		AddV(EventBox, Caption(Tree, TEXT("WHAT REALLY HAPPENED  \u00B7  OFFICIAL RECORDS"), OxygenColor()), FMargin(0.f, 18.f, 0.f, 6.f));
		for (int32 i = 0; i < D->RecordInfos.Num(); ++i)
		{
			const FString Type = D->RecordSourceTypes.IsValidIndex(i) ? D->RecordSourceTypes[i].ToString() : FString();
			const FString Link = D->RecordLinks.IsValidIndex(i) ? D->RecordLinks[i] : FString();

			UBorder* Rec = Panel(Tree, Card(), FMargin(12.f, 8.f), 8.f, FLinearColor::Transparent, 0.f);
			UVerticalBox* RecBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
			if (!Type.IsEmpty())
			{
				AddV(RecBox, Pill(Tree, Type.ToUpper(), OxygenColor()), FMargin(0.f, 0.f, 0.f, 4.f))->SetHorizontalAlignment(HAlign_Left);
			}
			AddV(RecBox, Text(Tree, D->RecordInfos[i].ToString(), 13, White()), FMargin(0.f));
			if (!Link.IsEmpty())
			{
				AddV(RecBox, Text(Tree, Link, 10, Dim()), FMargin(0.f, 3.f, 0.f, 0.f));
			}
			Rec->SetContent(RecBox);
			AddV(EventBox, Rec, FMargin(0.f, 0.f, 0.f, 6.f));
		}
	}
}

void UMarsComputerWidget::HandleChoiceClicked(int32 ChoiceIndex)
{
	// Resolve next frame: resolving rebuilds the choice list, which would remove
	// the very button that is still handling this click.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(
			FTimerDelegate::CreateUObject(this, &UMarsComputerWidget::ResolveChoiceDeferred, ChoiceIndex));
	}
}

void UMarsComputerWidget::ResolveChoiceDeferred(int32 ChoiceIndex)
{
	AMarsMainComputer* C = Computer.Get();
	UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (Sim)
	{
		FText Result;
		Sim->ResolveEvent(ChoiceIndex, Result);   // broadcasts -> Refresh -> event section rebuilt
	}
}

void UMarsComputerWidget::PushSlidersToSimulation()
{
	AMarsMainComputer* C = Computer.Get();
	UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!Sim || !LifeSlider)
	{
		return;
	}
	Sim->SetAllocation(LifeSlider->GetValue(), ShieldSlider->GetValue(), ResearchSlider->GetValue());

	// The simulation may scale values down (total cap): show what it really uses.
	const FMarsColonyState& S = Sim->GetStateRef();
	LifeSlider->SetValue(S.AllocLifeSupport);
	ShieldSlider->SetValue(S.AllocShield);
	ResearchSlider->SetValue(S.AllocResearch);
}

void UMarsComputerWidget::HandleLifeChanged(float Value)     { PushSlidersToSimulation(); }
void UMarsComputerWidget::HandleShieldChanged(float Value)   { PushSlidersToSimulation(); }
void UMarsComputerWidget::HandleResearchChanged(float Value) { PushSlidersToSimulation(); }

void UMarsComputerWidget::HandleCloseClicked()
{
	if (AMarsMainComputer* C = Computer.Get())
	{
		C->CloseComputer();
	}
}
