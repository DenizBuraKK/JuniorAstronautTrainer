// MarsStartWidget.h
// Title screen shown once when the game starts: black background, "MARS MISSION SIMULATION",
// mission goal, a short note about the real data, red EXIT and green START buttons.
// The day clock only starts after START (AMarsMainComputer::StartMission).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "MarsStartWidget.generated.h"

class AMarsMainComputer;

UCLASS()
class UMarsStartWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** Call before AddToViewport. */
	void Init(AMarsMainComputer* InComputer);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void BuildLayout();

	UFUNCTION() void HandleStartClicked();
	UFUNCTION() void HandleExitClicked();

	TWeakObjectPtr<AMarsMainComputer> Computer;
};
