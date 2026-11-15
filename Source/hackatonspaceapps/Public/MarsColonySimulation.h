// MarsColonySimulation.h
// All game mechanics of the Mars base: power split, oxygen, shield, radiation dose,
// crew health, research, events and win/lose. Lives on AMarsMainComputer.
// Every tuning value below is GAME BALANCE (not NASA data) and editable in BP_MainComputer.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MarsColonySimulation.generated.h"

class AMarsMainComputer;

/** Snapshot of everything the UI needs. Rates are "per day". */
USTRUCT(BlueprintType)
struct FMarsColonyState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mars") int32 Day = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") int32 DayCount = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float DayTimeRemaining = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float DayProgress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Mars") float Battery = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float BatteryMax = 100.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float Oxygen = 0.f;        // 0..100
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float Shield = 0.f;        // 0..100
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float CrewHealth = 0.f;    // 0..100
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float Dose = 0.f;          // 0..DoseLimit
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float DoseLimit = 100.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float Research = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float ResearchGoal = 200.f;

	UPROPERTY(BlueprintReadOnly, Category = "Mars") float AllocLifeSupport = 0.f;   // 0..1
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float AllocShield = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float AllocResearch = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float PowerGeneration = 0.f;    // per day

	UPROPERTY(BlueprintReadOnly, Category = "Mars") float BatteryRate = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float OxygenRate = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float ShieldRate = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float DoseRate = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float HealthRate = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") float ResearchRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Mars") float RadiationMultiplier = 1.f;   // active now
	UPROPERTY(BlueprintReadOnly, Category = "Mars") bool bStormActive = false;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") bool bStormIncoming = false;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") bool bOverdrive = false;        // allocations > 100%, battery pays
	UPROPERTY(BlueprintReadOnly, Category = "Mars") bool bPowerShortage = false;    // overdrive wanted, battery empty

	UPROPERTY(BlueprintReadOnly, Category = "Mars") bool bEventPending = false;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") bool bGameOver = false;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") bool bWon = false;
	UPROPERTY(BlueprintReadOnly, Category = "Mars") FText GameOverReason;

	/** Red warnings for the HUD, most important first. Empty = no alert. */
	UPROPERTY(BlueprintReadOnly, Category = "Mars") TArray<FText> Alerts;
	/** Last thing that happened (choice applied, alert ignored, new day...). */
	UPROPERTY(BlueprintReadOnly, Category = "Mars") FText LastMessage;
};

/** Side tasks at the small station consoles (research lab, power reactor, life support). */
UENUM(BlueprintType)
enum class EMarsSideTask : uint8
{
	Research    UMETA(DisplayName = "Research (research points)"),
	Energy      UMETA(DisplayName = "Energy (battery)"),
	LifeSupport UMETA(DisplayName = "Life Support (crew health)")
};

DECLARE_MULTICAST_DELEGATE(FMarsSimChangedNative);
DECLARE_MULTICAST_DELEGATE_TwoParams(FMarsSimGameOverNative, bool /*bWon*/, const FText& /*Reason*/);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMarsSimChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FMarsSimGameOver, bool, bWon, const FText&, Reason);

UCLASS(ClassGroup = (Mars), meta = (BlueprintSpawnableComponent))
class UMarsColonySimulation : public UActorComponent
{
	GENERATED_BODY()

public:
	UMarsColonySimulation();

	// ================================================================ Tuning (game balance)

