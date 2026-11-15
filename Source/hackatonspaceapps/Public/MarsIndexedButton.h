// MarsIndexedButton.h
// A UButton that remembers an index and reports it when clicked (used for event choices).

#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "MarsIndexedButton.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FMarsIndexedButtonClicked, int32 /*Index*/);

UCLASS()
class UMarsIndexedButton : public UButton
{
	GENERATED_BODY()

public:
	void Setup(int32 InIndex);
	int32 GetIndex() const { return ButtonIndex; }

	FMarsIndexedButtonClicked OnIndexedClicked;

private:
	UFUNCTION()
	void HandleClicked();

	int32 ButtonIndex = INDEX_NONE;
};
