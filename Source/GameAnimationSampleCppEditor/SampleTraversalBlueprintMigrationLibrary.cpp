#include "SampleTraversalBlueprintMigrationLibrary.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/SecureHash.h"
#include <initializer_list>

namespace
{
    constexpr TCHAR AssetPath[] = TEXT("/Game/Blueprints/AC_TraversalLogic.AC_TraversalLogic");
    constexpr TCHAR ParentPath[] = TEXT("/Script/GameAnimationSampleCpp.SampleTraversalComponent");

    // Connected-pin evidence from the 2026-11-27 native traversal graph inventory.
    // Each digest covers every recorded node name, direction, pin name and linked node name.
    struct FGraphEvidence
    {
        const TCHAR* Name;
        int32 Count;
        const TCHAR* Digest;
    };
    constexpr FGraphEvidence Evidence[] = {
        {TEXT("TryTraversalAction"), 92, TEXT("ACBBB459351B6FABEC9E1C9B403C284EA3400CE6")},
        {TEXT("SetWarpTargets"), 70, TEXT("B9CEE7DCE7472E8ECEF77E4D0FA24A10C3CD798C")},
        {TEXT("EventGraph"), 49, TEXT("7E7ED29D5575A1798CFD43F21614A592BF77FBB0")}
    };

    void SortStrings(TArray<FString>& Values)
    {
        Values.Sort([](const FString& A, const FString& B)
        {
            return A.Compare(B, ESearchCase::CaseSensitive) < 0;
        });
    }

