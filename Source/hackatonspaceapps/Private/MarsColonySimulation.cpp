// MarsColonySimulation.cpp

#include "MarsColonySimulation.h"
#include "MarsMainComputer.h"

#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	FString SignedInt(float Value)
	{
		const int32 Rounded = FMath::RoundToInt32(Value);
		return Rounded > 0 ? FString::Printf(TEXT("+%d"), Rounded) : FString::Printf(TEXT("%d"), Rounded);
	}

	void AppendEffect(FString& Out, const TCHAR* Name, int32 Value)
	{
		if (Value == 0)
		{
			return;
		}
		if (!Out.IsEmpty())
		{
			Out += TEXT(", ");
		}
		Out += FString::Printf(TEXT("%s %s"), Name, *SignedInt(static_cast<float>(Value)));
	}
}

UMarsColonySimulation::UMarsColonySimulation()
{
	PrimaryComponentTick.bCanEverTick = false;   // driven by a timer, not Tick
}

AMarsMainComputer* UMarsColonySimulation::GetComputer() const
{
	return Cast<AMarsMainComputer>(GetOwner());
}

// ==================================================================== Control

void UMarsColonySimulation::StartSimulation()
{
	State = FMarsColonyState();
	State.Battery = FMath::Clamp(BatteryStart, 0.f, BatteryMax);
	State.BatteryMax = BatteryMax;
	State.Oxygen = FMath::Clamp(OxygenStart, 0.f, 100.f);
	State.Shield = FMath::Clamp(ShieldStart, 0.f, 100.f);
	State.CrewHealth = FMath::Clamp(HealthStart, 0.f, 100.f);
	State.DoseLimit = DoseLimit;
	State.ResearchGoal = ResearchGoal;
	if (const AMarsMainComputer* Computer = GetComputer())
	{
		State.DayCount = Computer->GetDayCount();
	}
	PendingEventDay = 0;
	SetAllocation(StartAllocLifeSupport, StartAllocShield, StartAllocResearch);

	bRunning = true;
	if (UWorld* World = GetWorld())
	{
		LastStepTime = World->GetTimeSeconds();
		World->GetTimerManager().SetTimer(StepTimer, this, &UMarsColonySimulation::StepSimulation, FMath::Max(0.02f, SimStepSeconds), true);
	}
	Broadcast();
}

void UMarsColonySimulation::StopSimulation()
{
	bRunning = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StepTimer);
	}
}

void UMarsColonySimulation::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopSimulation();
	Super::EndPlay(EndPlayReason);
}

void UMarsColonySimulation::BeginDay(int32 Day)
{
	if (State.bGameOver)
	{
		return;
	}
	const AMarsMainComputer* Computer = GetComputer();
	bool bIgnoredAlert = false;

	// Player never answered yesterday's alert: the riskiest option happens on its own.
	if (PendingEventDay != 0 && Computer)
	{
		if (const FMarsComputerDay* Old = Computer->FindDay(PendingEventDay))
		{
			int32 Worst = 0;
			for (int32 i = 1; i < Old->Choices.Num(); ++i)
			{
				const FMarsComputerChoice& A = Old->Choices[i];
				const FMarsComputerChoice& B = Old->Choices[Worst];
				if (A.Dose > B.Dose || (A.Dose == B.Dose && A.Science > B.Science))
				{
					Worst = i;
				}
			}
			if (Old->Choices.IsValidIndex(Worst))
			{
				const FText ChoiceText = Old->Choices[Worst].Text;
				ApplyChoice(PendingEventDay, Worst, true);
				State.Research   = FMath::Max(0.f, State.Research - IgnoredAlertResearchPenalty);
				State.CrewHealth = FMath::Clamp(State.CrewHealth - IgnoredAlertHealthPenalty, 0.f, 100.f);
				bIgnoredAlert = true;
				State.LastMessage = FText::FromString(FString::Printf(
					TEXT("Alert ignored on day %d! Without orders the crew did \"%s\" and it went badly. Research -%d, crew health -%d."),
					PendingEventDay, *ChoiceText.ToString(), FMath::RoundToInt32(IgnoredAlertResearchPenalty), FMath::RoundToInt32(IgnoredAlertHealthPenalty)));
			}
		}
		PendingEventDay = 0;
		if (CheckFailure())
		{
			return;
		}
	}

	State.Day = Day;
	State.bEventPending = false;
	if (Computer)
	{
		if (const FMarsComputerDay* D = Computer->FindDay(Day))
		{
			if (D->bHasEvent && D->Choices.Num() > 0)
			{
				PendingEventDay = Day;
				State.bEventPending = true;
				if (!bIgnoredAlert)
				{
					State.LastMessage = FText::FromString(FString::Printf(TEXT("Day %d: %s"), Day, *D->EventTitle.ToString()));
				}
			}
			else if (Day > 1 && !bIgnoredAlert)
			{
				State.LastMessage = FText::FromString(FString::Printf(TEXT("Day %d started. No incidents reported."), Day));
			}
		}
	}
	UpdateRates();
	RebuildAlerts();
	Broadcast();
}

