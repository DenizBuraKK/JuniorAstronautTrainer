// MarsHUDWidget.h
// Always-on HUD, built entirely in C++:
//   top-left  : mission clock (day, sol, date, time to next day, weather)
//   top-right : base status (6 resource meters with per-day change)
//   top-center: flashing red alert
//   bottom    : "press E" prompt, last message, INFO button (bottom-right)
//   overlays  : "How to play" panel (the end screen is in MarsTransitionWidget)

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "InputCoreTypes.h"
#include "MarsHUDWidget.generated.h"

class AMarsMainComputer;
class UBorder;
class UButton;
class UCanvasPanel;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWidgetTree;

UCLASS()
class UMarsHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call before AddToViewport. */
	void Init(AMarsMainComputer* InComputer);

	/** Label empty = "Use the Main Computer". */
	void SetInteractPromptVisible(bool bVisible, const FKey& Key, const FText& Label = FText::GetEmpty());

	/** Shows / hides the "How to play" panel. Use AMarsMainComputer::OpenInfo / CloseInfo (they also pause the day). */
	void SetInfoVisible(bool bVisible);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	UWidget* BuildInfoPanel();
	void AddInfoSection(UVerticalBox* Box, const FString& Number, const FString& Title, const FString& Body);
	void Refresh();
	void HandleSimChanged();
	void HandleGameOver(bool bWon, const FText& Reason);
	void ToggleAlertFlash();

	UFUNCTION() void HandleInfoClicked();
	UFUNCTION() void HandleInfoCloseClicked();

	TWeakObjectPtr<AMarsMainComputer> Computer;
	FDelegateHandle SimChangedHandle;
	FDelegateHandle GameOverHandle;
	FTimerHandle FlashTimer;
	bool bFlashOn = true;

	// Mission clock
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DayText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> SolText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DateText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> DayBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WeatherText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TempText;

	// Base status
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MeterValues;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MeterRates;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> MeterBars;

	// Alert
	UPROPERTY(Transient) TObjectPtr<UBorder> AlertPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AlertText;

	// Bottom
	UPROPERTY(Transient) TObjectPtr<UBorder> PromptPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PromptKeyText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PromptLabelText;
	UPROPERTY(Transient) TObjectPtr<UBorder> MessagePanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;
	UPROPERTY(Transient) TObjectPtr<UButton> InfoButton;

	// Overlays
	UPROPERTY(Transient) TObjectPtr<UWidget> InfoOverlay;
};
