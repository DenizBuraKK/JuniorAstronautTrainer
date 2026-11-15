// MarsMainComputer.cpp

#include "MarsMainComputer.h"
#include "MarsColonySimulation.h"
#include "MarsHUDWidget.h"
#include "MarsTransitionWidget.h"
#include "MarsStartWidget.h"
#include "MarsStationConsole.h"
#include "MarsComputerWidget.h"

#include "Components/InputComponent.h"
#include "CoreGlobals.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogMarsComputer, Log, All);

// All JSON key names. If the JSON format changes, edit only this block.
namespace MarsJsonKeys
{
	static constexpr const TCHAR* Location     = TEXT("location");
	static constexpr const TCHAR* Week         = TEXT("week");
	static constexpr const TCHAR* DataSource   = TEXT("weather_source");
	static constexpr const TCHAR* Institution  = TEXT("institution");
	static constexpr const TCHAR* OfficialLink = TEXT("official_link");
	static constexpr const TCHAR* Note         = TEXT("note");
	static constexpr const TCHAR* Notes        = TEXT("notes");
	static constexpr const TCHAR* Days         = TEXT("days");

	static constexpr const TCHAR* DayNumber    = TEXT("day");
	static constexpr const TCHAR* Date         = TEXT("date");
	static constexpr const TCHAR* Sol          = TEXT("sol");
	static constexpr const TCHAR* Weather      = TEXT("weather");
	static constexpr const TCHAR* Event        = TEXT("event");
	static constexpr const TCHAR* Gameplay     = TEXT("gameplay");

	static constexpr const TCHAR* Condition    = TEXT("condition");
	static constexpr const TCHAR* AirMin       = TEXT("air_temp_min_c");
	static constexpr const TCHAR* AirMax       = TEXT("air_temp_max_c");
	static constexpr const TCHAR* GroundMin    = TEXT("ground_temp_min_c");
	static constexpr const TCHAR* GroundMax    = TEXT("ground_temp_max_c");
	static constexpr const TCHAR* Pressure     = TEXT("pressure_pa");
	static constexpr const TCHAR* UV           = TEXT("uv_index");
	static constexpr const TCHAR* Sunrise      = TEXT("sunrise");
	static constexpr const TCHAR* Sunset       = TEXT("sunset");
	static constexpr const TCHAR* Wind         = TEXT("wind");

	static constexpr const TCHAR* EventTitle   = TEXT("title");
	static constexpr const TCHAR* Description  = TEXT("description");
	static constexpr const TCHAR* Records      = TEXT("official_records");
	static constexpr const TCHAR* RecordInfo   = TEXT("info");
	static constexpr const TCHAR* RecordType   = TEXT("source_type");
	static constexpr const TCHAR* RecordLink   = TEXT("link");
	static constexpr const TCHAR* Choices      = TEXT("choices");
	static constexpr const TCHAR* ChoiceText   = TEXT("text");
	static constexpr const TCHAR* Effects      = TEXT("effects");

	static constexpr const TCHAR* Energy       = TEXT("energy");
	static constexpr const TCHAR* Oxygen       = TEXT("oxygen");
	static constexpr const TCHAR* Shield       = TEXT("shield");
	static constexpr const TCHAR* Dose         = TEXT("dose");
	static constexpr const TCHAR* Science      = TEXT("science");

	static constexpr const TCHAR* RadiationMul = TEXT("radiation_multiplier");
	static constexpr const TCHAR* StormStart   = TEXT("storm_start_fraction");
	static constexpr const TCHAR* SolarMul     = TEXT("solar_multiplier");
	static constexpr const TCHAR* ResearchMul  = TEXT("research_multiplier");
	static constexpr const TCHAR* StormWarning = TEXT("storm_warning");
}

namespace
{
	namespace K = MarsJsonKeys;

	// Null-safe readers: a missing key and a JSON null are treated the same way.
	TSharedPtr<FJsonValue> Field(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		if (!Obj.IsValid())
		{
			return TSharedPtr<FJsonValue>();
		}
		return Obj->TryGetField(Key);
	}

