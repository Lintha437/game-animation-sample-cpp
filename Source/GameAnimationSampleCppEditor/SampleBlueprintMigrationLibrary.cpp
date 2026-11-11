#include "SampleBlueprintMigrationLibrary.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "InputAction.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_CastByteToEnum.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_EnhancedInputAction.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_InputKey.h"
#include "K2Node_Knot.h"
#include "K2Node_Self.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
    bool CollectIsolatedChain(UEdGraphNode* Event, TSet<UEdGraphNode*>& Remove, const TSet<UEdGraphNode*>* AllowedEntries = nullptr)
    {
        TArray<UEdGraphNode*> Pending{Event};
        TSet<UEdGraphNode*> Visited;
        while (!Pending.IsEmpty())
        {
            UEdGraphNode* Current = Pending.Pop(EAllowShrinking::No);
            if (Visited.Contains(Current))
            {
                continue;
            }
            Visited.Add(Current);
            bool bHasExecOutput = false;
            bool bHasExecInput = false;
            for (const UEdGraphPin* Pin : Current->Pins)
            {
                if (Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    bHasExecInput |= Pin->Direction == EGPD_Input;
                    bHasExecOutput |= Pin->Direction == EGPD_Output;
                }
                for (const UEdGraphPin* Peer : Pin->LinkedTo)
                {
                    Pending.Add(Peer->GetOwningNode());
                }
            }
            if (Current != Event && bHasExecOutput && !bHasExecInput && (!AllowedEntries || !AllowedEntries->Contains(Current)))
            {
                return false;
            }
        }
        Remove.Append(Visited);
        return true;
    }

    void RemoveNodes(UBlueprint& Blueprint, UEdGraph& Graph, const TSet<UEdGraphNode*>& Nodes)
    {
        Blueprint.Modify();
        Graph.Modify();
        for (UEdGraphNode* Node : Nodes)
        {
            FBlueprintEditorUtils::RemoveNode(&Blueprint, Node, true);
        }
    }

    TSet<UEdGraphNode*> CollectExecutionNodes(const UEdGraphPin& Start, const UEdGraphNode* Stop = nullptr)
    {
        TArray<UEdGraphPin*> Pending(Start.LinkedTo);
        TSet<UEdGraphNode*> Nodes;
        while (!Pending.IsEmpty())
        {
            UEdGraphNode* Node = Pending.Pop(EAllowShrinking::No)->GetOwningNode();
            if (Node == Stop || Nodes.Contains(Node))
            {
                continue;
            }
            Nodes.Add(Node);
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    Pending.Append(Pin->LinkedTo);
                }
            }
        }
        return Nodes;
    }

    void CollectUnusedInputs(TSet<UEdGraphNode*>& Remove, const UEdGraphNode* Keep)
    {
        TSet<UEdGraphNode*> Candidates;
        TArray<UEdGraphNode*> Pending = Remove.Array();
        while (!Pending.IsEmpty())
        {
            for (const UEdGraphPin* Pin : Pending.Pop(EAllowShrinking::No)->Pins)
            {
                if (Pin->Direction != EGPD_Input || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
                {
                    continue;
                }
                for (const UEdGraphPin* Peer : Pin->LinkedTo)
                {
                    UEdGraphNode* Source = Peer->GetOwningNode();
                    if (Source != Keep && !Remove.Contains(Source) && !Candidates.Contains(Source) &&
                        !Source->Pins.ContainsByPredicate([](const UEdGraphPin* SourcePin)
                        {
                            return SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
                        }))
                    {
                        Candidates.Add(Source);
                        Pending.Add(Source);
                    }
                }
            }
        }
        bool bChanged;
        do
        {
            bChanged = false;
            for (auto It = Candidates.CreateIterator(); It; ++It)
            {
                if ((*It)->Pins.ContainsByPredicate([&](const UEdGraphPin* Pin)
                {
                    return Pin->Direction == EGPD_Output && Pin->LinkedTo.ContainsByPredicate([&](const UEdGraphPin* Peer)
                    {
                        const UEdGraphNode* Consumer = Peer->GetOwningNode();
                        return !Remove.Contains(Consumer) && !Candidates.Contains(Consumer);
                    });
                }))
                {
                    It.RemoveCurrent();
                    bChanged = true;
                }
            }
        } while (bChanged);
        Remove.Append(Candidates);
    }

    struct FTraversalCallReplacement
    {
        UEdGraphNode* First;
        UEdGraphNode* Last;
        const UFunction* Function;
        TArray<TPair<FName, UEdGraphPin*>> Inputs;
        TArray<UEdGraphNode*> AdditionalNodes = {};
    };

    bool InstallTraversalCall(UBlueprint& Blueprint, UEdGraph& Graph, const FTraversalCallReplacement& Replacement)
    {
        FGraphNodeCreator<UK2Node_CallFunction> CreateCall(Graph);
        UK2Node_CallFunction* Call = CreateCall.CreateNode();
        Call->SetFromFunction(Replacement.Function);
        Call->NodePosX = Replacement.First->NodePosX;
        Call->NodePosY = Replacement.First->NodePosY;
        CreateCall.Finalize();
        const UEdGraphSchema* Schema = Graph.GetSchema();
        for (const auto& [Name, Old] : Replacement.Inputs)
        {
            UEdGraphPin* New = Call->FindPin(Name, EGPD_Input);
            if (!New)
            {
                return false;
            }
            if (Old->LinkedTo.IsEmpty())
            {
                Schema->TrySetDefaultValue(*New, Old->DefaultValue);
            }
            else if (!Schema->TryCreateConnection(Old->LinkedTo[0], New))
            {
                return false;
            }
        }
        UEdGraphPin* Exec = Replacement.First->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
        UEdGraphPin* Then = Replacement.Last->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
        if (!Exec || !Then || !Schema->MovePinLinks(*Exec, *Call->GetExecPin()).CanSafeConnect() ||
            (!Then->LinkedTo.IsEmpty() && !Schema->MovePinLinks(*Then, *Call->GetThenPin()).CanSafeConnect()))
        {
            return false;
        }
        TSet<UEdGraphNode*> Remove{Replacement.First, Replacement.Last};
        for (UEdGraphNode* Node : Replacement.AdditionalNodes)
        {
            Remove.Add(Node);
        }
        CollectUnusedInputs(Remove, nullptr);
        RemoveNodes(Blueprint, Graph, Remove);
        return true;
    }

    UEdGraphNode* GetOnlyPeer(const UEdGraphPin* Pin)
    {
        return Pin && Pin->LinkedTo.Num() == 1 ? Pin->LinkedTo[0]->GetOwningNode() : nullptr;
    }

    UK2Node_CallFunction* GetNextCall(const UK2Node_CallFunction& Call, FName Name)
    {
        auto* Next = Cast<UK2Node_CallFunction>(GetOnlyPeer(Call.GetThenPin()));
        return Next && Next->FunctionReference.GetMemberName() == Name ? Next : nullptr;
    }

    UK2Node_CallFunction* FindUniqueCall(UEdGraph& Graph, FName Name)
    {
        TArray<UK2Node_CallFunction*> Calls;
        Graph.GetNodesOfClass(Calls);
        UK2Node_CallFunction* Match = nullptr;
        for (UK2Node_CallFunction* Call : Calls)
        {
            if (Call->FunctionReference.GetMemberName() == Name)
            {
                if (Match)
                {
                    return nullptr;
                }
                Match = Call;
            }
        }
        return Match;
    }

    bool InstallExecutionTail(UBlueprint& Blueprint, UEdGraph& Graph, UEdGraphPin* Exec,
        const UFunction& Function)
    {
        UEdGraphPin* Previous = Exec && Exec->LinkedTo.Num() == 1 ? Exec->LinkedTo[0] : nullptr;
        if (!Previous || Previous->LinkedTo.Num() != 1 || Previous->LinkedTo[0] != Exec)
        {
            return false;
        }
        const UEdGraphNode* First = Exec->GetOwningNode();

        TSet<UEdGraphNode*> Remove = CollectExecutionNodes(*Previous);
        for (const UEdGraphNode* Node : Remove)
        {
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
                if ((bExec && Pin->Direction == EGPD_Input && Pin != Exec) ||
                    (!bExec && Pin->Direction == EGPD_Output))
                {
                    for (const UEdGraphPin* Peer : Pin->LinkedTo)
                    {
                        if (!Remove.Contains(Peer->GetOwningNode()))
                        {
                            return false;
                        }
                    }
                }
            }
        }

        FGraphNodeCreator<UK2Node_CallFunction> Creator(Graph);
        UK2Node_CallFunction* Call = Creator.CreateNode();
        Call->SetFromFunction(&Function);
        Call->NodePosX = First->NodePosX;
        Call->NodePosY = First->NodePosY;
        Creator.Finalize();
        if (!Graph.GetSchema()->MovePinLinks(*Exec, *Call->GetExecPin()).CanSafeConnect())
        {
            return false;
        }
        CollectUnusedInputs(Remove, nullptr);
        RemoveNodes(Blueprint, Graph, Remove);
        return true;
    }
}

bool USampleBlueprintMigrationLibrary::RetargetSelfCall(UEdGraph* Graph, FName OldFunction, FName NewFunction)
{
    if (!Graph || OldFunction.IsNone() || NewFunction.IsNone())
    {
        return false;
    }

    UK2Node_CallFunction* Match = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
        if (Call && Call->FunctionReference.IsSelfContext() && Call->FunctionReference.GetMemberName() == OldFunction)
        {
            if (Match)
            {
                return false;
            }
            Match = Call;
        }
    }
    if (!Match)
    {
        return false;
    }

    Match->Modify();
    Match->FunctionReference.SetSelfMember(NewFunction, FGuid());
    return true;
}

TArray<FString> USampleBlueprintMigrationLibrary::ListEnhancedInputActions(const UEdGraph* Graph)
{
    TArray<FString> Paths;
    if (Graph)
    {
        for (const UEdGraphNode* Node : Graph->Nodes)
        {
            const UK2Node_EnhancedInputAction* Event = Cast<UK2Node_EnhancedInputAction>(Node);
            if (Event && Event->InputAction)
            {
                Paths.Add(Event->InputAction->GetPathName());
            }
        }
    }
    return Paths;
}