	/** Real seconds between simulation steps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float SimStepSeconds = 0.2f;
	/** Longest real time one step may cover (protects against frame hitches). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float MaxStepSeconds = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float PowerPerDay = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float BatteryStart = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float BatteryMax = 100.f;
	/** Sum of the three sliders may go up to this (e.g. 1.5 = 150%). Above 100% the battery pays. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float MaxTotalAllocation = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float OxygenStart = 80.f;
	/** Life-support power per day needed to keep oxygen level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float OxygenNeedPerDay = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float OxygenPerPower = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float OxygenLowThreshold = 30.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float OxygenLowDamagePerDay = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float HealthStart = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float HealthRegenPerDay = 5.f;
	/** Crew health lost per point of radiation dose. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float HealthPerDose = 0.6f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float ShieldStart = 50.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float ShieldDecayPerDay = 20.f;
	/** Extra shield drain per day for each 1x of radiation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float ShieldDrainPerRadiation = 5.f;
	/** Share of radiation blocked at shield 100 (0.9 = 90%). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float ShieldMaxProtection = 0.9f;

	/** Dose per day on a quiet day with no shield. Multiplied by the day's radiation multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float BaseDosePerDay = 6.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float DoseLimit = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float ResearchPerPower = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float ResearchGoal = 200.f;

	/** Ignoring an alert until the day ends: the riskiest option's BAD effects happen, plus these penalties. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float IgnoredAlertResearchPenalty = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float IgnoredAlertHealthPenalty = 8.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float StartAllocLifeSupport = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float StartAllocShield = 0.30f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Tuning") float StartAllocResearch = 0.35f;

	// ================================================================ Control (called by the computer / UI)

	/** Reset everything and start stepping. Called by AMarsMainComputer::StartDayTimer. */
	void StartSimulation();
	/** A new day started. Applies the "ignored alert" penalty for the previous day if needed. */
	void BeginDay(int32 Day);
	/** Day 7 ended: decide win or lose. */
	void EndWeek();
	void StopSimulation();

	/** Each value 0..1. If the sum is above MaxTotalAllocation, all three are scaled down. */
	UFUNCTION(BlueprintCallable, Category = "Mars")
	void SetAllocation(float LifeSupport, float ShieldPower, float ResearchPower);

	/** Apply today's event choice (0-based). Returns false if nothing is pending or the index is wrong. */
	UFUNCTION(BlueprintCallable, Category = "Mars")
	bool ResolveEvent(int32 ChoiceIndex, FText& ResultText);

	UFUNCTION(BlueprintPure, Category = "Mars")
	FMarsColonyState GetState() const { return State; }

	const FMarsColonyState& GetStateRef() const { return State; }

	/**
	 * Side-task reward from a station console. Energy = battery, Research = research points,
	 * LifeSupport = crew health. Battery and health never go above their maximum: if they are
	 * already full nothing is added. Returns the amount really added (0 = nothing).
	 */
	UFUNCTION(BlueprintCallable, Category = "Mars")
	float AddSideTaskBonus(EMarsSideTask Task, float Amount, FText& ResultText);

	/** Test shortcut (Ctrl+P / Ctrl+\u011E on the Main Computer): end the game now as won or failed. */
	UFUNCTION(BlueprintCallable, Category = "Mars|Debug")
	void DebugFinish(bool bWon);

	/** True if this side task would add nothing right now (battery / health already full, or game not running). */
	UFUNCTION(BlueprintPure, Category = "Mars")
	bool IsSideTaskFull(EMarsSideTask Task) const;

	/** "Battery -10, Shield +15" for a choice of a day (empty if none). */
	FText DescribeChoiceEffects(int32 Day, int32 ChoiceIndex) const;

	// ================================================================ Events

	FMarsSimChangedNative OnStateChangedNative;
	FMarsSimGameOverNative OnGameOverNative;

	UPROPERTY(BlueprintAssignable, Category = "Mars") FMarsSimChanged OnStateChanged;
	UPROPERTY(BlueprintAssignable, Category = "Mars") FMarsSimGameOver OnGameOver;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StepSimulation();
	/** Recalculate per-day rates from the current state (no time passes). */
	void UpdateRates();
	void ApplyRates(float DayFraction);
	void RebuildAlerts();
	bool CheckFailure();
	void FinishGame(bool bWon, const FText& Reason);
	void ApplyChoice(int32 Day, int32 ChoiceIndex, bool bOnlyBadEffects = false);
	void Broadcast();
	AMarsMainComputer* GetComputer() const;

	FMarsColonyState State;
	FTimerHandle StepTimer;
	double LastStepTime = 0.0;
	int32 PendingEventDay = 0;
	bool bRunning = false;
};
