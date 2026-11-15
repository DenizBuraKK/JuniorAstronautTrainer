// MarsStationConsole.cpp

#include "MarsStationConsole.h"
#include "MarsConsoleWidget.h"
#include "MarsHUDWidget.h"
#include "MarsMainComputer.h"

#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AMarsStationConsole::AMarsStationConsole()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
	InteractionSphere->SetupAttachment(RootComponent);
	InteractionSphere->SetSphereRadius(200.f);
	InteractionSphere->SetCollisionProfileName(TEXT("Trigger"));
	InteractionSphere->SetGenerateOverlapEvents(true);

	InteractionKey = EKeys::E;
}

AMarsMainComputer* AMarsStationConsole::GetComputer() const
{
	return MainComputer;
}

void AMarsStationConsole::BeginPlay()
{
	Super::BeginPlay();

	if (!MainComputer)
	{
		for (TActorIterator<AMarsMainComputer> It(GetWorld()); It; ++It)
		{
			MainComputer = *It;
			break;
		}
	}
	InteractionSphere->OnComponentBeginOverlap.AddDynamic(this, &AMarsStationConsole::HandleSphereBeginOverlap);
	InteractionSphere->OnComponentEndOverlap.AddDynamic(this, &AMarsStationConsole::HandleSphereEndOverlap);
	BindPlayerKeys();
}

void AMarsStationConsole::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ConsoleWidget)
	{
		ConsoleWidget->RemoveFromParent();
		ConsoleWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AMarsStationConsole::BindPlayerKeys()
{
	if (bKeyBound)
	{
		return;
	}
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC)
	{
		return;
	}
	EnableInput(PC);
	if (InputComponent)
	{
		// Not consumed: the Main Computer and other stations listen to the same key.
		InputComponent->BindKey(InteractionKey, IE_Pressed, this, &AMarsStationConsole::HandleInteractKey).bConsumeInput = false;
		bKeyBound = true;
	}
}

// ==================================================================== Interaction

void AMarsStationConsole::HandleSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!OtherActor || OtherActor != UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return;
	}
	bPlayerInRange = true;
	BindPlayerKeys();
	ShowPrompt(true);
}

void AMarsStationConsole::HandleSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (!OtherActor || OtherActor != UGameplayStatics::GetPlayerPawn(this, 0))
	{
		return;
	}
	bPlayerInRange = false;
	CloseConsole();
	ShowPrompt(false);
}

void AMarsStationConsole::ShowPrompt(bool bVisible)
{
	AMarsMainComputer* C = GetComputer();
	UMarsHUDWidget* HUD = C ? C->GetHUD() : nullptr;
	if (!HUD)
	{
		return;
	}
	const bool bBlocked = C->IsComputerOpen() || C->IsInfoOpen() || C->IsStartScreenOpen()
		|| (C->GetActiveConsole() != nullptr) || (C->GetSimulation() && C->GetSimulation()->GetStateRef().bGameOver);
	if (bVisible && !bBlocked)
	{
		HUD->SetInteractPromptVisible(true, InteractionKey,
			FText::FromString(FString::Printf(TEXT("Use the %s"), *GetStationName().ToString())));
	}
	else if (!bVisible)
	{
		// Give the prompt back to the Main Computer if the player is still next to it.
		HUD->SetInteractPromptVisible(C->IsPlayerInRange() && !bBlocked, C->InteractionKey);
	}
}

void AMarsStationConsole::HandleInteractKey()
{
	AMarsMainComputer* C = GetComputer();
	if (!C)
	{
		return;
	}
	if (bOpen)
	{
		if (C->TryClaimInteract())
		{
			CloseConsole();
		}
		return;
	}
	if (!bPlayerInRange || C->IsComputerOpen() || C->IsInfoOpen() || C->IsStartScreenOpen() || C->GetActiveConsole()
		|| (C->GetSimulation() && C->GetSimulation()->GetStateRef().bGameOver))
	{
		return;
	}
	if (C->TryClaimInteract())
	{
		OpenConsole();
	}
}