void UMarsColonySimulation::EndWeek()
{
	if (State.bGameOver)
	{
		return;
	}
	// Safety: the crew must be alive and safe at the very end too (a lost crew is never a win).
	if (CheckFailure())
	{
		return;
	}
	if (State.Research >= State.ResearchGoal)
	{
		FinishGame(true, FText::FromString(FString::Printf(TEXT("All %d sols survived. Research %d / %d. Crew health %d%%, radiation dose %d / %d."),
			State.DayCount, FMath::RoundToInt32(State.Research), FMath::RoundToInt32(State.ResearchGoal),
			FMath::RoundToInt32(State.CrewHealth), FMath::RoundToInt32(State.Dose), FMath::RoundToInt32(State.DoseLimit))));
	}
	else
	{
		FinishGame(false, FText::FromString(FString::Printf(TEXT("The crew survived the week, but research reached only %d / %d."),
			FMath::RoundToInt32(State.Research), FMath::RoundToInt32(State.ResearchGoal))));
	}
}

void UMarsColonySimulation::SetAllocation(float LifeSupport, float ShieldPower, float ResearchPower)
{
	float L = FMath::Clamp(LifeSupport, 0.f, 1.f);
	float S = FMath::Clamp(ShieldPower, 0.f, 1.f);
	float R = FMath::Clamp(ResearchPower, 0.f, 1.f);
	const float Total = L + S + R;
	if (Total > MaxTotalAllocation && Total > 0.f)
	{
		const float K = MaxTotalAllocation / Total;
		L *= K; S *= K; R *= K;
	}
	State.AllocLifeSupport = L;
	State.AllocShield = S;
	State.AllocResearch = R;
	UpdateRates();
	RebuildAlerts();
	Broadcast();
}

bool UMarsColonySimulation::ResolveEvent(int32 ChoiceIndex, FText& ResultText)
{
	ResultText = FText::GetEmpty();
	const AMarsMainComputer* Computer = GetComputer();
	if (State.bGameOver || PendingEventDay == 0 || !Computer)
	{
		return false;
	}
	const FMarsComputerDay* D = Computer->FindDay(PendingEventDay);
	if (!D || !D->Choices.IsValidIndex(ChoiceIndex))
	{
		return false;
	}

	const FText Effects = DescribeChoiceEffects(PendingEventDay, ChoiceIndex);
	ApplyChoice(PendingEventDay, ChoiceIndex);
	ResultText = FText::FromString(FString::Printf(TEXT("Done: \"%s\". %s"), *D->Choices[ChoiceIndex].Text.ToString(), *Effects.ToString()));
	State.LastMessage = ResultText;
	PendingEventDay = 0;
	State.bEventPending = false;

	UpdateRates();
	if (!CheckFailure())
	{
		RebuildAlerts();
		Broadcast();
	}
	return true;
}

void UMarsColonySimulation::DebugFinish(bool bWon)
{
	if (!bRunning || State.bGameOver || State.Day <= 0)
	{
		return;
	}
	FinishGame(bWon, FText::FromString(bWon
		? FString::Printf(TEXT("Test shortcut: mission marked as complete. Research %d / %d."), FMath::RoundToInt32(State.Research), FMath::RoundToInt32(State.ResearchGoal))
		: FString(TEXT("Test shortcut: mission marked as failed."))));
}

