#include "SampleNpcControllerDebugMigration.h"

#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_PromotableOperator.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"
#include "UObject/StructOnScope.h"

DEFINE_LOG_CATEGORY_STATIC(LogSampleNpcDebugMigration, Log, All);

namespace
{
    constexpr TCHAR AssetPath[] = TEXT("/Game/Blueprints/AI/AIC_NPC_SmartObject.AIC_NPC_SmartObject");
    constexpr TCHAR NativePath[] = TEXT("/Script/GameAnimationSampleCpp.SampleNpcController");
    constexpr TCHAR DrawerClass[] = TEXT("/Script/DrawDebugLibrary.DrawDebugLibrary");
    constexpr TCHAR MathClass[] = TEXT("/Script/Engine.KismetMathLibrary");
    const FName DebugName(TEXT("Debug"));
    const FName NativeName(TEXT("DebugNative"));

    struct FDebugGraph
    {
        UEdGraph* Graph = nullptr;
        UK2Node_FunctionEntry* Entry = nullptr;
        UFunction* Native = nullptr;
    };

    int32 Reject(const TCHAR* Reason)
    {
        UE_LOG(LogSampleNpcDebugMigration, Warning, TEXT("NPC Debug migration rejected: %s"), Reason);
        return 0;
    }

    UEdGraphPin* Input(const UEdGraphNode* Node, FName Name)
    {
        return Node ? Node->FindPin(Name, EGPD_Input) : nullptr;
    }

    UEdGraphPin* Output(const UEdGraphNode* Node, FName Name)
    {
        return Node ? Node->FindPin(Name, EGPD_Output) : nullptr;
    }

    bool Link(const UEdGraphPin* Source, const UEdGraphPin* Target)
    {
        return Source && Target && Target->LinkedTo.Num() == 1 &&
            Target->LinkedTo[0] == Source && Source->LinkedTo.Contains(Target);
    }

    bool Literal(const UEdGraphPin* Pin, const TCHAR* Value)
    {
        return Pin && Pin->LinkedTo.IsEmpty() && !Pin->DefaultObject && Pin->DefaultValue == Value;
    }

    bool StructLiteral(const UEdGraphPin* Pin, const TCHAR* TypePath, const TCHAR* Value)
    {
        const UScriptStruct* Type = Pin ? Cast<UScriptStruct>(Pin->PinType.PinSubCategoryObject.Get()) : nullptr;
        if (!Type || Type->GetPathName() != TypePath || !Pin->LinkedTo.IsEmpty() || Pin->DefaultObject)
        {
            return false;
        }
        FStructOnScope Actual(Type);
        FStructOnScope Expected(Type);
        const TCHAR* ActualEnd = Type->ImportText(*Pin->DefaultValue, Actual.GetStructMemory(), nullptr, 0, GWarn, Type->GetName());
        const TCHAR* ExpectedEnd = Type->ImportText(Value, Expected.GetStructMemory(), nullptr, 0, GWarn, Type->GetName());
        return ActualEnd && !*ActualEnd && ExpectedEnd && !*ExpectedEnd &&
            Type->CompareScriptStruct(Actual.GetStructMemory(), Expected.GetStructMemory(), 0);
    }

    bool ImplicitSelf(const UEdGraphPin* Pin)
    {
        return Pin && Pin->LinkedTo.IsEmpty() && !Pin->DefaultObject &&
            (Pin->DefaultValue.IsEmpty() || Pin->DefaultValue == TEXT("None"));
    }

    bool Matches(const UK2Node_CallFunction* Call, const TCHAR* Owner, FName Name)
    {
        const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
        return Function && Function->GetFName() == Name && Function->GetOwnerClass()->GetPathName() == Owner;
    }

