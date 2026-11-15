// MarsStartWidget.cpp

#include "MarsStartWidget.h"
#include "MarsColonySimulation.h"
#include "MarsMainComputer.h"
#include "MarsUIHelpers.h"

void UMarsStartWidget::Init(AMarsMainComputer* InComputer)
{
	Computer = InComputer;
}

TSharedRef<SWidget> UMarsStartWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildLayout();
	}
	return Super::RebuildWidget();
}

void UMarsStartWidget::BuildLayout()
{
	using namespace MarsUI;
	UWidgetTree* Tree = WidgetTree;

	const AMarsMainComputer* C = Computer.Get();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	const int32 Days = (C && C->GetDayCount() > 0) ? C->GetDayCount() : 7;
	const int32 Goal = Sim ? FMath::RoundToInt32(Sim->ResearchGoal) : 200;
	const FString HelpKey = C ? C->InfoKey.GetDisplayName().ToString() : FString(TEXT("I"));

	// Full black background (Visible = blocks clicks to the game behind it)
	UBorder* BlackBg = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
	BlackBg->SetBrushColor(FLinearColor::Black);
	BlackBg->SetHorizontalAlignment(HAlign_Center);
	BlackBg->SetVerticalAlignment(VAlign_Center);
	BlackBg->SetVisibility(ESlateVisibility::Visible);
	Tree->RootWidget = BlackBg;

	UVerticalBox* Box = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	BlackBg->SetContent(FixedWidth(Tree, Box, 860.f));

	// Title
	UTextBlock* Top = Caption(Tree, TEXT("NASA SPACE APPS CHALLENGE"), Accent(), 13);
	Top->SetJustification(ETextJustify::Center);
	AddV(Box, Top);

	UTextBlock* Title = Text(Tree, TEXT("MARS MISSION SIMULATION"), 54, White(), true, true, 160);
	Title->SetJustification(ETextJustify::Center);
	AddV(Box, Title, FMargin(0.f, 8.f, 0.f, 4.f));

	UTextBlock* Sub = Text(Tree, FString::Printf(TEXT("Gale Crater, Mars  \u00B7  %d sols  \u00B7  14 - 20 May 2024"), Days), 15, Muted(), false);
	Sub->SetJustification(ETextJustify::Center);
	AddV(Box, Sub, FMargin(0.f, 0.f, 0.f, 26.f));

	// Mission
	{
		UBorder* Card = Panel(Tree, Hex(TEXT("FFFFFF0D")), FMargin(22.f, 16.f), 12.f, Line(), 1.f);
		UVerticalBox* In = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(In, Caption(Tree, TEXT("YOUR MISSION"), Accent(), 12), FMargin(0.f, 0.f, 0.f, 8.f));
		AddV(In, Text(Tree, FString::Printf(
			TEXT("Keep your crew alive and reach %d research points by the end of the %d days."), Goal, Days),
			17, White(), true, true), FMargin(0.f, 0.f, 0.f, 10.f));
		AddV(In, Text(Tree,
			TEXT("Real astronauts face problems like these in space: solar storms, dangerous radiation, limited power and oxygen. ")
			TEXT("This game is a simple simulation of those challenges. Share your power wisely, answer the alerts at the Main Computer ")
			TEXT("and bring your crew and your base safely to the end of the week."),
			14, Muted(), true));
		Card->SetContent(In);
		AddV(Box, Card, FMargin(0.f, 0.f, 0.f, 14.f));
	}

	// Note about the data
	{
		UBorder* Card = Panel(Tree, WithAlpha(Warn(), 0.08f), FMargin(22.f, 14.f), 12.f, WithAlpha(Warn(), 0.45f), 1.f);
		UVerticalBox* In = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		AddV(In, Caption(Tree, TEXT("PLEASE NOTE"), Warn(), 12), FMargin(0.f, 0.f, 0.f, 6.f));
		AddV(In, Text(Tree,
			TEXT("The events in this game are largely based on real NASA data and records from May 2024. ")
			TEXT("Some events and numbers have been changed or estimated to make the game more fun to play."),
			14, White(), true));
		Card->SetContent(In);
		AddV(Box, Card, FMargin(0.f, 0.f, 0.f, 14.f));
	}

	UTextBlock* Hint = Text(Tree, FString::Printf(TEXT("Tip: press [%s] or HOW TO PLAY at any time to see how everything works."), *HelpKey), 13, Dim(), true);
	Hint->SetJustification(ETextJustify::Center);
	AddV(Box, Hint, FMargin(0.f, 4.f, 0.f, 26.f));

	// Buttons: red EXIT, green START
	{
		UHorizontalBox* Row = Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

		auto MakeColored = [Tree](const TCHAR* Label, const FLinearColor& Color, const FLinearColor& Hover, const FLinearColor& Pressed)
		{
			UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
			StyleButton(B, Color, Hover, Pressed, FLinearColor::Transparent, 10.f, FMargin(0.f, 12.f));
			UTextBlock* T = Text(Tree, Label, 18, Ink(), false, true, 200);
			T->SetShadowColorAndOpacity(FLinearColor::Transparent);
			B->AddChild(T);
			AlignButtonContent(B, HAlign_Center);
			return B;
		};

		UButton* ExitButton = MakeColored(TEXT("EXIT"), Hex(TEXT("EF4444")), Hex(TEXT("F87171")), Hex(TEXT("B91C1C")));
		ExitButton->OnClicked.AddDynamic(this, &UMarsStartWidget::HandleExitClicked);
		UButton* StartButton = MakeColored(TEXT("START"), Hex(TEXT("22C55E")), Hex(TEXT("4ADE80")), Hex(TEXT("15803D")));
		StartButton->OnClicked.AddDynamic(this, &UMarsStartWidget::HandleStartClicked);

		AddH(Row, FixedWidth(Tree, ExitButton, 220.f), 0.f, FMargin(0.f, 0.f, 12.f, 0.f));
		AddH(Row, FixedWidth(Tree, StartButton, 220.f), 0.f, FMargin(12.f, 0.f, 0.f, 0.f));
		AddV(Box, Row)->SetHorizontalAlignment(HAlign_Center);
	}
}

void UMarsStartWidget::HandleStartClicked()
{
	if (AMarsMainComputer* C = Computer.Get())
	{
		C->StartMission();
	}
}

void UMarsStartWidget::HandleExitClicked()
{
	if (AMarsMainComputer* C = Computer.Get())
	{
		C->ExitGame();
	}
}