bool UMarsColonySimulation::IsSideTaskFull(EMarsSideTask Task) const
{
	if (!bRunning || State.bGameOver)
	{
		return true;
	}
	switch (Task)
	{
	case EMarsSideTask::Energy:      return State.Battery >= State.BatteryMax - KINDA_SMALL_NUMBER;
	case EMarsSideTask::LifeSupport: return State.CrewHealth >= 100.f - KINDA_SMALL_NUMBER;
	case EMarsSideTask::Research:
	default:                         return false;   // research has no maximum
	}
}

float UMarsColonySimulation::AddSideTaskBonus(EMarsSideTask Task, float Amount, FText& ResultText)
{
	ResultText = FText::GetEmpty();
	if (!bRunning || State.bGameOver || Amount <= 0.f)
	{
		return 0.f;
	}
	float Added = 0.f;
	FString What;
	switch (Task)
	{
	case EMarsSideTask::Energy:
		Added = FMath::Min(Amount, FMath::Max(0.f, State.BatteryMax - State.Battery));
		State.Battery += Added;
		What = TEXT("battery");
		break;
	case EMarsSideTask::LifeSupport:
		Added = FMath::Min(Amount, FMath::Max(0.f, 100.f - State.CrewHealth));
		State.CrewHealth += Added;
		What = TEXT("crew health");
		break;
	case EMarsSideTask::Research:
	default:
		Added = Amount;
		State.Research += Added;
		What = TEXT("research");
		break;
	}

	if (Added <= KINDA_SMALL_NUMBER)
	{
		ResultText = FText::FromString(FString::Printf(TEXT("Side task done, but the %s is already full. Nothing gained."), *What));
		return 0.f;
	}
	ResultText = FText::FromString(FString::Printf(TEXT("Side task done: +%d %s."), FMath::RoundToInt32(Added), *What));
	State.LastMessage = ResultText;
	UpdateRates();
	RebuildAlerts();
	Broadcast();
	return Added;
}

FText UMarsColonySimulation::DescribeChoiceEffects(int32 Day, int32 ChoiceIndex) const
{
	const AMarsMainComputer* Computer = GetComputer();
	const FMarsComputerDay* D = Computer ? Computer->FindDay(Day) : nullptr;
	if (!D || !D->Choices.IsValidIndex(ChoiceIndex))
	{
		return FText::GetEmpty();
	}
	const FMarsComputerChoice& C = D->Choices[ChoiceIndex];
	FString Out;
	AppendEffect(Out, TEXT("Battery"), C.Energy);
	AppendEffect(Out, TEXT("Oxygen"), C.Oxygen);
	AppendEffect(Out, TEXT("Shield"), C.Shield);
	AppendEffect(Out, TEXT("Radiation dose"), C.Dose);
	AppendEffect(Out, TEXT("Research"), C.Science);
	return FText::FromString(Out);
}

void UMarsColonySimulation::ApplyChoice(int32 Day, int32 ChoiceIndex, bool bOnlyBadEffects)
{
	const AMarsMainComputer* Computer = GetComputer();
	const FMarsComputerDay* D = Computer ? Computer->FindDay(Day) : nullptr;
	if (!D || !D->Choices.IsValidIndex(ChoiceIndex))
	{
		return;
	}
	const FMarsComputerChoice& C = D->Choices[ChoiceIndex];
	// "Bad" = losing battery/oxygen/shield/research, or gaining dose.
	const int32 Energy  = bOnlyBadEffects ? FMath::Min(0, C.Energy)  : C.Energy;
	const int32 Oxygen  = bOnlyBadEffects ? FMath::Min(0, C.Oxygen)  : C.Oxygen;
	const int32 Shield  = bOnlyBadEffects ? FMath::Min(0, C.Shield)  : C.Shield;
	const int32 Science = bOnlyBadEffects ? FMath::Min(0, C.Science) : C.Science;
	const int32 Dose    = bOnlyBadEffects ? FMath::Max(0, C.Dose)    : C.Dose;

	State.Battery    = FMath::Clamp(State.Battery + Energy, 0.f, State.BatteryMax);
	State.Oxygen     = FMath::Clamp(State.Oxygen + Oxygen, 0.f, 100.f);
	State.Shield     = FMath::Clamp(State.Shield + Shield, 0.f, 100.f);
	State.Dose       = FMath::Max(0.f, State.Dose + Dose);
	State.CrewHealth = FMath::Clamp(State.CrewHealth - FMath::Max(0, Dose) * HealthPerDose, 0.f, 100.f);
	State.Research   = FMath::Max(0.f, State.Research + Science);
}