    UK2Node_CallFunction* FindCall(const UEdGraph* Graph, const TCHAR* Owner, FName Name)
    {
        UK2Node_CallFunction* Found = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            auto* Call = Cast<UK2Node_CallFunction>(Node);
            if (Matches(Call, Owner, Name))
            {
                if (Found)
                {
                    return nullptr;
                }
                Found = Call;
            }
        }
        return Found;
    }

    bool IsVector(const UEdGraphPin* Pin)
    {
        return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
            GetPathNameSafe(Pin->PinType.PinSubCategoryObject.Get()) == TEXT("/Script/CoreUObject.Vector");
    }

    bool IsDouble(const UEdGraphPin* Pin)
    {
        return Pin && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Real &&
            Pin->PinType.PinSubCategory == UEdGraphSchema_K2::PC_Double;
    }

    bool AuditPins(const UEdGraph* Graph, int32 ExpectedLinks)
    {
        int32 LinkCount = 0;
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node || Node->GetDesiredEnabledState() == ENodeEnabledState::Disabled)
            {
                return false;
            }
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->bOrphanedPin || !Pin->SubPins.IsEmpty() ||
                    Pin->PinType.ContainerType != EPinContainerType::None)
                {
                    return false;
                }
                LinkCount += Pin->LinkedTo.Num();
                for (const UEdGraphPin* Peer : Pin->LinkedTo)
                {
                    if (!Peer || Peer->GetOwningNode()->GetGraph() != Graph || !Peer->LinkedTo.Contains(Pin))
                    {
                        return false;
                    }
                }
            }
        }
        return LinkCount == 2 * ExpectedLinks;
    }

    bool AuditOriginal(const FDebugGraph& Plan)
    {
        const UEdGraph* Graph = Plan.Graph;
        const auto* Drawer = FindCall(Graph, DrawerClass, TEXT("MakeVisualLoggerDebugDrawer"));
        const auto* Arrow = FindCall(Graph, DrawerClass, TEXT("DrawDebugArrow"));
        const auto* Pawn = FindCall(Graph, TEXT("/Script/Engine.Controller"), TEXT("K2_GetPawn"));
        const auto* Location = FindCall(Graph, TEXT("/Script/Engine.Actor"), TEXT("K2_GetActorLocation"));
        const auto* Rotation = FindCall(Graph, TEXT("/Script/Engine.Pawn"), TEXT("GetControlRotation"));
        const auto* Forward = FindCall(Graph, MathClass, TEXT("GetForwardVector"));
        const auto* Add = Cast<UK2Node_PromotableOperator>(FindCall(Graph, MathClass, TEXT("Add_VectorVector")));
        const auto* Multiply = Cast<UK2Node_PromotableOperator>(FindCall(Graph, MathClass, TEXT("Multiply_VectorFloat")));
        if (!Multiply)
        {
            // The promotable multiply may still reference its vector/vector overload before compilation.
            Multiply = Cast<UK2Node_PromotableOperator>(FindCall(Graph, MathClass, TEXT("Multiply_VectorVector")));
        }
        if (!Drawer || !Arrow || !Pawn || !Location || !Rotation || !Forward || !Add || !Multiply ||
            !AuditPins(Graph, 10))
        {
            return false;
        }
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const ENodeEnabledState Expected = (Node == Drawer || Node == Arrow)
                ? ENodeEnabledState::DevelopmentOnly : ENodeEnabledState::Enabled;
            if (Node->GetDesiredEnabledState() != Expected)
            {
                return false;
            }
        }
        if (!Drawer->GetTargetFunction()->HasMetaData(TEXT("DevelopmentOnly")) ||
            !Arrow->GetTargetFunction()->HasMetaData(TEXT("DevelopmentOnly")) ||
            !ImplicitSelf(Input(Drawer, TEXT("Object"))) ||
            !ImplicitSelf(Input(Pawn, UEdGraphSchema_K2::PN_Self)) ||
            !Literal(Input(Drawer, TEXT("Category")), TEXT("Draw Debug Library")) ||
            !Literal(Input(Drawer, TEXT("Verbosity")), TEXT("Display")) ||
            !Literal(Input(Drawer, TEXT("bDrawToScene")), TEXT("true")) ||
            !Literal(Input(Drawer, TEXT("bDrawToSceneWhileRecording")), TEXT("true")) ||
            !Literal(Input(Arrow, TEXT("bDepthTest")), TEXT("true")) ||
            !StructLiteral(Input(Arrow, TEXT("LineStyle")), TEXT("/Script/DrawDebugLibrary.DrawDebugLineStyle"),
                TEXT("(Color=(R=1.000000,G=0.000000,B=1.000000,A=1.000000),Thickness=0.000000,LineType=Solid,DashWidth=2.500000,DashSpacing=1.000000,DotSpacing=1.000000)")) ||
            !StructLiteral(Input(Arrow, TEXT("Settings")), TEXT("/Script/DrawDebugLibrary.DrawDebugArrowSettings"),
                TEXT("(bArrowheadOnStart=False,bArrowLineEndsAtStartHead=False,ArrowHeadStartSize=5.000000,ArrowHeadStartType=Simple,bArrowheadOnEnd=True,bArrowLineEndsAtEndHead=False,ArrowHeadEndSize=5.000000,ArrowHeadEndType=Simple)")))
        {
            return false;
        }

        const UEdGraphPin* Length = Input(Multiply, TEXT("B"));
        if (!IsDouble(Length) || !Length->LinkedTo.IsEmpty() || Length->DefaultObject ||
            !Length->DefaultValue.IsNumeric() || FCString::Atod(*Length->DefaultValue) != 50.0 ||
            !IsVector(Input(Multiply, TEXT("A"))) || !IsVector(Output(Multiply, TEXT("ReturnValue"))) ||
            !IsVector(Input(Add, TEXT("A"))) || !IsVector(Input(Add, TEXT("B"))) ||
            !IsVector(Output(Add, TEXT("ReturnValue"))))
        {
            return false;
        }
        return Link(Output(Plan.Entry, UEdGraphSchema_K2::PN_Then), Input(Arrow, UEdGraphSchema_K2::PN_Execute)) &&
            Link(Output(Drawer, TEXT("ReturnValue")), Input(Arrow, TEXT("Drawer"))) &&
            Link(Output(Pawn, TEXT("ReturnValue")), Input(Location, UEdGraphSchema_K2::PN_Self)) &&
            Link(Output(Pawn, TEXT("ReturnValue")), Input(Rotation, UEdGraphSchema_K2::PN_Self)) &&
            Link(Output(Location, TEXT("ReturnValue")), Input(Arrow, TEXT("StartLocation"))) &&
            Link(Output(Location, TEXT("ReturnValue")), Input(Add, TEXT("A"))) &&
            Link(Output(Rotation, TEXT("ReturnValue")), Input(Forward, TEXT("InRot"))) &&
            Link(Output(Forward, TEXT("ReturnValue")), Input(Multiply, TEXT("A"))) &&
            Link(Output(Multiply, TEXT("ReturnValue")), Input(Add, TEXT("B"))) &&
            Link(Output(Add, TEXT("ReturnValue")), Input(Arrow, TEXT("EndLocation")));
    }

    int32 Inspect(UBlueprint* Blueprint, FDebugGraph& Plan)
    {
        if (!Blueprint || Blueprint->GetPathName() != AssetPath || !Blueprint->ParentClass ||
            Blueprint->ParentClass->GetPathName() != NativePath)
        {
            return Reject(TEXT("unexpected asset or native parent"));
        }
        Plan.Native = Blueprint->ParentClass->FindFunctionByName(NativeName);
        if (!Plan.Native || Plan.Native->GetOwnerClass() != Blueprint->ParentClass ||
            Plan.Native->NumParms != 0 || !Plan.Native->HasAllFunctionFlags(FUNC_Native | FUNC_BlueprintCallable) ||
            Plan.Native->HasAnyFunctionFlags(FUNC_Static | FUNC_BlueprintPure | FUNC_Net | FUNC_BlueprintEvent) ||
            !Plan.Native->HasMetaData(TEXT("DevelopmentOnly")))
        {
            return Reject(TEXT("native DebugNative signature or development-only metadata"));
        }
        const UFunction* Legacy = Blueprint->SkeletonGeneratedClass
            ? Blueprint->SkeletonGeneratedClass->FindFunctionByName(DebugName) : nullptr;
        if (!Legacy || Legacy->NumParms != 0 || !Legacy->HasAnyFunctionFlags(FUNC_BlueprintCallable) ||
            Legacy->HasAnyFunctionFlags(FUNC_Static | FUNC_BlueprintPure | FUNC_Net))
        {
            return Reject(TEXT("legacy Debug callable signature"));
        }
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetFName() == DebugName)
            {
                if (Plan.Graph || Graph->GetOuter() != Blueprint)
                {
                    return Reject(TEXT("ambiguous Debug graph"));
                }
                Plan.Graph = Graph;
            }
        }
        if (!Plan.Graph)
        {
            return Reject(TEXT("missing Debug graph"));
        }
        TArray<UK2Node_FunctionEntry*> Entries;
        Plan.Graph->GetNodesOfClass(Entries);
        if (Entries.Num() != 1 || !Entries[0]->LocalVariables.IsEmpty() || Entries[0]->Pins.Num() != 1 ||
            !Output(Entries[0], UEdGraphSchema_K2::PN_Then))
        {
            return Reject(TEXT("unexpected Debug entry pins or local variables"));
        }
        Plan.Entry = Entries[0];
        if (Plan.Graph->Nodes.Num() == 2)
        {
            const auto* Call = FindCall(Plan.Graph, NativePath, NativeName);
            return Call && Call->GetTargetFunction() == Plan.Native && AuditPins(Plan.Graph, 1) &&
                Call->GetDesiredEnabledState() == ENodeEnabledState::DevelopmentOnly &&
                ImplicitSelf(Input(Call, UEdGraphSchema_K2::PN_Self)) &&
                Link(Output(Plan.Entry, UEdGraphSchema_K2::PN_Then), Input(Call, UEdGraphSchema_K2::PN_Execute))
                ? 1 : Reject(TEXT("invalid existing forward"));
        }
        if (Plan.Graph->Nodes.Num() == 9 && AuditOriginal(Plan))
        {
            return 2;
        }
        return Reject(TEXT("original Debug body differs from the audited draw contract"));
    }
}