	FString ReadString(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		const TSharedPtr<FJsonValue> V = Field(Obj, Key);
		return (V.IsValid() && V->Type == EJson::String) ? V->AsString() : FString();
	}

	FText ReadText(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		return FText::FromString(ReadString(Obj, Key));
	}

	bool ReadInt(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, int32& Out)
	{
		const TSharedPtr<FJsonValue> V = Field(Obj, Key);
		if (V.IsValid() && V->Type == EJson::Number)
		{
			Out = FMath::RoundToInt32(V->AsNumber());
			return true;
		}
		return false;
	}

	bool ReadFloat(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key, float& Out)
	{
		const TSharedPtr<FJsonValue> V = Field(Obj, Key);
		if (V.IsValid() && V->Type == EJson::Number)
		{
			Out = static_cast<float>(V->AsNumber());
			return true;
		}
		return false;
	}

	TSharedPtr<FJsonObject> ReadObject(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		const TSharedPtr<FJsonValue> V = Field(Obj, Key);
		return (V.IsValid() && V->Type == EJson::Object) ? V->AsObject() : TSharedPtr<FJsonObject>();
	}

	// The returned pointer stays valid while the parent object is alive.
	const TArray<TSharedPtr<FJsonValue>>* ReadArray(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Key)
	{
		const TSharedPtr<FJsonValue> V = Field(Obj, Key);
		return (V.IsValid() && V->Type == EJson::Array) ? &V->AsArray() : nullptr;
	}

	TSharedPtr<FJsonObject> AsObjectOrNull(const TSharedPtr<FJsonValue>& V)
	{
		return (V.IsValid() && V->Type == EJson::Object) ? V->AsObject() : TSharedPtr<FJsonObject>();
	}

	bool ParseDay(const TSharedPtr<FJsonObject>& D, FMarsComputerDay& Out, FString& OutError)
	{
		if (!ReadInt(D, K::DayNumber, Out.DayNumber))
		{
			OutError = FString::Printf(TEXT("A day entry has no numeric '%s' field."), K::DayNumber);
			return false;
		}
		Out.Date = ReadString(D, K::Date);
		ReadInt(D, K::Sol, Out.Sol);

		const TSharedPtr<FJsonObject> W = ReadObject(D, K::Weather);
		Out.WeatherCondition = ReadText(W, K::Condition);
		ReadInt(W, K::AirMin, Out.AirTempMinC);
		ReadInt(W, K::AirMax, Out.AirTempMaxC);
		ReadInt(W, K::GroundMin, Out.GroundTempMinC);
		ReadInt(W, K::GroundMax, Out.GroundTempMaxC);
		ReadInt(W, K::Pressure, Out.PressurePa);
		Out.UVIndex = ReadString(W, K::UV);
		Out.Sunrise = ReadString(W, K::Sunrise);
		Out.Sunset  = ReadString(W, K::Sunset);
		Out.bHasWind = ReadInt(W, K::Wind, Out.Wind);   // null in this JSON -> false

		const TSharedPtr<FJsonObject> G = ReadObject(D, K::Gameplay);   // optional; defaults = calm day
		ReadFloat(G, K::RadiationMul, Out.RadiationMultiplier);
		ReadFloat(G, K::StormStart, Out.StormStartFraction);
		ReadFloat(G, K::SolarMul, Out.SolarMultiplier);
		ReadFloat(G, K::ResearchMul, Out.ResearchMultiplier);
		Out.StormWarning = ReadText(G, K::StormWarning);

		const TSharedPtr<FJsonObject> E = ReadObject(D, K::Event);   // null on calm days
		Out.bHasEvent = E.IsValid();
		if (!Out.bHasEvent)
		{
			return true;
		}
		Out.EventTitle       = ReadText(E, K::EventTitle);
		Out.EventDescription = ReadText(E, K::Description);

		if (const TArray<TSharedPtr<FJsonValue>>* RecordValues = ReadArray(E, K::Records))
		{
			Out.RecordInfos.Reserve(RecordValues->Num());
			Out.RecordSourceTypes.Reserve(RecordValues->Num());
			Out.RecordLinks.Reserve(RecordValues->Num());
			for (const TSharedPtr<FJsonValue>& RV : *RecordValues)
			{
				const TSharedPtr<FJsonObject> R = AsObjectOrNull(RV);
				if (!R.IsValid())
				{
					continue;
				}
				Out.RecordInfos.Add(ReadText(R, K::RecordInfo));
				Out.RecordSourceTypes.Add(ReadText(R, K::RecordType));
				Out.RecordLinks.Add(ReadString(R, K::RecordLink));
			}
		}

		if (const TArray<TSharedPtr<FJsonValue>>* ChoiceValues = ReadArray(E, K::Choices))
		{
			Out.Choices.Reserve(ChoiceValues->Num());
			for (const TSharedPtr<FJsonValue>& CV : *ChoiceValues)
			{
				const TSharedPtr<FJsonObject> C = AsObjectOrNull(CV);
				if (!C.IsValid())
				{
					continue;
				}
				FMarsComputerChoice& Choice = Out.Choices.AddDefaulted_GetRef();
				Choice.Text = ReadText(C, K::ChoiceText);
				const TSharedPtr<FJsonObject> Eff = ReadObject(C, K::Effects);
				ReadInt(Eff, K::Energy, Choice.Energy);    // missing key -> stays 0
				ReadInt(Eff, K::Oxygen, Choice.Oxygen);
				ReadInt(Eff, K::Shield, Choice.Shield);
				ReadInt(Eff, K::Dose, Choice.Dose);
				ReadInt(Eff, K::Science, Choice.Science);
			}
		}
		return true;
	}
}