// ==================================================================== Step

void UMarsColonySimulation::StepSimulation()
{
	UWorld* World = GetWorld();
	AMarsMainComputer* Computer = GetComputer();
	if (!World || !Computer)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	// Clamp: a long frame hitch (loading, shader compile, debugger, window drag) must not
	// apply minutes of base time in one jump.
	const float RealDelta = FMath::Clamp(static_cast<float>(Now - LastStepTime), 0.f, MaxStepSeconds);
	LastStepTime = Now;

	State.DayTimeRemaining = Computer->GetDayTimeRemaining();
	State.DayProgress = Computer->GetDayProgress();

	// Time only flows while the day clock runs (paused clock = paused base).
	if (!bRunning || State.bGameOver || !Computer->IsDayTimerRunning() || Computer->GetDayDuration() <= 0.f)
	{
		Broadcast();
		return;
	}

	UpdateRates();
	ApplyRates(RealDelta / Computer->GetDayDuration());
	if (CheckFailure())
	{
		return;
	}
	RebuildAlerts();
	Broadcast();
}

void UMarsColonySimulation::UpdateRates()
{
	float RadiationMult = 1.f, StormStart = 0.f, Solar = 1.f, ResearchMult = 1.f;
	if (const AMarsMainComputer* Computer = GetComputer())
	{
		if (const FMarsComputerDay* D = Computer->FindDay(State.Day))
		{
			RadiationMult = D->RadiationMultiplier;
			StormStart = D->StormStartFraction;
			Solar = D->SolarMultiplier;
			ResearchMult = D->ResearchMultiplier;
		}
	}

	const bool bStormyDay = RadiationMult > 1.f;
	State.bStormActive = bStormyDay && State.DayProgress >= StormStart;
	State.bStormIncoming = bStormyDay && !State.bStormActive;
	const float RadiationNow = State.bStormActive ? RadiationMult : 1.f;
	State.RadiationMultiplier = RadiationNow;

	// Power: allocations above 100% are paid from the battery ("overdrive").
	const float Generation = PowerPerDay * Solar;
	State.PowerGeneration = Generation;
	const float Total = State.AllocLifeSupport + State.AllocShield + State.AllocResearch;
	float Scale = 1.f;
	State.bOverdrive = false;
	State.bPowerShortage = false;
	if (Total > 1.f)
	{
		if (State.Battery > 0.5f)
		{
			State.bOverdrive = true;
			State.BatteryRate = -Generation * (Total - 1.f);
		}
		else
		{
			State.bPowerShortage = true;
			Scale = 1.f / Total;
			State.BatteryRate = 0.f;
		}
	}
	else
	{
		State.BatteryRate = Generation * (1.f - Total);
	}
	if (State.Battery >= State.BatteryMax && State.BatteryRate > 0.f)
	{
		State.BatteryRate = 0.f;   // full: extra power is wasted
	}

	const float LifePower = State.AllocLifeSupport * Generation * Scale;
	const float ShieldPower = State.AllocShield * Generation * Scale;
	const float ResearchPower = State.AllocResearch * Generation * Scale;

	State.OxygenRate = (LifePower - OxygenNeedPerDay) * OxygenPerPower;
	State.ShieldRate = ShieldPower - ShieldDecayPerDay - ShieldDrainPerRadiation * RadiationNow;
	const float Protection = ShieldMaxProtection * State.Shield / 100.f;
	State.DoseRate = BaseDosePerDay * RadiationNow * (1.f - Protection);
	State.ResearchRate = ResearchPower * ResearchPerPower * ResearchMult;

	State.HealthRate = -State.DoseRate * HealthPerDose;
	if (State.Oxygen < OxygenLowThreshold && OxygenLowThreshold > 0.f)
	{
		State.HealthRate -= (OxygenLowThreshold - State.Oxygen) / OxygenLowThreshold * OxygenLowDamagePerDay;
	}
	else
	{
		State.HealthRate += HealthRegenPerDay;
	}

	// Already full and would still go up: nothing changes (same as the battery). Without this the
	// crew health rate flipped between -3 and +2 every step while sitting at 100%.
	if (State.CrewHealth >= 100.f && State.HealthRate > 0.f)
	{
		State.HealthRate = 0.f;
	}
	if (State.Oxygen >= 100.f && State.OxygenRate > 0.f)
	{
		State.OxygenRate = 0.f;
	}
	if (State.Shield >= 100.f && State.ShieldRate > 0.f)
	{
		State.ShieldRate = 0.f;
	}
}