bool USampleBlueprintMigrationLibrary::RemoveEnhancedInputActions(
    UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FString>& ActionPaths)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || ActionPaths.IsEmpty())
    {
        return false;
    }

    TSet<FString> Matched;
    TSet<UEdGraphNode*> Remove;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_EnhancedInputAction* Event = Cast<UK2Node_EnhancedInputAction>(Node);
        if (!Event || !Event->InputAction || !ActionPaths.Contains(Event->InputAction->GetPathName()))
        {
            continue;
        }
        const FString Path = Event->InputAction->GetPathName();
        if (Matched.Contains(Path) || !CollectIsolatedChain(Event, Remove))
        {
            return false;
        }
        Matched.Add(Path);
    }
    if (Matched.Num() != ActionPaths.Num())
    {
        return false;
    }

    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

int64 USampleBlueprintMigrationLibrary::GetCompiledFunctionFlags(const UBlueprint* Blueprint, FName FunctionName)
{
    const UFunction* Function = Blueprint && Blueprint->GeneratedClass
        ? Blueprint->GeneratedClass->FindFunctionByName(FunctionName)
        : nullptr;
    return Function ? Function->FunctionFlags : 0;
}

bool USampleBlueprintMigrationLibrary::RemoveCustomEvent(UBlueprint* Blueprint, UEdGraph* Graph, FName EventName)
{
    return RemoveCustomEvents(Blueprint, Graph, {EventName});
}

bool USampleBlueprintMigrationLibrary::RemoveCustomEvents(UBlueprint* Blueprint, UEdGraph* Graph, const TArray<FName>& EventNames)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || EventNames.IsEmpty() || EventNames.Contains(NAME_None))
    {
        return false;
    }
    TSet<FName> MatchedNames;
    TSet<UEdGraphNode*> Entries;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        UK2Node_CustomEvent* Event = Cast<UK2Node_CustomEvent>(Node);
        if (Event && EventNames.Contains(Event->CustomFunctionName))
        {
            if (MatchedNames.Contains(Event->CustomFunctionName))
            {
                return false;
            }
            MatchedNames.Add(Event->CustomFunctionName);
            Entries.Add(Event);
        }
    }

    if (MatchedNames.Num() != EventNames.Num())
    {
        return false;
    }
    TSet<UEdGraphNode*> Remove;
    for (UEdGraphNode* Event : Entries)
    {
        if (!Remove.Contains(Event) && !CollectIsolatedChain(Event, Remove, &Entries))
        {
            return false;
        }
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

bool USampleBlueprintMigrationLibrary::RemoveIsolatedEventOverride(UBlueprint* Blueprint, UEdGraph* Graph, FName EventName)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || EventName.IsNone())
    {
        return false;
    }
    UK2Node_Event* Match = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (auto* Event = Cast<UK2Node_Event>(Node); Event && Event->EventReference.GetMemberName() == EventName)
        {
            if (Match)
            {
                return false;
            }
            Match = Event;
        }
    }
    if (!Match)
    {
        return true;
    }
    TSet<UEdGraphNode*> Remove;
    if (!CollectIsolatedChain(Match, Remove))
    {
        return false;
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

bool USampleBlueprintMigrationLibrary::RemoveLookAtGameplayButtons(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    const TSet<FName> Expected{TEXT("Button_Spawn"), TEXT("Button_DeSpawn"), TEXT("Button_Teleport")};
    TSet<FName> Found;
    TSet<UEdGraphNode*> Remove;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        const auto* Event = Cast<UK2Node_ComponentBoundEvent>(Node);
        if (!Event || !Expected.Contains(Event->GetComponentPropertyName()))
        {
            continue;
        }
        if (Event->DelegatePropertyName != TEXT("OnClicked") || Found.Contains(Event->GetComponentPropertyName()) ||
            !CollectIsolatedChain(Node, Remove))
        {
            return false;
        }
        Found.Add(Event->GetComponentPropertyName());
    }
    if (Found.Num() != Expected.Num())
    {
        return false;
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

bool USampleBlueprintMigrationLibrary::ForwardTraversalLedgeSetup(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("UserConstructionScript"))
    {
        return false;
    }
    const FName Name(TEXT("RebuildTraversalLedges"));
    if (FindUniqueCall(*Graph, Name))
    {
        return true;
    }
    const auto* Function = Blueprint->ParentClass->FindFunctionByName(Name);
    TArray<UK2Node_CallParentFunction*> ParentCalls;
    Graph->GetNodesOfClass(ParentCalls);
    const auto* Parent = ParentCalls.Num() == 1 ? ParentCalls[0] : nullptr;
    auto* Ledges = Cast<UK2Node_VariableSet>(GetOnlyPeer(Parent ? Parent->GetThenPin() : nullptr));
    auto* Opposites = Cast<UK2Node_VariableSet>(GetOnlyPeer(
        Ledges ? Ledges->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr));
    const auto* End = Opposites ? Opposites->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
    if (!Function || !Parent || Parent->FunctionReference.GetMemberName() != TEXT("UserConstructionScript") ||
        !Ledges || Ledges->GetVarName() != TEXT("Ledges") ||
        !Opposites || Opposites->GetVarName() != TEXT("OppositeLedges") || !End || !End->LinkedTo.IsEmpty())
    {
        return false;
    }
    return InstallExecutionTail(*Blueprint, *Graph, Ledges->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input), *Function);
}

bool USampleBlueprintMigrationLibrary::ForwardLookAtZoneSelection(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    const FName NativeName(TEXT("CaptureLookAtZoneSelection"));
    if (FindUniqueCall(*Graph, NativeName))
    {
        return true;
    }
    const auto* Native = Blueprint->ParentClass->FindFunctionByName(NativeName);
    TArray<UK2Node_CustomEvent*> Events;
    Graph->GetNodesOfClass(Events);
    const auto* Event = Events.FindByPredicate([](const auto* Node)
    {
        return Node->CustomFunctionName == TEXT("Zone Was Selected");
    });
    const auto NextSet = [](UEdGraphPin* Exec, FName Name) -> UK2Node_VariableSet*
    {
        auto* Node = Cast<UK2Node_VariableSet>(GetOnlyPeer(Exec));
        return Node && Node->GetVarName() == Name ? Node : nullptr;
    };
    const auto Then = [](const UEdGraphNode* Node)
    {
        return Node ? Node->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
    };
    auto* Type = NextSet(Then(Event ? *Event : nullptr), TEXT("Selected Zone Type"));
    auto* CastPawn = Cast<UK2Node_DynamicCast>(GetOnlyPeer(Then(Type)));
    auto* Pawn = NextSet(Then(CastPawn), TEXT("Active Pawn"));
    auto* Gizmo = NextSet(Then(Pawn), TEXT("Gizmo Obj"));
    auto* Anim = NextSet(Then(Gizmo), TEXT("AnimInstance"));
    auto* Enable = NextSet(Then(Anim), TEXT("Look At Enabled"));
    auto* TypeInput = Type ? Type->FindPin(TEXT("Selected Zone Type"), EGPD_Input) : nullptr;
    auto* PawnInput = CastPawn ? CastPawn->GetCastSourcePin() : nullptr;
    auto* GizmoInput = Gizmo ? Gizmo->FindPin(TEXT("Gizmo Obj"), EGPD_Input) : nullptr;
    const auto* Failure = CastPawn ? CastPawn->FindPin(TEXT("CastFailed"), EGPD_Output) : nullptr;
    if (!Native || !Enable || !Failure || !Failure->LinkedTo.IsEmpty() ||
        !TypeInput || !PawnInput || !GizmoInput ||
        !InstallTraversalCall(*Blueprint, *Graph, {Type, Anim, Native,
            {{TEXT("ZoneType"), TypeInput}, {TEXT("SelectedPawn"), PawnInput}, {TEXT("Gizmo"), GizmoInput}},
            {CastPawn, Pawn, Gizmo}}))
    {
        return false;
    }

    auto* Call = FindUniqueCall(*Graph, NativeName);
    auto* Continue = Enable->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
    if (!Call || !Continue || GetOnlyPeer(Call->GetThenPin()) != Enable)
    {
        return false;
    }
    FGraphNodeCreator<UK2Node_IfThenElse> CreateGate(*Graph);
    auto* Gate = CreateGate.CreateNode();
    Gate->NodePosX = Call->NodePosX + 350;
    Gate->NodePosY = Call->NodePosY;
    CreateGate.Finalize();
    Call->GetThenPin()->BreakLinkTo(Continue);
    const auto* Schema = Graph->GetSchema();
    return Schema->TryCreateConnection(Call->GetThenPin(), Gate->GetExecPin()) &&
        Schema->TryCreateConnection(Call->GetReturnValuePin(), Gate->GetConditionPin()) &&
        Schema->TryCreateConnection(Gate->GetThenPin(), Continue);
}

bool USampleBlueprintMigrationLibrary::ForwardPlayerInteractionCleanup(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    const FName FunctionName(TEXT("ClearPlayerInteractionState"));
    if (FindUniqueCall(*Graph, FunctionName))
    {
        return true;
    }
    const auto* Function = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_VariableSet*> Writes;
    Graph->GetNodesOfClass(Writes);
    const auto* Match = Writes.FindByPredicate([](const auto* Node) { return Node->GetVarName() == TEXT("PlayerIsInteracting"); });
    auto* State = Match ? *Match : nullptr;
    const auto* Value = State ? State->FindPin(TEXT("PlayerIsInteracting"), EGPD_Input) : nullptr;
    auto* Previous = Cast<UK2Node_ExecutionSequence>(GetOnlyPeer(State
        ? State->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input) : nullptr));
    const auto* Notify = Cast<UK2Node_CallFunction>(GetOnlyPeer(Previous ? Previous->GetThenPinGivenIndex(0) : nullptr));
    const auto* Close = Cast<UK2Node_Knot>(GetOnlyPeer(State
        ? State->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr));
    if (!Function || !Value || Value->DefaultValue != TEXT("false") || !Value->LinkedTo.IsEmpty() ||
        !Notify || Notify->FunctionReference.GetMemberName() != TEXT("On Montage Finished Requested") || !Close)
    {
        return false;
    }
    return InstallTraversalCall(*Blueprint, *Graph, {State, State, Function, {}});
}