// ==================================================================== Lifetime

AMarsMainComputer::AMarsMainComputer()
{
	PrimaryActorTick.bCanEverTick = false;   // never ticks
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(250.f);
	InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
	InteractionSphere->SetGenerateOverlapEvents(true);

	Simulation = CreateDefaultSubobject<UMarsColonySimulation>(TEXT("Simulation"));

	DataFilePath = TEXT("Data/mars_week_2024-05-14_20_verified_EN.json");
	InteractionKey = EKeys::E;
	InfoKey = EKeys::I;
	HUDWidgetClass = UMarsHUDWidget::StaticClass();
	ComputerWidgetClass = UMarsComputerWidget::StaticClass();
	CreditsText = FText::FromString(TEXT("NASA Space Apps Challenge\n\nTeam: (write your names in BP_MainComputer > Mars|UI > Credits Text)"));
}

UMarsColonySimulation* AMarsMainComputer::GetSimulation() const
{
	return Simulation;
}

void AMarsMainComputer::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// Only in a running game (not while the level is open in the editor).
	const UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	FString Error;
	if (!LoadData(DataFilePath, Error))
	{
		UE_LOG(LogMarsComputer, Error, TEXT("%s"), *Error);
	}
}

void AMarsMainComputer::BeginPlay()
{
	Super::BeginPlay();

	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AMarsMainComputer::HandleSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AMarsMainComputer::HandleSphereEndOverlap);
	Simulation->OnGameOverNative.AddUObject(this, &AMarsMainComputer::HandleSimGameOver);

	CreateHUD();
	BindPlayerKeys();

	if (bAutoStartGame && IsDataLoaded())
	{
		// PLAY AGAIN reloads the level with "?MarsSkipIntro" unless the title screen should show again.
		const UWorld* World = GetWorld();
		const bool bSkipIntro = World && World->URL.HasOption(TEXT("MarsSkipIntro"));
		if (bShowStartScreen && !bSkipIntro)
		{
			ShowStartScreen();   // the day clock starts when START is pressed
		}
		else
		{
			StartDayTimer(DefaultDayDurationSeconds);
		}
	}
}