int32 USampleNpcControllerDebugMigration::ClassifyNpcDebug(UBlueprint* Blueprint)
{
    FDebugGraph Plan;
    return Inspect(Blueprint, Plan);
}

int32 USampleNpcControllerDebugMigration::ForwardNpcDebug(UBlueprint* Blueprint)
{
    FDebugGraph Plan;
    const int32 State = Inspect(Blueprint, Plan);
    if (State != 2)
    {
        return State;
    }
    if (!GEditor || GEditor->PlayWorld || Blueprint->GetOutermost()->IsDirty())
    {
        return Reject(TEXT("PIE is active or the controller package is dirty"));
    }

    auto* Call = NewObject<UK2Node_CallFunction>(Plan.Graph);
    Call->SetFromFunction(Plan.Native);
    Call->AllocateDefaultPins();
    Call->SetEnabledState(ENodeEnabledState::DevelopmentOnly, false);
    UEdGraphPin* Entry = Output(Plan.Entry, UEdGraphSchema_K2::PN_Then);
    UEdGraphPin* Exec = Input(Call, UEdGraphSchema_K2::PN_Execute);
    const auto* Schema = Cast<UEdGraphSchema_K2>(Plan.Graph->GetSchema());
    if (!Schema || !Exec || !ImplicitSelf(Input(Call, UEdGraphSchema_K2::PN_Self)) ||
        !Schema->ArePinTypesCompatible(Entry->PinType, Exec->PinType, Blueprint->GeneratedClass))
    {
        return Reject(TEXT("native forward pins are incompatible"));
    }

    Blueprint->Modify();
    Plan.Graph->Modify();
    Plan.Entry->Modify();
    const TArray<TObjectPtr<UEdGraphNode>> OriginalNodes = Plan.Graph->Nodes;
    for (UEdGraphNode* Node : OriginalNodes)
    {
        if (Node != Plan.Entry)
        {
            FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
        }
    }
    Call->NodePosX = Plan.Entry->NodePosX + 400;
    Call->NodePosY = Plan.Entry->NodePosY;
    Call->SetFlags(RF_Transactional);
    Call->CreateNewGuid();
    Plan.Graph->AddNode(Call, false, false);
    Entry->MakeLinkTo(Exec);
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return ClassifyNpcDebug(Blueprint);
}
