#include "SampleMoverAnimationDiagnosticsMigration.h"

#include "Algo/Find.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimInstance.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Pawn.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CommutativeAssociativeBinaryOperator.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_GetEnumeratorNameAsString.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_MakeArray.h"
#include "K2Node_SwitchInteger.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetStringLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    using EState = ESampleAnimationStateLineMigrationState;

    struct FTarget
    {
        const TCHAR* Package;
        FName Graph;
        FName DirectionConversion;
        bool bIsCmc;
    };

    const FTarget Targets[] = {
        {TEXT("/Game/Blueprints/SandboxCharacter_CMC_ABP"), TEXT("Debug_ExperimentalStateMachine"),
            TEXT("K2Node_GetEnumeratorNameAsString_2"), true},
        {TEXT("/Game/Blueprints/SandboxCharacter_Mover_ABP"), TEXT("DebugDraws"),
            TEXT("K2Node_GetEnumeratorNameAsString_5"), false}
    };

    const FName FormatterName(TEXT("FormatStateLine"));
    const FName AppendName(TEXT("K2Node_CommutativeAssociativeBinaryOperator_2"));
    const FName Inputs[] = {TEXT("MovementMode"), TEXT("Stance"), TEXT("Gait"), TEXT("MovementDirection")};
    const FName AppendInputs[] = {TEXT("A"), TEXT("C"), TEXT("E"), TEXT("G")};
    const FName Separators[] = {TEXT("B"), TEXT("D"), TEXT("F")};

    const FName GraphFormatterName(TEXT("FormatGraphDiagnosticLine"));
    const FGuid DrawBoolStatesGuid(0x1BDFF432, 0x4489D94C, 0xBF73E0B9, 0x79E5F27F);
    const FGuid DrawStringArrayGuid(0xD8C99665, 0x42764FE3, 0x1B4529B5, 0x3799B601);

    struct FPinMove
    {
        UEdGraphPin* Source;
        FName OldInput;
        FName NewInput;
    };

    struct FReplacement
    {
        UK2Node_CallFunction* Formatter = nullptr;
        const UFunction* NativeFunction = nullptr;
        UEdGraphPin* Output = nullptr;
        UEdGraphPin* Consumer = nullptr;
        TArray<FPinMove> Inputs;
        const TCHAR* Field = nullptr;
        UK2Node_CallFunction* NewNode = nullptr;
    };

    struct FStateLine
    {
        UEdGraph* Graph = nullptr;
        UK2Node_CallFunction* Formatter = nullptr;
        const UFunction* NativeFunction = nullptr;
        UEdGraphPin* Sources[UE_ARRAY_COUNT(Inputs)] = {};
        UEdGraphPin* Output = nullptr;
        UEdGraphPin* Consumer = nullptr;
    };

    void LogInspectionRejection(const UObject* Context, const TCHAR* Stage, const TCHAR* Reason)
    {
        UE_LOG(LogTemp, Warning, TEXT("Animation diagnostics migration rejected %s at %s: %s"),
            *GetPathNameSafe(Context), Stage, Reason);
    }

    UEdGraphNode* FindNode(const UEdGraph& Graph, FName Name)
    {
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            if (Node && Node->GetFName() == Name)
            {
                return Node;
            }
        }
        return nullptr;
    }

    UEdGraphPin* SolePeer(UEdGraphPin* Pin)
    {
        if (Pin && Pin->LinkedTo.Num() == 1)
        {
            UEdGraphPin* Peer = Pin->LinkedTo[0];
            if (Peer && Peer->LinkedTo.Contains(Pin))
            {
                return Peer;
            }
        }
        return nullptr;
    }

    bool IsExactCall(const UK2Node_CallFunction* Call, const UFunction* Function)
    {
        return Call && Function && Call->GetTargetFunction() == Function;
    }

    bool IsExactHelpfulFunctionsCall(const UK2Node_CallFunction* Call, const UBlueprint* Blueprint,
        FName FunctionName, const FGuid& FunctionGuid)
    {
        if (!Call || !Blueprint || Blueprint->GetPathName() !=
            TEXT("/Game/Blueprints/Data/BFL_HelpfulFunctions.BFL_HelpfulFunctions"))
        {
            return false;
        }

        // Blueprint member resolution can select a generated or skeleton function and refresh
        // the reference's GUID. Audit the reference and its declaring graph before resolution.
        const FMemberReference& Reference = Call->FunctionReference;
        const UClass* Parent = Reference.GetMemberParentClass();
        if (!Parent || (Parent != Blueprint->GeneratedClass && Parent != Blueprint->SkeletonGeneratedClass) ||
            Reference.IsSelfContext() || Reference.IsLocalScope() ||
            Reference.GetMemberName() != FunctionName || Reference.GetMemberGuid() != FunctionGuid)
        {
            return false;
        }

        for (const UEdGraph* FunctionGraph : Blueprint->FunctionGraphs)
        {
            if (FunctionGraph && FunctionGraph->GetOuter() == Blueprint &&
                FunctionGraph->GetFName() == FunctionName && FunctionGraph->GraphGuid == FunctionGuid)
            {
                return true;
            }
        }
        return false;
    }

    bool HasInputLink(UEdGraphPin* Input, UEdGraphPin* Output)
    {
        return Output && SolePeer(Input) == Output;
    }

    bool IsVisibleString(const UEdGraphPin* Pin)
    {
        return Pin && !Pin->bHidden && !Pin->bOrphanedPin &&
            Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_String &&
            Pin->PinType.ContainerType == EPinContainerType::None;
    }

    bool HasStaticStringOutput(const UK2Node_CallFunction& Call, int32 PinCount)
    {
        const UEdGraphPin* Self = Call.FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
        return Call.Pins.Num() == PinCount && Self && Self->bHidden && !Self->bOrphanedPin &&
            Self->LinkedTo.IsEmpty() && IsVisibleString(Call.FindPin(TEXT("ReturnValue"), EGPD_Output));
    }

    bool HasFormatterPins(const UK2Node_CallFunction& Call, bool bOriginal)
    {
        if (!HasStaticStringOutput(Call, bOriginal ? 9 : 6))
        {
            return false;
        }
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Inputs); ++Index)
        {
            if (!IsVisibleString(Call.FindPin(bOriginal ? AppendInputs[Index] : Inputs[Index], EGPD_Input)))
            {
                return false;
            }
        }
        if (bOriginal)
        {
            for (const FName Name : Separators)
            {
                const UEdGraphPin* Pin = Call.FindPin(Name, EGPD_Input);
                if (!IsVisibleString(Pin) || !Pin->LinkedTo.IsEmpty() || Pin->DefaultValue != TEXT(" | "))
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool HasStateCVarGate(const UEdGraph& Graph, const UEdGraphNode& Consumer)
    {
        const auto* CVar = Cast<UK2Node_CallFunction>(FindNode(Graph, TEXT("K2Node_CallFunction_2")));
        const auto* Branch = Cast<UK2Node_IfThenElse>(FindNode(Graph, TEXT("K2Node_IfThenElse_1")));
        const auto* Sequence = Cast<UK2Node_ExecutionSequence>(FindNode(Graph, TEXT("K2Node_ExecutionSequence_0")));
        if (!IsExactCall(CVar, UKismetSystemLibrary::StaticClass()->FindFunctionByName(
                TEXT("GetConsoleVariableBoolValue"))) || !Branch || !Sequence)
        {
            return false;
        }

        const UEdGraphPin* Name = CVar->FindPin(TEXT("VariableName"), EGPD_Input);
        return IsVisibleString(Name) && Name->LinkedTo.IsEmpty() &&
            Name->DefaultValue == TEXT("DDCvar.DrawCharacterDebugStates") &&
            HasInputLink(CVar->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input),
                Sequence->FindPin(TEXT("then_0"), EGPD_Output)) &&
            HasInputLink(Branch->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input),
                CVar->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output)) &&
            HasInputLink(Branch->FindPin(TEXT("Condition"), EGPD_Input),
                CVar->FindPin(TEXT("ReturnValue"), EGPD_Output)) &&
            HasInputLink(Consumer.FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input),
                Branch->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output));
    }

    bool HasLocalControlGate(UEdGraph& Graph)
    {
        const auto* Entry = Cast<UK2Node_FunctionEntry>(FindNode(Graph, TEXT("K2Node_FunctionEntry_0")));
        const auto* Branch = Cast<UK2Node_IfThenElse>(FindNode(Graph, TEXT("K2Node_IfThenElse_2")));
        const auto* LocalControl = Cast<UK2Node_CallFunction>(FindNode(Graph, TEXT("K2Node_CallFunction_16")));
        const auto* PawnOwner = Cast<UK2Node_CallFunction>(FindNode(Graph, TEXT("K2Node_CallFunction_11")));
        const UEdGraphPin* Else = Branch ? Branch->FindPin(UEdGraphSchema_K2::PN_Else, EGPD_Output) : nullptr;
        return Entry && Else && Else->LinkedTo.IsEmpty() &&
            IsExactCall(LocalControl, APawn::StaticClass()->FindFunctionByName(TEXT("IsLocallyControlled"))) &&
            IsExactCall(PawnOwner, UAnimInstance::StaticClass()->FindFunctionByName(TEXT("TryGetPawnOwner"))) &&
            HasInputLink(Branch->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input),
                Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output)) &&
            HasInputLink(Branch->FindPin(TEXT("Condition"), EGPD_Input),
                LocalControl->FindPin(TEXT("ReturnValue"), EGPD_Output)) &&
            HasInputLink(LocalControl->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input),
                PawnOwner->FindPin(TEXT("ReturnValue"), EGPD_Output));
    }

    UEdGraphPin* FindEnumOutput(const UEdGraph& Graph, FName ConversionName, FName VariableName)
    {
        const auto* Conversion = Cast<UK2Node_GetEnumeratorNameAsString>(FindNode(Graph, ConversionName));
        if (!Conversion)
        {
            return nullptr;
        }
        UEdGraphPin* EnumSource = SolePeer(Conversion->FindPin(TEXT("Enumerator"), EGPD_Input));
        const auto* Variable = EnumSource ? Cast<UK2Node_VariableGet>(EnumSource->GetOwningNode()) : nullptr;
        if (!Variable || Variable->GetVarName() != VariableName ||
            EnumSource != Variable->FindPin(VariableName, EGPD_Output))
        {
            return nullptr;
        }
        return Conversion->FindPin(TEXT("ReturnValue"), EGPD_Output);
    }

    EState InspectStatus(UBlueprint* Blueprint, FStateLine& Line, const FTarget*& Target)
    {
        const auto Reject = [Blueprint](const TCHAR* Reason)
        {
            LogInspectionRejection(Blueprint, TEXT("InspectStatus"), Reason);
            return EState::Rejected;
        };

        if (!Cast<UAnimBlueprint>(Blueprint))
        {
            return Reject(TEXT("asset is not an animation Blueprint"));
        }
        const FString Package = Blueprint->GetOutermost()->GetName();
        Target = Algo::FindByPredicate(Targets,
            [&Package](const FTarget& Candidate) { return Package == Candidate.Package; });
        if (!Target)
        {
            return Reject(TEXT("asset package has no registered migration target"));
        }

        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetFName() == Target->Graph && Graph->GetOuter() == Blueprint)
            {
                Line.Graph = Graph;
                break;
            }
        }
        const UClass* NativeClass = FindObject<UClass>(nullptr,
            TEXT("/Script/GameAnimationSampleCpp.SampleMoverAnimationDiagnostics"));
        Line.NativeFunction = NativeClass ? NativeClass->FindFunctionByName(FormatterName) : nullptr;
        if (!Line.Graph)
        {
            return Reject(TEXT("target function graph was not found"));
        }
        if (!Line.NativeFunction)
        {
            return Reject(TEXT("native FormatStateLine function was not found"));
        }
        if (!Line.NativeFunction->HasAllFunctionFlags(FUNC_Static | FUNC_BlueprintPure))
        {
            return Reject(TEXT("native FormatStateLine function is not static and Blueprint-pure"));
        }

        UEdGraph& Graph = *Line.Graph;
        auto* Draw = Cast<UK2Node_CallFunction>(FindNode(Graph, TEXT("K2Node_CallFunction_17")));
        const UClass* DrawLibrary = FindObject<UClass>(nullptr, TEXT("/Script/DrawDebugLibrary.DrawDebugLibrary"));
        if (!DrawLibrary)
        {
            return Reject(TEXT("DrawDebugLibrary class was not found"));
        }
        if (!IsExactCall(Draw, DrawLibrary->FindFunctionByName(TEXT("DrawDebugString"))))
        {
            return Reject(TEXT("expected DrawDebugString node was not found"));
        }
        if (!HasStateCVarGate(Graph, *Draw))
        {
            return Reject(TEXT("state CVar execution gate did not match"));
        }
        if (!HasLocalControlGate(Graph))
        {
            return Reject(TEXT("local-control execution gate did not match"));
        }

        Line.Consumer = Draw->FindPin(TEXT("String"), EGPD_Input);
        Line.Output = SolePeer(Line.Consumer);
        Line.Formatter = Line.Output ? Cast<UK2Node_CallFunction>(Line.Output->GetOwningNode()) : nullptr;
        if (!IsVisibleString(Line.Consumer))
        {
            return Reject(TEXT("DrawDebugString String input is missing or is not a visible string pin"));
        }
        if (!Line.Formatter || !Graph.Nodes.Contains(Line.Formatter))
        {
            return Reject(TEXT("DrawDebugString String input is not linked to a formatter node"));
        }
        if (Line.Output != Line.Formatter->FindPin(TEXT("ReturnValue"), EGPD_Output) ||
            SolePeer(Line.Output) != Line.Consumer)
        {
            return Reject(TEXT("status formatter output is not linked exclusively to DrawDebugString"));
        }

        const bool bOriginal = Line.Formatter->GetFName() == AppendName;
        const UClass* ExpectedClass = bOriginal ? UK2Node_CommutativeAssociativeBinaryOperator::StaticClass()
            : UK2Node_CallFunction::StaticClass();
        const UFunction* ExpectedFunction = bOriginal
            ? UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("Concat_StrStr")) : Line.NativeFunction;
        if (Line.Formatter->GetClass() != ExpectedClass)
        {
            return Reject(TEXT("status formatter node class did not match its migration state"));
        }
        if (!IsExactCall(Line.Formatter, ExpectedFunction))
        {
            return Reject(TEXT("status formatter function did not match its migration state"));
        }
        if (!HasFormatterPins(*Line.Formatter, bOriginal))
        {
            return Reject(TEXT("status formatter pin layout or separator defaults did not match"));
        }

        for (const UEdGraphNode* Node : Graph.Nodes)
        {
            const auto* Call = Cast<UK2Node_CallFunction>(Node);
            if (Node && Node != Line.Formatter &&
                (Node->GetFName() == AppendName ||
                    (Call && Call->FunctionReference.GetMemberName() == FormatterName)))
            {
                return Reject(TEXT("graph contains an additional state-line formatter node"));
            }
        }

        const FName ConversionNames[] = {TEXT("K2Node_GetEnumeratorNameAsString_0"),
            TEXT("K2Node_GetEnumeratorNameAsString_3"), TEXT("K2Node_GetEnumeratorNameAsString_1"),
            Target->DirectionConversion};
        for (int32 Index = 0; Index < UE_ARRAY_COUNT(Inputs); ++Index)
        {
            UEdGraphPin* Source = FindEnumOutput(Graph, ConversionNames[Index], Inputs[Index]);
            UEdGraphPin* Input = Line.Formatter->FindPin(bOriginal ? AppendInputs[Index] : Inputs[Index], EGPD_Input);
            if (!IsVisibleString(Source))
            {
                return Reject(TEXT("expected enum-to-string source is missing or is not a visible string pin"));
            }
            if (SolePeer(Input) != Source || SolePeer(Source) != Input)
            {
                return Reject(TEXT("enum-to-string source is not linked to its matching formatter input"));
            }
            Line.Sources[Index] = Source;
        }
        return bOriginal ? EState::Original : EState::Installed;
    }
    struct FRow
    {
        FName Append;
        FName Source;
        FName Slot;
        const TCHAR* Label;
        const TCHAR* Field;
    };

    const FRow CmcStateRows[] = {
        {TEXT("K2Node_CommutativeAssociativeBinaryOperator_0"), TEXT("K2Node_GetEnumeratorNameAsString_4"),
            TEXT("[0]"), TEXT("Current State: "), TEXT("CurrentState")},
        {TEXT("K2Node_CommutativeAssociativeBinaryOperator_7"), TEXT("K2Node_GetArrayItem_0"),
            TEXT("[1]"), TEXT("Last Transition: "), TEXT("LastTransition")},
        {TEXT("K2Node_CommutativeAssociativeBinaryOperator_9"), TEXT("K2Node_CallFunction_37"),
            TEXT("[2]"), TEXT("Last MM Cost: "), TEXT("LastMotionMatchingCost")}
    };

    const FRow MoverStateRows[] = {
        CmcStateRows[0],
        {TEXT("K2Node_CommutativeAssociativeBinaryOperator_9"), TEXT("K2Node_CallFunction_31"),
            TEXT("[1]"), TEXT("Current Anim: "), TEXT("CurrentAnim")},
        {TEXT("K2Node_CommutativeAssociativeBinaryOperator_7"), TEXT("K2Node_GetArrayItem_0"),
            TEXT("[2]"), TEXT("Last Transition: "), TEXT("LastTransition")},
        {TEXT("K2Node_CommutativeAssociativeBinaryOperator_1"), TEXT("K2Node_CallFunction_37"),
            TEXT("[3]"), TEXT("Last MM Cost: "), TEXT("LastMotionMatchingCost")}
    };

    const FRow MotionMatchingRows[] = {
        {TEXT("K2Node_CommutativeAssociativeBinaryOperator_10"), TEXT("K2Node_CallFunction_35"),
            TEXT("[0]"), TEXT("Current Anim: "), TEXT("CurrentAnim")},
        {TEXT("K2Node_CommutativeAssociativeBinaryOperator_3"), TEXT("K2Node_CallFunction_14"),
            TEXT("[2]"), TEXT("Last MM Cost: "), TEXT("LastMotionMatchingCost")}
    };

    UEdGraphPin* FindVariableOutput(const UEdGraph& Graph, FName NodeName, FName VariableName)
    {
        const auto* Variable = Cast<UK2Node_VariableGet>(FindNode(Graph, NodeName));
        return Variable && Variable->GetVarName() == VariableName
            ? Variable->FindPin(VariableName, EGPD_Output) : nullptr;
    }

    UEdGraphPin* FindRowSource(const UEdGraph& Graph, const FRow& Row)
    {
        if (Row.Source == TEXT("K2Node_GetEnumeratorNameAsString_4"))
        {
            return FindEnumOutput(Graph, Row.Source, TEXT("StateMachineState"));
        }
        if (Row.Source == TEXT("K2Node_GetArrayItem_0"))
        {
            const auto* Item = Cast<UK2Node_GetArrayItem>(FindNode(Graph, Row.Source));
            const UEdGraphPin* Index = Item ? Item->FindPin(TEXT("Dimension 1"), EGPD_Input) : nullptr;
            if (Index && Index->LinkedTo.IsEmpty() && Index->DefaultValue == TEXT("0") &&
                HasInputLink(Item->FindPin(TEXT("Array"), EGPD_Input),
                    FindVariableOutput(Graph, TEXT("K2Node_VariableGet_8"), TEXT("TransitionHistory"))))
            {
                return Item->FindPin(TEXT("Output"), EGPD_Output);
            }
            return nullptr;
        }

        const auto* Call = Cast<UK2Node_CallFunction>(FindNode(Graph, Row.Source));
        if (Row.Source == TEXT("K2Node_CallFunction_31") || Row.Source == TEXT("K2Node_CallFunction_35"))
        {
            if (!IsExactCall(Call, UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("GetDisplayName"))))
            {
                return nullptr;
            }
            UEdGraphPin* ObjectSource = FindVariableOutput(Graph, TEXT("K2Node_VariableGet_50"),
                TEXT("CurrentSelectedAnim"));
            if (Row.Source == TEXT("K2Node_CallFunction_31"))
            {
                const auto* BlendStack = Cast<UK2Node_BreakStruct>(FindNode(Graph, TEXT("K2Node_BreakStruct_2")));
                ObjectSource = BlendStack && HasInputLink(BlendStack->FindPin(TEXT("S_BlendStackInputs"), EGPD_Input),
                    FindVariableOutput(Graph, TEXT("K2Node_VariableGet_47"), TEXT("BlendStackInputs")))
                    ? BlendStack->FindPin(TEXT("Anim_3_CE8F6C8948855759C43A24A538203DDC"), EGPD_Output) : nullptr;
            }
            return HasInputLink(Call->FindPin(TEXT("Object"), EGPD_Input), ObjectSource)
                ? Call->FindPin(TEXT("ReturnValue"), EGPD_Output) : nullptr;
        }

        const bool bMotionMatching = Row.Source == TEXT("K2Node_CallFunction_14");
        return IsExactCall(Call, UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("Conv_DoubleToString"))) &&
            HasInputLink(Call->FindPin(TEXT("InDouble"), EGPD_Input), FindVariableOutput(Graph,
                bMotionMatching ? TEXT("K2Node_VariableGet_44") : TEXT("K2Node_VariableGet_54"),
                bMotionMatching ? TEXT("MM Search Cost") : TEXT("Search Cost")))
            ? Call->FindPin(TEXT("ReturnValue"), EGPD_Output) : nullptr;
    }

    bool HasGraphFormatterPins(const UK2Node_CallFunction& Call, const TCHAR* Field)
    {
        const UEdGraphPin* Pin = Call.FindPin(TEXT("Field"), EGPD_Input);
        const UEnum* Enum = FindObject<UEnum>(nullptr,
            TEXT("/Script/GameAnimationSampleCpp.ESampleAnimationDiagnosticField"));
        return HasStaticStringOutput(Call, 4) && Pin && !Pin->bHidden && !Pin->bOrphanedPin &&
            Pin->LinkedTo.IsEmpty() && Enum && Pin->PinType.PinSubCategoryObject.Get() == Enum &&
            Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Byte &&
            Pin->PinType.ContainerType == EPinContainerType::None && Enum->GetIndexByNameString(Field) != INDEX_NONE &&
            Pin->DefaultValue == Field && IsVisibleString(Call.FindPin(TEXT("Value"), EGPD_Input));
    }

    bool InspectRow(const UEdGraph& Graph, const UK2Node_MakeArray& Array, const FRow& Row,
        const UFunction& NativeFunction, EState State, TArray<FReplacement>& Replacements)
    {
        const auto Reject = [&Graph, &Row](const TCHAR* Reason)
        {
            UE_LOG(LogTemp, Warning, TEXT("Animation diagnostics migration rejected %s at InspectRow [%s, %s]: %s"),
                *GetPathNameSafe(&Graph), Row.Label, Row.Field, Reason);
            return false;
        };

        UEdGraphPin* Consumer = Array.FindPin(Row.Slot, EGPD_Input);
        UEdGraphPin* Output = SolePeer(Consumer);
        auto* Call = Output ? Cast<UK2Node_CallFunction>(Output->GetOwningNode()) : nullptr;
        UEdGraphPin* Source = FindRowSource(Graph, Row);
        if (!IsVisibleString(Consumer))
        {
            return Reject(TEXT("array slot is missing or is not a visible string input"));
        }
        if (!Call || !Graph.Nodes.Contains(Call))
        {
            return Reject(TEXT("array slot is not linked to a formatter call node"));
        }
        if (!IsVisibleString(Source))
        {
            return Reject(TEXT("expected row source is missing or is not a visible string pin"));
        }
        if (Output != Call->FindPin(TEXT("ReturnValue"), EGPD_Output) || SolePeer(Output) != Consumer)
        {
            return Reject(TEXT("row formatter output is not linked exclusively to its array slot"));
        }

        const bool bOriginal = State == EState::Original;
        const FName InputName = bOriginal ? TEXT("B") : TEXT("Value");
        UEdGraphPin* Input = Call->FindPin(InputName, EGPD_Input);
        if (!IsVisibleString(Input))
        {
            return Reject(TEXT("formatter value input is missing or is not a visible string pin"));
        }
        if (SolePeer(Input) != Source || SolePeer(Source) != Input)
        {
            return Reject(TEXT("expected row source is not linked to the formatter value input"));
        }

        const UEdGraphNode* OldNode = FindNode(Graph, Row.Append);
        if (!bOriginal)
        {
            if (OldNode)
            {
                return Reject(TEXT("installed row still contains its original append node"));
            }
            if (Call->GetClass() != UK2Node_CallFunction::StaticClass())
            {
                return Reject(TEXT("installed row formatter has an unexpected node class"));
            }
            if (!IsExactCall(Call, &NativeFunction))
            {
                return Reject(TEXT("installed row does not call the native graph formatter"));
            }
            if (!HasGraphFormatterPins(*Call, Row.Field))
            {
                return Reject(TEXT("installed row formatter field or pin layout did not match"));
            }
            return true;
        }

        const UEdGraphPin* Label = Call->FindPin(TEXT("A"), EGPD_Input);
        if (OldNode != Call)
        {
            return Reject(TEXT("original row append node does not match the array formatter"));
        }
        if (Call->GetClass() != UK2Node_CommutativeAssociativeBinaryOperator::StaticClass())
        {
            return Reject(TEXT("original row formatter has an unexpected node class"));
        }
        if (!IsExactCall(Call, UKismetStringLibrary::StaticClass()->FindFunctionByName(TEXT("Concat_StrStr"))))
        {
            return Reject(TEXT("original row formatter is not Concat_StrStr"));
        }
        if (!HasStaticStringOutput(*Call, 4))
        {
            return Reject(TEXT("original row formatter pin layout did not match"));
        }
        if (!IsVisibleString(Label) || !Label->LinkedTo.IsEmpty() || Label->DefaultValue != Row.Label)
        {
            return Reject(TEXT("original row label pin is missing, linked, or has unexpected text"));
        }

        FReplacement& Replacement = Replacements.AddDefaulted_GetRef();
        Replacement.Formatter = Call;
        Replacement.NativeFunction = &NativeFunction;
        Replacement.Output = Output;
        Replacement.Consumer = Consumer;
        Replacement.Inputs.Add({Source, InputName, TEXT("Value")});
        Replacement.Field = Row.Field;
        return true;
    }

    const UK2Node_MakeArray* FindPanel(const UEdGraph& Graph, const UK2Node_SwitchInteger& Switch,
        const UBlueprint* DrawBlueprint, bool bMotionMatching, int32 RowCount)
    {
        const auto* Draw = Cast<UK2Node_CallFunction>(FindNode(Graph,
            bMotionMatching ? TEXT("K2Node_CallFunction_34") : TEXT("K2Node_CallFunction_0")));
        const auto* Knot = Cast<UK2Node_Knot>(FindNode(Graph,
            bMotionMatching ? TEXT("K2Node_Knot_0") : TEXT("K2Node_Knot_1")));
        const auto* Array = Cast<UK2Node_MakeArray>(FindNode(Graph,
            bMotionMatching ? TEXT("K2Node_MakeArray_7") : TEXT("K2Node_MakeArray_6")));
        if (!IsExactHelpfulFunctionsCall(Draw, DrawBlueprint, TEXT("DebugDraw_StringArray"), DrawStringArrayGuid) ||
            !Knot || !Array || Array->Pins.Num() != RowCount + 1 ||
            !HasInputLink(Knot->GetInputPin(), Switch.FindPin(bMotionMatching ? TEXT("0") : TEXT("1"), EGPD_Output)) ||
            !HasInputLink(Draw->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input), Knot->GetOutputPin()))
        {
            return nullptr;
        }

        UEdGraphPin* Input = Draw->FindPin(TEXT("Strings"), EGPD_Input);
        UEdGraphPin* Output = Array->FindPin(TEXT("Array"), EGPD_Output);
        return HasInputLink(Input, Output) && SolePeer(Output) == Input ? Array : nullptr;
    }

    bool InspectPanels(UEdGraph& Graph, const FTarget& Target, EState State, TArray<FReplacement>& Replacements)
    {
        const auto Reject = [&Graph](const TCHAR* Reason)
        {
            LogInspectionRejection(&Graph, TEXT("InspectPanels"), Reason);
            return false;
        };

        const UClass* DrawLibrary = FindObject<UClass>(nullptr,
            TEXT("/Game/Blueprints/Data/BFL_HelpfulFunctions.BFL_HelpfulFunctions_C"));
        const UBlueprint* DrawBlueprint = DrawLibrary ? Cast<UBlueprint>(DrawLibrary->ClassGeneratedBy) : nullptr;
        if (!DrawLibrary || !DrawBlueprint)
        {
            return Reject(TEXT("BFL_HelpfulFunctions Blueprint class was not found"));
        }
        const auto* Switch = Cast<UK2Node_SwitchInteger>(FindNode(Graph, TEXT("K2Node_SwitchInteger_0")));
        const auto* DrawBools = Cast<UK2Node_CallFunction>(FindNode(Graph, TEXT("K2Node_CallFunction_1")));
        const UEdGraphNode* DrawStatus = FindNode(Graph, TEXT("K2Node_CallFunction_17"));
        if (!Switch)
        {
            return Reject(TEXT("integer panel switch node was not found"));
        }
        if (!DrawStatus)
        {
            return Reject(TEXT("DrawDebugString node was not found"));
        }
        if (!IsExactHelpfulFunctionsCall(DrawBools, DrawBlueprint, TEXT("DebugDraw_BoolStates"), DrawBoolStatesGuid))
        {
            return Reject(TEXT("boolean panel node does not call DebugDraw_BoolStates"));
        }
        if (!HasInputLink(DrawBools->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input),
            DrawStatus->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output)))
        {
            return Reject(TEXT("boolean panel execution is not linked after DrawDebugString"));
        }
        if (!HasInputLink(Switch->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input),
            DrawBools->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output)))
        {
            return Reject(TEXT("integer panel switch is not linked after the boolean panel"));
        }
        if (!HasInputLink(Switch->FindPin(TEXT("Selection"), EGPD_Input),
            FindVariableOutput(Graph, TEXT("K2Node_VariableGet_17"), TEXT("LocomotionSetup"))))
        {
            return Reject(TEXT("integer panel switch is not driven by LocomotionSetup"));
        }

        const UClass* NativeClass = FindObject<UClass>(nullptr,
            TEXT("/Script/GameAnimationSampleCpp.SampleMoverAnimationDiagnostics"));
        const UFunction* Native = NativeClass ? NativeClass->FindFunctionByName(GraphFormatterName) : nullptr;
        const TArrayView<const FRow> StateRows = Target.bIsCmc ? MakeArrayView(CmcStateRows) : MakeArrayView(MoverStateRows);
        const UK2Node_MakeArray* StatePanel = FindPanel(Graph, *Switch, DrawBlueprint, false, StateRows.Num());
        if (!StatePanel)
        {
            return Reject(TEXT("state panel draw call, branch, array size, or wiring did not match"));
        }
        const UK2Node_MakeArray* MotionPanel = FindPanel(Graph, *Switch, DrawBlueprint, true, 3);
        if (!MotionPanel)
        {
            return Reject(TEXT("motion-matching panel draw call, branch, array size, or wiring did not match"));
        }
        if (!Native)
        {
            return Reject(TEXT("native FormatGraphDiagnosticLine function was not found"));
        }
        if (!Native->HasAllFunctionFlags(FUNC_Static | FUNC_BlueprintPure))
        {
            return Reject(TEXT("native FormatGraphDiagnosticLine function is not static and Blueprint-pure"));
        }

        int32 NativeCalls = 0;
        for (const UEdGraphNode* Node : Graph.Nodes)
        {
            const auto* Call = Cast<UK2Node_CallFunction>(Node);
            if (Call && Call->FunctionReference.GetMemberName() == GraphFormatterName)
            {
                ++NativeCalls;
            }
        }
        const int32 ExpectedNativeCalls = State == EState::Installed
            ? StateRows.Num() + UE_ARRAY_COUNT(MotionMatchingRows) : 0;
        if (NativeCalls != ExpectedNativeCalls)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("Animation diagnostics migration rejected %s at InspectPanels: expected %d native row formatters, found %d"),
                *GetPathNameSafe(&Graph), ExpectedNativeCalls, NativeCalls);
            return false;
        }

        for (const FRow& Row : StateRows)
        {
            if (!InspectRow(Graph, *StatePanel, Row, *Native, State, Replacements))
            {
                return false;
            }
        }
        for (const FRow& Row : MotionMatchingRows)
        {
            if (!InspectRow(Graph, *MotionPanel, Row, *Native, State, Replacements))
            {
                return false;
            }
        }
        return true;
    }

    EState Inspect(UBlueprint* Blueprint, FStateLine& Line, TArray<FReplacement>& Replacements)
    {
        const FTarget* Target = nullptr;
        const EState State = InspectStatus(Blueprint, Line, Target);
        if (State == EState::Rejected || !InspectPanels(*Line.Graph, *Target, State, Replacements))
        {
            return EState::Rejected;
        }
        if (State == EState::Original)
        {
            FReplacement& Replacement = Replacements.AddDefaulted_GetRef();
            Replacement.Formatter = Line.Formatter;
            Replacement.NativeFunction = Line.NativeFunction;
            Replacement.Output = Line.Output;
            Replacement.Consumer = Line.Consumer;
            for (int32 Index = 0; Index < UE_ARRAY_COUNT(Inputs); ++Index)
            {
                Replacement.Inputs.Add({Line.Sources[Index], AppendInputs[Index], Inputs[Index]});
            }
        }
        return State;
    }

    bool PrepareReplacement(UEdGraph& Graph, FReplacement& Replacement)
    {
        FGraphNodeCreator<UK2Node_CallFunction> Creator(Graph);
        UK2Node_CallFunction* Node = Creator.CreateNode();
        Replacement.NewNode = Node;
        Node->SetFromFunction(Replacement.NativeFunction);
        Node->NodePosX = Replacement.Formatter->NodePosX;
        Node->NodePosY = Replacement.Formatter->NodePosY;
        Creator.Finalize();

        if (!IsExactCall(Node, Replacement.NativeFunction))
        {
            return false;
        }
        if (!Replacement.Field)
        {
            return HasFormatterPins(*Node, false);
        }
        UEdGraphPin* Field = Node->FindPin(TEXT("Field"), EGPD_Input);
        if (!Field)
        {
            return false;
        }
        Graph.GetSchema()->TrySetDefaultValue(*Field, Replacement.Field, false);
        return HasGraphFormatterPins(*Node, Replacement.Field);
    }

    void MoveLinks(const FReplacement& Replacement)
    {
        Replacement.Formatter->Modify();
        Replacement.Consumer->GetOwningNode()->Modify();
        for (const FPinMove& Move : Replacement.Inputs)
        {
            Move.Source->GetOwningNode()->Modify();
            Move.Source->BreakLinkTo(Replacement.Formatter->FindPin(Move.OldInput, EGPD_Input));
            Move.Source->MakeLinkTo(Replacement.NewNode->FindPin(Move.NewInput, EGPD_Input));
        }
        Replacement.Output->BreakLinkTo(Replacement.Consumer);
        Replacement.NewNode->FindPin(TEXT("ReturnValue"), EGPD_Output)->MakeLinkTo(Replacement.Consumer);
        Replacement.Formatter->DestroyNode();
    }
}