bool USampleBlueprintMigrationLibrary::ReplacePairedInteractionCollision(UBlueprint* Blueprint, UEdGraph* MatchGraph,
    UEdGraph* EventGraph)
{
    if (!Blueprint || !Blueprint->ParentClass || !MatchGraph || !EventGraph ||
        MatchGraph->GetOuter() != Blueprint || EventGraph->GetOuter() != Blueprint ||
        MatchGraph->GetFName() != TEXT("Try_MultiCharacterInteraction") || EventGraph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }

    const auto* Begin = Blueprint->ParentClass->FindFunctionByName(TEXT("BeginPairedInteractionCollision"));
    const auto* End = Blueprint->ParentClass->FindFunctionByName(TEXT("EndPairedInteractionCollision"));
    if (!Begin || !End)
    {
        return false;
    }
    if (FindUniqueCall(*MatchGraph, Begin->GetFName()) && FindUniqueCall(*EventGraph, End->GetFName()))
    {
        return true;
    }

    auto* Prerequisite = FindUniqueCall(*MatchGraph, TEXT("AddTickPrerequisiteComponent"));
    auto* IgnorePartner = Prerequisite ? GetNextCall(*Prerequisite, TEXT("IgnoreComponentWhenMoving")) : nullptr;
    auto* IgnoreInitiator = IgnorePartner ? GetNextCall(*IgnorePartner, TEXT("IgnoreComponentWhenMoving")) : nullptr;
    auto* Cleanup = FindUniqueCall(*EventGraph, TEXT("ClearMoveIgnoreComponents"));
    UEdGraphPin* Capsule = IgnorePartner ? IgnorePartner->FindPin(TEXT("Component"), EGPD_Input) : nullptr;
    if (!IgnoreInitiator || !IgnoreInitiator->GetThenPin()->LinkedTo.IsEmpty() ||
        !Capsule || Capsule->LinkedTo.Num() != 1 || !Cleanup)
    {
        return false;
    }

    return InstallTraversalCall(*Blueprint, *MatchGraph,
        {Prerequisite, IgnoreInitiator, Begin, {{TEXT("PartnerCapsule"), Capsule}}, {IgnorePartner}}) &&
        InstallTraversalCall(*Blueprint, *EventGraph, {Cleanup, Cleanup, End, {}});
}

bool USampleBlueprintMigrationLibrary::RemoveSandboxCameraKeys(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint)
    {
        return false;
    }
    const TSet<FName> Keys{TEXT("MouseScrollDown"), TEXT("MouseScrollUp"), TEXT("Gamepad_DPad_Down"), TEXT("Gamepad_DPad_Up")};
    const TSet<FName> ExpectedCalls{TEXT("GetConsoleVariableIntValue"), TEXT("ExecuteConsoleCommand"),
        TEXT("PrintString"), TEXT("PlaySound2D")};
    TSet<FName> Matched;
    TSet<FName> Calls;
    TSet<UEdGraphNode*> Remove;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        auto* Input = Cast<UK2Node_InputKey>(Node);
        if (!Input || !Keys.Contains(Input->InputKey.GetFName()))
        {
            continue;
        }
        if (Matched.Contains(Input->InputKey.GetFName()) || !Input->bConsumeInput || Input->bExecuteWhenPaused ||
            !Input->GetReleasedPin()->LinkedTo.IsEmpty())
        {
            return false;
        }
        const TSet<UEdGraphNode*> Chain = CollectExecutionNodes(*Input->GetPressedPin());
        for (const UEdGraphNode* Step : Chain)
        {
            if (const auto* Call = Cast<UK2Node_CallFunction>(Step))
            {
                Calls.Add(Call->FunctionReference.GetMemberName());
            }
        }
        Matched.Add(Input->InputKey.GetFName());
        Remove.Add(Input);
        Remove.Append(Chain);
    }
    if (Matched.Num() != Keys.Num() || Calls.Num() != ExpectedCalls.Num() || !Calls.Includes(ExpectedCalls))
    {
        UE_LOG(LogTemp, Warning, TEXT("Unexpected camera input graph on %s: %s"), *Blueprint->GetName(),
            *FString::JoinBy(Calls.Array(), TEXT(", "), [](FName Name) { return Name.ToString(); }));
        return false;
    }
    CollectUnusedInputs(Remove, nullptr);
    for (const UEdGraphNode* Node : Remove)
    {
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                continue;
            }
            for (const UEdGraphPin* Peer : Pin->LinkedTo)
            {
                if (!Remove.Contains(Peer->GetOwningNode()))
                {
                    return false;
                }
            }
        }
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

bool USampleBlueprintMigrationLibrary::RemoveMoverFlightKeys(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint)
    {
        return false;
    }
    UEdGraphPin* StickPressed = nullptr;
    UEdGraphPin* DebugPressed = nullptr;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (auto* Key = Cast<UK2Node_InputKey>(Node); Key && Key->InputKey == EKeys::Gamepad_LeftThumbstick)
        {
            if (StickPressed || !Key->bConsumeInput || Key->bExecuteWhenPaused || !Key->GetReleasedPin()->LinkedTo.IsEmpty())
            {
                return false;
            }
            StickPressed = Key->GetPressedPin();
        }
        else if (Node->GetClass()->GetFName() == TEXT("K2Node_InputDebugKey"))
        {
            const auto* KeyField = FindFProperty<FStructProperty>(Node->GetClass(), TEXT("InputKey"));
            if (KeyField && KeyField->Struct == TBaseStructure<FKey>::Get() &&
                *KeyField->ContainerPtrToValuePtr<FKey>(Node) == EKeys::Z)
            {
                const auto* Paused = FindFProperty<FBoolProperty>(Node->GetClass(), TEXT("bExecuteWhenPaused"));
                auto* Released = Node->FindPin(TEXT("Released"), EGPD_Output);
                if (DebugPressed || !Released || !Released->LinkedTo.IsEmpty() || !Paused || Paused->GetPropertyValue_InContainer(Node))
                {
                    return false;
                }
                DebugPressed = Node->FindPin(TEXT("Pressed"), EGPD_Output);
            }
        }
    }
    auto* Gate = Cast<UK2Node_IfThenElse>(GetOnlyPeer(StickPressed));
    if (!Gate || GetOnlyPeer(DebugPressed) != Gate)
    {
        return false;
    }
    const auto MatchesMode = [](UEdGraphPin* Branch, const TCHAR* Expected)
    {
        const auto* Print = Cast<UK2Node_CallFunction>(GetOnlyPeer(Branch));
        const auto* Queue = Print && Print->FunctionReference.GetMemberName() == TEXT("PrintString")
            ? GetNextCall(*Print, TEXT("QueueNextMode")) : nullptr;
        const auto* Mode = Queue ? Queue->FindPin(TEXT("DesiredModeName"), EGPD_Input) : nullptr;
        return Mode && Mode->LinkedTo.IsEmpty() && Mode->DefaultValue == Expected && Queue->GetThenPin()->LinkedTo.IsEmpty();
    };
    if (!MatchesMode(Gate->GetThenPin(), TEXT("Flying")) || !MatchesMode(Gate->GetElsePin(), TEXT("Falling")))
    {
        return false;
    }
    const TSet<UEdGraphNode*> Entries{StickPressed->GetOwningNode(), DebugPressed->GetOwningNode()};
    TSet<UEdGraphNode*> Remove;
    if (!CollectIsolatedChain(StickPressed->GetOwningNode(), Remove, &Entries))
    {
        return false;
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return true;
}

bool USampleBlueprintMigrationLibrary::ForwardMoverTransitionCrouch(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("On_MovementModeChanged_PostFinalize"))
    {
        return false;
    }
    const FName SlideName(TEXT("ClearSlideCrouchForSprint"));
    const FName AirName(TEXT("ClearAirborneCrouch"));
    if (FindUniqueCall(*Graph, SlideName) && FindUniqueCall(*Graph, AirName))
    {
        return true;
    }
    const auto* Slide = Blueprint->ParentClass->FindFunctionByName(SlideName);
    const auto* Air = Blueprint->ParentClass->FindFunctionByName(AirName);
    auto* Fade = FindUniqueCall(*Graph, TEXT("FadeOut"));
    auto* SlideBranch = Cast<UK2Node_IfThenElse>(GetOnlyPeer(Fade ? Fade->GetThenPin() : nullptr));
    auto* IsCrouching = FindUniqueCall(*Graph, TEXT("IsCrouching"));
    auto* AirBranch = Cast<UK2Node_IfThenElse>(GetOnlyPeer(IsCrouching ? IsCrouching->GetReturnValuePin() : nullptr));
    const auto* Sprint = Cast<UK2Node_VariableGet>(GetOnlyPeer(SlideBranch ? SlideBranch->GetConditionPin() : nullptr));
    if (!Slide || !Air || !SlideBranch || !AirBranch || !Sprint || Sprint->GetVarName() != TEXT("PlayerInputState") ||
        !Cast<UK2Node_ExecutionSequence>(GetOnlyPeer(AirBranch->GetExecPin())) ||
        !FindUniqueCall(*Graph, TEXT("UnCrouch")))
    {
        return false;
    }
    return InstallExecutionTail(*Blueprint, *Graph, SlideBranch->GetExecPin(), *Slide) &&
        InstallExecutionTail(*Blueprint, *Graph, AirBranch->GetExecPin(), *Air);
}

bool USampleBlueprintMigrationLibrary::ForwardRagdollDemoSpawn(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("Demo_Spawn/RotateCharacter"))
    {
        return false;
    }

    const FName Function(TEXT("SpawnOrRotateRagdollDemoCharacter"));
    const FName ClassName(TEXT("DemoCharacterClass"));
    if (const auto* Existing = FindUniqueCall(*Graph, Function))
    {
        const auto* ClassPin = Existing->FindPin(ClassName, EGPD_Input);
        return ClassPin && ClassPin->DefaultObject == Blueprint->GeneratedClass;
    }

    TArray<UK2Node_SpawnActorFromClass*> Spawns;
    Graph->GetNodesOfClass(Spawns);
    const auto* ClassPin = Spawns.Num() == 1 ? Spawns[0]->FindPin(TEXT("Class"), EGPD_Input) : nullptr;
    auto* SpawnClass = ClassPin && ClassPin->LinkedTo.IsEmpty() ? Cast<UClass>(ClassPin->DefaultObject) : nullptr;
    if (!SpawnClass || SpawnClass != Blueprint->GeneratedClass ||
        !FindUniqueCall(*Graph, TEXT("LineTraceSingleForObjects")) || !FindUniqueCall(*Graph, TEXT("CustomTeleport")) ||
        !ReplaceMoverInputFunction(Blueprint, Graph, Function, NAME_None))
    {
        return false;
    }

    auto* Call = FindUniqueCall(*Graph, Function);
    auto* NativeClass = Call ? Call->FindPin(ClassName, EGPD_Input) : nullptr;
    if (!NativeClass)
    {
        return false;
    }
    Graph->GetSchema()->TrySetDefaultObject(*NativeClass, SpawnClass);
    return NativeClass->DefaultObject == SpawnClass;
}

