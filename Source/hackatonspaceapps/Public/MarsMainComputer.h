// MarsMainComputer.h
// The Main Computer of the Mars base. Place ONE BP_MainComputer in the level.
// - Loads the week JSON when the game starts (before any BeginPlay).
// - Runs the day clock (StartDayTimer) and the colony simulation (UMarsColonySimulation).
// - Creates the HUD, shows red alerts, and opens the computer screen when the player
//   walks close and presses the interaction key (E by default).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "MarsMainComputer.generated.h"

class USphereComponent;
class UPrimitiveComponent;
class UMarsColonySimulation;
class UMarsHUDWidget;
class UMarsTransitionWidget;
class UMarsStartWidget;
class UMarsComputerWidget;

// Internal storage (plain C++, filled once from the JSON).
struct FMarsComputerChoice
{
	FText Text;
	int32 Energy = 0;
	int32 Oxygen = 0;
	int32 Shield = 0;
	int32 Dose = 0;
	int32 Science = 0;
};

struct FMarsComputerDay
{
	int32 DayNumber = 0;
	FString Date;
	int32 Sol = 0;

	FText WeatherCondition;
	int32 AirTempMinC = 0;
	int32 AirTempMaxC = 0;
	int32 GroundTempMinC = 0;
	int32 GroundTempMaxC = 0;
	int32 PressurePa = 0;
	FString UVIndex;
	FString Sunrise;
	FString Sunset;
	bool bHasWind = false;
	int32 Wind = 0;

	bool bHasEvent = false;
	FText EventTitle;
	FText EventDescription;
	TArray<FText> RecordInfos;
	TArray<FText> RecordSourceTypes;
	TArray<FString> RecordLinks;
	TArray<FMarsComputerChoice> Choices;

	// "gameplay" block (game balance)
	float RadiationMultiplier = 1.f;
	float StormStartFraction = 0.f;
	float SolarMultiplier = 1.f;
	float ResearchMultiplier = 1.f;
	FText StormWarning;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMarsDayChangedSignature, int32, NewDay);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMarsWeekFinishedSignature);

UCLASS(Blueprintable)
class AMarsMainComputer : public AActor
{
	GENERATED_BODY()

public:
	AMarsMainComputer();

	// ================================================================ Components

