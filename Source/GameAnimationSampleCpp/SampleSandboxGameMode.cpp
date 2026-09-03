#include "SampleSandboxGameMode.h"

#include "DataDrivenCVars/DataDrivenCVars.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UnrealType.h"

namespace
{
    constexpr TCHAR PawnClassVariable[] = TEXT("DDCvar.PawnClass");
}

TSoftClassPtr<APawn> ASampleSandboxGameMode::GetSelectedPawn() const
{
    const IConsoleVariable* Selection = IConsoleManager::Get().FindConsoleVariable(PawnClassVariable);
    const int32 Index = Selection ? Selection->GetInt() : INDEX_NONE;
    const FArrayProperty* Options = FindFProperty<FArrayProperty>(GetClass(), TEXT("PawnClasses_Soft"));
    const FSoftClassProperty* ClassProperty = Options ? CastField<FSoftClassProperty>(Options->Inner) : nullptr;
    if (Index >= 0 && ClassProperty)
    {
        FScriptArrayHelper Classes(Options, Options->ContainerPtrToValuePtr<void>(this));
        if (Classes.IsValidIndex(Index))
        {
            const FSoftObjectPtr Choice = ClassProperty->GetPropertyValue(Classes.GetRawPtr(Index));
            if (!Choice.IsNull())
            {
                return TSoftClassPtr<APawn>(Choice.ToSoftObjectPath());
            }
        }
    }
    return TSoftClassPtr<APawn>(FSoftObjectPath(DefaultPawnClass.Get()));
}

UClass* ASampleSandboxGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    UClass* Selected = GetSelectedPawn().LoadSynchronous();
    return Selected && !Selected->HasAnyClassFlags(CLASS_Abstract)
        ? Selected
        : Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ASampleSandboxGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (UDataDrivenCVarEngineSubsystem* Variables = GEngine ? GEngine->GetEngineSubsystem<UDataDrivenCVarEngineSubsystem>() : nullptr)
    {
        Variables->OnDataDrivenCVarDelegate.AddUniqueDynamic(this, &ASampleSandboxGameMode::HandlePawnSelectionChanged);
    }
}

void ASampleSandboxGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UDataDrivenCVarEngineSubsystem* Variables = GEngine ? GEngine->GetEngineSubsystem<UDataDrivenCVarEngineSubsystem>() : nullptr)
    {
        Variables->OnDataDrivenCVarDelegate.RemoveDynamic(this, &ASampleSandboxGameMode::HandlePawnSelectionChanged);
    }
    if (PendingPawnLoad)
    {
        PendingPawnLoad->CancelHandle();
        PendingPawnLoad.Reset();
    }
    Super::EndPlay(EndPlayReason);
}

void ASampleSandboxGameMode::HandlePawnSelectionChanged(FString VariableName)
{
    if (VariableName != PawnClassVariable)
    {
        return;
    }
    if (PendingPawnLoad)
    {
        PendingPawnLoad->CancelHandle();
        PendingPawnLoad.Reset();
    }
    const FSoftObjectPath Choice = GetSelectedPawn().ToSoftObjectPath();
    if (!Choice.IsNull())
    {
        PendingPawnLoad = UAssetManager::GetStreamableManager().RequestAsyncLoad(
            Choice, FStreamableDelegate::CreateUObject(this, &ASampleSandboxGameMode::ResetAllPlayers));
    }
}

void ASampleSandboxGameMode::ResetAllPlayers()
{
    const UClass* Selected = GetSelectedPawn().Get();
    if (!Selected || Selected->HasAnyClassFlags(CLASS_Abstract))
    {
        return;
    }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Controller = It->Get();
        APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
        if (IsValid(Pawn))
        {
            const FTransform Transform = Pawn->GetActorTransform();
            const FRotator Aim = Pawn->GetBaseAimRotation();
            if (Pawn->Destroy())
            {
                RestartPlayerAtTransform(Controller, Transform);
                Controller->SetControlRotation(Aim);
            }
        }
    }
}
