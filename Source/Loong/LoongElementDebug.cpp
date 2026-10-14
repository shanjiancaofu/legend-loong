#include "LoongElementDebug.h"
#include "HAL/IConsoleManager.h"
#include "LoongCombatDemo.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"

namespace
{
#if !UE_BUILD_SHIPPING
TAutoConsoleVariable<int32> DebugEnabled(
    TEXT("Loong.Elements.Debug"), 0,
    TEXT("0 off; 1 element HUD, world labels, tick markers. No Niagara required."));
TAutoConsoleVariable<int32> TestEnabled(
    TEXT("Loong.Elements.TestMode"), 0,
    TEXT(
        "PIE/Game test only: guarantee configured element hits, lower direct hit damage and skill cooldown. Does not equip missing dragons or change assets."));
TAutoConsoleVariable<float> DebugInterval(TEXT("Loong.Elements.DrawInterval"), 0.2f,
                                          TEXT("World debug redraw seconds (clamped 0.1..2)."));
TAutoConsoleVariable<int32> DebugTargets(TEXT("Loong.Elements.MaxTargets"), 1,
                                         TEXT("Max marked enemies in a world (clamped 1..30)."));
#endif
}

bool LoongElementDebug::Enabled()
{
#if !UE_BUILD_SHIPPING
    return DebugEnabled.GetValueOnGameThread() != 0;
#else
    return false;
#endif
}

bool LoongElementDebug::TestMode()
{
#if !UE_BUILD_SHIPPING
    return TestEnabled.GetValueOnGameThread() != 0;
#else
    return false;
#endif
}

float LoongElementDebug::DrawInterval()
{
#if !UE_BUILD_SHIPPING
    return FMath::Clamp(DebugInterval.GetValueOnGameThread(), 0.1f, 2.f);
#else
    return 0.2f;
#endif
}

int32 LoongElementDebug::MaxTargets()
{
#if !UE_BUILD_SHIPPING
    return FMath::Clamp(DebugTargets.GetValueOnGameThread(), 1, 30);
#else
    return 0;
#endif
}

void LoongElementDebug::SetEnabled(bool bEnabled)
{
#if !UE_BUILD_SHIPPING
    DebugEnabled->Set(bEnabled ? 1 : 0, ECVF_SetByConsole);
#endif
}

void LoongElementDebug::SetTestMode(bool bEnabled)
{
#if !UE_BUILD_SHIPPING
    TestEnabled->Set(bEnabled ? 1 : 0, ECVF_SetByConsole);
#endif
}