void AMarsMainComputer::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopDayTimer();
	if (Simulation)
	{
		Simulation->OnGameOverNative.RemoveAll(this);
	}
	if (ComputerWidget)
	{
		ComputerWidget->RemoveFromParent();
		ComputerWidget = nullptr;
	}
	if (HUDWidget)
	{
		HUDWidget->RemoveFromParent();
		HUDWidget = nullptr;
	}
	if (StartWidget)
	{
		StartWidget->RemoveFromParent();
		StartWidget = nullptr;
	}
	if (TransitionWidget)
	{
		TransitionWidget->RemoveFromParent();
		TransitionWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AMarsMainComputer::CreateHUD()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !HUDWidgetClass)
	{
		return;
	}
	HUDWidget = CreateWidget<UMarsHUDWidget>(PC, HUDWidgetClass);
	if (HUDWidget)
	{
		HUDWidget->Init(this);
		HUDWidget->AddToViewport(0);
	}
	// Fades and end screen: above the HUD (0) and the Main Computer screen (10)
	TransitionWidget = CreateWidget<UMarsTransitionWidget>(PC, UMarsTransitionWidget::StaticClass());
	if (TransitionWidget)
	{
		TransitionWidget->Init(this);
		TransitionWidget->AddToViewport(100);
	}
}

void AMarsMainComputer::RestartLevel()
{
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true,
		bShowStartScreenOnRestart ? FString() : FString(TEXT("MarsSkipIntro")));
}

// ==================================================================== Title screen

void AMarsMainComputer::ShowStartScreen()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (bStartScreenOpen)
	{
		return;
	}
	if (!PC)
	{
		StartDayTimer(DefaultDayDurationSeconds);   // no player controller: just start the game
		return;
	}
	StartWidget = CreateWidget<UMarsStartWidget>(PC, UMarsStartWidget::StaticClass());
	if (!StartWidget)
	{
		StartDayTimer(DefaultDayDurationSeconds);
		return;
	}
	StartWidget->Init(this);
	StartWidget->AddToViewport(200);   // above everything
	bStartScreenOpen = true;

	bStartCursorWasVisible = PC->ShouldShowMouseCursor();
	SetPlayerUIMode(true);
	PC->SetIgnoreMoveInput(true);
	if (HUDWidget)
	{
		HUDWidget->SetInteractPromptVisible(false, InteractionKey);
	}
}

void AMarsMainComputer::StartMission()
{
	if (!bStartScreenOpen)
	{
		return;
	}
	bStartScreenOpen = false;
	if (StartWidget)
	{
		StartWidget->RemoveFromParent();
		StartWidget = nullptr;
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetIgnoreMoveInput(false);
	}
	SetPlayerUIMode(bStartCursorWasVisible);
	if (HUDWidget)
	{
		HUDWidget->SetInteractPromptVisible(bPlayerInRange, InteractionKey);
	}
	StartDayTimer(DefaultDayDurationSeconds);   // day 1 begins (fades in with the big "DAY 1" title)
}

void AMarsMainComputer::ExitGame()
{
	UKismetSystemLibrary::QuitGame(this, UGameplayStatics::GetPlayerController(this, 0), EQuitPreference::Quit, false);
}

// ==================================================================== Day timer

bool AMarsMainComputer::StartDayTimer(float DayDurationSeconds)
{
	if (!IsDataLoaded())
	{
		UE_LOG(LogMarsComputer, Warning, TEXT("StartDayTimer: no data loaded."));
		return false;
	}
	if (DayDurationSeconds <= 0.f)
	{
		UE_LOG(LogMarsComputer, Warning, TEXT("StartDayTimer: DayDurationSeconds must be greater than 0."));
		return false;
	}

	StopDayTimer();
	CurrentDayDuration = DayDurationSeconds;
	CurrentDay = StoredDays[0].DayNumber;   // 1
	bWeekFinished = false;

	// Looping timer: fires once per day, no per-frame cost.
	GetWorldTimerManager().SetTimer(DayTimerHandle, this, &AMarsMainComputer::HandleDayTimerElapsed, CurrentDayDuration, true);

	Simulation->StartSimulation();
	Simulation->BeginDay(CurrentDay);
	OnDayChanged.Broadcast(CurrentDay);
	return true;
}

