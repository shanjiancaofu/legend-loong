#pragma once

#include "CoreMinimal.h"

namespace LoongElementDebug
{
bool Enabled();
bool TestMode();
void SetEnabled(bool bEnabled);
void SetTestMode(bool bEnabled);
float DrawInterval();
int32 MaxTargets();
}