ESampleAnimationStateLineMigrationState USampleMoverAnimationDiagnosticsMigration::ClassifyStateLine(
    UBlueprint* Blueprint)
{
    FStateLine Line;
    TArray<FReplacement> Replacements;
    return Inspect(Blueprint, Line, Replacements);
}

bool USampleMoverAnimationDiagnosticsMigration::ReplaceStateLine(UBlueprint* Blueprint)
{
    FStateLine Line;
    TArray<FReplacement> Replacements;
    if (Inspect(Blueprint, Line, Replacements) != EState::Original)
    {
        return false;
    }

    Blueprint->Modify();
    Line.Graph->Modify();
    for (FReplacement& Replacement : Replacements)
    {
        if (!PrepareReplacement(*Line.Graph, Replacement))
        {
            for (const FReplacement& Prepared : Replacements)
            {
                if (Prepared.NewNode)
                {
                    Prepared.NewNode->DestroyNode();
                }
            }
            return false;
        }
    }

    // Move only validated scalar string links after every replacement has its expected pins.
    for (const FReplacement& Replacement : Replacements)
    {
        MoveLinks(Replacement);
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return ClassifyStateLine(Blueprint) == EState::Installed;
}

bool USampleMoverAnimationDiagnosticsMigration::CompileAndVerifyStateLine(UBlueprint* Blueprint)
{
    if (ClassifyStateLine(Blueprint) != EState::Installed)
    {
        return false;
    }

    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave);
    return Blueprint->IsUpToDate() && ClassifyStateLine(Blueprint) == EState::Installed;
}
