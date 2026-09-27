#include "SampleMoverModes.h"

#include "DefaultMovementSet/CharacterMoverComponent.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "MoveLibrary/MovementUtils.h"
#include "MoverDataModelTypes.h"
#include "MoverSimulationTypes.h"
#include "UObject/UnrealType.h"
#include "UserDefinedStructSupport.h"

namespace
{
    double ReadMoverAirRotationOffset(const FMoverDataCollection& Command)
    {
        for (const TSharedPtr<FMoverDataStructBase>& Data : Command.GetDataArray())
        {
            if (!Data || Data->GetScriptStruct() != FMoverUserDefinedDataStruct::StaticStruct())
            {
                continue;
            }
            const auto& Snapshot = static_cast<const FMoverUserDefinedDataStruct&>(*Data).StructInstance;
            const UScriptStruct* Type = Snapshot.GetScriptStruct();
            if (Type && Type->GetFName() == TEXT("S_MoverCustomInputs"))
            {
                const auto* Field = FindFProperty<FNumericProperty>(Type, TEXT("RotationOffset_14_9438753848B6A37A4F76569318195C60"));
                return Field && Field->IsFloatingPoint()
                    ? Field->GetFloatingPointPropertyValue(Field->ContainerPtrToValuePtr<void>(Snapshot.GetMemory())) : 0.0;
            }
        }
        return 0.0;
    }
}

void USampleMoverFallingMode::GenerateMove_Implementation(const FMoverSimContext& SimContext,
    const FMoverTickStartData& StartState, const FMoverTimeStep& TimeStep, FProposedMove& OutProposedMove) const
{
    Super::GenerateMove_Implementation(SimContext, StartState, TimeStep, OutProposedMove);
    const FMoverDataCollection& Command = StartState.InputCmd.InputCollection;
    const auto* Input = Command.FindDataByType<FCharacterDefaultInputs>();
    const auto* State = StartState.SyncState.SyncStateCollection.FindDataByType<FMoverDefaultSyncState>();
    FRotator Facing = Input ? Input->OrientationIntent.Rotation() : FRotator::ZeroRotator;
    Facing.Yaw += ReadMoverAirRotationOffset(Command);
    const FRotator Current = State ? State->GetOrientation_BaseSpace() : FRotator::ZeroRotator;
    const UWorld* World = GetWorld();
    const float DeltaSeconds = World ? World->GetDeltaSeconds() : 0.0f;
    OutProposedMove.AngularVelocityDegrees = UMovementUtils::ComputeAngularVelocityDegrees(Current, Facing, DeltaSeconds, 300.0f);
}

FTransitionEvalResult USampleMoverEnterSlideTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
    const UMoverComponent* Mover = Params.MovingComps.MoverComponent.Get();
    static const FGameplayTag Crouching = FGameplayTag::RequestGameplayTag(TEXT("Mover.Stance.IsCrouching"), false);
    return Mover && Crouching.IsValid() && Mover->HasGameplayTag(Crouching, false) &&
        Params.ProposedMove.LinearVelocity.SizeSquared2D() > FMath::Square(380.0)
        ? FTransitionEvalResult(TEXT("Sliding")) : FTransitionEvalResult::NoTransition;
}

FTransitionEvalResult USampleMoverExitSlideTransition::Evaluate_Implementation(const FSimulationTickParams& Params) const
{
    const auto* Mover = Cast<UCharacterMoverComponent>(Params.MovingComps.MoverComponent.Get());
    return !Mover || !Mover->IsCrouching() || Params.ProposedMove.LinearVelocity.SizeSquared2D() <= FMath::Square(200.0)
        ? FTransitionEvalResult(TEXT("Walking")) : FTransitionEvalResult::NoTransition;
}
