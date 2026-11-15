// MarsTransitionWidget.h
// Full-screen layer drawn above everything (HUD and Main Computer):
//   - last seconds of a day: the screen fades to black
//   - new day: fades back in while a big "DAY 2" title is shown
//   - end of the game: the screen goes fully black, then MISSION COMPLETE / FAILED,
//     the mission stats, a credits block (text from BP_MainComputer > Credits Text) and PLAY AGAIN.
// It only READS the simulation and the day timer; it never changes game state.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MarsTransitionWidget.generated.h"

class AMarsMainComputer;
class UBorder;
class UTextBlock;
class UWidget;

UCLASS()
class UMarsTransitionWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call before AddToViewport. */
	void Init(AMarsMainComputer* InComputer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

private:
	void BuildLayout();
	void UpdateTransition();
	void ShowDayTitle(int32 Day);
	void ShowEndScreen();
	void SetBlack(float Alpha);

	UFUNCTION() void HandlePlayAgainClicked();

	TWeakObjectPtr<AMarsMainComputer> Computer;
	FTimerHandle UpdateTimer;
	double LastUpdateTime = -1.0;

	int32 LastDay = 0;
	float FadeInTime = -1.f;     // < 0 = no fade-in running
	float TitleTime = -1.f;      // < 0 = no title shown
	float EndTime = -1.f;        // < 0 = game not over yet
	float EndContentTime = -1.f; // < 0 = end text not shown yet
	float BlackAlpha = 0.f;

	UPROPERTY(Transient) TObjectPtr<UBorder> BlackLayer;

	UPROPERTY(Transient) TObjectPtr<UWidget> TitleBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleCaption;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleDay;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleSub;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> TitleHint;

	UPROPERTY(Transient) TObjectPtr<UWidget> EndBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndTitle;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndReason;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> EndStats;
	UPROPERTY(Transient) TObjectPtr<UWidget> CreditsBox;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> CreditsBody;
};