bool USampleBlueprintMigrationLibrary::ForwardCmcLandingState(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    const FName RecordName(TEXT("RecordLandingVelocity"));
    const FName StartName(TEXT("StartLandingWindow"));
    if (FindUniqueCall(*Graph, RecordName) && FindUniqueCall(*Graph, StartName))
    {
        return true;
    }

    const auto* Record = Blueprint->ParentClass->FindFunctionByName(RecordName);
    const auto* Start = Blueprint->ParentClass->FindFunctionByName(StartName);
    auto* Delay = FindUniqueCall(*Graph, TEXT("RetriggerableDelay"));
    auto* Enter = Cast<UK2Node_VariableSet>(GetOnlyPeer(Delay ? Delay->GetExecPin() : nullptr));
    auto* Exit = Cast<UK2Node_VariableSet>(GetOnlyPeer(Delay ? Delay->GetThenPin() : nullptr));
    auto* Foley = Cast<UK2Node_CallFunction>(GetOnlyPeer(Enter ? Enter->FindPin(UEdGraphSchema_K2::PN_Execute) : nullptr));
    auto* Velocity = Cast<UK2Node_VariableSet>(GetOnlyPeer(Foley ? Foley->GetExecPin() : nullptr));
    auto* Value = Velocity ? Velocity->FindPin(TEXT("LandVelocity"), EGPD_Input) : nullptr;
    auto* Event = Cast<UK2Node_CustomEvent>(GetOnlyPeer(Value));
    const auto* Duration = Delay ? Delay->FindPin(TEXT("Duration"), EGPD_Input) : nullptr;
    const auto MatchesFlag = [](const UK2Node_VariableSet* Node, const TCHAR* Expected)
    {
        const auto* Pin = Node ? Node->FindPin(TEXT("JustLanded"), EGPD_Input) : nullptr;
        return Pin && Node->GetVarName() == TEXT("JustLanded") && Pin->LinkedTo.IsEmpty() && Pin->DefaultValue == Expected;
    };
    if (!Record || !Start || !Duration || !Duration->LinkedTo.IsEmpty() ||
        !FMath::IsNearlyEqual(FCString::Atof(*Duration->DefaultValue), 0.3f) ||
        !MatchesFlag(Enter, TEXT("true")) || !MatchesFlag(Exit, TEXT("false")) ||
        !Foley || Foley->FunctionReference.GetMemberName() != TEXT("PlayFoleyEvent") ||
        !Velocity || Velocity->GetVarName() != TEXT("LandVelocity") || !Event ||
        Event->CustomFunctionName != TEXT("Custom OnLanded Event"))
    {
        return false;
    }
    return InstallTraversalCall(*Blueprint, *Graph, {Velocity, Velocity, Record, {{TEXT("LandVelocity"), Value}}}) &&
        InstallExecutionTail(*Blueprint, *Graph, Enter->FindPin(UEdGraphSchema_K2::PN_Execute), *Start);
}

bool USampleBlueprintMigrationLibrary::ForwardMoverDirectionSector(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("Get_MovementDirectionAndOffset"))
    {
        return false;
    }
    const FName NativeName(TEXT("PrepareMoverDirectionalOffset"));
    if (FindUniqueCall(*Graph, NativeName))
    {
        return true;
    }

    const auto* Function = Blueprint->ParentClass->FindFunctionByName(NativeName);
    auto* Curve = FindUniqueCall(*Graph, TEXT("GetFloatValue"));
    auto* Chooser = GetOnlyPeer(Curve ? Curve->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input) : nullptr);
    auto* Angle = Curve ? Curve->FindPin(TEXT("InTime"), EGPD_Input) : nullptr;
    auto* Context = GetOnlyPeer(Chooser
        ? Chooser->FindPin(TEXT("S_RotationOffsetCurveChooser_Inputs"), EGPD_Input) : nullptr);
    auto* ChooserSector = Context ? Context->FindPin(TEXT("MovementDirection_2_D5D2C02345E313621C7390A0620142F6"), EGPD_Input) : nullptr;
    auto* Mode = Cast<UK2Node_CallFunction>(GetOnlyPeer(Context
        ? Context->FindPin(TEXT("MovementMode_5_6869EC0C4C816E7A69DFEBA731B5A584"), EGPD_Input) : nullptr));
    auto* ChooserExec = Chooser ? Chooser->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input) : nullptr;
    auto* Success = Cast<UK2Node_FunctionResult>(GetOnlyPeer(
        Chooser ? Chooser->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Output) : nullptr));
    auto* ResultSector = Success ? Success->FindPin(TEXT("MovementDirection"), EGPD_Input) : nullptr;
    auto* Definition = ResultSector ? Cast<UEnum>(ResultSector->PinType.PinSubCategoryObject.Get()) : nullptr;
    TArray<UK2Node_FunctionEntry*> Entries;
    TArray<UK2Node_FunctionResult*> Results;
    Graph->GetNodesOfClass(Entries);
    Graph->GetNodesOfClass(Results);
    if (!Function || !Angle || !ChooserExec || Chooser->GetClass()->GetFName() != TEXT("K2Node_EvaluateChooser2") ||
        !ChooserSector || !Definition || !Mode || Mode->FunctionReference.GetMemberName() != TEXT("Get_CurrentMovementMode") ||
        Entries.Num() != 1 || Results.Num() != 3)
    {
        return false;
    }

    UK2Node_FunctionResult* Fallback = nullptr;
    for (auto* Result : Results)
    {
        if (Result == Success)
        {
            continue;
        }
        auto* Direction = Result->FindPin(TEXT("MovementDirection"), EGPD_Input);
        auto* Offset = Result->FindPin(TEXT("RotationOffset"), EGPD_Input);
        if (!Direction || !Offset || !Direction->LinkedTo.IsEmpty() || !Offset->LinkedTo.IsEmpty() ||
            Direction->DefaultValue != TEXT("NewEnumerator0") || FCString::Atod(*Offset->DefaultValue) != 0.0)
        {
            return false;
        }
        Fallback = Result;
    }
    if (!Fallback)
    {
        return false;
    }
    auto* Entry = Entries[0];
    const TSet<UEdGraphNode*> Keep{Entry, Fallback, Chooser, Context, Mode, Success, Curve};
    TSet<UEdGraphNode*> Remove;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (!Keep.Contains(Node))
        {
            Remove.Add(Node);
        }
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    Entry->Modify();
    Entry->LocalVariables.Reset();

    FGraphNodeCreator<UK2Node_CallFunction> CreateCall(*Graph);
    auto* Call = CreateCall.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = Entry->NodePosX + 300;
    Call->NodePosY = Entry->NodePosY;
    CreateCall.Finalize();

    FGraphNodeCreator<UK2Node_IfThenElse> CreateGate(*Graph);
    auto* Gate = CreateGate.CreateNode();
    Gate->NodePosX = Call->NodePosX + 380;
    Gate->NodePosY = Entry->NodePosY;
    CreateGate.Finalize();

    FGraphNodeCreator<UK2Node_CastByteToEnum> CreateConversion(*Graph);
    auto* Convert = CreateConversion.CreateNode();
    Convert->Enum = Definition;
    Convert->bSafe = true;
    Convert->NodePosX = Call->NodePosX + 380;
    Convert->NodePosY = Call->NodePosY + 240;
    CreateConversion.Finalize();

    const auto* Schema = Graph->GetSchema();
    auto* Sector = Convert->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output);
    return Schema->TryCreateConnection(Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output), Call->GetExecPin()) &&
        Schema->TryCreateConnection(Call->GetThenPin(), Gate->GetExecPin()) &&
        Schema->TryCreateConnection(Call->GetReturnValuePin(), Gate->GetConditionPin()) &&
        Schema->TryCreateConnection(Gate->GetThenPin(), ChooserExec) &&
        Schema->TryCreateConnection(Gate->GetElsePin(), Fallback->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input)) &&
        Schema->TryCreateConnection(Call->FindPin(TEXT("Angle"), EGPD_Output), Angle) &&
        Schema->TryCreateConnection(Call->FindPin(TEXT("Sector"), EGPD_Output), Convert->FindPin(TEXT("Byte"), EGPD_Input)) &&
        Schema->TryCreateConnection(Sector, ChooserSector) && Schema->TryCreateConnection(Sector, ResultSector);
}

bool USampleBlueprintMigrationLibrary::ForwardIsolatedEventOverride(UBlueprint* Blueprint, UEdGraph* Graph,
    FName EventName, FName FunctionName)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint)
    {
        return false;
    }
    const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_Event*> Events;
    Graph->GetNodesOfClass(Events);
    const auto* Match = Events.FindByPredicate([&](const UK2Node_Event* Event)
    {
        if (const auto* CustomEvent = Cast<UK2Node_CustomEvent>(Event))
        {
            return CustomEvent->CustomFunctionName == EventName;
        }
        return Event->EventReference.GetMemberName() == EventName;
    });
    UK2Node_Event* Event = Match ? *Match : nullptr;
    UEdGraphPin* Then = Event ? Event->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
    UEdGraphNode* First = GetOnlyPeer(Then);
    if (!Function || !First)
    {
        return false;
    }
    if (const auto* Existing = Cast<UK2Node_CallFunction>(First);
        Existing && Existing->FunctionReference.GetMemberName() == FunctionName)
    {
        return true;
    }
    TSet<UEdGraphNode*> Remove;
    if (!CollectIsolatedChain(Event, Remove))
    {
        return false;
    }
    Remove.Remove(Event);

    FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
    UK2Node_CallFunction* Call = Creator.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = First->NodePosX;
    Call->NodePosY = First->NodePosY;
    Creator.Finalize();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    for (UEdGraphPin* Pin : Call->Pins)
    {
        if (Pin->Direction == EGPD_Input && Pin->PinName != UEdGraphSchema_K2::PN_Self &&
            Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
        {
            UEdGraphPin* Input = Event->FindPin(Pin->PinName, EGPD_Output);
            if (!Input || !Schema->TryCreateConnection(Input, Pin))
            {
                return false;
            }
        }
    }
    RemoveNodes(*Blueprint, *Graph, Remove);
    return Schema->TryCreateConnection(Then, Call->GetExecPin());
}

