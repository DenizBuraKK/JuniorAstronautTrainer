// MarsHUDWidget.h
// Always-on HUD, built entirely in C++: day + clock, weather, 6 resource bars, flashing
// red alerts, "press E" prompt, last message, data credits, and the end-of-game panel.

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

UCLASS()
class UMarsHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call before AddToViewport. */
	void Init(AMarsMainComputer* InComputer);

	void SetInteractPromptVisible(bool bVisible, const FKey& Key);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	void Refresh();
	void HandleSimChanged();
	void HandleGameOver(bool bWon, const FText& Reason);
	void ToggleAlertFlash();

	UFUNCTION()
	void HandleRestartClicked();

	TWeakObjectPtr<AMarsMainComputer> Computer;
	FDelegateHandle SimChangedHandle;
	FDelegateHandle GameOverHandle;
	FTimerHandle FlashTimer;
	bool bFlashOn = true;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> DayText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ClockText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> DayBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WeatherText;

	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> MeterLabels;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> MeterBars;

	UPROPERTY(Transient) TObjectPtr<UBorder> AlertPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> AlertText;

	UPROPERTY(Transient) TObjectPtr<UBorder> PromptPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PromptText;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> MessageText;

	UPROPERTY(Transient) TObjectPtr<UBorder> EndPanel;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndReason;
	UPROPERTY(Transient) TObjectPtr<UButton> RestartButton;
};
