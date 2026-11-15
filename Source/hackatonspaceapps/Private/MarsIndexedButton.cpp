// MarsIndexedButton.cpp

#include "MarsIndexedButton.h"

void UMarsIndexedButton::Setup(int32 InIndex)
{
	ButtonIndex = InIndex;
	OnClicked.AddUniqueDynamic(this, &UMarsIndexedButton::HandleClicked);
}

void UMarsIndexedButton::HandleClicked()
{
	OnIndexedClicked.Broadcast(ButtonIndex);
}
