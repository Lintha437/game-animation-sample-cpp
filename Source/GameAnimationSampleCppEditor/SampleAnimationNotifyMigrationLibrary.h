#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SampleAnimationNotifyMigrationLibrary.generated.h"

class UAnimSequenceBase;

/** Transfers authored event instances from a graphless Blueprint notify to its native parent. */
UCLASS()
class USampleAnimationNotifyMigrationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Replaces exact-class notify instances in place while preserving the complete event record.
     * The animation owns new notify instances. The caller owns backups and persistence.
     * @param Animation Animation sequence, composite or montage to inspect or update.
     * @param LegacyClass Imported Blueprint class with a concrete native notify parent.
     * @param FieldRenames Map of authored Blueprint property names to native settings.
     * @param bValidateOnly Validate property conversion without modifying the animation.
     * @return Number of matching events, or -1 on an unsupported class or property.
     */
    UFUNCTION(BlueprintCallable, Category="Sample|Migration")
    static int32 ReplaceAnimationNotifyInstances(UAnimSequenceBase* Animation, UClass* LegacyClass,
        const TMap<FName, FName>& FieldRenames, bool bValidateOnly);
};