void AMarsMainComputer::HandleDayTimerElapsed()
{
	const int32 LastDay = StoredDays.Num() > 0 ? StoredDays.Last().DayNumber : 0;   // 7
	if (CurrentDay >= LastDay)
	{
		// Day 7 is over: would go to 8, so the week ends here.
		StopDayTimer();
		bWeekFinished = true;
		OnWeekFinished.Broadcast();
		Simulation->EndWeek();
		return;
	}
	++CurrentDay;
	Simulation->BeginDay(CurrentDay);
	OnDayChanged.Broadcast(CurrentDay);
}

void AMarsMainComputer::StopDayTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(DayTimerHandle);
	}
}

void AMarsMainComputer::PauseDayTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().PauseTimer(DayTimerHandle);
	}
}

void AMarsMainComputer::ResumeDayTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().UnPauseTimer(DayTimerHandle);
	}
}

bool AMarsMainComputer::IsDayTimerRunning() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerActive(DayTimerHandle);
}

bool AMarsMainComputer::IsDayTimerPaused() const
{
	const UWorld* World = GetWorld();
	return World && World->GetTimerManager().IsTimerPaused(DayTimerHandle);
}

float AMarsMainComputer::GetDayTimeRemaining() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->GetTimerManager().TimerExists(DayTimerHandle))
	{
		return 0.f;
	}
	return FMath::Max(0.f, World->GetTimerManager().GetTimerRemaining(DayTimerHandle));
}

float AMarsMainComputer::GetDayTimeElapsed() const
{
	const UWorld* World = GetWorld();
	if (!World || !World->GetTimerManager().TimerExists(DayTimerHandle))
	{
		return 0.f;
	}
	return FMath::Max(0.f, World->GetTimerManager().GetTimerElapsed(DayTimerHandle));
}

float AMarsMainComputer::GetDayProgress() const
{
	return CurrentDayDuration > 0.f ? FMath::Clamp(GetDayTimeElapsed() / CurrentDayDuration, 0.f, 1.f) : 0.f;
}

// ==================================================================== Interaction

void AMarsMainComputer::HandleSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor != UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return;
	}
	bPlayerInRange = true;
	BindPlayerKeys();   // in case the player controller did not exist yet at BeginPlay
	if (HUDWidget && !bInfoOpen && !bComputerOpen && !bStartScreenOpen)
	{
		HUDWidget->SetInteractPromptVisible(true, InteractionKey);
	}
}

void AMarsMainComputer::HandleSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor != UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return;
	}
	bPlayerInRange = false;
	CloseComputer();
	if (HUDWidget)
	{
		HUDWidget->SetInteractPromptVisible(false, InteractionKey);
	}
}

void AMarsMainComputer::BindPlayerKeys()
{
	if (bInteractKeyBound)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	EnableInput(PC);   // keys stay active for the whole game; E only works near the computer
	if (InputComponent)
	{
		// bConsumeInput = false: the station consoles listen to the same E key.
		InputComponent->BindKey(InteractionKey, IE_Pressed, this, &AMarsMainComputer::HandleInteractKey).bConsumeInput = false;
		InputComponent->BindKey(InfoKey, IE_Pressed, this, &AMarsMainComputer::ToggleInfo).bConsumeInput = false;
		bInteractKeyBound = true;
	}
}

void AMarsMainComputer::HandleInteractKey()
{
	if (bInfoOpen || ActiveConsole.IsValid())
	{
		return;
	}
	if (bComputerOpen)
	{
		if (TryClaimInteract())
		{
			CloseComputer();
		}
	}
	else if (bPlayerInRange && !bStartScreenOpen && TryClaimInteract())
	{
		OpenComputer();
	}
}

UMarsHUDWidget* AMarsMainComputer::GetHUD() const
{
	return HUDWidget;
}

bool AMarsMainComputer::TryClaimInteract()
{
	if (LastInteractFrame == GFrameCounter)
	{
		return false;
	}
	LastInteractFrame = GFrameCounter;
	return true;
}

void AMarsMainComputer::SetActiveConsole(AMarsStationConsole* Console)
{
	ActiveConsole = Console;
}

void AMarsMainComputer::CloseActiveConsole()
{
	if (AMarsStationConsole* Console = ActiveConsole.Get())
	{
		Console->CloseConsole();
	}
	ActiveConsole = nullptr;
}

