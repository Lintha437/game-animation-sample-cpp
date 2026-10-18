#include "SamplePawnSetup.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameplayCameraComponentBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/UnrealType.h"

void SamplePawnSetup::InstallInputMapping(APawn& Pawn)
{
    const auto* Player = Cast<APlayerController>(Pawn.GetController());
    auto* Input = Player ? ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Player->GetLocalPlayer()) : nullptr;
    if (Input)
    {
        const TSoftObjectPtr<UInputMappingContext> SandboxMapping(FSoftObjectPath(TEXT("/Game/Input/IMC_Sandbox.IMC_Sandbox")));
        if (const auto* Mapping = SandboxMapping.LoadSynchronous(); Mapping && !Input->HasMappingContext(Mapping))
        {
            Input->AddMappingContext(Mapping, 0);
        }
    }
}

void SamplePawnSetup::ActivateCamera(APawn& Pawn, FName LegacyCamera)
{
    auto* Player = Cast<APlayerController>(Pawn.GetController());
    if (!Player)
    {
        return;
    }

    const bool bUseGameplayCamera = UKismetSystemLibrary::GetConsoleVariableBoolValue(TEXT("DDCVar.NewGameplayCameraSystem.Enable"));
    const FName CameraName = bUseGameplayCamera ? FName(TEXT("GameplayCamera")) : LegacyCamera;
    const auto* Field = FindFProperty<FObjectPropertyBase>(Pawn.GetClass(), CameraName);
    auto* Component = Field ? Cast<UActorComponent>(Field->GetObjectPropertyValue_InContainer(&Pawn)) : nullptr;
    if (bUseGameplayCamera)
    {
        if (auto* Camera = Cast<UGameplayCameraComponentBase>(Component))
        {
            Camera->ActivateCameraForPlayerController(Player, true);
        }
    }
    else
    {
        if (Component)
        {
            Component->Activate();
        }
        Player->SetViewTarget(&Pawn);
    }
}
