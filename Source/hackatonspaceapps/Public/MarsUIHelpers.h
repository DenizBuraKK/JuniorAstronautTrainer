// MarsUIHelpers.h
// Small helpers + one shared visual theme to build UMG widgets from C++ (no Widget Blueprint needed).
// Change the colors in the "Theme" block to restyle the whole game.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

namespace MarsUI
{
	// ------------------------------------------------------------------ Theme
	/** "RRGGBB" or "RRGGBBAA" (sRGB, like a web color). */
	inline FLinearColor Hex(const TCHAR* Value) { return FLinearColor::FromSRGBColor(FColor::FromHex(Value)); }

	inline FLinearColor PanelBg()    { return Hex(TEXT("0B111CE8")); }   // dark glass
	inline FLinearColor WindowBg()   { return Hex(TEXT("0B111CFA")); }
	inline FLinearColor Card()       { return Hex(TEXT("FFFFFF0B")); }   // slightly lighter block inside a panel
	inline FLinearColor CardHover()  { return Hex(TEXT("FFFFFF1C")); }
	inline FLinearColor Line()       { return Hex(TEXT("FFFFFF1F")); }   // outlines and dividers
	inline FLinearColor Track()      { return Hex(TEXT("FFFFFF17")); }   // empty part of bars
	inline FLinearColor Accent()     { return Hex(TEXT("FF7A3D")); }     // Mars orange
	inline FLinearColor AccentDark() { return Hex(TEXT("C2522A")); }

	inline FLinearColor White() { return Hex(TEXT("EEF1F5")); }
	inline FLinearColor Muted() { return Hex(TEXT("9AA4B2")); }
	inline FLinearColor Dim()   { return Hex(TEXT("677185")); }
	inline FLinearColor Ink()   { return Hex(TEXT("0B111C")); }          // dark text on light buttons

	inline FLinearColor Good() { return Hex(TEXT("4ADE80")); }
	inline FLinearColor Warn() { return Hex(TEXT("FBBF24")); }
	inline FLinearColor Bad()  { return Hex(TEXT("F87171")); }

	inline FLinearColor BatteryColor()  { return Hex(TEXT("FBBF24")); }
	inline FLinearColor OxygenColor()   { return Hex(TEXT("38BDF8")); }
	inline FLinearColor ShieldColor()   { return Hex(TEXT("818CF8")); }
	inline FLinearColor HealthColor()   { return Hex(TEXT("4ADE80")); }
	inline FLinearColor DoseColor()     { return Hex(TEXT("FB923C")); }
	inline FLinearColor ResearchColor() { return Hex(TEXT("C084FC")); }

	inline FLinearColor AlertBg()   { return Hex(TEXT("5B1414F2")); }
	inline FLinearColor AlertLine() { return Hex(TEXT("F87171")); }

	inline FLinearColor WithAlpha(FLinearColor C, float A) { C.A = A; return C; }

	// ------------------------------------------------------------------ Brushes
	/** Rounded box drawn by Slate itself (no texture needed). */
	inline FSlateBrush RoundBrush(const FLinearColor& Fill, float Radius = 10.f,
		const FLinearColor& Outline = FLinearColor::Transparent, float OutlineWidth = 0.f)
	{
		FSlateBrush B;
		B.DrawAs = ESlateBrushDrawType::RoundedBox;
		B.TintColor = FSlateColor(Fill);
		B.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
		B.OutlineSettings.Color = FSlateColor(Outline);
		B.OutlineSettings.Width = OutlineWidth;
		B.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		return B;
	}