bool USampleBlueprintMigrationLibrary::RemoveDeclaredVariable(UBlueprint* Blueprint, FName VariableName)
{
    if (!Blueprint || FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName) == INDEX_NONE)
    {
        return false;
    }
    FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, VariableName);
    return true;
}

bool USampleBlueprintMigrationLibrary::ReplaceVoidFunctionBody(
    UBlueprint* Blueprint, UEdGraph* Graph, UClass* Library, FName FunctionName)
{
    const UFunction* Function = Library ? Library->FindFunctionByName(FunctionName) : nullptr;
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || !Function ||
        !Function->HasAllFunctionFlags(FUNC_Static | FUNC_Native) || Function->GetReturnProperty())
    {
        return false;
    }

    TArray<UK2Node_FunctionEntry*> Entries;
    Graph->GetNodesOfClass(Entries);
    if (Entries.Num() != 1)
    {
        return false;
    }
    UK2Node_FunctionEntry* Entry = Entries[0];
    TSet<UEdGraphNode*> OldBody;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (const auto* Result = Cast<UK2Node_FunctionResult>(Node))
        {
            for (const UEdGraphPin* Pin : Result->Pins)
            {
                if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                {
                    return false;
                }
            }
        }
        if (Node != Entry)
        {
            OldBody.Add(Node);
        }
    }

    FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
    UK2Node_CallFunction* Call = Creator.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = Entry->NodePosX + 400;
    Call->NodePosY = Entry->NodePosY;
    Creator.Finalize();

    RemoveNodes(*Blueprint, *Graph, OldBody);
    Entry->Modify();
    Entry->LocalVariables.Reset();
    const UEdGraphSchema* Schema = Graph->GetSchema();
    for (UEdGraphPin* Output : Entry->Pins)
    {
        if (Output->Direction == EGPD_Output)
        {
            const FName InputName = Output->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
                ? UEdGraphSchema_K2::PN_Execute
                : Output->PinName;
            UEdGraphPin* Input = Call->FindPin(InputName, EGPD_Input);
            if (!Input || !Schema->TryCreateConnection(Output, Input))
            {
                return false;
            }
        }
    }
    return true;
}

bool USampleBlueprintMigrationLibrary::ReplaceTraversalGeometry(UBlueprint* Blueprint, UEdGraph* Graph)
{
    static const FName FunctionName(TEXT("EvaluateTraversalClearance"));
    static const FName ResultName(TEXT("TraversalCheckResult"));
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || Graph->GetFName() != TEXT("TryTraversalAction"))
    {
        return false;
    }
    TArray<UK2Node_CallFunction*> Calls;
    Graph->GetNodesOfClass(Calls);
    if (Calls.ContainsByPredicate([](const UK2Node_CallFunction* Call)
    {
        return Call->FunctionReference.GetMemberName() == FunctionName;
    }))
    {
        return true;
    }
    const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_FunctionEntry*> Entries;
    Graph->GetNodesOfClass(Entries);
    if (!Function || Entries.Num() != 1)
    {
        return false;
    }
    UK2Node_FunctionEntry* Entry = Entries[0];
    UEdGraphPin* Start = Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    UK2Node_ExecutionSequence* Sequence = Start && Start->LinkedTo.Num() == 1
        ? Cast<UK2Node_ExecutionSequence>(Start->LinkedTo[0]->GetOwningNode()) : nullptr;
    UEdGraphPin* Probe = Sequence ? Sequence->GetThenPinGivenIndex(1) : nullptr;
    UEdGraphPin* Clearance = Sequence ? Sequence->GetThenPinGivenIndex(2) : nullptr;
    UEdGraphPin* Inputs = Entry->FindPin(TEXT("Inputs"), EGPD_Output);
    UEdGraphPin* Debug = Entry->FindPin(TEXT("DebugType"), EGPD_Output);
    if (!Probe || !Clearance || !Inputs || !Debug || !Sequence->GetThenPinGivenIndex(4))
    {
        return false;
    }
    TSet<UEdGraphNode*> Remove = CollectExecutionNodes(*Probe);
    Remove.Append(CollectExecutionNodes(*Clearance));
    int32 ApproachTraces = 0;
    int32 ClearanceTraces = 0;
    UK2Node_FunctionResult* Rejection = nullptr;
    for (UEdGraphNode* Node : Remove)
    {
        if (const auto* Call = Cast<UK2Node_CallFunction>(Node);
            Call && Call->FunctionReference.GetMemberName() == TEXT("CapsuleTraceSingle"))
        {
            const UEdGraphPin* Channel = Call->FindPin(TEXT("TraceChannel"), EGPD_Input);
            ApproachTraces += Channel && Channel->DefaultValue == TEXT("TraceTypeQuery3");
            ClearanceTraces += Channel && Channel->DefaultValue == TEXT("TraceTypeQuery1");
        }
        if (auto* Result = Cast<UK2Node_FunctionResult>(Node))
        {
            const UEdGraphPin* TraversalFailed = Result->FindPin(TEXT("TraversalCheckFailed"), EGPD_Input);
            const UEdGraphPin* MontageFailed = Result->FindPin(TEXT("MontageSelectionFailed"), EGPD_Input);
            if (TraversalFailed && TraversalFailed->DefaultValue == TEXT("true") &&
                MontageFailed && MontageFailed->DefaultValue == TEXT("false"))
            {
                Rejection = Result;
            }
        }
    }
    TArray<UK2Node_VariableGet*> Gets;
    Graph->GetNodesOfClass(Gets);
    UEdGraphPin* LocalResult = nullptr;
    for (UK2Node_VariableGet* Get : Gets)
    {
        if (Get->GetVarName() == ResultName && Get->VariableReference.IsLocalScope())
        {
            UEdGraphPin* Candidate = Get->FindPin(ResultName, EGPD_Output);
            if (Candidate && Candidate->SubPins.IsEmpty())
            {
                LocalResult = Candidate;
                break;
            }
        }
    }
    if (ApproachTraces != 1 || ClearanceTraces != 3 || !LocalResult || !Rejection)
    {
        return false;
    }

    Remove.Remove(Rejection);
    CollectUnusedInputs(Remove, LocalResult->GetOwningNode());
    RemoveNodes(*Blueprint, *Graph, Remove);
    const UEdGraphSchema* Schema = Graph->GetSchema();
    if (!Inputs->SubPins.IsEmpty())
    {
        Schema->RecombinePin(Inputs->SubPins[0]);
    }
    FGraphNodeCreator<UK2Node_CallFunction> CreateCall(*Graph);
    UK2Node_CallFunction* Call = CreateCall.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = Sequence->NodePosX + 350;
    Call->NodePosY = Sequence->NodePosY;
    CreateCall.Finalize();
    FGraphNodeCreator<UK2Node_IfThenElse> CreateBranch(*Graph);
    UK2Node_IfThenElse* Branch = CreateBranch.CreateNode();
    Branch->NodePosX = Call->NodePosX + 420;
    Branch->NodePosY = Call->NodePosY;
    CreateBranch.Finalize();
    return Schema->TryCreateConnection(Probe, Call->GetExecPin()) &&
        Schema->TryCreateConnection(Inputs, Call->FindPin(TEXT("Inputs"), EGPD_Input)) &&
        Schema->TryCreateConnection(Debug, Call->FindPin(TEXT("DebugType"), EGPD_Input)) &&
        Schema->TryCreateConnection(LocalResult, Call->FindPin(ResultName, EGPD_Input)) &&
        Schema->TryCreateConnection(Call->GetThenPin(), Branch->GetExecPin()) &&
        Schema->TryCreateConnection(Call->GetReturnValuePin(), Branch->GetConditionPin()) &&
        Schema->TryCreateConnection(Branch->GetElsePin(), Rejection->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input));
}

bool USampleBlueprintMigrationLibrary::ForwardTraversalCheckInputs(UBlueprint* Blueprint, UEdGraph* Graph, UClass* Library)
{
    if (!Blueprint || !Graph || Graph->GetOuter() != Blueprint || Graph->GetFName() != TEXT("GetTraversalCheckInputs"))
    {
        return false;
    }
    const FName NativeName(TEXT("BuildImportedTraversalInputs"));
    const UFunction* Native = Library ? Library->FindFunctionByName(NativeName) : nullptr;
    TArray<UK2Node_FunctionEntry*> Entries;
    TArray<UK2Node_FunctionResult*> Results;
    Graph->GetNodesOfClass(Entries);
    Graph->GetNodesOfClass(Results);
    if (!Native || Entries.Num() != 1 || Results.IsEmpty() || Results.Num() > 2)
    {
        return false;
    }
    auto* Entry = Entries[0];
    auto* Result = Results[0];
    UEdGraphPin* Output = Result->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Input);
    UEdGraphPin* Exec = Result->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
    UEdGraphPin* Then = Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    if (!Output || !Exec || !Then || Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct ||
        !Output->PinType.PinSubCategoryObject.IsValid() ||
        Output->PinType.PinSubCategoryObject->GetFName() != TEXT("S_TraversalCheckInputs"))
    {
        return false;
    }
    if (const auto* Call = FindUniqueCall(*Graph, NativeName))
    {
        return Results.Num() == 1 && GetOnlyPeer(Then) == Call &&
            GetOnlyPeer(Exec) == Call && GetOnlyPeer(Output) == Call;
    }
    if (Results.Num() != 2 || Output->SubPins.Num() != 6)
    {
        return false;
    }

    TSet<UEdGraphNode*> OldBody;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node != Entry && Node != Result)
        {
            OldBody.Add(Node);
        }
    }
    RemoveNodes(*Blueprint, *Graph, OldBody);
    const UEdGraphSchema* Schema = Graph->GetSchema();
    Schema->RecombinePin(Output->SubPins[0]);

    FGraphNodeCreator<UK2Node_CallFunction> CreateCall(*Graph);
    auto* Call = CreateCall.CreateNode();
    Call->SetFromFunction(Native);
    Call->NodePosX = Entry->NodePosX + 340;
    Call->NodePosY = Entry->NodePosY;
    CreateCall.Finalize();
    FGraphNodeCreator<UK2Node_Self> CreateSelf(*Graph);
    auto* Self = CreateSelf.CreateNode();
    Self->NodePosX = Entry->NodePosX + 100;
    Self->NodePosY = Entry->NodePosY + 120;
    CreateSelf.Finalize();
    Result->NodePosX = Call->NodePosX + 380;
    Result->NodePosY = Entry->NodePosY;
    return Schema->TryCreateConnection(Then, Call->GetExecPin()) &&
        Schema->TryCreateConnection(Self->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Output), Call->FindPin(TEXT("Pawn"), EGPD_Input)) &&
        Schema->TryCreateConnection(Call->FindPin(TEXT("Inputs"), EGPD_Output), Output) &&
        Schema->TryCreateConnection(Call->GetThenPin(), Exec);
}

