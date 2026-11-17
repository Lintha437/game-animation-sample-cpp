#include "Examples/SampleLookAtLevelRuntime.h"

#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"

void USampleLookAtLevelRuntime::SetControlRigDebugState(const UObject* WorldContextObject, bool bEnabled)
{
    UKismetSystemLibrary::ExecuteConsoleCommand(
        WorldContextObject,
        bEnabled ? TEXT("a.AnimNode.ControlRig.Debug 1") : TEXT("a.AnimNode.ControlRig.Debug 0"),
        UGameplayStatics::GetPlayerController(WorldContextObject, 0));

    UKismetSystemLibrary::ExecuteConsoleCommand(
        WorldContextObject,
        bEnabled ? TEXT("ControlRig.EnableDrawInterfaceInGame 1")
                 : TEXT("ControlRig.EnableDrawInterfaceInGame 0"),
        UGameplayStatics::GetPlayerController(WorldContextObject, 0));
}