void AMarsStationConsole::OpenConsole()
{
	AMarsMainComputer* C = GetComputer();
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (bOpen || !C || !PC || C->IsStartScreenOpen())
	{
		return;
	}
	if (AMarsStationConsole* Other = C->GetActiveConsole())
	{
		Other->CloseConsole();
	}
	C->CloseComputer();
	C->CloseInfo();

	if (!ConsoleWidget)
	{
		ConsoleWidget = CreateWidget<UMarsConsoleWidget>(PC, UMarsConsoleWidget::StaticClass());
		if (!ConsoleWidget)
		{
			return;
		}
		ConsoleWidget->Init(this);
	}
	ConsoleWidget->AddToViewport(10);
	ConsoleWidget->OnOpened();
	bOpen = true;
	C->SetActiveConsole(this);

	bCursorWasVisible = PC->ShouldShowMouseCursor();
	C->SetPlayerUIMode(true);
	if (bBlockMovementWhileOpen)
	{
		PC->SetIgnoreMoveInput(true);
	}
	if (UMarsHUDWidget* HUD = C->GetHUD())
	{
		HUD->SetInteractPromptVisible(false, InteractionKey);
	}
}

void AMarsStationConsole::CloseConsole()
{
	if (!bOpen)
	{
		return;
	}
	bOpen = false;
	if (ConsoleWidget)
	{
		ConsoleWidget->RemoveFromParent();
	}
	AMarsMainComputer* C = GetComputer();
	if (C && C->GetActiveConsole() == this)
	{
		C->SetActiveConsole(nullptr);
	}
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
	{
		if (bBlockMovementWhileOpen)
		{
			PC->SetIgnoreMoveInput(false);
		}
	}
	const bool bGameOver = C && C->GetSimulation() && C->GetSimulation()->GetStateRef().bGameOver;
	if (C && !bGameOver)
	{
		C->SetPlayerUIMode(bCursorWasVisible);
	}
	if (bPlayerInRange)
	{
		ShowPrompt(true);
	}
}

// ==================================================================== Task

FText AMarsStationConsole::GetStationName() const
{
	if (!StationName.IsEmpty())
	{
		return StationName;
	}
	switch (Task)
	{
	case EMarsSideTask::Energy:      return FText::FromString(TEXT("Power Reactor"));
	case EMarsSideTask::LifeSupport: return FText::FromString(TEXT("Life Support Bay"));
	case EMarsSideTask::Research:
	default:                         return FText::FromString(TEXT("Research Lab"));
	}
}

void AMarsStationConsole::RefreshUsesForToday() const
{
	const AMarsMainComputer* C = GetComputer();
	const int32 Day = (C && C->GetSimulation()) ? C->GetSimulation()->GetStateRef().Day : 0;
	if (Day != UsesDay)
	{
		UsesDay = Day;
		UsesToday = 0;
	}
}

int32 AMarsStationConsole::GetUsesLeftToday() const
{
	if (UsesPerDay <= 0)
	{
		return -1;
	}
	RefreshUsesForToday();
	return FMath::Max(0, UsesPerDay - UsesToday);
}

bool AMarsStationConsole::CanDoTask(FText& Reason) const
{
	Reason = FText::GetEmpty();
	const AMarsMainComputer* C = GetComputer();
	const UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!Sim || Sim->GetStateRef().bGameOver || Sim->GetStateRef().Day <= 0)
	{
		Reason = FText::FromString(TEXT("The mission is not running."));
		return false;
	}
	if (GetUsesLeftToday() == 0)
	{
		Reason = FText::FromString(TEXT("Already done today. Come back tomorrow!"));
		return false;
	}
	if (Sim->IsSideTaskFull(Task))
	{
		Reason = FText::FromString(Task == EMarsSideTask::Energy
			? TEXT("The battery is already full. Nothing to gain right now.")
			: TEXT("The crew is already at full health. Nothing to gain right now."));
		return false;
	}
	return true;
}

float AMarsStationConsole::CompleteTask(FText& Result)
{
	Result = FText::GetEmpty();
	if (!CanDoTask(Result))
	{
		return 0.f;
	}
	AMarsMainComputer* C = GetComputer();
	UMarsColonySimulation* Sim = C ? C->GetSimulation() : nullptr;
	if (!Sim)
	{
		return 0.f;
	}
	const float Added = Sim->AddSideTaskBonus(Task, RewardAmount, Result);
	if (Added > 0.f)
	{
		RefreshUsesForToday();
		++UsesToday;
	}
	return Added;
}
