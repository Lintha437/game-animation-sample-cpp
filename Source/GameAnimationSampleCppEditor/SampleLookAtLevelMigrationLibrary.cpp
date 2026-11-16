#include "SampleLookAtLevelMigrationLibrary.h"

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/LevelScriptBlueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
    constexpr TCHAR LevelScriptPath[] =
        TEXT("/Game/Levels/LookAtPOI_Level.LookAtPOI_Level:PersistentLevel.LookAtPOI_Level");

    struct FCommandChain
    {
        UK2Node_Event* Event = nullptr;
        UK2Node_CallFunction* First = nullptr;
        UK2Node_CallFunction* Second = nullptr;
        UK2Node_CallFunction* Player = nullptr;
    };

    struct FLevelCommandAnalysis
    {
        UEdGraph* Graph = nullptr;
        UK2Node_Event* Begin = nullptr;
        UK2Node_Event* Destroyed = nullptr;
        const UFunction* NativeFunction = nullptr;
        FCommandChain Enable;
        FCommandChain Disable;
    };

    UEdGraphPin* OnlyPeer(const UEdGraphPin* Pin)
    {
        return Pin && Pin->LinkedTo.Num() == 1 ? Pin->LinkedTo[0] : nullptr;
    }

    UK2Node_Event* FindEvent(const TArray<UK2Node_Event*>& Events, const FName Name)
    {
        UK2Node_Event* Result = nullptr;
        for (UK2Node_Event* Event : Events)
        {
            if (Event->EventReference.GetMemberName() == Name)
            {
                if (Result)
                {
                    return nullptr;
                }
                Result = Event;
            }
        }
        return Result;
    }

    UEdGraphPin* EventThen(UK2Node_Event* Event)
    {
        return Event ? Event->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output) : nullptr;
    }

    bool MatchesChain(UK2Node_Event* Event, const FString& FirstText,
        const FString& SecondText, const UFunction* CommandFunction,
        const UFunction* PlayerFunction, FCommandChain& Out)
    {
        UEdGraphPin* EventExec = EventThen(Event);
        UEdGraphPin* FirstExec = OnlyPeer(EventExec);
        auto* First = FirstExec ? Cast<UK2Node_CallFunction>(FirstExec->GetOwningNode()) : nullptr;
        auto* Second = First && OnlyPeer(First->GetThenPin())
            ? Cast<UK2Node_CallFunction>(OnlyPeer(First->GetThenPin())->GetOwningNode()) : nullptr;
        if (!First || !Second || First == Second || First->GetTargetFunction() != CommandFunction ||
            Second->GetTargetFunction() != CommandFunction ||
            OnlyPeer(First->GetExecPin()) != EventExec ||
            OnlyPeer(Second->GetExecPin()) != First->GetThenPin() ||
            !Second->GetThenPin() || !Second->GetThenPin()->LinkedTo.IsEmpty())
        {
            return false;
        }

        UEdGraphPin* FirstCommand = First->FindPin(TEXT("Command"), EGPD_Input);
        UEdGraphPin* SecondCommand = Second->FindPin(TEXT("Command"), EGPD_Input);
        UEdGraphPin* FirstPlayer = First->FindPin(TEXT("SpecificPlayer"), EGPD_Input);
        UEdGraphPin* SecondPlayer = Second->FindPin(TEXT("SpecificPlayer"), EGPD_Input);
        if (!FirstCommand || !SecondCommand || !FirstPlayer || !SecondPlayer ||
            !FirstCommand->LinkedTo.IsEmpty() || !SecondCommand->LinkedTo.IsEmpty() ||
            FirstCommand->DefaultValue != FirstText || SecondCommand->DefaultValue != SecondText)
        {
            return false;
        }

        UEdGraphPin* Source = OnlyPeer(FirstPlayer);
        auto* Player = Source ? Cast<UK2Node_CallFunction>(Source->GetOwningNode()) : nullptr;
        UEdGraphPin* PlayerOutput = Player ? Player->FindPin(TEXT("ReturnValue"), EGPD_Output) : nullptr;
        UEdGraphPin* PlayerIndex = Player ? Player->FindPin(TEXT("PlayerIndex"), EGPD_Input) : nullptr;
        if (!Player || Player->GetTargetFunction() != PlayerFunction || Source != PlayerOutput ||
            OnlyPeer(SecondPlayer) != PlayerOutput || !PlayerIndex ||
            !PlayerIndex->LinkedTo.IsEmpty() || PlayerIndex->DefaultValue != TEXT("0") ||
            PlayerOutput->LinkedTo.Num() != 2 ||
            !PlayerOutput->LinkedTo.Contains(FirstPlayer) || !PlayerOutput->LinkedTo.Contains(SecondPlayer))
        {
            return false;
        }

        Out = {Event, First, Second, Player};
        return true;
    }

    bool MatchesNativeCall(UK2Node_Event* Event, const UFunction* NativeFunction, bool bEnabled,
        UK2Node_CallFunction*& OutCall)
    {
        UEdGraphPin* EventExec = EventThen(Event);
        UEdGraphPin* Peer = OnlyPeer(EventExec);
        auto* Call = Peer ? Cast<UK2Node_CallFunction>(Peer->GetOwningNode()) : nullptr;
        UEdGraphPin* Enabled = Call ? Call->FindPin(TEXT("bEnabled"), EGPD_Input) : nullptr;
        if (!Call || Call->GetTargetFunction() != NativeFunction ||
            OnlyPeer(Call->GetExecPin()) != EventExec ||
            !Call->GetThenPin() || !Call->GetThenPin()->LinkedTo.IsEmpty() || !Enabled ||
            !Enabled->LinkedTo.IsEmpty() ||
            Enabled->DefaultValue != (bEnabled ? TEXT("true") : TEXT("false")))
        {
            return false;
        }

        OutCall = Call;
        return true;
    }

    bool MatchesNodeSet(const UEdGraph& Graph, const TSet<UEdGraphNode*>& Expected)
    {
        if (Expected.Num() != Graph.Nodes.Num())
        {
            return false;
        }
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            if (!Expected.Contains(Node))
            {
                return false;
            }
        }
        return true;
    }

    int32 AnalyzeLevelCommandChains(UBlueprint* LevelScript, FLevelCommandAnalysis& OutAnalysis)
    {
        if (!LevelScript || !LevelScript->IsA<ULevelScriptBlueprint>() ||
            LevelScript->GetPathName() != LevelScriptPath)
        {
            return 0;
        }

        TArray<UEdGraph*> Graphs;
        LevelScript->GetAllGraphs(Graphs);
        if (Graphs.Num() != 1 || !Graphs[0] || Graphs[0]->GetFName() != TEXT("EventGraph"))
        {
            return 0;
        }
        UEdGraph& Graph = *Graphs[0];

        TArray<UK2Node_Event*> Events;
        Graph.GetNodesOfClass(Events);
        UK2Node_Event* Begin = FindEvent(Events, TEXT("ReceiveBeginPlay"));
        UK2Node_Event* Destroyed = FindEvent(Events, TEXT("ReceiveDestroyed"));
        UK2Node_Event* Tick = FindEvent(Events, TEXT("ReceiveTick"));
        if (Events.Num() != 3 || !Begin || !Destroyed || !Tick ||
            Begin == Destroyed || Begin == Tick || Destroyed == Tick ||
            !EventThen(Tick) || !EventThen(Tick)->LinkedTo.IsEmpty())
        {
            return 0;
        }

        UClass* NativeClass = FindObject<UClass>(nullptr,
            TEXT("/Script/GameAnimationSampleCpp.SampleLookAtLevelRuntime"));
        const UFunction* NativeFunction = NativeClass
            ? NativeClass->FindFunctionByName(TEXT("SetControlRigDebugState")) : nullptr;
        if (!NativeFunction)
        {
            return 0;
        }

        if (Graph.Nodes.Num() == 5)
        {
            UK2Node_CallFunction* Enable = nullptr;
            UK2Node_CallFunction* Disable = nullptr;
            if (!MatchesNativeCall(Begin, NativeFunction, true, Enable) ||
                !MatchesNativeCall(Destroyed, NativeFunction, false, Disable) || Enable == Disable ||
                !MatchesNodeSet(Graph, {Begin, Destroyed, Tick, Enable, Disable}))
            {
                return 0;
            }

            OutAnalysis.Graph = &Graph;
            OutAnalysis.Begin = Begin;
            OutAnalysis.Destroyed = Destroyed;
            OutAnalysis.NativeFunction = NativeFunction;
            return 1;
        }

        if (Graph.Nodes.Num() != 9)
        {
            return 0;
        }

        const UFunction* CommandFunction = UKismetSystemLibrary::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, ExecuteConsoleCommand));
        const UFunction* PlayerFunction = UGameplayStatics::StaticClass()->FindFunctionByName(
            GET_FUNCTION_NAME_CHECKED(UGameplayStatics, GetPlayerController));
        FCommandChain Enable;
        FCommandChain Disable;
        if (!CommandFunction || !PlayerFunction ||
            !MatchesChain(Begin, TEXT("a.AnimNode.ControlRig.Debug 1"),
                TEXT("ControlRig.EnableDrawInterfaceInGame 1"), CommandFunction, PlayerFunction, Enable) ||
            !MatchesChain(Destroyed, TEXT("a.AnimNode.ControlRig.Debug 0"),
                TEXT("ControlRig.EnableDrawInterfaceInGame 0"), CommandFunction, PlayerFunction, Disable) ||
            !MatchesNodeSet(Graph, {Begin, Destroyed, Tick,
                Enable.First, Enable.Second, Enable.Player,
                Disable.First, Disable.Second, Disable.Player}))
        {
            return 0;
        }

        OutAnalysis.Graph = &Graph;
        OutAnalysis.Begin = Begin;
        OutAnalysis.Destroyed = Destroyed;
        OutAnalysis.NativeFunction = NativeFunction;
        OutAnalysis.Enable = Enable;
        OutAnalysis.Disable = Disable;
        return 2;
    }

    UK2Node_CallFunction* CreateNativeCall(UEdGraph& Graph, const FCommandChain& Chain,
        const UFunction& Function, bool bEnabled)
    {
        FGraphNodeCreator<UK2Node_CallFunction> Creator(Graph);
        UK2Node_CallFunction* Call = Creator.CreateNode();
        Call->SetFromFunction(&Function);
        Call->NodePosX = Chain.First->NodePosX;
        Call->NodePosY = Chain.First->NodePosY;
        Creator.Finalize();

        const UEdGraphSchema* Schema = Graph.GetSchema();
        UEdGraphPin* Enabled = Call->FindPin(TEXT("bEnabled"), EGPD_Input);
        if (!Schema || !Enabled || !Call->GetExecPin() || !Call->GetThenPin())
        {
            Call->DestroyNode();
            return nullptr;
        }
        Schema->TrySetDefaultValue(*Enabled, bEnabled ? TEXT("true") : TEXT("false"));
        if (Enabled->DefaultValue != (bEnabled ? TEXT("true") : TEXT("false")))
        {
            Call->DestroyNode();
            return nullptr;
        }

        return Call;
    }

    bool RestoreEventConnection(const UEdGraphSchema& Schema, UEdGraphPin* EventThen,
        UEdGraphPin* OriginalExec, UEdGraphPin* NativeExec)
    {
        if (!EventThen || !OriginalExec || !NativeExec)
        {
            return false;
        }

        if (EventThen->LinkedTo.Contains(NativeExec))
        {
            EventThen->BreakLinkTo(NativeExec);
        }

        if (!EventThen->LinkedTo.Contains(OriginalExec))
        {
            if (!EventThen->LinkedTo.IsEmpty() || !OriginalExec->LinkedTo.IsEmpty() ||
                !Schema.TryCreateConnection(EventThen, OriginalExec))
            {
                return false;
            }
        }

        return OnlyPeer(EventThen) == OriginalExec && OnlyPeer(OriginalExec) == EventThen;
    }

    bool InstallEventConnection(const UEdGraphSchema& Schema, UEdGraphPin* EventThen,
        UEdGraphPin* OriginalExec, UEdGraphPin* NativeExec)
    {
        const ECanCreateConnectionResponse Response = EventThen && NativeExec
            ? Schema.CanCreateConnection(EventThen, NativeExec).Response.GetValue()
            : CONNECT_RESPONSE_DISALLOW;
        if (!EventThen || !OriginalExec || !NativeExec ||
            OnlyPeer(EventThen) != OriginalExec || OnlyPeer(OriginalExec) != EventThen ||
            !NativeExec->LinkedTo.IsEmpty() ||
            (Response != CONNECT_RESPONSE_MAKE && Response != CONNECT_RESPONSE_BREAK_OTHERS_A))
        {
            return false;
        }

        return Schema.TryCreateConnection(EventThen, NativeExec) &&
            OnlyPeer(EventThen) == NativeExec && OnlyPeer(NativeExec) == EventThen;
    }

    bool RollBackNativeCalls(const UEdGraphSchema& Schema, UBlueprint& Blueprint,
        UEdGraphPin* BeginThen, UEdGraphPin* BeginOriginalExec, UK2Node_CallFunction* EnableCall,
        UEdGraphPin* DestroyedThen, UEdGraphPin* DestroyedOriginalExec, UK2Node_CallFunction* DisableCall)
    {
        const bool bBeginRestored = RestoreEventConnection(
            Schema, BeginThen, BeginOriginalExec, EnableCall ? EnableCall->GetExecPin() : nullptr);
        const bool bDestroyedRestored = RestoreEventConnection(
            Schema, DestroyedThen, DestroyedOriginalExec, DisableCall ? DisableCall->GetExecPin() : nullptr);

        if (bBeginRestored && bDestroyedRestored)
        {
            if (EnableCall)
            {
                FBlueprintEditorUtils::RemoveNode(&Blueprint, EnableCall, true);
            }
            if (DisableCall)
            {
                FBlueprintEditorUtils::RemoveNode(&Blueprint, DisableCall, true);
            }
            return true;
        }

        FBlueprintEditorUtils::MarkBlueprintAsModified(&Blueprint);
        return false;
    }
}