void AMarsMainComputer::ToggleComputer()
{
	if (bComputerOpen)
	{
		CloseComputer();
	}
	else
	{
		OpenComputer();
	}
}

void AMarsMainComputer::OpenComputer()
{
	if (bComputerOpen || bStartScreenOpen || !ComputerWidgetClass || (Simulation && Simulation->GetStateRef().bGameOver))
	{
		return;
	}
	CloseInfo();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	if (!ComputerWidget)
	{
		ComputerWidget = CreateWidget<UMarsComputerWidget>(PC, ComputerWidgetClass);
		if (!ComputerWidget)
		{
			return;
		}
		ComputerWidget->Init(this);
	}
	ComputerWidget->AddToViewport(10);
	ComputerWidget->OnOpened();
	bComputerOpen = true;

	bCursorWasVisible = PC->ShouldShowMouseCursor();
	SetPlayerUIMode(true);
	if (bBlockMovementWhileComputerOpen)
	{
		PC->SetIgnoreMoveInput(true);
	}
	if (bPauseDayWhileComputerOpen)
	{
		PauseDayTimer();
	}
	if (HUDWidget)
	{
		HUDWidget->SetInteractPromptVisible(false, InteractionKey);
	}
}

void AMarsMainComputer::CloseComputer()
{
	if (!bComputerOpen)
	{
		return;
	}
	bComputerOpen = false;
	if (ComputerWidget)
	{
		ComputerWidget->RemoveFromParent();
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bBlockMovementWhileComputerOpen)
		{
			PC->SetIgnoreMoveInput(false);
		}
	}
	SetPlayerUIMode(bCursorWasVisible);
	if (bPauseDayWhileComputerOpen)
	{
		ResumeDayTimer();
	}
	if (HUDWidget)
	{
		HUDWidget->SetInteractPromptVisible(bPlayerInRange, InteractionKey);
	}
}

// ==================================================================== "How to play" panel

void AMarsMainComputer::ToggleInfo()
{
	if (bInfoOpen)
	{
		CloseInfo();
	}
	else
	{
		OpenInfo();
	}
}

void AMarsMainComputer::OpenInfo()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (bInfoOpen || bStartScreenOpen || !HUDWidget || !PC || (Simulation && Simulation->GetStateRef().bGameOver))
	{
		return;
	}
	CloseComputer();
	CloseActiveConsole();
	bInfoOpen = true;

	bInfoCursorWasVisible = PC->ShouldShowMouseCursor();
	SetPlayerUIMode(true);
	PC->SetIgnoreMoveInput(true);

	bInfoPausedTimer = false;
	if (bPauseDayWhileInfoOpen && IsDayTimerRunning())
	{
		PauseDayTimer();
		bInfoPausedTimer = true;
	}
	HUDWidget->SetInteractPromptVisible(false, InteractionKey);
	HUDWidget->SetInfoVisible(true);
}

void AMarsMainComputer::CloseInfo()
{
	if (!bInfoOpen)
	{
		return;
	}
	bInfoOpen = false;
	if (HUDWidget)
	{
		HUDWidget->SetInfoVisible(false);
	}
	const bool bGameOver = Simulation && Simulation->GetStateRef().bGameOver;
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		PC->SetIgnoreMoveInput(false);
	}
	SetPlayerUIMode(bGameOver || bInfoCursorWasVisible);
	if (bInfoPausedTimer)
	{
		ResumeDayTimer();
		bInfoPausedTimer = false;
	}
	if (HUDWidget)
	{
		HUDWidget->SetInteractPromptVisible(bPlayerInRange && !bGameOver, InteractionKey);
	}
}

void AMarsMainComputer::SetPlayerUIMode(bool bUIMode)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	if (bUIMode)
	{
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
		PC->SetShowMouseCursor(true);
	}
	else
	{
		PC->SetInputMode(FInputModeGameOnly());
		PC->SetShowMouseCursor(false);
	}
}

void AMarsMainComputer::HandleSimGameOver(bool bWon, const FText& Reason)
{
	CloseActiveConsole();
	CloseComputer();
	CloseInfo();
	SetPlayerUIMode(true);   // cursor for the Restart button
}

