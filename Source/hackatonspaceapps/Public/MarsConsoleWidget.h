// MarsConsoleWidget.h
// Screen of a station console (side task): station name, what it gives, a progress bar and a
// HOLD button next to it. Holding fills the bar in HoldSeconds; releasing drains it back.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MarsConsoleWidget.generated.h"

class AMarsStationConsole;
class UButton;
class UProgressBar;
class UTextBlock;

UCLASS()
class UMarsConsoleWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call once after CreateWidget. */
	void Init(AMarsStationConsole* InConsole);

	/** Called by the console every time the screen opens. */
	void OnOpened();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	void UpdateHold();
	void RefreshTexts(bool bCanDo, const FText& Reason);

	UFUNCTION() void HandleHoldPressed();
	UFUNCTION() void HandleHoldReleased();
	UFUNCTION() void HandleCloseClicked();

	TWeakObjectPtr<AMarsStationConsole> Console;
	FTimerHandle UpdateTimer;
	double LastUpdateTime = 0.0;
	float Progress = 0.f;
	bool bHolding = false;
	bool bNeedsRelease = false;   // after finishing, let go before starting again
	FText LastResult;

	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> DescriptionText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ValueText;
	UPROPERTY(Transient) TObjectPtr<UProgressBar> HoldBar;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PercentText;
	UPROPERTY(Transient) TObjectPtr<UButton> HoldButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> HoldButtonText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> StatusText;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> UsesText;
};