int32 USampleLookAtLevelMigrationLibrary::ClassifyLevelCommandChains(UBlueprint* LevelScript)
{
    FLevelCommandAnalysis Analysis;
    return AnalyzeLevelCommandChains(LevelScript, Analysis);
}

int32 USampleLookAtLevelMigrationLibrary::ForwardLevelCommandChains(UBlueprint* LevelScript)
{
    FLevelCommandAnalysis Analysis;
    const int32 Classification = AnalyzeLevelCommandChains(LevelScript, Analysis);
    if (Classification != 2)
    {
        return Classification;
    }

    UEdGraph& Graph = *Analysis.Graph;
    UK2Node_Event* Begin = Analysis.Begin;
    UK2Node_Event* Destroyed = Analysis.Destroyed;
    const FCommandChain& Enable = Analysis.Enable;
    const FCommandChain& Disable = Analysis.Disable;
    const UFunction* NativeFunction = Analysis.NativeFunction;
    const UEdGraphSchema* Schema = Graph.GetSchema();
    UEdGraphPin* BeginThen = EventThen(Begin);
    UEdGraphPin* DestroyedThen = EventThen(Destroyed);
    UK2Node_CallFunction* EnableCall = CreateNativeCall(Graph, Enable, *NativeFunction, true);
    UK2Node_CallFunction* DisableCall = EnableCall
        ? CreateNativeCall(Graph, Disable, *NativeFunction, false) : nullptr;
    if (!Schema || !EnableCall || !DisableCall)
    {
        if (EnableCall)
        {
            FBlueprintEditorUtils::RemoveNode(LevelScript, EnableCall, true);
        }
        if (DisableCall)
        {
            FBlueprintEditorUtils::RemoveNode(LevelScript, DisableCall, true);
        }
        return 0;
    }

    LevelScript->Modify();
    Graph.Modify();
    Begin->Modify();
    Destroyed->Modify();
    Enable.First->Modify();
    Disable.First->Modify();

    const bool bEnableConnected = InstallEventConnection(
        *Schema, BeginThen, Enable.First->GetExecPin(), EnableCall->GetExecPin());
    const bool bDisableConnected = bEnableConnected && InstallEventConnection(
        *Schema, DestroyedThen, Disable.First->GetExecPin(), DisableCall->GetExecPin());
    const bool bBothConnectionsVerified = bEnableConnected && bDisableConnected &&
        OnlyPeer(BeginThen) == EnableCall->GetExecPin() &&
        OnlyPeer(EnableCall->GetExecPin()) == BeginThen &&
        OnlyPeer(DestroyedThen) == DisableCall->GetExecPin() &&
        OnlyPeer(DisableCall->GetExecPin()) == DestroyedThen &&
        MatchesNativeCall(Begin, NativeFunction, true, EnableCall) &&
        MatchesNativeCall(Destroyed, NativeFunction, false, DisableCall);

    if (!bBothConnectionsVerified)
    {
        if (!RollBackNativeCalls(*Schema, *LevelScript, BeginThen, Enable.First->GetExecPin(), EnableCall,
            DestroyedThen, Disable.First->GetExecPin(), DisableCall))
        {
            UE_LOG(LogTemp, Error,
                TEXT("Look At level migration failed and could not restore both original event connections."));
        }
        return 0;
    }

    FBlueprintEditorUtils::RemoveNode(LevelScript, Enable.First, true);
    FBlueprintEditorUtils::RemoveNode(LevelScript, Enable.Second, true);
    FBlueprintEditorUtils::RemoveNode(LevelScript, Enable.Player, true);
    FBlueprintEditorUtils::RemoveNode(LevelScript, Disable.First, true);
    FBlueprintEditorUtils::RemoveNode(LevelScript, Disable.Second, true);
    FBlueprintEditorUtils::RemoveNode(LevelScript, Disable.Player, true);

    FBlueprintEditorUtils::MarkBlueprintAsModified(LevelScript);
    return 2;
}
