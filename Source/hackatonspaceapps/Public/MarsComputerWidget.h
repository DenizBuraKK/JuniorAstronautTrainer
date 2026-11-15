// MarsComputerWidget.h
// The Main Computer screen, built entirely in C++.
// Left: today's event, official NASA records, decision buttons.
// Right: power split sliders (life support / shield / research) with live effect preview.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MarsComputerWidget.generated.h"

class AMarsMainComputer;
class UBorder;
class UButton;
class UProgressBar;
class USlider;
class UTextBlock;
class UVerticalBox;
class UWidget;

UCLASS()
class UMarsComputerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call once after CreateWidget. */
	void Init(AMarsMainComputer* InComputer);

	/** Called by the computer every time the screen opens. */
	void OnOpened();

	virtual void BeginDestroy() override;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildLayout();
	UWidget* MakeSliderBlock(const FString& Title, const FString& Hint, const FLinearColor& Color, USlider*& OutSlider, UTextBlock*& OutValue);
	void Refresh();
	void RebuildEventSection();
	void PushSlidersToSimulation();
	void HandleSimChanged();
	void HandleChoiceClicked(int32 ChoiceIndex);
	void ResolveChoiceDeferred(int32 ChoiceIndex);

	UFUNCTION() void HandleLifeChanged(float Value);
	UFUNCTION() void HandleShieldChanged(float Value);
	UFUNCTION() void HandleResearchChanged(float Value);
	UFUNCTION() void HandleCloseClicked();

	TWeakObjectPtr<AMarsMainComputer> Computer;
	FDelegateHandle SimChangedHandle;
	int32 BuiltForDay = -1;
	bool bBuiltWithPending = false;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> HeaderText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WeatherText;
	UPROPERTY(Transient) TObjectPtr<UBorder> AlertPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AlertText;
	UPROPERTY(Transient) TObjectPtr<UVerticalBox> EventBox;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> GenerationText;
	UPROPERTY(Transient) TObjectPtr<USlider> LifeSlider;
	UPROPERTY(Transient) TObjectPtr<USlider> ShieldSlider;
	UPROPERTY(Transient) TObjectPtr<USlider> ResearchSlider;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> LifeValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ShieldValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResearchValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TotalText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> TotalBar;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> PreviewValues;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StormText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> GoalText;
	UPROPERTY(Transient) TObjectPtr<UButton> CloseButton;
};