    FString Digest(TArray<FString> Lines)
    {
        SortStrings(Lines);
        const FString Text = FString::Join(Lines, TEXT("\n"));
        const FTCHARToUTF8 Bytes(*Text);
        FSHAHash Hash;
        FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), Hash.Hash);
        return Hash.ToString().ToUpper();
    }

    FString GraphDigest(const UEdGraph& Graph, bool bIncludeDefaults, int32* OutNodeCount = nullptr)
    {
        TArray<FString> Lines;
        int32 NodeCount = 0;
        for (const UEdGraphNode* Node : Graph.Nodes)
        {
            if (!Node)
            {
                return {};
            }
            // The canonical node inventory excludes comments. Retain them in the full signature.
            if (!bIncludeDefaults && Node->IsA<UEdGraphNode_Comment>())
            {
                if (!Node->Pins.IsEmpty())
                {
                    return {};
                }
                continue;
            }
            ++NodeCount;
            const FString Name = Node->GetName();
            Lines.Add(Name);
            if (bIncludeDefaults)
            {
                Lines.Add(Name + TEXT("|class|") + Node->GetClass()->GetPathName());
                if (const auto* Call = Cast<UK2Node_CallFunction>(Node))
                {
                    Lines.Add(Name + TEXT("|function|") + Call->FunctionReference.GetMemberName().ToString() +
                        TEXT("|") + GetPathNameSafe(Call->GetTargetFunction()));
                }
                if (const auto* Variable = Cast<UK2Node_Variable>(Node))
                {
                    const FMemberReference& Reference = Variable->VariableReference;
                    Lines.Add(Name + TEXT("|variable|") + Reference.GetMemberName().ToString() + TEXT("|") +
                        Reference.GetMemberScopeName() + TEXT("|") + Reference.GetMemberGuid().ToString());
                }
                if (const auto* Entry = Cast<UK2Node_FunctionEntry>(Node))
                {
                    for (const FBPVariableDescription& Local : Entry->LocalVariables)
                    {
                        Lines.Add(Name + TEXT("|local|") + Local.VarName.ToString() + TEXT("|") +
                            Local.VarType.PinCategory.ToString() + TEXT("|") + Local.DefaultValue);
                    }
                }
            }
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin)
                {
                    return {};
                }
                const FString Prefix = Name + (Pin->Direction == EGPD_Input ? TEXT("|in|") : TEXT("|out|")) +
                    Pin->PinName.ToString() + TEXT("|");
                TArray<FString> Peers;
                for (const UEdGraphPin* Peer : Pin->LinkedTo)
                {
                    if (!Peer || !Peer->GetOwningNode() || Peer->GetOwningNode()->GetGraph() != &Graph ||
                        Peer->Direction == Pin->Direction || !Peer->LinkedTo.Contains(Pin))
                    {
                        return {};
                    }
                    FString PeerName = Peer->GetOwningNode()->GetName();
                    if (bIncludeDefaults)
                    {
                        PeerName += TEXT(":") + Peer->PinName.ToString();
                    }
                    Peers.Add(MoveTemp(PeerName));
                }
                if (!Peers.IsEmpty())
                {
                    SortStrings(Peers);
                    Lines.Add(Prefix + FString::Join(Peers, TEXT(",")));
                }
                if (bIncludeDefaults)
                {
                    Lines.Add(Prefix + TEXT("default|") + Pin->PinType.PinCategory.ToString() + TEXT("|") +
                        Pin->PinType.PinSubCategory.ToString() + TEXT("|") +
                        GetPathNameSafe(Pin->PinType.PinSubCategoryObject.Get()) + TEXT("|") +
                        Pin->DefaultValue + TEXT("|") + GetPathNameSafe(Pin->DefaultObject) +
                        TEXT("|") + Pin->DefaultTextValue.ToString() + TEXT("|") +
                        FString::FromInt(static_cast<int32>(Pin->PinType.ContainerType)) + TEXT("|") +
                        FString::FromInt(Pin->PinType.bIsReference) + TEXT("|") +
                        FString::FromInt(Pin->PinType.bIsConst) + TEXT("|") +
                        FString::FromInt(Pin->bOrphanedPin));
                }
            }
        }
        if (OutNodeCount)
        {
            *OutNodeCount = NodeCount;
        }
        return Digest(MoveTemp(Lines));
    }

    bool IsTarget(const UBlueprint* Blueprint)
    {
        return Blueprint && Blueprint->GetPathName() == AssetPath && Blueprint->ParentClass &&
            Blueprint->ParentClass->GetPathName() == ParentPath;
    }

    UEdGraph* FindGraph(UBlueprint& Blueprint, const TCHAR* Name)
    {
        TArray<UEdGraph*> Graphs;
        Blueprint.GetAllGraphs(Graphs);
        UEdGraph* Result = nullptr;
        for (UEdGraph* Graph : Graphs)
        {
            if (Graph && Graph->GetOuter() == &Blueprint && Graph->GetName() == Name)
            {
                if (Result)
                {
                    return nullptr;
                }
                Result = Graph;
            }
        }
        return Result;
    }

    UEdGraphNode* FindNode(UEdGraph& Graph, const TCHAR* Name)
    {
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            if (Node && Node->GetName() == Name)
            {
                return Node;
            }
        }
        return nullptr;
    }

    UK2Node_CallFunction* FindCall(UEdGraph& Graph, const TCHAR* Name, const TCHAR* Function)
    {
        auto* Call = Cast<UK2Node_CallFunction>(FindNode(Graph, Name));
        return Call && Call->FunctionReference.GetMemberName() == FName(Function) ? Call : nullptr;
    }

    UEdGraphPin* In(UEdGraphNode* Node, const TCHAR* Name)
    {
        return Node ? Node->FindPin(FName(Name), EGPD_Input) : nullptr;
    }

    UEdGraphPin* Out(UEdGraphNode* Node, const TCHAR* Name)
    {
        return Node ? Node->FindPin(FName(Name), EGPD_Output) : nullptr;
    }

    UEdGraphPin* Exec(UEdGraphNode* Node) { return In(Node, TEXT("execute")); }
    UEdGraphPin* Then(UEdGraphNode* Node) { return Out(Node, TEXT("then")); }

    UEdGraphPin* OnlyPeer(const UEdGraphPin* Pin)
    {
        return Pin && Pin->LinkedTo.Num() == 1 ? Pin->LinkedTo[0] : nullptr;
    }

    bool HasOnlyConnection(const UEdGraphPin* Pin, const UEdGraphPin* Peer)
    {
        return Pin && Peer && OnlyPeer(Pin) == Peer && Peer->LinkedTo.Contains(Pin);
    }

    bool Literal(const UEdGraphPin* Pin, const TCHAR* Value)
    {
        return Pin && Pin->LinkedTo.IsEmpty() && Pin->DefaultValue == Value;
    }

    bool BooleanLiteral(const UEdGraphPin* Pin)
    {
        return Literal(Pin, TEXT("true")) || Literal(Pin, TEXT("false"));
    }

    bool Link(UEdGraph& Graph, UEdGraphPin* A, UEdGraphPin* B)
    {
        if (!A || !B || A->GetOwningNode()->GetGraph() != &Graph || B->GetOwningNode()->GetGraph() != &Graph)
        {
            return false;
        }
        if (A->LinkedTo.Contains(B))
        {
            return B->LinkedTo.Contains(A);
        }
        // A failed schema connection leaves the migration unsaved. Never silently replace another input.
        const FPinConnectionResponse Response = Graph.GetSchema()->CanCreateConnection(A, B);
        if (Response.Response != CONNECT_RESPONSE_MAKE)
        {
            return false;
        }
        return Graph.GetSchema()->TryCreateConnection(A, B) && A->LinkedTo.Contains(B) && B->LinkedTo.Contains(A);
    }

    bool Redirect(UEdGraph& Graph, UEdGraphPin* Old, UEdGraphPin* New)
    {
        if (!Old || !New || Old->Direction != New->Direction)
        {
            return false;
        }
        const TArray<UEdGraphPin*> Peers(Old->LinkedTo);
        Old->BreakAllPinLinks();
        for (UEdGraphPin* Peer : Peers)
        {
            if (!Link(Graph, New, Peer))
            {
                return false;
            }
        }
        return true;
    }

    bool CopyInput(UEdGraph& Graph, UEdGraphPin* Old, UEdGraphPin* New)
    {
        if (!Old || !New || Old->Direction != EGPD_Input || New->Direction != EGPD_Input ||
            !New->LinkedTo.IsEmpty() || Old->LinkedTo.Num() > 1 || !Old->SubPins.IsEmpty() ||
            !New->SubPins.IsEmpty() || Old->bOrphanedPin || New->bOrphanedPin)
        {
            return false;
        }
        if (UEdGraphPin* Peer = OnlyPeer(Old))
        {
            return Link(Graph, Peer, New);
        }
        if (Old->DefaultObject || !Old->DefaultTextValue.IsEmpty())
        {
            return false;
        }
        Graph.GetSchema()->TrySetDefaultValue(*New, Old->DefaultValue);
        return New->DefaultValue == Old->DefaultValue;
    }

    bool ReplaceInput(UEdGraph& Graph, UEdGraphPin* Input, UEdGraphPin* NewSource)
    {
        if (!Input || !NewSource || Input->LinkedTo.Num() != 1)
        {
            return false;
        }
        Input->BreakAllPinLinks();
        return Link(Graph, NewSource, Input);
    }

    const UFunction* Native(UBlueprint& Blueprint, const TCHAR* Name)
    {
        const UFunction* Function = Blueprint.ParentClass->FindFunctionByName(FName(Name));
        return Function && Function->HasAllFunctionFlags(FUNC_Native | FUNC_BlueprintCallable) ? Function : nullptr;
    }

    UK2Node_CallFunction* AddCall(UEdGraph& Graph, const UFunction* Function, const UEdGraphNode& At, int32 Dx = 0)
    {
        if (!Function)
        {
            return nullptr;
        }
        FGraphNodeCreator<UK2Node_CallFunction> Creator(Graph);
        auto* Call = Creator.CreateNode();
        Call->SetFromFunction(Function);
        Call->NodePosX = At.NodePosX + Dx;
        Call->NodePosY = At.NodePosY;
        Creator.Finalize();
        return Call;
    }

    void Remove(UBlueprint& Blueprint, std::initializer_list<UEdGraphNode*> Nodes)
    {
        for (UEdGraphNode* Node : Nodes)
        {
            FBlueprintEditorUtils::RemoveNode(&Blueprint, Node, true);
        }
    }

    bool ReturnFlags(UEdGraphNode* Node, bool* bGeometry = nullptr, bool* bMontage = nullptr)
    {
        auto* Return = Cast<UK2Node_FunctionResult>(Node);
        const UEdGraphPin* Geometry = In(Return, TEXT("TraversalCheckFailed"));
        const UEdGraphPin* Montage = In(Return, TEXT("MontageSelectionFailed"));
        if (!BooleanLiteral(Geometry) || !BooleanLiteral(Montage))
        {
            return false;
        }
        if (bGeometry) { *bGeometry = Literal(Geometry, TEXT("true")); }
        if (bMontage) { *bMontage = Literal(Montage, TEXT("true")); }
        return true;
    }

    bool CheckSelectionReturns(UEdGraph& Graph)
    {
        bool bGeometry = false;
        bool bMontage = false;
        return ReturnFlags(FindNode(Graph, TEXT("K2Node_FunctionResult_1")), &bGeometry) && bGeometry &&
            ReturnFlags(FindNode(Graph, TEXT("K2Node_FunctionResult_2"))) &&
            ReturnFlags(FindNode(Graph, TEXT("K2Node_FunctionResult_5")), nullptr, &bMontage) && bMontage &&
            ReturnFlags(FindNode(Graph, TEXT("K2Node_FunctionResult_4")), &bGeometry, &bMontage) &&
            !bGeometry && !bMontage;
    }

    bool CheckWarpLiterals(UEdGraph& Graph)
    {
        struct FTarget { const TCHAR* Node; const TCHAR* Function; const TCHAR* Name; };
        const FTarget Targets[] = {
            {TEXT("K2Node_CallFunction_1"), TEXT("AddOrUpdateWarpTargetFromLocationAndRotation"), TEXT("FrontLedge")},
            {TEXT("K2Node_CallFunction_6"), TEXT("AddOrUpdateWarpTargetFromLocationAndRotation"), TEXT("BackLedge")},
            {TEXT("K2Node_CallFunction_5"), TEXT("AddOrUpdateWarpTargetFromLocationAndRotation"), TEXT("BackFloor")},
            {TEXT("K2Node_CallFunction_2"), TEXT("RemoveWarpTarget"), TEXT("BackLedge")},
            {TEXT("K2Node_CallFunction_3"), TEXT("RemoveWarpTarget"), TEXT("BackLedge")},
            {TEXT("K2Node_CallFunction_7"), TEXT("RemoveWarpTarget"), TEXT("BackFloor")},
            {TEXT("K2Node_CallFunction_20"), TEXT("RemoveWarpTarget"), TEXT("BackFloor")},
            {TEXT("K2Node_CallFunction_21"), TEXT("GetMotionWarpingWindowsForWarpTargetFromAnimation"), TEXT("BackLedge")},
            {TEXT("K2Node_CallFunction_10"), TEXT("GetMotionWarpingWindowsForWarpTargetFromAnimation"), TEXT("BackFloor")}
        };
        for (const FTarget& Target : Targets)
        {
            if (!Literal(In(FindCall(Graph, Target.Node, Target.Function), TEXT("WarpTargetName")), Target.Name))
            {
                return false;
            }
        }
        for (const TCHAR* Name : {TEXT("K2Node_GetArrayItem_2"), TEXT("K2Node_GetArrayItem_0")})
        {
            const auto* Index = In(FindNode(Graph, Name), TEXT("Dimension 1"));
            if (!Index || Index->PinType.PinCategory != UEdGraphSchema_K2::PC_Int ||
                !Index->LinkedTo.IsEmpty() || !Index->SubPins.IsEmpty() || Index->DefaultValue != TEXT("0"))
            {
                return false;
            }
        }
        const auto* Offset = In(FindNode(Graph, TEXT("K2Node_PromotableOperator_0")), TEXT("B"));
        if (!Offset || !Offset->LinkedTo.IsEmpty() || !Offset->SubPins.IsEmpty() ||
            Offset->DefaultValue.IsEmpty() || Offset->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct ||
            Offset->PinType.PinSubCategoryObject.Get() != TBaseStructure<FVector>::Get())
        {
            return false;
        }

        // The census records peer node names, not peer pin names. Confirm the
        // axis-specific links replaced by the native floor calculation explicitly.
        auto* FloorMath = FindNode(Graph, TEXT("K2Node_PromotableOperator_5"));
        auto* MakeVector = FindCall(Graph, TEXT("K2Node_CallFunction_17"), TEXT("MakeVector"));
        auto* BreakVector = FindCall(Graph, TEXT("K2Node_CallFunction_18"), TEXT("BreakVector"));
        return HasOnlyConnection(In(MakeVector, TEXT("X")), Out(FloorMath, TEXT("ReturnValue_X"))) &&
            HasOnlyConnection(In(MakeVector, TEXT("Y")), Out(FloorMath, TEXT("ReturnValue_Y"))) &&
            HasOnlyConnection(In(MakeVector, TEXT("Z")), Out(BreakVector, TEXT("Z")));
    }

    bool CheckSource(UBlueprint& Blueprint)
    {
        for (const FGraphEvidence& Item : Evidence)
        {
            UEdGraph* Graph = FindGraph(Blueprint, Item.Name);
            int32 NodeCount = 0;
            if (!Graph || GraphDigest(*Graph, false, &NodeCount) != Item.Digest || NodeCount != Item.Count)
            {
                UE_LOG(LogTemp, Error, TEXT("Traversal graph does not match the recorded topology: %s"), Item.Name);
                return false;
            }
        }
        for (const TCHAR* Function : {TEXT("ReadTraversalDebugSettings"), TEXT("PrintTraversalDiagnostic"),
            TEXT("TryPublishTraversalSelection"), TEXT("UpdateTraversalWarpTargets"),
            TEXT("BeginTraversalGameplay"), TEXT("FinishTraversalGameplay")})
        {
            if (!Native(Blueprint, Function))
            {
                return false;
            }
        }
        UEdGraph& Try = *FindGraph(Blueprint, TEXT("TryTraversalAction"));
        auto* DebugLevel = FindCall(Try, TEXT("K2Node_CallFunction_1"), TEXT("GetConsoleVariableIntValue"));
        auto* DebugDuration = FindCall(Try, TEXT("K2Node_CallFunction_2"), TEXT("GetConsoleVariableFloatValue"));
        return Literal(In(DebugLevel, TEXT("VariableName")), TEXT("DDCvar.Traversal.DrawDebugLevel")) &&
            Literal(In(DebugDuration, TEXT("VariableName")), TEXT("DDCvar.Traversal.DrawDebugDuration")) &&
            CheckSelectionReturns(Try) && CheckWarpLiterals(*FindGraph(Blueprint, TEXT("SetWarpTargets")));
    }

    bool RewireSelection(UBlueprint& Blueprint, UEdGraph& Graph)
    {
        auto* OldLevel = FindCall(Graph, TEXT("K2Node_CallFunction_1"), TEXT("GetConsoleVariableIntValue"));
        auto* OldDuration = FindCall(Graph, TEXT("K2Node_CallFunction_2"), TEXT("GetConsoleVariableFloatValue"));
        auto* LevelSet = FindNode(Graph, TEXT("K2Node_VariableSet_0"));
        auto* DurationSet = FindNode(Graph, TEXT("K2Node_VariableSet_1"));
        auto* OldPrintGate = Cast<UK2Node_IfThenElse>(FindNode(Graph, TEXT("K2Node_IfThenElse_1")));
        auto* Threshold = FindNode(Graph, TEXT("K2Node_PromotableOperator_4"));
        auto* PrintA = FindCall(Graph, TEXT("K2Node_CallFunction_80"), TEXT("PrintString"));
        auto* PrintB = FindCall(Graph, TEXT("K2Node_CallFunction_23"), TEXT("PrintString"));
        auto* ActionGate = Cast<UK2Node_IfThenElse>(FindNode(Graph, TEXT("K2Node_IfThenElse_0")));
        auto* OldPublish = FindCall(Graph, TEXT("K2Node_CallFunction_0"), TEXT("PublishTraversalAction"));
        auto* Failure = FindNode(Graph, TEXT("K2Node_FunctionResult_5"));
        if (!OldLevel || !OldDuration || !LevelSet || !DurationSet || !OldPrintGate || !Threshold ||
            !PrintA || !PrintB || !ActionGate || !OldPublish || !Failure ||
            !OldPrintGate->GetElsePin() || !Then(PrintB) ||
            !OldPrintGate->GetElsePin()->LinkedTo.IsEmpty() || !Then(PrintB)->LinkedTo.IsEmpty() ||
            !Exec(OldDuration) || OnlyPeer(Exec(OldDuration)) != Then(LevelSet))
        {
            return false;
        }

        auto* Debug = AddCall(Graph, Native(Blueprint, TEXT("ReadTraversalDebugSettings")), *OldLevel);
        if (!Debug || !Redirect(Graph, Exec(OldLevel), Exec(Debug)) ||
            !ReplaceInput(Graph, Exec(LevelSet), Then(Debug)) ||
            !ReplaceInput(Graph, In(LevelSet, TEXT("DrawDebugLevel")), Out(Debug, TEXT("Level"))) ||
            !ReplaceInput(Graph, In(DurationSet, TEXT("DrawDebugDuration")), Out(Debug, TEXT("Duration"))))
        {
            return false;
        }
        // The schema permits only one outgoing execution link. Release the old
        // duration read's incoming edge before connecting the retained setter.
        Exec(OldDuration)->BreakAllPinLinks();
        if (!ReplaceInput(Graph, Exec(DurationSet), Then(LevelSet)))
        {
            return false;
        }
        Remove(Blueprint, {OldLevel, OldDuration});

        UK2Node_CallFunction* Diagnostics[2] = {};
        UK2Node_CallFunction* OldPrints[] = {PrintA, PrintB};
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto* Old = OldPrints[Index];
            auto* New = AddCall(Graph, Native(Blueprint, TEXT("PrintTraversalDiagnostic")), *Old);
            if (!New || !CopyInput(Graph, In(Threshold, TEXT("A")), In(New, TEXT("Level"))) ||
                !CopyInput(Graph, In(Threshold, TEXT("B")), In(New, TEXT("Threshold"))))
            {
                return false;
            }
            const TPair<const TCHAR*, const TCHAR*> Inputs[] = {
                {TEXT("InString"), TEXT("Message")}, {TEXT("bPrintToScreen"), TEXT("bPrintToScreen")},
                {TEXT("bPrintToLog"), TEXT("bPrintToLog")}, {TEXT("TextColor"), TEXT("Color")},
                {TEXT("Duration"), TEXT("Duration")}, {TEXT("Key"), TEXT("Key")}
            };
            for (const auto& Pair : Inputs)
            {
                if (!CopyInput(Graph, In(Old, Pair.Key), In(New, Pair.Value)))
                {
                    return false;
                }
            }
            Diagnostics[Index] = New;
        }
        if (!Redirect(Graph, Exec(OldPrintGate), Exec(Diagnostics[0])) ||
            !Link(Graph, Then(Diagnostics[0]), Exec(Diagnostics[1])))
        {
            return false;
        }
        Remove(Blueprint, {OldPrintGate, Threshold, PrintA, PrintB});

        auto* Publish = AddCall(Graph, Native(Blueprint, TEXT("TryPublishTraversalSelection")), *OldPublish);
        FGraphNodeCreator<UK2Node_IfThenElse> BranchCreator(Graph);
        auto* Branch = BranchCreator.CreateNode();
        Branch->NodePosX = OldPublish->NodePosX + 300;
        Branch->NodePosY = OldPublish->NodePosY;
        BranchCreator.Finalize();
        if (!Publish || !CopyInput(Graph, In(ActionGate, TEXT("Condition")), In(Publish, TEXT("bActionAllowed"))) ||
            !CopyInput(Graph, In(OldPublish, TEXT("TraversalResult")), In(Publish, TEXT("TraversalResult"))) ||
            !Redirect(Graph, Exec(OldPublish), Exec(Publish)) ||
            !Link(Graph, Then(Publish), Exec(Branch)) ||
            !Link(Graph, Out(Publish, TEXT("ReturnValue")), In(Branch, TEXT("Condition"))) ||
            !Redirect(Graph, Then(OldPublish), Then(Branch)) ||
            !Link(Graph, Branch->GetElsePin(), Exec(Failure)))
        {
            return false;
        }
        Remove(Blueprint, {OldPublish});
        return true;
    }

    // Function-local flags are explicitly reset before either optional window is evaluated.
    const TCHAR* WarpFlags[] = {TEXT("TraversalUseBackLedge"), TEXT("TraversalLedgeSampleValid"),
        TEXT("TraversalUseBackFloor"), TEXT("TraversalFloorSampleValid")};

    template <typename T>
    T* AddLocalNode(UBlueprint& Blueprint, UEdGraph& Graph, FName Name, const UEdGraphNode& At, int32 Dx)
    {
        const FGuid Guid = FBlueprintEditorUtils::FindLocalVariableGuidByName(&Blueprint, &Graph, Name);
        if (!Guid.IsValid())
        {
            return nullptr;
        }
        FGraphNodeCreator<T> Creator(Graph);
        T* Node = Creator.CreateNode();
        Node->VariableReference.SetLocalMember(Name, Graph.GetName(), Guid);
        Node->NodePosX = At.NodePosX + Dx;
        Node->NodePosY = At.NodePosY;
        Creator.Finalize();
        return Node;
    }

    UK2Node_VariableSet* AddFlagSet(UBlueprint& Blueprint, UEdGraph& Graph, const TCHAR* Name,
        const UEdGraphNode& At, int32 Dx, bool bValue)
    {
        auto* Set = AddLocalNode<UK2Node_VariableSet>(Blueprint, Graph, FName(Name), At, Dx);
        UEdGraphPin* Value = In(Set, Name);
        if (!Value || Value->PinType.PinCategory != UEdGraphSchema_K2::PC_Boolean)
        {
            return nullptr;
        }
        Graph.GetSchema()->TrySetDefaultValue(*Value, bValue ? TEXT("true") : TEXT("false"));
        return Literal(Value, bValue ? TEXT("true") : TEXT("false")) ? Set : nullptr;
    }

    bool RewireWarp(UBlueprint& Blueprint, UEdGraph& Graph)
    {
        FEdGraphPinType BoolType;
        BoolType.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        for (const TCHAR* Name : WarpFlags)
        {
            if (FBlueprintEditorUtils::FindLocalVariableGuidByName(&Blueprint, &Graph, FName(Name)).IsValid() ||
                !FBlueprintEditorUtils::AddLocalVariable(&Blueprint, &Graph, FName(Name), BoolType, TEXT("false")))
            {
                return false;
            }
        }

        auto* Front = FindCall(Graph, TEXT("K2Node_CallFunction_1"), TEXT("AddOrUpdateWarpTargetFromLocationAndRotation"));
        auto* Ledge = FindCall(Graph, TEXT("K2Node_CallFunction_6"), TEXT("AddOrUpdateWarpTargetFromLocationAndRotation"));
        auto* Floor = FindCall(Graph, TEXT("K2Node_CallFunction_5"), TEXT("AddOrUpdateWarpTargetFromLocationAndRotation"));
        auto* LedgeCurve = FindCall(Graph, TEXT("K2Node_CallFunction_22"), TEXT("GetCurveValueFromAnimation"));
        auto* FloorCurve = FindCall(Graph, TEXT("K2Node_CallFunction_11"), TEXT("GetCurveValueFromAnimation"));
        auto* Offset = FindNode(Graph, TEXT("K2Node_PromotableOperator_0"));
        auto* Negate = FindNode(Graph, TEXT("K2Node_CallFunction_32"));
        auto* FloorMath = FindNode(Graph, TEXT("K2Node_PromotableOperator_5"));
        auto* NormalMath = FindNode(Graph, TEXT("K2Node_PromotableOperator_2"));
        auto* FloorZ = FindNode(Graph, TEXT("K2Node_CallFunction_18"));
        auto* Sequence = Cast<UK2Node_ExecutionSequence>(FindNode(Graph, TEXT("K2Node_ExecutionSequence_0")));
        if (!Front || !Ledge || !Floor || !LedgeCurve || !FloorCurve || !Offset || !Negate ||
            !FloorMath || !NormalMath || !FloorZ || !Sequence || Sequence->GetThenPinGivenIndex(2))
        {
            return false;
        }

        auto* Update = AddCall(Graph, Native(Blueprint, TEXT("UpdateTraversalWarpTargets")), *Floor, 800);
        if (!Update)
        {
            return false;
        }
        const TPair<UEdGraphPin*, const TCHAR*> Inputs[] = {
            {In(Front, TEXT("self")), TEXT("MotionWarping")},
            {In(Offset, TEXT("A")), TEXT("FrontLedge")}, {In(Negate, TEXT("A")), TEXT("FrontNormal")},
            {In(Offset, TEXT("B")), TEXT("FrontOffset")},
            {In(Ledge, TEXT("TargetLocation")), TEXT("BackLedge")},
            {In(Ledge, TEXT("TargetRotation")), TEXT("BackLedgeRotation")},
            {In(NormalMath, TEXT("A")), TEXT("BackNormal")}, {In(FloorZ, TEXT("InVec")), TEXT("BackFloor")},
            {In(Floor, TEXT("TargetRotation")), TEXT("BackFloorRotation")}
        };
        for (const auto& Pair : Inputs)
        {
            if (!CopyInput(Graph, Pair.Key, In(Update, Pair.Value)))
            {
                return false;
            }
        }
        // The two existing authored distance assignments remain available to other consumers.
        auto* LedgeDistance = FindNode(Graph, TEXT("K2Node_VariableGet_17"));
        auto* FloorDistance = FindNode(Graph, TEXT("K2Node_VariableGet_18"));
        if (!Link(Graph, Out(LedgeDistance, TEXT("AnimatedDistanceFromFrontLedgeToBackLedge")),
                In(Update, TEXT("AnimatedLedgeDistance"))) ||
            !Link(Graph, Out(FloorDistance, TEXT("AnimatedDistanceFromFrontLedgeToBackFloor")),
                In(Update, TEXT("AnimatedFloorDistance"))))
        {
            return false;
        }

        UK2Node_VariableGet* Gets[4] = {};
        UEdGraphPin* ResetTail = nullptr;
        for (int32 Index = 0; Index < 4; ++Index)
        {
            const TCHAR* Name = WarpFlags[Index];
            auto* Reset = AddFlagSet(Blueprint, Graph, Name, *Front, Index * 200, false);
            Gets[Index] = AddLocalNode<UK2Node_VariableGet>(Blueprint, Graph, FName(Name), *Update, -300 - Index * 100);
            if (!Reset || !Gets[Index] ||
                (Index == 0 ? !Redirect(Graph, Exec(Front), Exec(Reset)) : !Link(Graph, ResetTail, Exec(Reset))))
            {
                return false;
            }
            ResetTail = Then(Reset);
        }
        if (!ReplaceInput(Graph, Exec(Sequence), ResetTail))
        {
            return false;
        }

        UK2Node_CallFunction* OldTargets[] = {Ledge, Floor};
        UK2Node_CallFunction* Curves[] = {LedgeCurve, FloorCurve};
        for (int32 Index = 0; Index < 2; ++Index)
        {
            auto* Use = AddFlagSet(Blueprint, Graph, WarpFlags[Index * 2], *OldTargets[Index], 0, true);
            auto* Valid = AddFlagSet(Blueprint, Graph, WarpFlags[Index * 2 + 1], *OldTargets[Index], 220, false);
            if (!Use || !Valid || !Redirect(Graph, Exec(OldTargets[Index]), Exec(Use)) ||
                !Link(Graph, Then(Use), Exec(Valid)) ||
                !Link(Graph, Out(Curves[Index], TEXT("ReturnValue")), In(Valid, WarpFlags[Index * 2 + 1])))
            {
                return false;
            }
        }

        const UFunction* AndFunction = UKismetMathLibrary::StaticClass()->FindFunctionByName(TEXT("BooleanAND"));
        auto* BothSamples = AddCall(Graph, AndFunction, *Update, -150);
        if (!BothSamples ||
            !Link(Graph, Out(Gets[0], WarpFlags[0]), In(Update, TEXT("bUseBackLedge"))) ||
            !Link(Graph, Out(Gets[2], WarpFlags[2]), In(Update, TEXT("bUseBackFloor"))) ||
            !Link(Graph, Out(Gets[1], WarpFlags[1]), In(BothSamples, TEXT("A"))) ||
            !Link(Graph, Out(Gets[3], WarpFlags[3]), In(BothSamples, TEXT("B"))) ||
            !Link(Graph, Out(BothSamples, TEXT("ReturnValue")), In(Update, TEXT("bHasDistanceSamples"))))
        {
            return false;
        }
        Sequence->AddInputPin();
        if (!Link(Graph, Sequence->GetThenPinGivenIndex(2), Exec(Update)))
        {
            return false;
        }

        // The reference starts its montage before this function and has no warp failure return.
        // Native cleanup handles invalid targets; no new playback-cancellation policy is introduced here.
        Remove(Blueprint, {Front, Ledge, Floor,
            FindNode(Graph, TEXT("K2Node_CallFunction_2")), FindNode(Graph, TEXT("K2Node_CallFunction_3")),
            FindNode(Graph, TEXT("K2Node_CallFunction_7")), FindNode(Graph, TEXT("K2Node_CallFunction_20")),
            Offset, Negate, FindNode(Graph, TEXT("K2Node_CallFunction_34")), FloorMath, NormalMath,
            FindNode(Graph, TEXT("K2Node_PromotableOperator_3")), FindNode(Graph, TEXT("K2Node_CallFunction_12")),
            FindNode(Graph, TEXT("K2Node_CallFunction_17")), FloorZ,
            FindNode(Graph, TEXT("K2Node_VariableGet_16")), FindNode(Graph, TEXT("K2Node_VariableGet_22")),
            FindNode(Graph, TEXT("K2Node_VariableGet_23")), FindNode(Graph, TEXT("K2Node_VariableGet_24")),
            FindNode(Graph, TEXT("K2Node_VariableGet_25")), FindNode(Graph, TEXT("K2Node_VariableGet_26"))});
        return true;
    }

    struct FPlaybackEvidence
    {
        const TCHAR* Montage;
        const TCHAR* StartActive;
        const TCHAR* Warp;
        const TCHAR* BeginCollision;
        const TCHAR* BeginMode;
        const TCHAR* FinishActive;
        const TCHAR* EndCollision;
        const TCHAR* FinishMode;
        bool bMover;
    };
    constexpr FPlaybackEvidence Playback[] = {
        {TEXT("K2Node_PlayMontage_0"), TEXT("K2Node_CallFunction_32"), TEXT("K2Node_CallFunction_16"),
            TEXT("K2Node_CallFunction_28"), TEXT("K2Node_CallFunction_22"), TEXT("K2Node_CallFunction_31"),
            TEXT("K2Node_CallFunction_27"), TEXT("K2Node_CallFunction_12"), false},
        {TEXT("K2Node_PlayMontageOnMoverActor_0"), TEXT("K2Node_CallFunction_30"), TEXT("K2Node_CallFunction_19"),
            TEXT("K2Node_CallFunction_24"), TEXT("K2Node_CallFunction_20"), TEXT("K2Node_CallFunction_29"),
            TEXT("K2Node_CallFunction_15"), TEXT("K2Node_CallFunction_21"), true}
    };

    bool RewirePlayback(UBlueprint& Blueprint, UEdGraph& Graph)
    {
        for (const FPlaybackEvidence& Path : Playback)
        {
            auto* BeginCollision = FindCall(Graph, Path.BeginCollision, TEXT("BeginTraversalCollision"));
            auto* BeginMode = FindCall(Graph, Path.BeginMode,
                Path.bMover ? TEXT("BeginMoverTraversalMode") : TEXT("BeginCmcTraversalMode"));
            auto* FinishActive = FindCall(Graph, Path.FinishActive, TEXT("SetTraversalActionActive"));
            auto* EndCollision = FindCall(Graph, Path.EndCollision, TEXT("EndTraversalCollision"));
            auto* FinishMode = FindCall(Graph, Path.FinishMode,
                Path.bMover ? TEXT("FinishMoverTraversalMode") : TEXT("FinishCmcTraversalMode"));
            auto* StartActive = FindCall(Graph, Path.StartActive, TEXT("SetTraversalActionActive"));
            auto* Warp = FindCall(Graph, Path.Warp, TEXT("SetWarpTargets"));
            if (!BeginCollision || !BeginMode || !FinishActive || !EndCollision || !FinishMode || !Warp ||
                !Literal(In(StartActive, TEXT("bActive")), TEXT("true")) ||
                !Literal(In(FinishActive, TEXT("bActive")), TEXT("false")) ||
                !Then(BeginMode)->LinkedTo.IsEmpty() || !Then(FinishMode)->LinkedTo.IsEmpty())
            {
                return false;
            }
            auto* Begin = AddCall(Graph, Native(Blueprint, TEXT("BeginTraversalGameplay")), *BeginCollision);
            auto* Finish = AddCall(Graph, Native(Blueprint, TEXT("FinishTraversalGameplay")), *FinishActive);
            if (!Begin || !Finish ||
                !CopyInput(Graph, In(BeginCollision, TEXT("Capsule")), In(Begin, TEXT("Capsule"))) ||
                !CopyInput(Graph, In(BeginCollision, TEXT("Obstacle")), In(Begin, TEXT("Obstacle"))))
            {
                return false;
            }
            auto* MoverFlag = In(Begin, TEXT("bMover"));
            if (!MoverFlag)
            {
                return false;
            }
            Graph.GetSchema()->TrySetDefaultValue(*MoverFlag, Path.bMover ? TEXT("true") : TEXT("false"));
            if (!Literal(MoverFlag, Path.bMover ? TEXT("true") : TEXT("false")) ||
                (Path.bMover && (!CopyInput(Graph, In(BeginCollision, TEXT("Mesh")), In(Begin, TEXT("Mesh"))) ||
                    !CopyInput(Graph, In(BeginMode, TEXT("Mover")), In(Begin, TEXT("Mover"))))) ||
                !Redirect(Graph, Exec(BeginCollision), Exec(Begin)) ||
                !Redirect(Graph, Exec(FinishActive), Exec(Finish)))
            {
                return false;
            }
            Remove(Blueprint, {BeginCollision, BeginMode, FinishActive, EndCollision, FinishMode});
        }
        return true;
    }

    int32 CountCalls(UEdGraph& Graph, const TCHAR* Function)
    {
        int32 Count = 0;
        for (const UEdGraphNode* Node : Graph.Nodes)
        {
            const auto* Call = Cast<UK2Node_CallFunction>(Node);
            Count += Call && Call->FunctionReference.GetMemberName() == FName(Function);
        }
        return Count;
    }

    bool CheckNativeCalls(UBlueprint& Blueprint, UEdGraph& Graph, const TCHAR* Name, int32 Expected)
    {
        const UFunction* Function = Native(Blueprint, Name);
        if (!Function)
        {
            return false;
        }
        int32 Count = 0;
        for (const UEdGraphNode* Node : Graph.Nodes)
        {
            const auto* Call = Cast<UK2Node_CallFunction>(Node);
            if (Call && Call->FunctionReference.GetMemberName() == FName(Name))
            {
                if (Call->GetTargetFunction() != Function)
                {
                    return false;
                }
                ++Count;
            }
        }
        return Count == Expected;
    }

    bool CallsFrom(const UEdGraphPin* Source, const TCHAR* Function)
    {
        const UEdGraphPin* Peer = OnlyPeer(Source);
        const auto* Call = Peer ? Cast<UK2Node_CallFunction>(Peer->GetOwningNode()) : nullptr;
        return Call && Peer == Exec(const_cast<UK2Node_CallFunction*>(Call)) &&
            Call->FunctionReference.GetMemberName() == FName(Function);
    }

    bool Validate(UBlueprint& Blueprint)
    {
        UEdGraph* Try = FindGraph(Blueprint, TEXT("TryTraversalAction"));
        UEdGraph* Warp = FindGraph(Blueprint, TEXT("SetWarpTargets"));
        UEdGraph* Event = FindGraph(Blueprint, TEXT("EventGraph"));
        if (!Try || !Warp || !Event || !CheckSelectionReturns(*Try) ||
            !CheckNativeCalls(Blueprint, *Try, TEXT("ReadTraversalDebugSettings"), 1) ||
            !CheckNativeCalls(Blueprint, *Try, TEXT("PrintTraversalDiagnostic"), 2) ||
            !CheckNativeCalls(Blueprint, *Try, TEXT("TryPublishTraversalSelection"), 1) ||
            CountCalls(*Try, TEXT("PublishTraversalAction")) || CountCalls(*Try, TEXT("PrintString")) ||
            CountCalls(*Try, TEXT("GetConsoleVariableIntValue")) || CountCalls(*Try, TEXT("GetConsoleVariableFloatValue")) ||
            !CheckNativeCalls(Blueprint, *Warp, TEXT("UpdateTraversalWarpTargets"), 1) ||
            CountCalls(*Warp, TEXT("AddOrUpdateWarpTargetFromLocationAndRotation")) ||
            CountCalls(*Warp, TEXT("RemoveWarpTarget")) ||
            CountCalls(*Warp, TEXT("GetCurveValueFromAnimation")) != 2 ||
            CountCalls(*Warp, TEXT("GetMotionWarpingWindowsForWarpTargetFromAnimation")) != 2 ||
            !CheckNativeCalls(Blueprint, *Event, TEXT("BeginTraversalGameplay"), 2) ||
            !CheckNativeCalls(Blueprint, *Event, TEXT("FinishTraversalGameplay"), 2) ||
            CountCalls(*Event, TEXT("SetTraversalActionActive")) != 2 ||
            CountCalls(*Event, TEXT("BeginTraversalCollision")) || CountCalls(*Event, TEXT("EndTraversalCollision")) ||
            CountCalls(*Event, TEXT("BeginMoverTraversalMode")) || CountCalls(*Event, TEXT("BeginCmcTraversalMode")) ||
            CountCalls(*Event, TEXT("FinishMoverTraversalMode")) || CountCalls(*Event, TEXT("FinishCmcTraversalMode")))
        {
            return false;
        }
        auto* Gate = Cast<UK2Node_IfThenElse>(FindNode(*Try, TEXT("K2Node_IfThenElse_0")));
        UEdGraphPin* PublishExec = Gate ? OnlyPeer(Then(Gate)) : nullptr;
        auto* Publish = PublishExec ? Cast<UK2Node_CallFunction>(PublishExec->GetOwningNode()) : nullptr;
        UEdGraphPin* BranchExec = Publish ? OnlyPeer(Then(Publish)) : nullptr;
        auto* Branch = BranchExec ? Cast<UK2Node_IfThenElse>(BranchExec->GetOwningNode()) : nullptr;
        if (!Gate || !CallsFrom(Then(Gate), TEXT("TryPublishTraversalSelection")) || !Branch ||
            OnlyPeer(Out(Publish, TEXT("ReturnValue"))) != In(Branch, TEXT("Condition")) ||
            OnlyPeer(Then(Branch)) != Exec(FindNode(*Try, TEXT("K2Node_FunctionResult_4"))) ||
            OnlyPeer(Branch->GetElsePin()) != Exec(FindNode(*Try, TEXT("K2Node_FunctionResult_5"))) ||
            OnlyPeer(Gate->GetElsePin()) != Exec(FindNode(*Try, TEXT("K2Node_FunctionResult_2"))) ||
            !FindNode(*Try, TEXT("K2Node_EvaluateChooser2_0")) ||
            !FindNode(*Try, TEXT("K2Node_EvaluateChooser2_1")) ||
            !FindNode(*Try, TEXT("K2Node_Message_1")))
        {
            return false;
        }
        auto* Sequence = Cast<UK2Node_ExecutionSequence>(FindNode(*Warp, TEXT("K2Node_ExecutionSequence_0")));
        if (!Sequence || !CallsFrom(Sequence->GetThenPinGivenIndex(2), TEXT("UpdateTraversalWarpTargets")))
        {
            return false;
        }
        for (const TCHAR* Flag : WarpFlags)
        {
            if (!FBlueprintEditorUtils::FindLocalVariableGuidByName(&Blueprint, Warp, FName(Flag)).IsValid())
            {
                return false;
            }
        }
        for (const FPlaybackEvidence& Path : Playback)
        {
            auto* Montage = FindNode(*Event, Path.Montage);
            auto* Active = FindCall(*Event, Path.StartActive, TEXT("SetTraversalActionActive"));
            auto* WarpCall = FindCall(*Event, Path.Warp, TEXT("SetWarpTargets"));
            if (!Montage || !Active || !WarpCall ||
                OnlyPeer(Then(Montage)) != Exec(Active) || OnlyPeer(Then(Active)) != Exec(WarpCall) ||
                !CallsFrom(Then(WarpCall), TEXT("BeginTraversalGameplay")) ||
                !CallsFrom(Out(Montage, TEXT("OnBlendOut")), TEXT("FinishTraversalGameplay")) ||
                !CallsFrom(Out(Montage, TEXT("OnInterrupted")), TEXT("FinishTraversalGameplay")) ||
                OnlyPeer(Out(Montage, TEXT("OnBlendOut"))) != OnlyPeer(Out(Montage, TEXT("OnInterrupted"))))
            {
                return false;
            }
            const auto* Begin = OnlyPeer(Then(WarpCall))->GetOwningNode();
            if (!Literal(In(const_cast<UEdGraphNode*>(Begin), TEXT("bMover")), Path.bMover ? TEXT("true") : TEXT("false")))
            {
                return false;
            }
        }
        return CountCalls(*Event, TEXT("ApplyTraversalReplay")) == 1 &&
            CountCalls(*Event, TEXT("InitializeTraversalRuntime")) == 1;
    }
}