bool USampleBlueprintMigrationLibrary::ReplaceTraversalCollision(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    const UFunction* Begin = Blueprint->ParentClass->FindFunctionByName(TEXT("BeginTraversalCollision"));
    const UFunction* End = Blueprint->ParentClass->FindFunctionByName(TEXT("EndTraversalCollision"));
    const UFunction* Activity = Blueprint->ParentClass->FindFunctionByName(TEXT("SetTraversalActionActive"));
    if (!Begin || !End || !Activity)
    {
        return false;
    }

    TArray<UK2Node_CallFunction*> Calls;
    Graph->GetNodesOfClass(Calls);
    TArray<FTraversalCallReplacement> Replacements;
    int32 ExistingBegin = 0;
    int32 ExistingEnd = 0;
    int32 ExistingActivity = 0;
    int32 MeshChanges = 0;
    for (UK2Node_CallFunction* Call : Calls)
    {
        const FName Name = Call->FunctionReference.GetMemberName();
        ExistingBegin += Name == Begin->GetFName();
        ExistingEnd += Name == End->GetFName();
        ExistingActivity += Name == Activity->GetFName();
        if (Name != TEXT("IgnoreComponentWhenMoving"))
        {
            continue;
        }
        UEdGraphPin* Ignore = Call->FindPin(TEXT("bShouldIgnore"), EGPD_Input);
        UEdGraphPin* Capsule = Call->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
        UEdGraphPin* Obstacle = Call->FindPin(TEXT("Component"), EGPD_Input);
        if (!Ignore || !Ignore->LinkedTo.IsEmpty() || !Capsule || Capsule->LinkedTo.Num() != 1 ||
            !Obstacle || Obstacle->LinkedTo.Num() != 1 || Call->GetThenPin()->LinkedTo.Num() != 1)
        {
            return false;
        }
        const bool bStart = Ignore->DefaultValue == TEXT("true");
        auto* Next = Cast<UK2Node_CallFunction>(Call->GetThenPin()->LinkedTo[0]->GetOwningNode());
        const bool bChangesMesh = Next && Next->FunctionReference.GetMemberName() == TEXT("SetCollisionResponseToChannel");
        UEdGraphPin* Mesh = bChangesMesh ? Next->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input) : nullptr;
        if (bChangesMesh && (!Mesh || Mesh->LinkedTo.Num() != 1))
        {
            return false;
        }
        MeshChanges += bChangesMesh;
        FTraversalCallReplacement& Replacement = Replacements.Add_GetRef(
            {Call, bChangesMesh ? Next : Call, bStart ? Begin : End, {}});
        if (bStart)
        {
            Replacement.Inputs.Emplace(TEXT("Capsule"), Capsule);
            Replacement.Inputs.Emplace(TEXT("Obstacle"), Obstacle);
            if (Mesh)
            {
                Replacement.Inputs.Emplace(TEXT("Mesh"), Mesh);
            }
        }
    }

    TArray<UK2Node_VariableSet*> Setters;
    Graph->GetNodesOfClass(Setters);
    int32 ActivityChanges = 0;
    for (UK2Node_VariableSet* Set : Setters)
    {
        if (Set->GetVarName() == TEXT("DoingTraversalAction"))
        {
            UEdGraphPin* Active = Set->FindPin(TEXT("DoingTraversalAction"), EGPD_Input);
            if (!Active || !Active->LinkedTo.IsEmpty())
            {
                return false;
            }
            Replacements.Add({Set, Set, Activity, {{TEXT("bActive"), Active}}});
            ++ActivityChanges;
        }
    }
    if (Replacements.IsEmpty())
    {
        return ExistingBegin == 2 && ExistingEnd == 2 && ExistingActivity == 4;
    }
    if (Replacements.Num() != 8 || MeshChanges != 2 || ActivityChanges != 4 ||
        ExistingBegin || ExistingEnd || ExistingActivity)
    {
        return false;
    }
    for (const FTraversalCallReplacement& Replacement : Replacements)
    {
        if (!InstallTraversalCall(*Blueprint, *Graph, Replacement))
        {
            return false;
        }
    }
    return true;
}

bool USampleBlueprintMigrationLibrary::ReplaceTraversalModes(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    const UClass* Parent = Blueprint->ParentClass;
    const UFunction* BeginCmc = Parent->FindFunctionByName(TEXT("BeginCmcTraversalMode"));
    const UFunction* FinishCmc = Parent->FindFunctionByName(TEXT("FinishCmcTraversalMode"));
    const UFunction* BeginMover = Parent->FindFunctionByName(TEXT("BeginMoverTraversalMode"));
    const UFunction* FinishMover = Parent->FindFunctionByName(TEXT("FinishMoverTraversalMode"));
    if (!BeginCmc || !FinishCmc || !BeginMover || !FinishMover)
    {
        return false;
    }

    TArray<UK2Node_CallFunction*> Calls;
    Graph->GetNodesOfClass(Calls);
    const auto Count = [&](FName Name)
    {
        return Calls.FilterByPredicate([&](const UK2Node_CallFunction* Call)
        {
            return Call->FunctionReference.GetMemberName() == Name;
        }).Num();
    };
    if (Count(TEXT("BeginTraversalCollision")) != 2 || Count(TEXT("EndTraversalCollision")) != 2)
    {
        return false;
    }
    if (Count(BeginCmc->GetFName()) == 1 && Count(FinishCmc->GetFName()) == 1 &&
        Count(BeginMover->GetFName()) == 1 && Count(FinishMover->GetFName()) == 1)
    {
        return true;
    }
    if (Count(TEXT("SetMovementMode_CMC")) != 2 || Count(TEXT("QueueNextMode")) != 2 ||
        Count(TEXT("RetriggerableDelay")) != 1)
    {
        return false;
    }

    TArray<FTraversalCallReplacement> Replacements;
    for (UK2Node_CallFunction* Call : Calls)
    {
        const FName Name = Call->FunctionReference.GetMemberName();
        if (Name == TEXT("SetMovementMode_CMC"))
        {
            if (UK2Node_CallFunction* Delay = GetNextCall(*Call, TEXT("RetriggerableDelay")))
            {
                auto* Correction = GetNextCall(*Delay, TEXT("SetReplicationBehavior"));
                if (!Correction)
                {
                    return false;
                }
                Replacements.Add({Call, Correction, FinishCmc, {}, {Delay}});
            }
            else if (UK2Node_CallFunction* Correction = GetNextCall(*Call, TEXT("SetReplicationBehavior")))
            {
                Replacements.Add({Call, Correction, BeginCmc, {}});
            }
        }
        else if (Name == TEXT("QueueNextMode"))
        {
            const UEdGraphPin* Mode = Call->FindPin(TEXT("DesiredModeName"), EGPD_Input);
            UEdGraphPin* Mover = Call->FindPin(UEdGraphSchema_K2::PN_Self, EGPD_Input);
            if (!Mode || !Mode->LinkedTo.IsEmpty() || !Mover || Mover->LinkedTo.Num() != 1)
            {
                return false;
            }
            if (Mode->DefaultValue == TEXT("Traversing"))
            {
                Replacements.Add({Call, Call, BeginMover, {{TEXT("Mover"), Mover}}});
            }
            else if (Mode->DefaultValue == TEXT("Walking"))
            {
                auto* Gate = Cast<UK2Node_IfThenElse>(GetOnlyPeer(Call->GetExecPin()));
                UEdGraphPin* Then = Gate ? Gate->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
                if (!Gate || GetOnlyPeer(Then) != Call || !Gate->GetElsePin()->LinkedTo.IsEmpty())
                {
                    return false;
                }
                Replacements.Add({Gate, Call, FinishMover, {}});
            }
        }
    }
    for (const UFunction* Function : {BeginCmc, FinishCmc, BeginMover, FinishMover})
    {
        if (Replacements.FilterByPredicate([&](const FTraversalCallReplacement& Item)
        {
            return Item.Function == Function;
        }).Num() != 1)
        {
            return false;
        }
    }
    for (const FTraversalCallReplacement& Replacement : Replacements)
    {
        if (!InstallTraversalCall(*Blueprint, *Graph, Replacement))
        {
            return false;
        }
    }
    return true;
}

bool USampleBlueprintMigrationLibrary::ReplaceTraversalReplay(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    static const FName NativeName(TEXT("ApplyTraversalReplay"));
    static const FName ResultName(TEXT("TraversalResult"));
    const UFunction* NativeReplay = Blueprint->ParentClass->FindFunctionByName(NativeName);
    TArray<UK2Node_CustomEvent*> Events;
    Graph->GetNodesOfClass(Events);
    const auto* Match = Events.FindByPredicate([](const UK2Node_CustomEvent* Event)
    {
        return Event->CustomFunctionName == TEXT("PerformTraversalAction_Clients");
    });
    UEdGraphPin* EventExec = Match ? (*Match)->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
    UEdGraphNode* Body = GetOnlyPeer(EventExec);
    if (!NativeReplay || !Body)
    {
        return false;
    }
    if (const auto* Call = Cast<UK2Node_CallFunction>(Body);
        Call && Call->FunctionReference.GetMemberName() == NativeName)
    {
        return true;
    }
    const auto* CastOwner = Cast<UK2Node_DynamicCast>(Body);
    UEdGraphPin* Then = CastOwner ? CastOwner->GetValidCastPin() : nullptr;
    auto* Store = Cast<UK2Node_VariableSet>(CastOwner ? GetOnlyPeer(Then) : Body);
    auto* Playback = Store ? Cast<UK2Node_CallFunction>(
        GetOnlyPeer(Store->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output))) : nullptr;
    UEdGraphPin* Payload = Store ? Store->FindPin(ResultName, EGPD_Input) : nullptr;
    if (!Store || Store->GetVarName() != ResultName || !Payload || Payload->LinkedTo.Num() != 1 ||
        !Playback || Playback->FunctionReference.GetMemberName() != TEXT("PerformTraversalAction") ||
        (CastOwner && !CastOwner->GetInvalidCastPin()->LinkedTo.IsEmpty()))
    {
        return false;
    }
    FTraversalCallReplacement Replacement{Body, Playback, NativeReplay, {{ResultName, Payload}}};
    Replacement.AdditionalNodes.Add(Store);
    return InstallTraversalCall(*Blueprint, *Graph, Replacement);
}