// ==================================================================== Loading

bool AMarsMainComputer::LoadData(const FString& FilePath, FString& Error)
{
	Error.Reset();
	const FString FullPath = FPaths::IsRelative(FilePath)
		? FPaths::Combine(FPaths::ProjectContentDir(), FilePath)
		: FilePath;

	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *FullPath))
	{
		Error = FString::Printf(TEXT("File not found or unreadable: %s"), *FullPath);
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		Error = FString::Printf(TEXT("Invalid JSON in %s: %s"), *FullPath, *Reader->GetErrorMessage());
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* DayValues = ReadArray(Root, K::Days);
	if (!DayValues || DayValues->Num() == 0)
	{
		Error = FString::Printf(TEXT("'%s' array is missing or empty."), K::Days);
		return false;
	}

	// Parse into locals; members change only if everything succeeds.
	TArray<FMarsComputerDay> NewDays;
	NewDays.Reserve(DayValues->Num());
	for (const TSharedPtr<FJsonValue>& DV : *DayValues)
	{
		const TSharedPtr<FJsonObject> D = AsObjectOrNull(DV);
		if (!D.IsValid())
		{
			Error = TEXT("A day entry is not a JSON object.");
			return false;
		}
		if (!ParseDay(D, NewDays.AddDefaulted_GetRef(), Error))
		{
			return false;
		}
	}
	NewDays.Sort([](const FMarsComputerDay& A, const FMarsComputerDay& B) { return A.DayNumber < B.DayNumber; });

	TMap<int32, int32> NewIndex;
	NewIndex.Reserve(NewDays.Num());
	for (int32 i = 0; i < NewDays.Num(); ++i)
	{
		if (NewIndex.Contains(NewDays[i].DayNumber))
		{
			Error = FString::Printf(TEXT("Duplicate day number %d."), NewDays[i].DayNumber);
			return false;
		}
		NewIndex.Add(NewDays[i].DayNumber, i);
	}

	TArray<FText> NewNotes;
	if (const TArray<TSharedPtr<FJsonValue>>* NoteValues = ReadArray(Root, K::Notes))
	{
		NewNotes.Reserve(NoteValues->Num());
		for (const TSharedPtr<FJsonValue>& N : *NoteValues)
		{
			if (N.IsValid() && N->Type == EJson::String)
			{
				NewNotes.Add(FText::FromString(N->AsString()));
			}
		}
	}
	const TSharedPtr<FJsonObject> Src = ReadObject(Root, K::DataSource);

	StoredDays            = MoveTemp(NewDays);
	DayIndexByNumber      = MoveTemp(NewIndex);
	StoredNotes           = MoveTemp(NewNotes);
	StoredLocation        = ReadText(Root, K::Location);
	StoredWeekLabel       = ReadText(Root, K::Week);
	StoredDataInstitution = ReadText(Src, K::Institution);
	StoredDataLink        = ReadString(Src, K::OfficialLink);
	StoredDataNote        = ReadText(Src, K::Note);

	UE_LOG(LogMarsComputer, Log, TEXT("Loaded %d days from %s"), StoredDays.Num(), *FullPath);
	return true;
}

// ==================================================================== Getters

const FMarsComputerDay* AMarsMainComputer::FindDay(int32 DayNumber) const
{
	const int32* Index = DayIndexByNumber.Find(DayNumber);
	return Index ? &StoredDays[*Index] : nullptr;
}