	/** Player must be inside this sphere to use the computer. Change its radius in BP_MainComputer. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars")
	TObjectPtr<USphereComponent> InteractionSphere;

	/** All game mechanics. Its tuning values are in the Details panel. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mars")
	TObjectPtr<UMarsColonySimulation> Simulation;

	// ================================================================ Settings

	/** JSON path, relative to the Content folder. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Setup")
	FString DataFilePath;

	/** Start the week automatically when the level starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Setup")
	bool bAutoStartGame = true;

	/** Seconds per day used by the auto start (60 = 1 min per day, 7 days = 7 min). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Setup", meta = (ClampMin = "1.0"))
	float DefaultDayDurationSeconds = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Interaction")
	FKey InteractionKey;

	/** Opens / closes the "How to play" panel (the INFO button in the bottom-right does the same). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Interaction")
	FKey InfoKey;

	/** Stop the day clock while the "How to play" panel is open, so kids can read in peace. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Interaction")
	bool bPauseDayWhileInfoOpen = true;

	/** Stop the day clock while the computer screen is open. Off = time keeps running (more pressure). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Interaction")
	bool bPauseDayWhileComputerOpen = false;

	/** Stop player movement while the computer screen is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Interaction")
	bool bBlockMovementWhileComputerOpen = true;

	/** Leave as is to use the built-in C++ HUD, or pick a Blueprint child of MarsHUDWidget. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|UI")
	TSubclassOf<UMarsHUDWidget> HUDWidgetClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|UI")
	TSubclassOf<UMarsComputerWidget> ComputerWidgetClass;

	/** Shown on the black end screen under the result. Write your team / thanks here (Shift+Enter = new line). Empty = hidden. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|UI", meta = (MultiLine = true))
	FText CreditsText;

	/** Show the "MARS MISSION SIMULATION" title screen (START / EXIT) before day 1 begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|UI")
	bool bShowStartScreen = true;

	/** Also show the title screen after PLAY AGAIN. Off = PLAY AGAIN jumps straight into day 1. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|UI")
	bool bShowStartScreenOnRestart = false;

	// ================================================================ Transitions (visual only)

	/** Fade to black at the end of each day, fade in with a big "DAY 2" title, black end screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Transitions")
	bool bDayTransitions = true;

	/** Seconds before the day ends when the screen starts going black. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Transitions", meta = (ClampMin = "0.0"))
	float DayFadeOutSeconds = 3.f;

	/** How long the screen stays fully black when the new day starts. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Transitions", meta = (ClampMin = "0.0"))
	float DayBlackHoldSeconds = 0.6f;

	/** How long the new day takes to fade back in. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Transitions", meta = (ClampMin = "0.05"))
	float DayFadeInSeconds = 1.5f;

	/** How long the big "DAY X" title stays on screen. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Transitions", meta = (ClampMin = "0.8"))
	float DayTitleSeconds = 3.f;

	/** How long the screen takes to go fully black when the game ends. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mars|Transitions", meta = (ClampMin = "0.05"))
	float EndFadeSeconds = 2.f;

	// ================================================================ Data

	/**
	 * Returns everything in one day of the JSON. DayNumber = 1..7.
	 * Return value is false if the day doesn't exist (all outputs are then empty / 0).
	 * Record* and Choice* arrays are parallel (same index = same record / choice).
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Mars")
	bool GetDayData(
		int32 DayNumber,
		FString& Date,
		int32& Sol,
		FText& WeatherCondition,
		int32& AirTempMinC,
		int32& AirTempMaxC,
		int32& GroundTempMinC,
		int32& GroundTempMaxC,
		int32& PressurePa,
		FString& UVIndex,
		FString& Sunrise,
		FString& Sunset,
		bool& bHasWind,
		int32& Wind,
		bool& bHasEvent,
		FText& EventTitle,
		FText& EventDescription,
		TArray<FText>& RecordInfos,
		TArray<FText>& RecordSourceTypes,
		TArray<FString>& RecordLinks,
		int32& ChoiceCount,
		TArray<FText>& ChoiceTexts,
		TArray<int32>& ChoiceEnergy,
		TArray<int32>& ChoiceOxygen,
		TArray<int32>& ChoiceShield,
		TArray<int32>& ChoiceDose,
		TArray<int32>& ChoiceScience) const;

	/** The day's gameplay modifiers (game balance) from the JSON. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Mars")
	bool GetDayGameplay(int32 DayNumber, float& RadiationMultiplier, float& StormStartFraction,
		float& SolarMultiplier, float& ResearchMultiplier, FText& StormWarning) const;

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "Mars")
	void GetWeekInfo(FText& Location, FText& WeekLabel, FText& DataInstitution, FString& DataLink,
		FText& DataNote, TArray<FText>& Notes, int32& DayCount) const;

	UFUNCTION(BlueprintPure, Category = "Mars")
	int32 GetDayCount() const { return StoredDays.Num(); }

	UFUNCTION(BlueprintPure, Category = "Mars")
	bool IsDataLoaded() const { return StoredDays.Num() > 0; }

	/** Load (or reload) a JSON file. Relative paths start at Content/. Old data is kept on failure. */
	UFUNCTION(BlueprintCallable, Category = "Mars")
	bool LoadData(const FString& FilePath, FString& Error);

	/** C++ access without copying. nullptr if the day doesn't exist. */
	const FMarsComputerDay* FindDay(int32 DayNumber) const;

	// ================================================================ Day timer (also starts the simulation)

	/** Starts the week at day 1 and resets the base. Every DayDurationSeconds the day goes up by one. */
	UFUNCTION(BlueprintCallable, Category = "Mars|Timer")
	bool StartDayTimer(float DayDurationSeconds);

	UFUNCTION(BlueprintCallable, Category = "Mars|Timer") void StopDayTimer();
	UFUNCTION(BlueprintCallable, Category = "Mars|Timer") void PauseDayTimer();
	UFUNCTION(BlueprintCallable, Category = "Mars|Timer") void ResumeDayTimer();