bool USampleBlueprintMigrationLibrary::ForwardTraversalActionDispatch(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("TryTraversalAction"))
    {
        return false;
    }
    const FName Name(TEXT("PublishTraversalAction"));
    const FName Result(TEXT("TraversalResult"));
    if (FindUniqueCall(*Graph, Name))
    {
        return true;
    }
    const auto* Native = Blueprint->ParentClass->FindFunctionByName(Name);
    auto* Play = FindUniqueCall(*Graph, TEXT("PerformTraversalAction"));
    auto* Server = Play ? GetNextCall(*Play, TEXT("PerformTraversalAction_Server")) : nullptr;
    auto* Store = Cast<UK2Node_VariableSet>(GetOnlyPeer(Play ? Play->GetExecPin() : nullptr));
    auto* Input = Store ? Store->FindPin(Result, EGPD_Input) : nullptr;
    const auto* ServerInput = Server ? Server->FindPin(Result, EGPD_Input) : nullptr;
    const auto* StoredResult = Cast<UK2Node_VariableGet>(GetOnlyPeer(ServerInput));
    if (!Native || !Server || !Store || Store->GetVarName() != Result || !Input || Input->LinkedTo.Num() != 1 ||
        !StoredResult || StoredResult->GetVarName() != Result ||
        !Cast<UK2Node_FunctionResult>(GetOnlyPeer(Server->GetThenPin())))
    {
        return false;
    }
    return InstallTraversalCall(*Blueprint, *Graph, {Store, Server, Native, {{Result, Input}}, {Play}});
}

bool USampleBlueprintMigrationLibrary::ReplaceEnumQuery(UBlueprint* Blueprint, UEdGraph* Graph, FName FunctionName)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint)
    {
        return false;
    }
    const UFunction* Native = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_FunctionEntry*> Entries;
    TArray<UK2Node_FunctionResult*> Results;
    Graph->GetNodesOfClass(Entries);
    Graph->GetNodesOfClass(Results);
    if (!Native || !Native->HasAllFunctionFlags(FUNC_Native | FUNC_BlueprintPure) || Native->NumParms != 1 ||
        !CastField<FByteProperty>(Native->GetReturnProperty()) || Entries.Num() != 1 || Results.IsEmpty())
    {
        return false;
    }
    UK2Node_FunctionEntry* Entry = Entries[0];
    UK2Node_FunctionResult* Result = Results[0];
    UEdGraphPin* Output = Result->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Input);
    UEnum* Definition = Output ? Cast<UEnum>(Output->PinType.PinSubCategoryObject.Get()) : nullptr;
    if (!Definition || Output->PinType.PinCategory != UEdGraphSchema_K2::PC_Byte)
    {
        return false;
    }

    TSet<UEdGraphNode*> OldBody;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node != Entry && Node != Result)
        {
            OldBody.Add(Node);
        }
    }
    RemoveNodes(*Blueprint, *Graph, OldBody);

    FGraphNodeCreator<UK2Node_CallFunction> CreateCall(*Graph);
    UK2Node_CallFunction* Call = CreateCall.CreateNode();
    Call->SetFromFunction(Native);
    Call->NodePosX = Entry->NodePosX + 240;
    Call->NodePosY = Entry->NodePosY + 100;
    CreateCall.Finalize();

    FGraphNodeCreator<UK2Node_CastByteToEnum> CreateConversion(*Graph);
    UK2Node_CastByteToEnum* Convert = CreateConversion.CreateNode();
    Convert->Enum = Definition;
    Convert->bSafe = true;
    Convert->NodePosX = Call->NodePosX + 310;
    Convert->NodePosY = Call->NodePosY;
    CreateConversion.Finalize();
    Result->NodePosX = Convert->NodePosX + 230;
    Result->NodePosY = Entry->NodePosY;

    const UEdGraphSchema* Schema = Graph->GetSchema();
    UEdGraphPin* Then = Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    UEdGraphPin* Exec = Result->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input);
    return (!Then || !Exec || Schema->TryCreateConnection(Then, Exec)) &&
        Schema->TryCreateConnection(Call->GetReturnValuePin(), Convert->FindPin(TEXT("Byte"), EGPD_Input)) &&
        Schema->TryCreateConnection(Convert->FindPin(UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Output);
}

bool USampleBlueprintMigrationLibrary::ReplaceMoverInputFunction(
    UBlueprint* Blueprint, UEdGraph* Graph, FName FunctionName, FName OutputName)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint)
    {
        return false;
    }
    const UFunction* Native = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    TArray<UK2Node_FunctionEntry*> Entries;
    TArray<UK2Node_FunctionResult*> Results;
    Graph->GetNodesOfClass(Entries);
    Graph->GetNodesOfClass(Results);
    if (!Native || Entries.Num() != 1 || (!OutputName.IsNone() && Results.IsEmpty()))
    {
        return false;
    }
    auto* Entry = Entries[0];
    auto* Result = Results.IsEmpty() ? nullptr : Results[0];
    UEdGraphPin* Output = Result ? Result->FindPin(OutputName, EGPD_Input) : nullptr;
    UEdGraphPin* Then = Entry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output);
    UEdGraphPin* Exec = Result ? Result->FindPin(UEdGraphSchema_K2::PN_Execute, EGPD_Input) : nullptr;
    if ((!OutputName.IsNone() && !Output) || (Native->GetReturnProperty() != nullptr) != (Output != nullptr))
    {
        return false;
    }
    TSet<UEdGraphNode*> OldBody;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Node != Entry && Node != Result)
        {
            OldBody.Add(Node);
        }
    }
    if (OldBody.Num() == 1)
    {
        const auto* Forward = Cast<UK2Node_CallFunction>(*OldBody.CreateConstIterator());
        if (Forward && Forward->FunctionReference.GetMemberName() == FunctionName)
        {
            return true;
        }
    }

    RemoveNodes(*Blueprint, *Graph, OldBody);
    Entry->Modify();
    Entry->LocalVariables.Reset();
    FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
    UK2Node_CallFunction* Call = Creator.CreateNode();
    Call->SetFromFunction(Native);
    Call->NodePosX = Entry->NodePosX + 350;
    Call->NodePosY = Entry->NodePosY;
    Creator.Finalize();
    if (Result)
    {
        Result->NodePosX = Call->NodePosX + 400;
        Result->NodePosY = Entry->NodePosY;
    }
    const UEdGraphSchema* Schema = Graph->GetSchema();
    for (UEdGraphPin* Pin : Entry->Pins)
    {
        if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
        {
            UEdGraphPin* Input = Call->FindPin(Pin->PinName, EGPD_Input);
            if (!Input)
            {
                const FString NativeName = Pin->PinName.ToString().Replace(TEXT(" "), TEXT(""));
                Input = Call->FindPin(FName(*NativeName), EGPD_Input);
            }
            if (!Input || !Input->LinkedTo.IsEmpty() || !Schema->TryCreateConnection(Pin, Input))
            {
                return false;
            }
        }
    }
    if (Output && !Schema->TryCreateConnection(Call->GetReturnValuePin(), Output))
    {
        return false;
    }
    if (Result && OutputName.IsNone())
    {
        for (UEdGraphPin* Pin : Result->Pins)
        {
            if (Pin->Direction == EGPD_Input && Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                UEdGraphPin* Source = Call->FindPin(Pin->PinName, EGPD_Output);
                if (!Source || !Schema->TryCreateConnection(Source, Pin))
                {
                    return false;
                }
            }
        }
    }
    if (Call->IsNodePure())
    {
        return !Then || !Exec || Schema->TryCreateConnection(Then, Exec);
    }
    return Then && Schema->TryCreateConnection(Then, Call->GetExecPin()) &&
        (!Exec || Schema->TryCreateConnection(Call->GetThenPin(), Exec));
}

bool USampleBlueprintMigrationLibrary::ReplaceRagdollLifecycle(
    UBlueprint* Blueprint, UEdGraph* EventGraph, UEdGraph* ModeGraph)
{
    if (!Blueprint || !Blueprint->ParentClass || !EventGraph || !ModeGraph ||
        EventGraph->GetOuter() != Blueprint || ModeGraph->GetOuter() != Blueprint)
    {
        return false;
    }

    const auto InstallTail = [&](FName Native, UK2Node_CallFunction* First)
    {
        if (FindUniqueCall(*EventGraph, Native))
        {
            return true;
        }
        const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(Native);
        return First && Function && InstallExecutionTail(*Blueprint, *EventGraph, First->GetExecPin(), *Function);
    };
    TArray<UK2Node_CustomEvent*> Events;
    EventGraph->GetNodesOfClass(Events);
    const auto* Exit = Events.FindByPredicate([](const UK2Node_CustomEvent* Event)
    {
        return Event->CustomFunctionName == TEXT("ExitRagdoll");
    });
    auto* ExitQueue = Cast<UK2Node_CallFunction>(Exit ? GetOnlyPeer(
        (*Exit)->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output)) : nullptr);
    if (!InstallTail(TEXT("BeginRagdollPhysics"), FindUniqueCall(*EventGraph, TEXT("SetSimulatePhysics"))) ||
        !InstallTail(TEXT("FinishRagdollPhysicsRecovery"), FindUniqueCall(*EventGraph, TEXT("SetControlDatasInSet"))) ||
        !InstallTail(TEXT("RequestRagdollRecovery"), ExitQueue && ExitQueue->FunctionReference.GetMemberName() == TEXT("QueueNextMode")
            ? ExitQueue : nullptr))
    {
        return false;
    }

    const auto InstallCollision = [&](UEdGraph& Graph, FName Native, bool bEnter)
    {
        if (FindUniqueCall(Graph, Native))
        {
            return true;
        }
        const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(Native);
        UK2Node_CallFunction* First = FindUniqueCall(Graph, TEXT("SetCollisionProfileName"));
        auto* Last = bEnter && First ? GetNextCall(*First, TEXT("SetCollisionResponseToChannel")) : First;
        return Function && First && Last && InstallTraversalCall(*Blueprint, Graph, {First, Last, Function, {}});
    };
    if (!InstallCollision(*ModeGraph, TEXT("ApplyRagdollCapsuleCollision"), true) ||
        !InstallCollision(*EventGraph, TEXT("RestoreRagdollCapsuleCollision"), false))
    {
        return false;
    }

    static const FName OldCheck(TEXT("Entered Ragdoll Successfully Check"));
    return !Events.ContainsByPredicate([](const UK2Node_CustomEvent* Event)
        {
            return Event->CustomFunctionName == OldCheck;
        }) || RemoveCustomEvent(Blueprint, EventGraph, OldCheck);
}

