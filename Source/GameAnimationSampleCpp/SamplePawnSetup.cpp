#include "SamplePawnSetup.h"

#include "Components/InputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/GameplayCameraComponentBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Sound/SoundBase.h"
#include "UObject/UnrealType.h"

namespace
{
    void ChangeSandboxCameraStyle(APawn& Pawn, int32 Step)
    {
        constexpr TCHAR Variable[] = TEXT("DDCVar.CameraStyle");
        auto* Player = Cast<APlayerController>(Pawn.GetController());
        const int32 Previous = UKismetSystemLibrary::GetConsoleVariableIntValue(Variable);
        const int32 Selection = static_cast<int32>(FMath::Clamp(static_cast<int64>(Previous) + Step, 0LL, 3LL));
        UKismetSystemLibrary::ExecuteConsoleCommand(&Pawn, FString::Printf(TEXT("%s %d"), Variable, Selection), Player);
        static const TCHAR* const Names[] = {TEXT("Close"), TEXT("Medium"), TEXT("Far"), TEXT("Debug")};
        const int32 Current = UKismetSystemLibrary::GetConsoleVariableIntValue(Variable);
        if (Current >= 0 && Current < UE_ARRAY_COUNT(Names))
        {
            UKismetSystemLibrary::PrintString(&Pawn, FString::Printf(TEXT("Camera Style: %s"), Names[Current]),
                true, false, FLinearColor(1.0f, 0.666667f, 0.0f), 1.0f, TEXT("Camera change"));
        }
        const TSoftObjectPtr<USoundBase> Sound(
            FSoftObjectPath(TEXT("/Engine/VREditor/Sounds/UI/Object_Snaps_To_Another_Actor.Object_Snaps_To_Another_Actor")));
        if (USoundBase* Loaded = Sound.LoadSynchronous())
        {
            UGameplayStatics::PlaySound2D(&Pawn, Loaded);
        }
    }
}

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

void SamplePawnSetup::BindCameraStyleKeys(APawn& Pawn, UInputComponent& Input)
{
    const auto Bind = [&Pawn, &Input](const FKey& Key, int32 Step)
    {
        FInputKeyBinding Binding(FInputChord(Key), IE_Pressed);
        Binding.KeyDelegate.GetDelegateForManualSet().BindWeakLambda(&Pawn, [&Pawn, Step]
        {
            ChangeSandboxCameraStyle(Pawn, Step);
        });
        Input.KeyBindings.Add(MoveTemp(Binding));
    };
    Bind(EKeys::MouseScrollDown, 1);
    Bind(EKeys::Gamepad_DPad_Down, 1);
    Bind(EKeys::MouseScrollUp, -1);
    Bind(EKeys::Gamepad_DPad_Up, -1);
}
