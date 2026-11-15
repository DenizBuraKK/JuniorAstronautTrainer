// MarsStationConsole.h
// Small station computer for a side task: Research Lab, Power Reactor or Life Support Bay.
// Walk up, press E, hold the button until the bar is full -> reward through the simulation:
//   Research    -> +research points
//   Energy      -> +battery
//   LifeSupport -> +crew health
// Nothing is gained if battery / crew health are already full.
// Place one actor per station in the level (or a Blueprint child of it next to your mesh)
// and pick the Task in the Details panel. It finds BP_MainComputer by itself.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "MarsColonySimulation.h"
#include "MarsStationConsole.generated.h"

class AMarsMainComputer;
class UMarsConsoleWidget;
class USphereComponent;

UCLASS(Blueprintable)
class AMarsStationConsole : public AActor
{
	GENERATED_BODY()

public:
	AMarsStationConsole();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars")
	TObjectPtr<USphereComponent> InteractionSphere;

	// ================================================================ Settings

	/** What this station gives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Station")
	EMarsSideTask Task = EMarsSideTask::Research;

	/** Points given when the bar is full (research points / battery / crew health). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Station", meta = (ClampMin = "0.0"))
	float RewardAmount = 5.f;

	/** Seconds the button must be held. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Station", meta = (ClampMin = "0.5"))
	float HoldSeconds = 10.f;

	/** How many times per day this station can be used. 0 = unlimited. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Station", meta = (ClampMin = "0"))
	int32 UsesPerDay = 2;

	/** Shown on the screen and the prompt. Empty = "Research Lab" / "Power Reactor" / "Life Support Bay". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Station")
	FText StationName;

	/** Leave empty: the station finds the Main Computer in the level by itself. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Station")
	TObjectPtr<AMarsMainComputer> MainComputer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Station")
	FKey InteractionKey;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Station")
	bool bBlockMovementWhileOpen = true;

	// ================================================================ API

	UFUNCTION(BlueprintCallable, Category = "Mars|Station") void OpenConsole();
	UFUNCTION(BlueprintCallable, Category = "Mars|Station") void CloseConsole();
	UFUNCTION(BlueprintPure, Category = "Mars|Station") bool IsConsoleOpen() const { return bOpen; }
	UFUNCTION(BlueprintPure, Category = "Mars|Station") bool IsPlayerInRange() const { return bPlayerInRange; }

	UFUNCTION(BlueprintPure, Category = "Mars|Station") FText GetStationName() const;
	/** Uses left today (-1 = unlimited). */
	UFUNCTION(BlueprintPure, Category = "Mars|Station") int32 GetUsesLeftToday() const;

	/** False + reason if holding the button would give nothing right now. */
	bool CanDoTask(FText& Reason) const;
	/** Called by the screen when the bar is full. Returns the amount really added. */
	float CompleteTask(FText& Result);

	AMarsMainComputer* GetComputer() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void HandleSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	void BindPlayerKeys();
	void HandleInteractKey();
	void ShowPrompt(bool bVisible);
	void RefreshUsesForToday() const;

	bool bPlayerInRange = false;
	bool bOpen = false;
	bool bKeyBound = false;
	bool bCursorWasVisible = false;

	mutable int32 UsesDay = -1;
	mutable int32 UsesToday = 0;

	UPROPERTY(Transient) TObjectPtr<UMarsConsoleWidget> ConsoleWidget;
};