bool USampleBlueprintMigrationLibrary::ReplacePlayerSmartObjectInput(UBlueprint* Blueprint, UEdGraph* EventGraph)
{
    if (!Blueprint || !Blueprint->ParentClass || !EventGraph || EventGraph->GetOuter() != Blueprint)
    {
        return false;
    }

    const FName FunctionName(TEXT("TryStartPlayerInteraction"));
    const UFunction* Function = Blueprint->ParentClass->FindFunctionByName(FunctionName);
    if (FindUniqueCall(*EventGraph, FunctionName))
    {
        return true;
    }
    TArray<UK2Node_Event*> Events;
    EventGraph->GetNodesOfClass(Events);
    if (Events.ContainsByPredicate([](const UK2Node_Event* Event)
    {
        return Event->EventReference.GetMemberName() == TEXT("OpenPlayerInteractionGate");
    }))
    {
        return true;
    }
    TArray<UK2Node_EnhancedInputAction*> Actions;
    EventGraph->GetNodesOfClass(Actions);
    auto* Action = Actions.FindByPredicate([](const UK2Node_EnhancedInputAction* Candidate)
    {
        return Candidate->InputAction && Candidate->InputAction->GetPathName() == TEXT("/Game/Input/IA_Interact.IA_Interact");
    });
    UEdGraphPin* Trigger = Action ? (*Action)->FindPin(TEXT("Triggered"), EGPD_Output) : nullptr;
    if (!Function || !GetOnlyPeer(Trigger))
    {
        UE_LOG(LogTemp, Warning, TEXT("Player interaction input or native request function is missing."));
        return false;
    }

    UEdGraphPin* GateOpen = nullptr;
    for (UEdGraphNode* Node : CollectExecutionNodes(*Trigger))
    {
        const auto* Flag = Cast<UK2Node_VariableSet>(Node);
        const UEdGraphPin* Then = Flag ? Flag->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
        if (Flag && Flag->GetVarName() == TEXT("PlayerIsInteracting") && Then && Then->LinkedTo.Num() == 1)
        {
            UEdGraphPin* GateInput = Then->LinkedTo[0];
            const UEdGraphPin* Endpoint = GateInput;
            TSet<const UEdGraphNode*> Reroutes;
            while (const auto* Reroute = Cast<UK2Node_Knot>(Endpoint->GetOwningNode()))
            {
                if (Reroutes.Contains(Reroute) || Reroute->GetOutputPin()->LinkedTo.Num() != 1)
                {
                    return false;
                }
                Reroutes.Add(Reroute);
                Endpoint = Reroute->GetOutputPin()->LinkedTo[0];
            }
            if (Endpoint->PinName != TEXT("Open"))
            {
                continue;
            }
            if (GateOpen)
            {
                return false;
            }
            GateOpen = GateInput;
        }
    }
    if (!GateOpen)
    {
        UE_LOG(LogTemp, Warning, TEXT("Player interaction did not lead to the playback cancellation gate."));
        return false;
    }

    TSet<UEdGraphNode*> Remove = CollectExecutionNodes(*Trigger, GateOpen->GetOwningNode());
    CollectUnusedInputs(Remove, *Action);
    for (const UEdGraphNode* Node : Remove)
    {
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            const bool bExec = Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
            if (bExec || Pin->Direction == EGPD_Output)
            {
                for (const UEdGraphPin* Peer : Pin->LinkedTo)
                {
                    if (Peer != Trigger && Peer != GateOpen && !Remove.Contains(Peer->GetOwningNode()))
                    {
                        UE_LOG(LogTemp, Warning, TEXT("Player interaction shares pin %s.%s with %s.%s."),
                            *Node->GetName(), *Pin->PinName.ToString(), *Peer->GetOwningNode()->GetName(), *Peer->PinName.ToString());
                        return false;
                    }
                }
            }
        }
    }

    const UEdGraphNode* First = GetOnlyPeer(Trigger);
    FGraphNodeCreator<UK2Node_CallFunction> CreateCall(*EventGraph);
    UK2Node_CallFunction* Call = CreateCall.CreateNode();
    Call->SetFromFunction(Function);
    Call->NodePosX = First->NodePosX;
    Call->NodePosY = First->NodePosY;
    CreateCall.Finalize();
    FGraphNodeCreator<UK2Node_IfThenElse> CreateBranch(*EventGraph);
    UK2Node_IfThenElse* Branch = CreateBranch.CreateNode();
    Branch->NodePosX = Call->NodePosX + 350;
    Branch->NodePosY = Call->NodePosY;
    CreateBranch.Finalize();

    RemoveNodes(*Blueprint, *EventGraph, Remove);
    const UEdGraphSchema* Schema = EventGraph->GetSchema();
    return Schema->TryCreateConnection(Trigger, Call->GetExecPin()) &&
        Schema->TryCreateConnection(Call->GetThenPin(), Branch->FindPin(UEdGraphSchema_K2::PN_Execute)) &&
        Schema->TryCreateConnection(Call->GetReturnValuePin(), Branch->FindPin(UEdGraphSchema_K2::PN_Condition)) &&
        Schema->TryCreateConnection(Branch->FindPin(UEdGraphSchema_K2::PN_Then), GateOpen);
}

bool USampleBlueprintMigrationLibrary::ReplacePlayerInteractionInputBindings(UBlueprint* Blueprint, UEdGraph* Graph)
{
    if (!Blueprint || !Blueprint->ParentClass || !Graph || Graph->GetOuter() != Blueprint ||
        Graph->GetFName() != TEXT("EventGraph"))
    {
        return false;
    }
    static const FName OpenName(TEXT("OpenPlayerInteractionGate"));
    static const FName CancelName(TEXT("RequestPlayerInteractionCancellation"));
    TArray<UK2Node_Event*> Events;
    Graph->GetNodesOfClass(Events);
    if (Events.ContainsByPredicate([](const UK2Node_Event* Event) { return Event->GetFunctionName() == OpenName; }) &&
        Events.ContainsByPredicate([](const UK2Node_Event* Event) { return Event->GetFunctionName() == CancelName; }))
    {
        return ListEnhancedInputActions(Graph).IsEmpty();
    }

    TArray<UK2Node_EnhancedInputAction*> Actions;
    Graph->GetNodesOfClass(Actions);
    const auto FindTrigger = [&Actions](const TCHAR* Path) -> UEdGraphPin*
    {
        for (UK2Node_EnhancedInputAction* Action : Actions)
        {
            if (!Action->InputAction || Action->InputAction->GetPathName() != Path)
            {
                continue;
            }
            UEdGraphPin* Trigger = Action->FindPin(TEXT("Triggered"), EGPD_Output);
            if (GetOnlyPeer(Trigger) && !Action->Pins.ContainsByPredicate([Trigger](const UEdGraphPin* Pin)
            {
                return Pin != Trigger && !Pin->LinkedTo.IsEmpty();
            }))
            {
                return Trigger;
            }
        }
        return nullptr;
    };
    UEdGraphPin* Interact = FindTrigger(TEXT("/Game/Input/IA_Interact.IA_Interact"));
    UEdGraphPin* Move = FindTrigger(TEXT("/Game/Input/IA_Move.IA_Move"));
    auto* Start = Cast<UK2Node_CallFunction>(GetOnlyPeer(Interact));
    auto* Accepted = Cast<UK2Node_IfThenElse>(Start ? GetOnlyPeer(Start->GetThenPin()) : nullptr);
    UEdGraphPin* Open = Accepted ? Accepted->GetThenPin() : nullptr;
    const UFunction* OpenEvent = Blueprint->ParentClass->FindFunctionByName(OpenName);
    const UFunction* CancelEvent = Blueprint->ParentClass->FindFunctionByName(CancelName);
    if (Actions.Num() != 2 || !Move || !Start || Start->FunctionReference.GetMemberName() != TEXT("TryStartPlayerInteraction") ||
        !Accepted || Start->GetReturnValuePin()->LinkedTo.Num() != 1 ||
        Start->GetReturnValuePin()->LinkedTo[0] != Accepted->GetConditionPin() ||
        !Accepted->GetElsePin()->LinkedTo.IsEmpty() || !GetOnlyPeer(Open) || !OpenEvent || !CancelEvent)
    {
        return false;
    }

    const auto Connect = [Graph](UEdGraphPin* OldPin, const UFunction& Event) -> bool
    {
        UEdGraphPin* Next = OldPin->LinkedTo[0];
        FGraphNodeCreator<UK2Node_Event> Creator(*Graph);
        UK2Node_Event* Callback = Creator.CreateNode();
        Callback->EventReference.SetExternalMember(Event.GetFName(), Event.GetOwnerClass());
        Callback->bOverrideFunction = true;
        Callback->NodePosX = OldPin->GetOwningNode()->NodePosX;
        Callback->NodePosY = OldPin->GetOwningNode()->NodePosY;
        Creator.Finalize();
        OldPin->BreakLinkTo(Next);
        return Graph->GetSchema()->TryCreateConnection(Callback->FindPin(UEdGraphSchema_K2::PN_Then), Next);
    };
    if (!Connect(Open, *OpenEvent) || !Connect(Move, *CancelEvent))
    {
        return false;
    }
    RemoveNodes(*Blueprint, *Graph, {Interact->GetOwningNode(), Move->GetOwningNode(), Start, Accepted});
    return true;
}