bool AMarsMainComputer::GetDayData(
	int32 DayNumber,
	FString& Date, int32& Sol,
	FText& WeatherCondition, int32& AirTempMinC, int32& AirTempMaxC, int32& GroundTempMinC, int32& GroundTempMaxC,
	int32& PressurePa, FString& UVIndex, FString& Sunrise, FString& Sunset, bool& bHasWind, int32& Wind,
	bool& bHasEvent, FText& EventTitle, FText& EventDescription,
	TArray<FText>& RecordInfos, TArray<FText>& RecordSourceTypes, TArray<FString>& RecordLinks,
	int32& ChoiceCount, TArray<FText>& ChoiceTexts,
	TArray<int32>& ChoiceEnergy, TArray<int32>& ChoiceOxygen, TArray<int32>& ChoiceShield,
	TArray<int32>& ChoiceDose, TArray<int32>& ChoiceScience) const
{
	// Start from empty outputs so a missing day never returns stale values.
	Date.Reset(); Sol = 0;
	WeatherCondition = FText::GetEmpty();
	AirTempMinC = AirTempMaxC = GroundTempMinC = GroundTempMaxC = PressurePa = 0;
	UVIndex.Reset(); Sunrise.Reset(); Sunset.Reset();
	bHasWind = false; Wind = 0;
	bHasEvent = false; EventTitle = FText::GetEmpty(); EventDescription = FText::GetEmpty();
	RecordInfos.Reset(); RecordSourceTypes.Reset(); RecordLinks.Reset();
	ChoiceCount = 0; ChoiceTexts.Reset();
	ChoiceEnergy.Reset(); ChoiceOxygen.Reset(); ChoiceShield.Reset(); ChoiceDose.Reset(); ChoiceScience.Reset();

	const FMarsComputerDay* D = FindDay(DayNumber);
	if (!D)
	{
		return false;
	}

	Date = D->Date;
	Sol = D->Sol;
	WeatherCondition = D->WeatherCondition;
	AirTempMinC = D->AirTempMinC;
	AirTempMaxC = D->AirTempMaxC;
	GroundTempMinC = D->GroundTempMinC;
	GroundTempMaxC = D->GroundTempMaxC;
	PressurePa = D->PressurePa;
	UVIndex = D->UVIndex;
	Sunrise = D->Sunrise;
	Sunset = D->Sunset;
	bHasWind = D->bHasWind;
	Wind = D->Wind;

	bHasEvent = D->bHasEvent;
	EventTitle = D->EventTitle;
	EventDescription = D->EventDescription;
	RecordInfos = D->RecordInfos;
	RecordSourceTypes = D->RecordSourceTypes;
	RecordLinks = D->RecordLinks;

	ChoiceCount = D->Choices.Num();
	ChoiceTexts.Reserve(ChoiceCount);
	ChoiceEnergy.Reserve(ChoiceCount);
	ChoiceOxygen.Reserve(ChoiceCount);
	ChoiceShield.Reserve(ChoiceCount);
	ChoiceDose.Reserve(ChoiceCount);
	ChoiceScience.Reserve(ChoiceCount);
	for (const FMarsComputerChoice& C : D->Choices)
	{
		ChoiceTexts.Add(C.Text);
		ChoiceEnergy.Add(C.Energy);
		ChoiceOxygen.Add(C.Oxygen);
		ChoiceShield.Add(C.Shield);
		ChoiceDose.Add(C.Dose);
		ChoiceScience.Add(C.Science);
	}
	return true;
}

bool AMarsMainComputer::GetDayGameplay(int32 DayNumber, float& RadiationMultiplier, float& StormStartFraction,
	float& SolarMultiplier, float& ResearchMultiplier, FText& StormWarning) const
{
	const FMarsComputerDay* D = FindDay(DayNumber);
	RadiationMultiplier = D ? D->RadiationMultiplier : 1.f;
	StormStartFraction = D ? D->StormStartFraction : 0.f;
	SolarMultiplier = D ? D->SolarMultiplier : 1.f;
	ResearchMultiplier = D ? D->ResearchMultiplier : 1.f;
	StormWarning = D ? D->StormWarning : FText::GetEmpty();
	return D != nullptr;
}

void AMarsMainComputer::GetWeekInfo(
	FText& Location, FText& WeekLabel, FText& DataInstitution, FString& DataLink,
	FText& DataNote, TArray<FText>& Notes, int32& DayCount) const
{
	Location = StoredLocation;
	WeekLabel = StoredWeekLabel;
	DataInstitution = StoredDataInstitution;
	DataLink = StoredDataLink;
	DataNote = StoredDataNote;
	Notes = StoredNotes;
	DayCount = StoredDays.Num();
}
