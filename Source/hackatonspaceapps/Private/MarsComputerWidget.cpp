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

void UMarsComputerWidget::BuildLayout()
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass());
	Tree->RootWidget = Root;

	// Dim the whole screen
	UBorder* Dim = Panel(Tree, FLinearColor(0.f, 0.f, 0.f, 0.55f), FMargin(0.f));
	UCanvasPanelSlot* DimSlot = Root->AddChildToCanvas(Dim);
	DimSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	DimSlot->SetOffsets(FMargin(0.f));

	// Main window
	USizeBox* Window = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Window->SetWidthOverride(1240.f);
	Window->SetHeightOverride(740.f);
	UBorder* Frame = Panel(Tree, FLinearColor(0.03f, 0.06f, 0.09f, 0.97f), FMargin(22.f));
	Window->AddChild(Frame);
	UCanvasPanelSlot* WinSlot = Place(Root, Window, FVector2D(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.f, 0.f));
	WinSlot->SetAutoSize(true);

	UVerticalBox* Outer = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Frame->SetContent(Outer);

	HeaderText = Text(Tree, TEXT("MAIN COMPUTER"), 22, Cyan(), false);
	WeatherText = Text(Tree, TEXT(""), 13, Grey());
	AlertText = Text(Tree, TEXT(""), 15, Red());
	AddV(Outer, HeaderText);
	AddV(Outer, WeatherText);
	AddV(Outer, AlertText, FMargin(0.f, 6.f));

	UHorizontalBox* Columns = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	AddV(Outer, Columns, FMargin(0.f, 8.f))->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// ---- Left column: event (scrollable)
	{
		UBorder* LeftPanel = Panel(Tree, FLinearColor(0.f, 0.f, 0.f, 0.35f), FMargin(14.f));
		UScrollBox* Scroll = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
		EventBox = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		Scroll->AddChild(EventBox);
		LeftPanel->SetContent(Scroll);
		AddH(Columns, LeftPanel, 1.25f, FMargin(0.f, 0.f, 10.f, 0.f));
	}

	// ---- Right column: power split
	{
		UBorder* RightPanel = Panel(Tree, FLinearColor(0.f, 0.f, 0.f, 0.35f), FMargin(14.f));
		UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		RightPanel->SetContent(Box);
		AddH(Columns, RightPanel, 1.f, FMargin(10.f, 0.f, 0.f, 0.f));

		AddV(Box, Text(Tree, TEXT("POWER SPLIT"), 18, Yellow(), false));
		GenerationText = Text(Tree, TEXT(""), 13, Grey());
		AddV(Box, GenerationText, FMargin(0.f, 2.f, 0.f, 10.f));

		auto MakeSlider = [Tree](USlider*& OutSlider)
		{
			OutSlider = Tree->ConstructWidget<USlider>(USlider::StaticClass());
			OutSlider->SetMinValue(0.f);
			OutSlider->SetMaxValue(1.f);
			OutSlider->SetStepSize(0.05f);
			return OutSlider;
		};
		USlider* L = nullptr;
		USlider* Sh = nullptr;
		USlider* R = nullptr;

		LifeLabel = Text(Tree, TEXT(""), 15, Cyan(), false);
		AddV(Box, LifeLabel);
		AddV(Box, MakeSlider(L));
		AddV(Box, Text(Tree, TEXT("Makes oxygen. Like NASA's MOXIE on Perseverance, turning CO2 into oxygen needs electricity."), 12, Grey()), FMargin(0.f, 0.f, 0.f, 10.f));

		ShieldLabel = Text(Tree, TEXT(""), 15, Blue(), false);
		AddV(Box, ShieldLabel);
		AddV(Box, MakeSlider(Sh));
		AddV(Box, Text(Tree, TEXT("Blocks radiation. Mars' thin air can't stop particles from solar storms."), 12, Grey()), FMargin(0.f, 0.f, 0.f, 10.f));

		ResearchLabel = Text(Tree, TEXT(""), 15, Purple(), false);
		AddV(Box, ResearchLabel);
		AddV(Box, MakeSlider(R));
		AddV(Box, Text(Tree, TEXT("Science is the mission's goal. Reach the research target by the end of the week."), 12, Grey()), FMargin(0.f, 0.f, 0.f, 10.f));

		LifeSlider = L;
		ShieldSlider = Sh;
		ResearchSlider = R;
		LifeSlider->OnValueChanged.AddDynamic(this, &UMarsComputerWidget::HandleLifeChanged);
		ShieldSlider->OnValueChanged.AddDynamic(this, &UMarsComputerWidget::HandleShieldChanged);
		ResearchSlider->OnValueChanged.AddDynamic(this, &UMarsComputerWidget::HandleResearchChanged);

		TotalText = Text(Tree, TEXT(""), 14, White());
		PreviewText = Text(Tree, TEXT(""), 14, White());
		GoalText = Text(Tree, TEXT(""), 14, Purple());
		AddV(Box, TotalText, FMargin(0.f, 6.f));
		AddV(Box, PreviewText, FMargin(0.f, 6.f));
		AddV(Box, GoalText, FMargin(0.f, 6.f));

		CloseButton = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		CloseButton->AddChild(Text(Tree, TEXT("CLOSE  [E]"), 16, FLinearColor::Black, false));
		CloseButton->OnClicked.AddDynamic(this, &UMarsComputerWidget::HandleCloseClicked);
		AddV(Box, CloseButton, FMargin(0.f, 14.f, 0.f, 0.f))->SetHorizontalAlignment(HAlign_Right);
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
	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!Sim || !HeaderText)
	{
		return;
	}
	const FMarsColonyState& S = Sim->GetStateRef();
	const FMarsComputerDay* D = C->FindDay(S.Day);

	if (D)
	{
		HeaderText->SetText(FText::FromString(FString::Printf(TEXT("MAIN COMPUTER  -  Day %d / %d  -  Sol %d  -  %s  -  next day in %s"),
			S.Day, S.DayCount, D->Sol, *D->Date, *MarsUI::Clock(S.DayTimeRemaining))));
		WeatherText->SetText(FText::FromString(FString::Printf(
			TEXT("Gale Crater weather (Curiosity REMS): %s, air %d / %d C, ground %d / %d C, pressure %d Pa, UV %s"),
			*D->WeatherCondition.ToString(), D->AirTempMinC, D->AirTempMaxC, D->GroundTempMinC, D->GroundTempMaxC, D->PressurePa, *D->UVIndex)));
	}

	FString Alerts;
	for (const FText& A : S.Alerts)
	{
		Alerts += (Alerts.IsEmpty() ? TEXT("") : TEXT("\n")) + A.ToString();
	}
	AlertText->SetText(FText::FromString(Alerts));

	// Power panel
	const float G = S.PowerGeneration;
	GenerationText->SetText(FText::FromString(FString::Printf(TEXT("Solar power today: %d units/day. Battery: %d / %d."),
		FMath::RoundToInt32(G), FMath::RoundToInt32(S.Battery), FMath::RoundToInt32(S.BatteryMax))));
	LifeLabel->SetText(FText::FromString(FString::Printf(TEXT("Life support  %d%%  (%d power/day, needs %d)"),
		FMath::RoundToInt32(S.AllocLifeSupport * 100.f), FMath::RoundToInt32(S.AllocLifeSupport * G), FMath::RoundToInt32(Sim->OxygenNeedPerDay))));
	ShieldLabel->SetText(FText::FromString(FString::Printf(TEXT("Radiation shield  %d%%  (%d power/day)"),
		FMath::RoundToInt32(S.AllocShield * 100.f), FMath::RoundToInt32(S.AllocShield * G))));
	ResearchLabel->SetText(FText::FromString(FString::Printf(TEXT("Research  %d%%  (%d power/day)"),
		FMath::RoundToInt32(S.AllocResearch * 100.f), FMath::RoundToInt32(S.AllocResearch * G))));

	const int32 TotalPct = FMath::RoundToInt32((S.AllocLifeSupport + S.AllocShield + S.AllocResearch) * 100.f);
	FString Total = FString::Printf(TEXT("Total: %d%% of solar power."), TotalPct);
	if (S.bPowerShortage)
	{
		Total += TEXT(" Battery is EMPTY: every system gets less power!");
	}
	else if (S.bOverdrive)
	{
		Total += TEXT(" Over 100%: the battery is draining.");
	}
	else if (TotalPct < 100)
	{
		Total += TEXT(" The rest charges the battery.");
	}
	TotalText->SetText(FText::FromString(Total));
	MarsUI::SetTextColor(TotalText, S.bPowerShortage ? MarsUI::Red() : (S.bOverdrive ? MarsUI::Orange() : MarsUI::White()));

	PreviewText->SetText(FText::FromString(FString::Printf(
		TEXT("Effect per day right now:\n  Oxygen %s   Shield %s   Battery %s\n  Radiation dose %s   Crew health %s   Research %s\n  Radiation level now: %.1fx%s"),
		*MarsUI::Signed(S.OxygenRate), *MarsUI::Signed(S.ShieldRate), *MarsUI::Signed(S.BatteryRate),
		*MarsUI::Signed(S.DoseRate), *MarsUI::Signed(S.HealthRate), *MarsUI::Signed(S.ResearchRate),
		S.RadiationMultiplier, S.bStormActive ? TEXT("  (STORM)") : TEXT(""))));
	GoalText->SetText(FText::FromString(FString::Printf(TEXT("Research: %d / %d    Crew dose: %d / %d    Crew health: %d%%"),
		FMath::RoundToInt32(S.Research), FMath::RoundToInt32(S.ResearchGoal), FMath::RoundToInt32(S.Dose), FMath::RoundToInt32(S.DoseLimit),
		FMath::RoundToInt32(S.CrewHealth))));

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
		AddV(EventBox, Text(Tree, TEXT("NO INCIDENTS TODAY"), 20, Green(), false));
		AddV(EventBox, Text(Tree, TEXT("A calm day. Use it to charge the battery and the shield, or push research."), 15, White()));
		if (!S.LastMessage.IsEmpty())
		{
			AddV(EventBox, Text(Tree, S.LastMessage.ToString(), 13, Grey()), FMargin(0.f, 12.f, 0.f, 0.f));
		}
		return;
	}

	AddV(EventBox, Text(Tree, D->EventTitle.ToString().ToUpper(), 20, Red()));
	AddV(EventBox, Text(Tree, D->EventDescription.ToString(), 15, White()), FMargin(0.f, 4.f, 0.f, 10.f));

	if (S.bEventPending)
	{
		AddV(EventBox, Text(Tree, TEXT("DECIDE:"), 16, Yellow(), false));
		for (int32 i = 0; i < D->Choices.Num(); ++i)
		{
			UMarsIndexedButton* Button = Tree->ConstructWidget<UMarsIndexedButton>(UMarsIndexedButton::StaticClass());
			Button->Setup(i);
			Button->OnIndexedClicked.AddUObject(this, &UMarsComputerWidget::HandleChoiceClicked);
			const FString Label = FString::Printf(TEXT("%s\n%s"), *D->Choices[i].Text.ToString(), *Sim->DescribeChoiceEffects(S.Day, i).ToString());
			Button->AddChild(Text(Tree, Label, 15, FLinearColor::Black));
			AddV(EventBox, Button, FMargin(0.f, 4.f));
		}
	}
	else
	{
		AddV(EventBox, Text(Tree, TEXT("DECISION MADE"), 16, Green(), false));
		AddV(EventBox, Text(Tree, S.LastMessage.ToString(), 14, White()));
	}

	// Official sources (real NASA records behind this event)
	AddV(EventBox, Text(Tree, TEXT("WHAT REALLY HAPPENED (official records):"), 14, Cyan(), false), FMargin(0.f, 14.f, 0.f, 2.f));
	for (int32 i = 0; i < D->RecordInfos.Num(); ++i)
	{
		const FString Type = D->RecordSourceTypes.IsValidIndex(i) ? D->RecordSourceTypes[i].ToString() : FString();
		const FString Link = D->RecordLinks.IsValidIndex(i) ? D->RecordLinks[i] : FString();
		AddV(EventBox, Text(Tree, FString::Printf(TEXT("- [%s] %s"), *Type, *D->RecordInfos[i].ToString()), 13, White()));
		AddV(EventBox, Text(Tree, Link, 11, Grey()), FMargin(12.f, 0.f, 0.f, 4.f));
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
