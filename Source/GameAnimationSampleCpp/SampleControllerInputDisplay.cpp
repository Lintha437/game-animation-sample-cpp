#include "SampleControllerInputDisplay.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"

float USampleControllerInputDisplay::SampleAnalogKeys(APlayerController* PlayerController, const TArray<FKey>& Keys)
{
    float AnalogValue = 0.0f;
    if (PlayerController)
    {
        for (const FKey& Key : Keys)
        {
            const double KeyValue = PlayerController->GetInputAnalogKeyState(Key);
            AnalogValue = static_cast<float>(static_cast<double>(AnalogValue) + KeyValue);
        }
    }
    return AnalogValue;
}

FVector USampleControllerInputDisplay::SampleWSAD(APlayerController* PlayerController)
{
    if (!PlayerController)
    {
        return FVector::ZeroVector;
    }

    const double D = PlayerController->GetInputAnalogKeyState(EKeys::D);
    const double A = PlayerController->GetInputAnalogKeyState(EKeys::A);
    const double W = PlayerController->GetInputAnalogKeyState(EKeys::W);
    const double S = PlayerController->GetInputAnalogKeyState(EKeys::S);
    return FVector(D + A * -1.0, W + S * -1.0, 0.0);
}

FVector USampleControllerInputDisplay::SampleMouse(
    const UObject* WorldContextObject, APlayerController* PlayerController, FVector Current)
{
    float DeltaX = 0.0f;
    float DeltaY = 0.0f;
    if (PlayerController)
    {
        PlayerController->GetInputMouseDelta(DeltaX, DeltaY);
    }

    const double WorldDelta = UGameplayStatics::GetWorldDeltaSeconds(WorldContextObject);
    // K2 expands the third operand as a second division. Its divide helper returns zero for a zero divisor.
    const double X = UKismetMathLibrary::Divide_DoubleDouble(
        UKismetMathLibrary::Divide_DoubleDouble(DeltaX, WorldDelta), 100.0);
    const double Y = UKismetMathLibrary::Divide_DoubleDouble(
        UKismetMathLibrary::Divide_DoubleDouble(DeltaY, WorldDelta), 100.0);
    const FVector Target = UKismetMathLibrary::ClampVectorSize(FVector(X, Y * -1.0, 0.0), 0.0, 1.0);
    return UKismetMathLibrary::VInterpTo(Current, Target, static_cast<float>(WorldDelta), 10.0f);
}

namespace
{
    void SampleStick(APlayerController* PlayerController, const FKey& StickKey, const FVector& Contribution,
        double YScale, float& Opacity, double& TranslationX, double& TranslationY)
    {
        const FVector Gamepad = PlayerController ? PlayerController->GetInputVectorKeyState(StickKey) : FVector::ZeroVector;
        const FVector Combined = Gamepad + Contribution;

        // The authored opacity uses the unbounded XY magnitude. Only the translation uses the size clamp.
        const double Magnitude = UKismetMathLibrary::VSizeXY(Combined);
        Opacity = Magnitude > 0.2 ? static_cast<float>(Magnitude) : 0.0f;
        const FVector Position = UKismetMathLibrary::ClampVectorSize(Combined, 0.0, 1.0);
        TranslationX = Position.X * 40.0;
        TranslationY = Position.Y * YScale;
    }
}

void USampleControllerInputDisplay::SampleLeftStick(APlayerController* PlayerController, FVector Contribution,
    float& Opacity, double& TranslationX, double& TranslationY)
{
    SampleStick(PlayerController, EKeys::Gamepad_Left2D, Contribution, -40.0, Opacity, TranslationX, TranslationY);
}

void USampleControllerInputDisplay::SampleRightStick(APlayerController* PlayerController, FVector Contribution,
    float& Opacity, double& TranslationX, double& TranslationY)
{
    SampleStick(PlayerController, EKeys::Gamepad_Right2D, Contribution, 40.0, Opacity, TranslationX, TranslationY);
}
