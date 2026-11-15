// MarsUIHelpers.h
// Small helpers to build UMG widgets from C++ (no Widget Blueprint needed).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace MarsUI
{
	// Colors used everywhere
	inline FLinearColor White()  { return FLinearColor(0.95f, 0.95f, 0.95f, 1.f); }
	inline FLinearColor Grey()   { return FLinearColor(0.65f, 0.67f, 0.70f, 1.f); }
	inline FLinearColor Red()    { return FLinearColor(0.90f, 0.15f, 0.12f, 1.f); }
	inline FLinearColor Orange() { return FLinearColor(1.00f, 0.55f, 0.10f, 1.f); }
	inline FLinearColor Yellow() { return FLinearColor(1.00f, 0.85f, 0.20f, 1.f); }
	inline FLinearColor Green()  { return FLinearColor(0.25f, 0.85f, 0.35f, 1.f); }
	inline FLinearColor Cyan()   { return FLinearColor(0.20f, 0.80f, 0.95f, 1.f); }
	inline FLinearColor Blue()   { return FLinearColor(0.30f, 0.50f, 1.00f, 1.f); }
	inline FLinearColor Purple() { return FLinearColor(0.70f, 0.45f, 1.00f, 1.f); }
	inline FLinearColor PanelBg(float Alpha = 0.80f) { return FLinearColor(0.02f, 0.03f, 0.05f, Alpha); }

	inline UTextBlock* Text(UWidgetTree* Tree, const FString& Value, int32 Size = 16,
		const FLinearColor& Color = White(), bool bWrap = true)
	{
		UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		T->SetText(FText::FromString(Value));
		FSlateFontInfo FontInfo = T->GetFont();
		FontInfo.Size = Size;
		T->SetFont(FontInfo);
		T->SetColorAndOpacity(FSlateColor(Color));
		T->SetAutoWrapText(bWrap);
		return T;
	}

	inline void SetTextColor(UTextBlock* T, const FLinearColor& Color)
	{
		if (T)
		{
			T->SetColorAndOpacity(FSlateColor(Color));
		}
	}

	inline UBorder* Panel(UWidgetTree* Tree, const FLinearColor& Color, const FMargin& Pad = FMargin(12.f))
	{
		UBorder* B = Tree->ConstructWidget<UBorder>(UBorder::StaticClass());
		B->SetBrushColor(Color);
		B->SetPadding(Pad);
		return B;
	}

	inline UProgressBar* Bar(UWidgetTree* Tree, const FLinearColor& Color)
	{
		UProgressBar* P = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
		P->SetFillColorAndOpacity(Color);
		P->SetPercent(0.f);
		return P;
	}

	inline UVerticalBoxSlot* AddV(UVerticalBox* Box, UWidget* Child, const FMargin& Pad = FMargin(0.f, 2.f))
	{
		UVerticalBoxSlot* VS = Box->AddChildToVerticalBox(Child);
		VS->SetPadding(Pad);
		VS->SetHorizontalAlignment(HAlign_Fill);
		return VS;
	}

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
}