bool USampleTraversalBlueprintMigrationLibrary::MatchesTraversalSource(UBlueprint* Blueprint)
{
    return IsTarget(Blueprint) && CheckSource(*Blueprint);
}

bool USampleTraversalBlueprintMigrationLibrary::MigrateTraversalGraphs(UBlueprint* Blueprint)
{
    if (!MatchesTraversalSource(Blueprint))
    {
        return false;
    }
    Blueprint->Modify();
    for (const FGraphEvidence& Item : Evidence)
    {
        FindGraph(*Blueprint, Item.Name)->Modify();
    }
    // Add function locals first. Their creation refreshes the Blueprint skeleton.
    if (!RewireWarp(*Blueprint, *FindGraph(*Blueprint, TEXT("SetWarpTargets"))) ||
        !RewireSelection(*Blueprint, *FindGraph(*Blueprint, TEXT("TryTraversalAction"))) ||
        !RewirePlayback(*Blueprint, *FindGraph(*Blueprint, TEXT("EventGraph"))) ||
        !Validate(*Blueprint))
    {
        return false;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return Validate(*Blueprint);
}

bool USampleTraversalBlueprintMigrationLibrary::ValidateTraversalGraphs(UBlueprint* Blueprint)
{
    return IsTarget(Blueprint) && Validate(*Blueprint);
}

FString USampleTraversalBlueprintMigrationLibrary::TraversalGraphSignature(UBlueprint* Blueprint)
{
    if (!IsTarget(Blueprint))
    {
        return {};
    }
    TArray<FString> Signatures;
    for (const FGraphEvidence& Item : Evidence)
    {
        UEdGraph* Graph = FindGraph(*Blueprint, Item.Name);
        const FString Signature = Graph ? GraphDigest(*Graph, true) : FString();
        if (Signature.IsEmpty())
        {
            return {};
        }
        Signatures.Add(FString(Item.Name) + TEXT("|") + Signature);
    }
    return Digest(MoveTemp(Signatures));
}

int64 USampleTraversalBlueprintMigrationLibrary::TraversalFunctionFlags(UBlueprint* Blueprint, FName FunctionName)
{
    const UFunction* Function = IsTarget(Blueprint) && Blueprint->GeneratedClass
        ? Blueprint->GeneratedClass->FindFunctionByName(FunctionName) : nullptr;
    return Function ? Function->FunctionFlags : 0;
}