	// ------------------------------------------------------------------ Widgets
	inline UTextBlock* Text(UWidgetTree* Tree, const FString& Value, int32 Size = 15,
		const FLinearColor& Color = White(), bool bWrap = true, bool bBold = false, int32 LetterSpacing = 0)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Value));
		FSlateFontInfo FontInfo = T->GetFont();
		FontInfo.Size = Size;
		FontInfo.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
		FontInfo.LetterSpacing = LetterSpacing;
		T->SetFont(FontInfo);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetShadowOffset(FVector2D(0.f, 1.f));
		T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.45f));
		T->SetAutoWrapText(bWrap);
		return T;
	}

	/** Small spaced-out caption, e.g. "BASE STATUS". */
	inline UTextBlock* Caption(UWidgetTree* Tree, const FString& Value, const FLinearColor& Color = Muted(), int32 Size = 11)
	{
		return Text(Tree, Value, Size, Color, false, true, 180);
	}

	inline void SetTextColor(UTextBlock* T, const FLinearColor& Color)
	{
		if (T)
		{
			T->SetColorAndOpacity(FSlateColor(Color));
		}
	}

	inline UBorder* Panel(UWidgetTree* Tree, const FLinearColor& Fill = PanelBg(), const FMargin& Pad = FMargin(16.f),
		float Radius = 12.f, const FLinearColor& Outline = Line(), float OutlineWidth = 1.f)
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrush(RoundBrush(Fill, Radius, Outline, OutlineWidth));
		B->SetPadding(Pad);
		return B;
	}

	inline void SetPanelColors(UBorder* B, const FLinearColor& Fill, float Radius, const FLinearColor& Outline, float OutlineWidth = 1.f)
	{
		if (B)
		{
			B->SetBrush(RoundBrush(Fill, Radius, Outline, OutlineWidth));
		}
	}

	/** Thin rounded progress bar. Returns the box to add to a layout; OutBar is the bar to update. */
	inline USizeBox* Bar(UWidgetTree* Tree, const FLinearColor& Color, float Height, UProgressBar*& OutBar)
	{
		UProgressBar* P = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		FProgressBarStyle Style;
		Style.SetBackgroundImage(RoundBrush(Track(), Height * 0.5f));
		Style.SetFillImage(RoundBrush(FLinearColor::White, Height * 0.5f));     // tinted by the fill color
		Style.SetMarqueeImage(RoundBrush(FLinearColor::White, Height * 0.5f));
		P->SetWidgetStyle(Style);
		P->SetFillColorAndOpacity(Color);
		P->SetPercent(0.f);

		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(Height);
		Box->AddChild(P);
		OutBar = P;
		return Box;
	}

	/** 1 px line. */
	inline USizeBox* Divider(UWidgetTree* Tree)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetHeightOverride(1.f);
		UBorder* L = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		L->SetBrushColor(Line());
		Box->AddChild(L);
		return Box;
	}

	inline USpacer* Space(UWidgetTree* Tree, float Height)
	{
		USpacer* S = Tree->ConstructWidget<USpacer>(USpacer::StaticClass());
		S->SetSize(FVector2D(1.f, Height));
		return S;
	}

	/** Rounded button style. Fill = normal background. */
	inline void StyleButton(UButton* Button, const FLinearColor& Fill, const FLinearColor& Hover,
		const FLinearColor& Pressed, const FLinearColor& Outline, float Radius = 8.f, const FMargin& Pad = FMargin(16.f, 9.f))
	{
		FButtonStyle Style;
		Style.SetNormal(RoundBrush(Fill, Radius, Outline, 1.f));
		Style.SetHovered(RoundBrush(Hover, Radius, Outline, 1.f));
		Style.SetPressed(RoundBrush(Pressed, Radius, Outline, 1.f));
		Style.SetDisabled(RoundBrush(WithAlpha(Fill, 0.3f), Radius));
		Style.SetNormalPadding(Pad);
		Style.SetPressedPadding(FMargin(Pad.Left, Pad.Top + 1.f, Pad.Right, Pad.Bottom - 1.f));
		Button->SetStyle(Style);
		Button->SetBackgroundColor(FLinearColor::White);
	}

	enum class EButtonKind : uint8 { Primary, Ghost, Choice };

	inline void StyleButtonKind(UButton* Button, EButtonKind Kind)
	{
		switch (Kind)
		{
		case EButtonKind::Primary:
			StyleButton(Button, Accent(), Hex(TEXT("FF9461")), AccentDark(), FLinearColor::Transparent);
			break;
		case EButtonKind::Ghost:
			StyleButton(Button, Card(), CardHover(), Hex(TEXT("FFFFFF08")), Line());
			break;
		case EButtonKind::Choice:
			StyleButton(Button, Card(), WithAlpha(Accent(), 0.16f), WithAlpha(Accent(), 0.28f), WithAlpha(Accent(), 0.55f), 10.f, FMargin(16.f, 12.f));
			break;
		}
	}

	/** Button with one line of text. */
	inline UButton* MakeButton(UWidgetTree* Tree, const FString& Label, EButtonKind Kind, int32 Size = 14)
	{
		UButton* B = Tree->ConstructWidget<UButton>(UButton::StaticClass());
		StyleButtonKind(B, Kind);
		UTextBlock* T = Text(Tree, Label, Size, Kind == EButtonKind::Primary ? Ink() : White(), false, true, 60);
		T->SetShadowColorAndOpacity(FLinearColor::Transparent);
		B->AddChild(T);
		return B;
	}

	/** Small rounded label like a keyboard key: [ E ]. */
	inline UBorder* KeyCap(UWidgetTree* Tree, const FString& Key, int32 Size = 13)
	{
		UBorder* B = Panel(Tree, White(), FMargin(8.f, 2.f), 5.f, FLinearColor::Transparent, 0.f);
		UTextBlock* T = Text(Tree, Key, Size, Ink(), false, true);
		T->SetShadowColorAndOpacity(FLinearColor::Transparent);
		B->SetContent(T);
		return B;
	}

	/** Small colored tag, e.g. a source type. */
	inline UBorder* Pill(UWidgetTree* Tree, const FString& Label, const FLinearColor& Color)
	{
		UBorder* B = Panel(Tree, WithAlpha(Color, 0.16f), FMargin(8.f, 2.f), 9.f, WithAlpha(Color, 0.5f), 1.f);
		B->SetContent(Text(Tree, Label, 10, Color, false, true, 80));
		return B;
	}

	inline USizeBox* FixedWidth(UWidgetTree* Tree, UWidget* Child, float Width)
	{
		USizeBox* Box = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		Box->SetWidthOverride(Width);
		Box->AddChild(Child);
		return Box;
	}

	// ------------------------------------------------------------------ Layout
	inline UVerticalBoxSlot* AddV(UVerticalBox* Box, UWidget* Child, const FMargin& Pad = FMargin(0.f, 2.f))
	{
		UVerticalBoxSlot* VS = Box->AddChildToVerticalBox(Child);
		VS->SetPadding(Pad);
		VS->SetHorizontalAlignment(HAlign_Fill);
		return VS;
	}

	/** Fill > 0 = take that share of the free width, Fill = 0 = only as wide as the content. */
	inline UHorizontalBoxSlot* AddH(UHorizontalBox* Box, UWidget* Child, float Fill = 1.f, const FMargin& Pad = FMargin(6.f, 0.f))
	{
		UHorizontalBoxSlot* HS = Box->AddChildToHorizontalBox(Child);
		HS->SetPadding(Pad);
		FSlateChildSize ChildSize(Fill > 0.f ? ESlateSizeRule::Fill : ESlateSizeRule::Automatic);
		ChildSize.Value = Fill > 0.f ? Fill : 1.f;
		HS->SetSize(ChildSize);
		HS->SetVerticalAlignment(VAlign_Center);
		return HS;
	}

	inline void AlignButtonContent(UButton* Button, EHorizontalAlignment H)
	{
		if (UButtonSlot* BS = Button ? Cast<UButtonSlot>(Button->GetContentSlot()) : nullptr)
		{
			BS->SetHorizontalAlignment(H);
			BS->SetVerticalAlignment(VAlign_Center);
		}
	}

	/** Place a widget on a canvas. Anchor/Alignment in 0..1 (0,0 = top-left, 1,1 = bottom-right). */
	inline UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Child, const FVector2D& Anchor,
		const FVector2D& Alignment, const FVector2D& Position)
	{
		UCanvasPanelSlot* CS = Canvas->AddChildToCanvas(Child);
		CS->SetAnchors(FAnchors(static_cast<float>(Anchor.X), static_cast<float>(Anchor.Y)));
		CS->SetAlignment(Alignment);
		CS->SetPosition(Position);
		CS->SetAutoSize(true);
		return CS;
	}

	/** Stretch a widget over the whole canvas. */
	inline UCanvasPanelSlot* FillCanvas(UCanvasPanel* Canvas, UWidget* Child)
	{
		UCanvasPanelSlot* CS = Canvas->AddChildToCanvas(Child);
		CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		CS->SetOffsets(FMargin(0.f));
		return CS;
	}

	// ------------------------------------------------------------------ Formatting
	/** "+2.5" / "-12.0" */
	inline FString Signed(float Value)
	{
		return FString::Printf(TEXT("%+.1f"), Value);
	}

	inline FString Clock(float Seconds)
	{
		const int32 Total = FMath::Max(0, FMath::CeilToInt32(Seconds));
		return FString::Printf(TEXT("%02d:%02d"), Total / 60, Total % 60);
	}

	/** Color for a "per day" change. bHigherIsBad for things like radiation dose. */
	inline FLinearColor RateColor(float Rate, bool bHigherIsBad = false)
	{
		if (FMath::Abs(Rate) < 0.05f)
		{
			return Dim();
		}
		const bool bGood = bHigherIsBad ? Rate < 0.f : Rate > 0.f;
		return bGood ? Good() : Bad();
	}
}