	UFUNCTION(BlueprintPure, Category = "Mars|Timer") int32 GetCurrentDay() const { return CurrentDay; }
	UFUNCTION(BlueprintPure, Category = "Mars|Timer") float GetDayTimeRemaining() const;
	UFUNCTION(BlueprintPure, Category = "Mars|Timer") float GetDayTimeElapsed() const;
	UFUNCTION(BlueprintPure, Category = "Mars|Timer") float GetDayProgress() const;
	UFUNCTION(BlueprintPure, Category = "Mars|Timer") float GetDayDuration() const { return CurrentDayDuration; }
	UFUNCTION(BlueprintPure, Category = "Mars|Timer") bool IsDayTimerRunning() const;
	UFUNCTION(BlueprintPure, Category = "Mars|Timer") bool IsDayTimerPaused() const;
	UFUNCTION(BlueprintPure, Category = "Mars|Timer") bool IsWeekFinished() const { return bWeekFinished; }

	UPROPERTY(BlueprintAssignable, Category = "Mars|Timer") FMarsDayChangedSignature OnDayChanged;
	UPROPERTY(BlueprintAssignable, Category = "Mars|Timer") FMarsWeekFinishedSignature OnWeekFinished;

	// ================================================================ Computer screen

	UFUNCTION(BlueprintCallable, Category = "Mars|Interaction") void OpenComputer();
	UFUNCTION(BlueprintCallable, Category = "Mars|Interaction") void CloseComputer();
	UFUNCTION(BlueprintCallable, Category = "Mars|Interaction") void ToggleComputer();
	UFUNCTION(BlueprintPure, Category = "Mars|Interaction") bool IsComputerOpen() const { return bComputerOpen; }
	UFUNCTION(BlueprintPure, Category = "Mars|Interaction") bool IsPlayerInRange() const { return bPlayerInRange; }

	// ================================================================ "How to play" panel

	UFUNCTION(BlueprintCallable, Category = "Mars|Interaction") void OpenInfo();
	UFUNCTION(BlueprintCallable, Category = "Mars|Interaction") void CloseInfo();
	UFUNCTION(BlueprintCallable, Category = "Mars|Interaction") void ToggleInfo();
	UFUNCTION(BlueprintPure, Category = "Mars|Interaction") bool IsInfoOpen() const { return bInfoOpen; }

	// ================================================================ Title screen

	/** START button: closes the title screen and starts the day clock. */
	UFUNCTION(BlueprintCallable, Category = "Mars|Start") void StartMission();
	/** EXIT button: quits the game (ends Play In Editor in the editor). */
	UFUNCTION(BlueprintCallable, Category = "Mars|Start") void ExitGame();
	UFUNCTION(BlueprintPure, Category = "Mars|Start") bool IsStartScreenOpen() const { return bStartScreenOpen; }

	/** Reloads the current level (used by the Restart button). */
	UFUNCTION(BlueprintCallable, Category = "Mars")
	void RestartLevel();

	UFUNCTION(BlueprintPure, Category = "Mars")
	UMarsColonySimulation* GetSimulation() const;

protected:
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleDayTimerElapsed();
	void CreateHUD();
	void SetPlayerUIMode(bool bUIMode);

	UFUNCTION()
	void HandleSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	void HandleSimGameOver(bool bWon, const FText& Reason);
	void BindPlayerKeys();
	void ShowStartScreen();
	void HandleInteractKey();

	// Timer
	FTimerHandle DayTimerHandle;
	int32 CurrentDay = 0;
	float CurrentDayDuration = 0.f;
	bool bWeekFinished = false;

	// Interaction / UI
	bool bPlayerInRange = false;
	bool bComputerOpen = false;
	bool bInteractKeyBound = false;
	bool bCursorWasVisible = false;
	bool bInfoOpen = false;
	bool bStartScreenOpen = false;
	bool bStartCursorWasVisible = false;
	bool bInfoPausedTimer = false;
	bool bInfoCursorWasVisible = false;

	UPROPERTY(Transient) TObjectPtr<UMarsHUDWidget> HUDWidget;
	UPROPERTY(Transient) TObjectPtr<UMarsComputerWidget> ComputerWidget;
	UPROPERTY(Transient) TObjectPtr<UMarsTransitionWidget> TransitionWidget;
	UPROPERTY(Transient) TObjectPtr<UMarsStartWidget> StartWidget;

	// Data
	TArray<FMarsComputerDay> StoredDays;        // sorted by DayNumber
	TMap<int32, int32> DayIndexByNumber;        // DayNumber -> index in StoredDays
	FText StoredLocation;
	FText StoredWeekLabel;
	FText StoredDataInstitution;
	FString StoredDataLink;
	FText StoredDataNote;
	TArray<FText> StoredNotes;
};