void UMarsColonySimulation::ApplyRates(float DayFraction)
{
	if (DayFraction <= 0.f)
	{
		return;
	}
	State.Battery    = FMath::Clamp(State.Battery + State.BatteryRate * DayFraction, 0.f, State.BatteryMax);
	State.Oxygen     = FMath::Clamp(State.Oxygen + State.OxygenRate * DayFraction, 0.f, 100.f);
	State.Shield     = FMath::Clamp(State.Shield + State.ShieldRate * DayFraction, 0.f, 100.f);
	State.Dose       = FMath::Max(0.f, State.Dose + State.DoseRate * DayFraction);
	State.CrewHealth = FMath::Clamp(State.CrewHealth + State.HealthRate * DayFraction, 0.f, 100.f);
	State.Research   = FMath::Max(0.f, State.Research + State.ResearchRate * DayFraction);
}

void UMarsColonySimulation::RebuildAlerts()
{
	State.Alerts.Reset();
	const AMarsMainComputer* Computer = GetComputer();
	const FMarsComputerDay* D = Computer ? Computer->FindDay(State.Day) : nullptr;

	if (State.bEventPending && D)
	{
		State.Alerts.Add(FText::FromString(FString::Printf(TEXT("ALERT: %s - go to the Main Computer!"), *D->EventTitle.ToString())));
	}
	if (State.bStormActive)
	{
		State.Alerts.Add(FText::FromString(TEXT("RADIATION STORM ACTIVE - give power to the shield!")));
	}
	else if (State.bStormIncoming && D)
	{
		const FString Warning = D->StormWarning.IsEmpty() ? FString(TEXT("Radiation storm expected later today.")) : D->StormWarning.ToString();
		State.Alerts.Add(FText::FromString(FString::Printf(TEXT("WARNING: %s"), *Warning)));
	}
	if (State.Oxygen < OxygenLowThreshold)
	{
		State.Alerts.Add(FText::FromString(TEXT("LOW OXYGEN - give power to life support!")));
	}
	if (State.bPowerShortage)
	{
		State.Alerts.Add(FText::FromString(TEXT("BATTERY EMPTY - systems are running at reduced power.")));
	}
	if (State.Dose >= State.DoseLimit * 0.75f)
	{
		State.Alerts.Add(FText::FromString(TEXT("Crew radiation dose is close to the safety limit!")));
	}
	if (State.CrewHealth < 35.f)
	{
		State.Alerts.Add(FText::FromString(TEXT("Crew health is critical!")));
	}
}

bool UMarsColonySimulation::CheckFailure()
{
	if (State.bGameOver)
	{
		return true;
	}
	if (State.Oxygen <= 0.f)
	{
		FinishGame(false, FText::FromString(TEXT("The base ran out of oxygen. Life support needed more power.")));
		return true;
	}
	if (State.CrewHealth <= 0.f)
	{
		FinishGame(false, FText::FromString(TEXT("The crew's health reached zero.")));
		return true;
	}
	if (State.Dose >= State.DoseLimit)
	{
		FinishGame(false, FText::FromString(TEXT("The crew's radiation dose passed the safety limit. The shield needed more power during the storms.")));
		return true;
	}
	return false;
}

void UMarsColonySimulation::FinishGame(bool bWon, const FText& Reason)
{
	State.bGameOver = true;
	State.bWon = bWon;
	State.GameOverReason = Reason;
	State.bEventPending = false;
	State.Alerts.Reset();
	PendingEventDay = 0;
	StopSimulation();
	if (AMarsMainComputer* Computer = GetComputer())
	{
		Computer->StopDayTimer();
	}
	Broadcast();
	OnGameOverNative.Broadcast(bWon, Reason);
	OnGameOver.Broadcast(bWon, Reason);
}

void UMarsColonySimulation::Broadcast()
{
	OnStateChangedNative.Broadcast();
	OnStateChanged.Broadcast();
}
