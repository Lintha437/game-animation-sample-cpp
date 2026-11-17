#include "SampleMoverBlueprintMigrationLibrary.h"

#include "Editor.h"
#include "Components/AudioComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_Event.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SwitchEnum.h"
#include "K2Node_SwitchName.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include <initializer_list>

DEFINE_LOG_CATEGORY_STATIC(LogMoverModeMigration, Log, All);

namespace
{
    constexpr TCHAR BaseAsset[] = TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover");
    constexpr TCHAR ChildAsset[] = TEXT("/Game/Blueprints/SandboxCharacter_Mover_Ragdoll.SandboxCharacter_Mover_Ragdoll");
    constexpr TCHAR BaseClass[] = TEXT("/Game/Blueprints/SandboxCharacter_Mover.SandboxCharacter_Mover_C");
    constexpr TCHAR NativeClass[] = TEXT("/Script/GameAnimationSampleCpp.SampleMoverCharacter");
    const FName ModeGraph(TEXT("On_MovementModeChanged_PostFinalize"));
    const FName Previous(TEXT("PreviousMovementModeName"));
    const FName Next(TEXT("NewMovementModeName"));
    const FName Execute(TEXT("execute"));
    const FName Then(TEXT("then"));
    const FName Self(TEXT("self"));

    struct FHook
    {
        FName Name;
        TArray<UEdGraphNode*> Nodes;
        UEdGraphPin* Exec = nullptr;
        UEdGraphPin* Argument = nullptr;
        FName ArgumentName;
        UFunction* Signature = nullptr;
        UEdGraphPin* OmittedOutput = nullptr;
    };

    struct FPlan
    {
        UEdGraph* Modes = nullptr;
        UEdGraph* Events = nullptr;
        UK2Node_FunctionEntry* Entry = nullptr;
        UEdGraphPin* Dispatch = nullptr;
        UEdGraphPin* Fallback = nullptr;
        UFunction* Native = nullptr;
        TArray<FHook> Hooks;
        bool bChild = false;
    };

    int32 Reject(const TCHAR* Reason)
    {
        UE_LOG(LogMoverModeMigration, Warning, TEXT("Mover mode migration rejected: %s"), Reason);
        return 0;
    }

    UEdGraphPin* In(const UEdGraphNode* Node, FName Name)
    {
        return Node ? Node->FindPin(Name, EGPD_Input) : nullptr;
    }

    UEdGraphPin* Out(const UEdGraphNode* Node, FName Name)
    {
        return Node ? Node->FindPin(Name, EGPD_Output) : nullptr;
    }

    UEdGraphPin* Peer(const UEdGraphPin* Pin)
    {
        return Pin && Pin->LinkedTo.Num() == 1 ? Pin->LinkedTo[0] : nullptr;
    }

    UEdGraphNode* Owner(const UEdGraphPin* Pin)
    {
        return Pin ? Pin->GetOwningNodeUnchecked() : nullptr;
    }

    bool Edge(const UEdGraphPin* Source, const UEdGraphPin* Target)
    {
        return Source && Target && Peer(Target) == Source && Source->LinkedTo.Contains(Target);
    }

    bool ExactEdge(const UEdGraphPin* Source, const UEdGraphPin* Target)
    {
        return Edge(Source, Target) && Peer(Source) == Target;
    }

    bool Open(const UEdGraphPin* Pin)
    {
        return Pin && Pin->LinkedTo.IsEmpty();
    }

    bool ImplicitSelf(const UEdGraphNode* Node)
    {
        const auto* Pin = In(Node, Self);
        return Open(Pin) && !Pin->DefaultObject &&
            (Pin->DefaultValue.IsEmpty() || Pin->DefaultValue == TEXT("None"));
    }

    UEdGraphNode* ByGuid(const UEdGraph& Graph, const TCHAR* Text)
    {
        FGuid Id;
        if (!FGuid::Parse(Text, Id))
        {
            return nullptr;
        }
        UEdGraphNode* Found = nullptr;
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            if (Node && Node->NodeGuid == Id)
            {
                if (Found)
                {
                    return nullptr;
                }
                Found = Node;
            }
        }
        return Found;
    }

    template<class T>
    T* UniqueNode(const UEdGraph& Graph)
    {
        T* Found = nullptr;
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            if (auto* Match = Cast<T>(Node))
            {
                if (Found)
                {
                    return nullptr;
                }
                Found = Match;
            }
        }
        return Found;
    }

    bool CallIs(const UEdGraphNode* Node, FName Name, const TCHAR* Class = nullptr)
    {
        const auto* Call = Cast<UK2Node_CallFunction>(Node);
        const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
        return Function && Function->GetFName() == Name &&
            (!Class || Function->GetOwnerClass()->GetAuthoritativeClass()->GetPathName() == Class);
    }

    UFunction* NativeFunction(const UBlueprint& Blueprint, FName Name)
    {
        UFunction* Function = Blueprint.ParentClass ? Blueprint.ParentClass->FindFunctionByName(Name) : nullptr;
        return Function && Function->GetOwnerClass()->GetPathName() == NativeClass ? Function : nullptr;
    }

    bool Parameter(const UEdGraphPin* Pin, FName Category)
    {
        return Pin && Pin->PinType.PinCategory == Category && Pin->PinType.ContainerType == EPinContainerType::None;
    }

    bool GraphPinsValid(const UEdGraph& Graph)
    {
        TSet<FGuid> Ids;
        for (const UEdGraphNode* Node : Graph.Nodes)
        {
            if (!Node || !Node->NodeGuid.IsValid() || Ids.Contains(Node->NodeGuid) ||
                Node->GetDesiredEnabledState() == ENodeEnabledState::Disabled)
            {
                return false;
            }
            Ids.Add(Node->NodeGuid);
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin || Pin->bOrphanedPin)
                {
                    return false;
                }
                for (const UEdGraphPin* Link : Pin->LinkedTo)
                {
                    if (!Link || !Owner(Link) || Owner(Link)->GetGraph() != &Graph || !Link->LinkedTo.Contains(Pin) ||
                        Link->Direction == Pin->Direction)
                    {
                        return false;
                    }
                }
            }
        }
        return true;
    }

    bool Lifecycle(const UEdGraph& Events, const TCHAR* BeginId, const TCHAR* InitId,
        const TCHAR* TickId, const TCHAR* UpdateId, bool bChild)
    {
        const auto* Begin = Cast<UK2Node_Event>(ByGuid(Events, BeginId));
        const auto* Tick = Cast<UK2Node_Event>(ByGuid(Events, TickId));
        const auto* Init = ByGuid(Events, InitId);
        const auto* Update = ByGuid(Events, UpdateId);
        return Begin && Tick && Begin->GetFunctionName() == TEXT("ReceiveBeginPlay") &&
            Tick->GetFunctionName() == TEXT("ReceiveTick") && Begin->bOverrideFunction && Tick->bOverrideFunction &&
            Begin->EventReference.GetMemberParentClass() == AActor::StaticClass() &&
            Tick->EventReference.GetMemberParentClass() == AActor::StaticClass() &&
            CallIs(Init, bChild ? TEXT("InitializeRagdollRuntime") : TEXT("InitializeMoverRuntime"), NativeClass) &&
            CallIs(Update, bChild ? TEXT("UpdateRagdollRuntime") : TEXT("UpdateMoverRuntime"), NativeClass) &&
            ExactEdge(Out(Begin, Then), In(Init, Execute)) && ExactEdge(Out(Tick, Then), In(Update, Execute)) &&
            Open(Out(Begin, TEXT("OutputDelegate"))) && Open(Out(Tick, TEXT("OutputDelegate"))) &&
            Open(Out(Tick, TEXT("DeltaSeconds"))) && Init->Pins.Num() == 3 && Update->Pins.Num() == 3 &&
            ImplicitSelf(Init) && ImplicitSelf(Update) && Open(Out(Init, Then)) && Open(Out(Update, Then));
    }

    // The pre-crouch export identifies the retained nodes and literals. The current census
    // and native crouch tails identify the supported, partially migrated source graph.
    bool OnlyExecOutputs(const UEdGraphNode* Node, std::initializer_list<FName> Names)
    {
        if (!Node)
        {
            return false;
        }
        for (const UEdGraphPin* Pin : Node->Pins)
        {
            if (Pin->Direction != EGPD_Output || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
            {
                continue;
            }
            bool bExpected = false;
            for (FName Name : Names)
            {
                bExpected |= Pin->PinName == Name;
            }
            if (Pin->LinkedTo.Num() != (bExpected ? 1 : 0))
            {
                return false;
            }
        }
        return true;
    }

    bool NamedVariable(const UEdGraphNode* Node, FName Name)
    {
        const auto* Variable = Cast<UK2Node_Variable>(Node);
        return Variable && Variable->GetVarName() == Name;
    }

    bool ParameterSource(const UEdGraphPin* Pin, const UK2Node_FunctionEntry* Entry, FName Name)
    {
        const UEdGraphPin* Source = Peer(Pin);
        const auto* Getter = Cast<UK2Node_VariableGet>(Owner(Source));
        return Source && Source->PinName == Name &&
            (Owner(Source) == Entry || (Getter && Getter->GetVarName() == Name &&
                Getter->VariableReference.GetMemberScopeName() == ModeGraph.ToString()));
    }

    bool Literal(const UEdGraphNode* Node, FName PinName, const TCHAR* Value)
    {
        const auto* Pin = In(Node, PinName);
        return Open(Pin) && !Pin->DefaultObject && Pin->DefaultValue == Value;
    }

    bool Number(const UEdGraphNode* Node, FName PinName, double Value)
    {
        const auto* Pin = In(Node, PinName);
        return Open(Pin) && !Pin->DefaultObject && Pin->DefaultValue.IsNumeric() &&
            FMath::IsNearlyEqual(FCString::Atod(*Pin->DefaultValue), Value);
    }

    bool FoleyHook(FPlan& Plan, FName HookName, const TCHAR* CallId, const TCHAR* GetterId,
        const TCHAR* StructId, const TCHAR* MapId, const TCHAR* Tag, FName Argument,
        double InA, double InB, double OutA, double OutB)
    {
        auto* Call = ByGuid(*Plan.Modes, CallId);
        auto* Getter = ByGuid(*Plan.Modes, GetterId);
        auto* Params = Cast<UK2Node_MakeStruct>(ByGuid(*Plan.Modes, StructId));
        if (!CallIs(Call, TEXT("PlayFoleyEvent"), TEXT("/Game/Audio/Foley/AC_FoleyEvents.AC_FoleyEvents_C")) ||
            !Cast<UK2Node_VariableGet>(Getter) || !NamedVariable(Getter, TEXT("AC_FoleyEvents")) || !ImplicitSelf(Getter) ||
            !Params || Params->Pins.Num() != (MapId ? 2 : 1) ||
            GetPathNameSafe(Params->StructType.Get()) != TEXT("/Game/Audio/Foley/S_FoleyEventParams.S_FoleyEventParams") ||
            !Literal(Call, TEXT("Event"), Tag) ||
            !ExactEdge(Out(Getter, TEXT("AC_FoleyEvents")), In(Call, Self)) ||
            !ExactEdge(Out(Params, TEXT("S_FoleyEventParams")), In(Call, TEXT("Params"))))
        {
            return false;
        }
        FHook Hook{HookName, {Call, Getter, Params}, In(Call, Execute), nullptr, Argument};
        if (MapId)
        {
            auto* Map = ByGuid(*Plan.Modes, MapId);
            if (!CallIs(Map, TEXT("MapRangeClamped"), TEXT("/Script/Engine.KismetMathLibrary")) ||
                !Number(Map, TEXT("InRangeA"), InA) || !Number(Map, TEXT("InRangeB"), InB) ||
                !Number(Map, TEXT("OutRangeA"), OutA) || !Number(Map, TEXT("OutRangeB"), OutB) ||
                !ExactEdge(Out(Map, TEXT("ReturnValue")), In(Params, TEXT("Volume_4_693373E847A8B941D3BDCEA05B20F090"))) ||
                !Open(Out(Call, Then)) || !Open(Out(Call, TEXT("AudioComponent"))))
            {
                return false;
            }
            Hook.Nodes.Add(Map);
            Hook.Argument = In(Map, TEXT("Value"));
        }
        else
        {
            auto* Set = Cast<UK2Node_VariableSet>(ByGuid(*Plan.Modes, TEXT("F89DFE0343E5A049A2C350BD220A8BDA")));
            if (!Set || !NamedVariable(Set, TEXT("SlidingAudioComponent")) || !ImplicitSelf(Set) ||
                !ExactEdge(Out(Call, Then), In(Set, Execute)) ||
                !ExactEdge(Out(Call, TEXT("AudioComponent")), In(Set, TEXT("SlidingAudioComponent"))) ||
                !Open(Out(Set, Then)) || !Open(Out(Set, TEXT("Output_Get"))))
            {
                return false;
            }
            Hook.Nodes.Add(Set);
        }
        Plan.Hooks.Add(MoveTemp(Hook));
        return true;
    }

    bool ParentSource(FPlan& Plan)
    {
        auto& Graph = *Plan.Modes;
        const auto Node = [&Graph](const TCHAR* Id) { return ByGuid(Graph, Id); };
        auto* PrevSet = Node(TEXT("33288634447C90CD6ADC3B89B16A1849"));
        auto* NewSet = Node(TEXT("18FF65F34B6A35B2663867A0F8228768"));
        auto* Sequence = Cast<UK2Node_ExecutionSequence>(Node(TEXT("709C3E1845B7E9DF539FD48049DAB0F6")));
        auto* NewSwitch = Cast<UK2Node_SwitchEnum>(Node(TEXT("8A83E17444180220C899798F972ADBE7")));
        auto* PrevSwitch = Cast<UK2Node_SwitchEnum>(Node(TEXT("AAB5E5CB4BE7312D4239629F667E6A88")));
        auto* LandSwitch = Cast<UK2Node_SwitchEnum>(Node(TEXT("D5F07B7F4ADC65C44A2F90829A3D7C79")));
        auto* JumpSwitch = Cast<UK2Node_SwitchEnum>(Node(TEXT("BFD2A04D4E53956A5CC7DCBEF0F175D8")));
        auto* AirSequence = Cast<UK2Node_ExecutionSequence>(Node(TEXT("679B04D1431DE01F3BC70087B2DB8415")));
        auto* JumpBranch = Cast<UK2Node_IfThenElse>(Node(TEXT("0EB2454648D1332B1234CFB98B4EE520")));
        auto* Knot = Cast<UK2Node_Knot>(Node(TEXT("E4E6AD2E4E8F42CAF3D158AFA1488306")));
        auto* ValidatedAudio = Cast<UK2Node_VariableGet>(Node(TEXT("B2AC4AA64C517CAD246D9BBC74823A6D")));
        auto* Fade = Node(TEXT("BD8945814AA0F1C33D6638A414BA9775"));
        auto* AirTail = Owner(Peer(Out(AirSequence, TEXT("then_1"))));
        auto* SlideTail = Owner(Peer(Out(Fade, Then)));
        auto* PrevFind = Node(TEXT("26B3099B43711DEFB38C4791EB784D75"));
        auto* NewFind = Node(TEXT("E5014C1A4E405564E8B713996779EA11"));
        Plan.Dispatch = Out(Plan.Entry, Then);
        Plan.Fallback = In(PrevSet, Execute);
        const auto* Result = UniqueNode<UK2Node_FunctionResult>(Graph);
        const auto ExpectedEnum = [](const UK2Node_SwitchEnum* Switch)
        {
            return Switch && GetPathNameSafe(Switch->GetEnum()) == TEXT("/Game/Blueprints/Data/E_MovementMode.E_MovementMode");
        };

        if (Plan.Entry != Node(TEXT("9912AD1640BF3D10000A48957A11E619")) ||
            Result != Node(TEXT("22A5ECE44BC0ACA3837C898409F40E2F")) || !Open(In(Result, Execute)) ||
            !ExpectedEnum(NewSwitch) || !ExpectedEnum(PrevSwitch) || !ExpectedEnum(LandSwitch) || !ExpectedEnum(JumpSwitch) ||
            !Cast<UK2Node_VariableSet>(PrevSet) || !Cast<UK2Node_VariableSet>(NewSet) ||
            !NamedVariable(PrevSet, TEXT("PreviousMovementMode")) || !NamedVariable(NewSet, TEXT("NewMovementMode")) ||
            !CallIs(PrevFind, TEXT("Map_Find"), TEXT("/Script/Engine.BlueprintMapLibrary")) ||
            !CallIs(NewFind, TEXT("Map_Find"), TEXT("/Script/Engine.BlueprintMapLibrary")) ||
            !ParameterSource(In(PrevFind, TEXT("Key")), Plan.Entry, Previous) ||
            !ParameterSource(In(NewFind, TEXT("Key")), Plan.Entry, Next) ||
            !NamedVariable(Owner(Peer(In(PrevFind, TEXT("TargetMap")))), TEXT("MovementModeMap")) ||
            !NamedVariable(Owner(Peer(In(NewFind, TEXT("TargetMap")))), TEXT("MovementModeMap")) ||
            !ExactEdge(Out(PrevFind, TEXT("Value")), In(PrevSet, TEXT("PreviousMovementMode"))) ||
            !ExactEdge(Out(NewFind, TEXT("Value")), In(NewSet, TEXT("NewMovementMode"))) ||
            !ExactEdge(Out(PrevSet, Then), In(NewSet, Execute)) ||
            !ExactEdge(Out(NewSet, Then), In(Sequence, Execute)) ||
            !ExactEdge(Out(Sequence, TEXT("then_0")), In(NewSwitch, Execute)) ||
            !ExactEdge(Out(Sequence, TEXT("then_1")), In(PrevSwitch, Execute)) ||
            !ExactEdge(Out(NewSwitch, TEXT("NewEnumerator4")), In(LandSwitch, Execute)) ||
            !ExactEdge(Out(NewSwitch, TEXT("NewEnumerator5")), In(AirSequence, Execute)) ||
            !ExactEdge(Out(NewSwitch, TEXT("NewEnumerator6")), In(Knot, TEXT("InputPin"))) ||
            !ExactEdge(Out(AirSequence, TEXT("then_0")), In(JumpSwitch, Execute)) ||
            !ExactEdge(Out(JumpSwitch, TEXT("NewEnumerator4")), In(JumpBranch, Execute)) ||
            !NamedVariable(Owner(Peer(In(NewSwitch, TEXT("Selection")))), TEXT("NewMovementMode")) ||
            !NamedVariable(Owner(Peer(In(PrevSwitch, TEXT("Selection")))), TEXT("PreviousMovementMode")) ||
            !NamedVariable(Owner(Peer(In(LandSwitch, TEXT("Selection")))), TEXT("PreviousMovementMode")) ||
            !NamedVariable(Owner(Peer(In(JumpSwitch, TEXT("Selection")))), TEXT("PreviousMovementMode")) ||
            !ExactEdge(Out(PrevSwitch, TEXT("NewEnumerator6")), In(ValidatedAudio, Execute)) ||
            !NamedVariable(ValidatedAudio, TEXT("SlidingAudioComponent")) ||
            !ExactEdge(Out(ValidatedAudio, Then), In(Fade, Execute)) ||
            !ExactEdge(Out(ValidatedAudio, TEXT("SlidingAudioComponent")), In(Fade, Self)) ||
            !CallIs(AirTail, TEXT("ClearAirborneCrouch"), NativeClass) ||
            !CallIs(SlideTail, TEXT("ClearSlideCrouchForSprint"), NativeClass) ||
            !ExactEdge(Out(AirSequence, TEXT("then_1")), In(AirTail, Execute)) ||
            !ExactEdge(Out(Fade, Then), In(SlideTail, Execute)) ||
            !ImplicitSelf(AirTail) || !ImplicitSelf(SlideTail) ||
            !Open(Out(AirTail, Then)) || !Open(Out(SlideTail, Then)) ||
            !OnlyExecOutputs(Sequence, {TEXT("then_0"), TEXT("then_1")}) ||
            !OnlyExecOutputs(AirSequence, {TEXT("then_0"), TEXT("then_1")}) ||
            !OnlyExecOutputs(NewSwitch, {TEXT("NewEnumerator4"), TEXT("NewEnumerator5"), TEXT("NewEnumerator6")}) ||
            !OnlyExecOutputs(PrevSwitch, {TEXT("NewEnumerator6")}) ||
            !OnlyExecOutputs(LandSwitch, {TEXT("NewEnumerator5")}) ||
            !OnlyExecOutputs(JumpSwitch, {TEXT("NewEnumerator4")}) ||
            !OnlyExecOutputs(JumpBranch, {Then}) || !OnlyExecOutputs(ValidatedAudio, {Then}))
        {
            return false;
        }

        if (!FoleyHook(Plan, TEXT("PlayMoverLandingFoley"), TEXT("E73E964C400806F8C7D2408A57C244FE"),
                TEXT("7B0C03014E3C5A537E2A048AB5B3444F"), TEXT("2BB0F84F405E8F0594FB90B73CB561A6"),
                TEXT("83B215CD43D89B71AF1CFEB7A64A3987"), TEXT("(TagName=\"Foley.Event.Land\")"),
                TEXT("VerticalVelocity"), -500, -900, .5, 1.5) ||
            !FoleyHook(Plan, TEXT("PlayMoverJumpFoley"), TEXT("D045B41E4E70FB315D63D3BB7A81B8F9"),
                TEXT("D323DA6842EFA620B41CACAF65096400"), TEXT("A7C7FBAE45B17B6DF91CE1BF8A4C660A"),
                TEXT("44293F644788AA3B654EC99C8C4E6FD3"), TEXT("(TagName=\"Foley.Event.Jump\")"),
                TEXT("HorizontalSpeed"), 0, 375, .5, 1) ||
            !FoleyHook(Plan, TEXT("StartMoverSlideFoley"), TEXT("C618151542BFFC72F0C6CB81BF6C3125"),
                TEXT("A882486643C0FD0BC78E3481B5797000"), TEXT("FEF1E5E14BBA6B892F43D3B69E347471"),
                nullptr, TEXT("(TagName=\"Foley.Event.Slide.Loop\")"), NAME_None, 0, 0, 0, 0) ||
            !ExactEdge(Out(LandSwitch, TEXT("NewEnumerator5")), Plan.Hooks[0].Exec) ||
            !ExactEdge(Out(JumpBranch, Then), Plan.Hooks[1].Exec) ||
            !ExactEdge(Out(Knot, TEXT("OutputPin")), Plan.Hooks[2].Exec))
        {
            return false;
        }

        auto* LandVelocity = Node(TEXT("F4890A7048AD1E9097B581B191202567"));
        auto* JumpVelocity = Node(TEXT("F54E427847513DDC19A925BD50E79C03"));
        auto* Speed = Node(TEXT("54B1C54149BC2BAE46D08386FDF63813"));
        auto* Break = Node(TEXT("5FE193804C12D65D6FC1AA9EF1EFB9FD"));
        if (!CallIs(LandVelocity, TEXT("GetVelocity"), TEXT("/Script/Mover.MoverComponent")) ||
            !CallIs(JumpVelocity, TEXT("GetVelocity"), TEXT("/Script/Mover.MoverComponent")) ||
            !NamedVariable(Owner(Peer(In(LandVelocity, Self))), TEXT("CharacterMover")) ||
            !NamedVariable(Owner(Peer(In(JumpVelocity, Self))), TEXT("CharacterMover")) ||
            !CallIs(Speed, TEXT("VSizeXY"), TEXT("/Script/Engine.KismetMathLibrary")) ||
            !ExactEdge(Out(LandVelocity, TEXT("ReturnValue_Z")), Plan.Hooks[0].Argument) ||
            !ExactEdge(Out(JumpVelocity, TEXT("ReturnValue")), In(Speed, TEXT("A"))) ||
            !ExactEdge(Out(Speed, TEXT("ReturnValue")), Plan.Hooks[1].Argument) ||
            !ExactEdge(Out(Break, TEXT("bIsJumpJustPressed")), In(JumpBranch, TEXT("Condition"))) ||
            !NamedVariable(Owner(Peer(In(Break, TEXT("CharacterDefaultInputs")))), TEXT("MoverDefaultInputs_PreSim")) ||
            !CallIs(Fade, TEXT("FadeOut"), TEXT("/Script/Engine.AudioComponent")) ||
            !Number(Fade, TEXT("FadeOutDuration"), .5) || !Number(Fade, TEXT("FadeVolumeLevel"), 0) ||
            !Literal(Fade, TEXT("FadeCurve"), TEXT("Linear")))
        {
            return false;
        }
        Plan.Hooks.Add({TEXT("FadeMoverSlideFoley"), {Fade}, In(Fade, Execute), In(Fade, Self), TEXT("AudioComponent")});
        Plan.Hooks.Last().OmittedOutput = Out(Fade, Then);
        return true;
    }

    bool ChildSource(FPlan& Plan)
    {
        auto& Graph = *Plan.Modes;
        auto* Parent = UniqueNode<UK2Node_CallParentFunction>(Graph);
        auto* Sequence = UniqueNode<UK2Node_ExecutionSequence>(Graph);
        auto* Enter = Cast<UK2Node_SwitchName>(Owner(Peer(Out(Sequence, TEXT("then_0")))));
        auto* Exit = Cast<UK2Node_SwitchName>(Owner(Peer(Out(Sequence, TEXT("then_1")))));
        auto* Apply = Owner(Peer(Out(Enter, TEXT("Ragdoll"))));
        auto* ExitCall = Owner(Peer(Out(Exit, TEXT("Ragdoll"))));
        const auto* ExitEvent = Cast<UK2Node_CustomEvent>(ByGuid(*Plan.Events, TEXT("DBFDBB8D4328EF9E167ACF804F70C1DF")));
        const auto* Restore = ByGuid(*Plan.Events, TEXT("88426FAE483F64A2F6B849AE9851EC07"));
        const auto* Anim = ByGuid(*Plan.Events, TEXT("1DFA9E6A4CBA9216AE94139C73F13043"));
        Plan.Dispatch = Out(Parent, Then);
        Plan.Fallback = In(Sequence, Execute);
        const auto* Result = UniqueNode<UK2Node_FunctionResult>(Graph);
        const auto* ReturnExec = In(Result, Execute);
        const auto* Then0 = Out(Sequence, TEXT("then_0"));
        const auto* Then1 = Out(Sequence, TEXT("then_1"));
        const auto* Then2 = Out(Sequence, TEXT("then_2"));
        // An explicit void return after both switches is equivalent to falling through.
        const bool bExplicitReturn = Sequence && Sequence->Pins.Num() == 4 &&
            Plan.Fallback && Then0 && Then1 && Then2 &&
            Sequence->Pins[0] == Plan.Fallback && Sequence->Pins[1] == Then0 &&
            Sequence->Pins[2] == Then1 && Sequence->Pins[3] == Then2 &&
            Plan.Fallback->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
            Then0->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
            Then1->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
            Then2->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
            Result && Result->Pins.Num() == 1 && ReturnExec &&
            Result->Pins[0] == ReturnExec && ReturnExec->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec &&
            ExactEdge(Then2, ReturnExec) &&
            OnlyExecOutputs(Sequence, {TEXT("then_0"), TEXT("then_1"), TEXT("then_2")});
        if (!Parent || !CallIs(Parent, ModeGraph, BaseClass) ||
            !ExactEdge(Out(Plan.Entry, Then), In(Parent, Execute)) ||
            !ParameterSource(In(Parent, Previous), Plan.Entry, Previous) ||
            !ParameterSource(In(Parent, Next), Plan.Entry, Next) || !Enter || !Exit || Enter == Exit ||
            Enter->PinNames.Num() != 1 || Exit->PinNames.Num() != 1 ||
            Enter->PinNames[0] != TEXT("Ragdoll") || Exit->PinNames[0] != TEXT("Ragdoll") ||
            !ExactEdge(Out(Sequence, TEXT("then_0")), In(Enter, Execute)) ||
            !ExactEdge(Out(Sequence, TEXT("then_1")), In(Exit, Execute)) ||
            !ParameterSource(In(Enter, TEXT("Selection")), Plan.Entry, Next) ||
            !ParameterSource(In(Exit, TEXT("Selection")), Plan.Entry, Previous) ||
            !(OnlyExecOutputs(Sequence, {TEXT("then_0"), TEXT("then_1")}) || bExplicitReturn) ||
            !OnlyExecOutputs(Enter, {TEXT("Ragdoll")}) || !OnlyExecOutputs(Exit, {TEXT("Ragdoll")}) ||
            !CallIs(Apply, TEXT("ApplyRagdollCapsuleCollision"), NativeClass) ||
            !CallIs(ExitCall, TEXT("On_RagdollMode_Exit"), TEXT("/Game/Blueprints/SandboxCharacter_Mover_Ragdoll.SandboxCharacter_Mover_Ragdoll_C")) ||
            !ImplicitSelf(Apply) || !ImplicitSelf(ExitCall) ||
            !ExactEdge(Out(Enter, TEXT("Ragdoll")), In(Apply, Execute)) ||
            !ExactEdge(Out(Exit, TEXT("Ragdoll")), In(ExitCall, Execute)) ||
            !Open(Out(Apply, Then)) || !Open(Out(ExitCall, Then)) ||
            !ExitEvent || ExitEvent->CustomFunctionName != TEXT("On_RagdollMode_Exit") ||
            !CallIs(Restore, TEXT("RestoreRagdollCapsuleCollision"), NativeClass) ||
            !ExactEdge(Out(ExitEvent, Then), In(Restore, Execute)) ||
            !CallIs(Anim, TEXT("GetAnimInstance")) || !ExactEdge(Out(Restore, Then), In(Anim, Execute)))
        {
            return false;
        }
        Plan.Hooks.Add({TEXT("RunMoverRagdollExit"), {ExitCall}, In(ExitCall, Execute), nullptr, NAME_None});
        return true;
    }

    bool SourceHookBoundary(const FHook& Hook)
    {
        for (const UEdGraphNode* Node : Hook.Nodes)
        {
            if (Node->GetDesiredEnabledState() != ENodeEnabledState::Enabled)
            {
                return false;
            }
            if (const auto* Make = Cast<UK2Node_MakeStruct>(Node); Make && !Make->bMadeAfterOverridePinRemoval)
            {
                // New Make Struct nodes use the current optional-pin representation.
                return false;
            }
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (!Pin->SubPins.IsEmpty())
                {
                    return false;
                }
                int32 External = 0;
                for (const UEdGraphPin* Link : Pin->LinkedTo)
                {
                    External += Hook.Nodes.Contains(Owner(Link)) ? 0 : 1;
                }
                const bool bBoundary = Pin == Hook.Exec || Pin == Hook.Argument || Pin == Hook.OmittedOutput;
                if (External != (bBoundary ? 1 : 0) || (bBoundary && Pin->LinkedTo.Num() != 1))
                {
                    return false;
                }
            }
        }
        return Hook.Exec && Hook.Exec->Direction == EGPD_Input &&
            (!Hook.Argument || Hook.Argument->Direction == EGPD_Input) &&
            (!Hook.OmittedOutput || Hook.OmittedOutput->Direction == EGPD_Output);
    }

    bool HookSignature(const FHook& Hook)
    {
        const UFunction* Function = Hook.Signature;
        if (!Function || !Function->HasAnyFunctionFlags(FUNC_BlueprintEvent) ||
            Function->HasAnyFunctionFlags(FUNC_BlueprintPure) || Function->GetReturnProperty() ||
            Function->NumParms != (Hook.Argument ? 1 : 0))
        {
            return false;
        }
        if (!Hook.Argument)
        {
            return true;
        }
        const FProperty* Property = FindFProperty<FProperty>(Function, Hook.ArgumentName);
        if (!Property || !Property->HasAnyPropertyFlags(CPF_Parm) || Property->HasAnyPropertyFlags(CPF_OutParm))
        {
            return false;
        }
        if (Hook.ArgumentName == TEXT("AudioComponent"))
        {
            const auto* Object = CastField<FObjectPropertyBase>(Property);
            return Object && Object->PropertyClass == UAudioComponent::StaticClass();
        }
        return CastField<FDoubleProperty>(Property) != nullptr;
    }

    FGuid CopyId(const FGuid& Original)
    {
        // Stable identifiers let a subsequent invocation audit the exact installed copies.
        return FGuid(Original.A ^ 0x5D813CC1u, Original.B ^ 0x4399AA67u, Original.C ^ 0xBFA34F02u, Original.D ^ 0x2D8E117Bu);
    }

    UEdGraphNode* HookCopy(const UEdGraph& Events, const UEdGraphNode& Source)
    {
        return ByGuid(Events, *CopyId(Source.NodeGuid).ToString(EGuidFormats::Digits));
    }

    UK2Node_Event* FindHook(const UEdGraph& Events, FName Name, bool& bDuplicate)
    {
        UK2Node_Event* Found = nullptr;
        for (UEdGraphNode* Node : Events.Nodes)
        {
            auto* Event = Cast<UK2Node_Event>(Node);
            if (Event && Event->GetFunctionName() == Name)
            {
                bDuplicate |= Found != nullptr;
                Found = Event;
            }
        }
        return Found;
    }

    UEdGraphPin* CopyPin(const TMap<UEdGraphNode*, UEdGraphNode*>& Copies, const UEdGraphPin* Original)
    {
        auto* const* Node = Copies.Find(Owner(Original));
        return Node ? (*Node)->FindPin(Original->PinName, Original->Direction) : nullptr;
    }

    bool SameNode(const UEdGraphNode& Source, const UEdGraphNode& Copy)
    {
        if (Source.GetClass() != Copy.GetClass() || Source.Pins.Num() != Copy.Pins.Num() ||
            Source.GetDesiredEnabledState() != Copy.GetDesiredEnabledState())
        {
            return false;
        }
        if (const auto* Call = Cast<UK2Node_CallFunction>(&Source))
        {
            const UFunction* Original = Call->GetTargetFunction();
            const UFunction* Other = CastChecked<UK2Node_CallFunction>(&Copy)->GetTargetFunction();
            return Original && Other && Original->GetFName() == Other->GetFName() &&
                Original->GetOwnerClass()->GetAuthoritativeClass() == Other->GetOwnerClass()->GetAuthoritativeClass();
        }
        if (const auto* Make = Cast<UK2Node_MakeStruct>(&Source))
        {
            const auto* Other = CastChecked<UK2Node_MakeStruct>(&Copy);
            return Other->StructType == Make->StructType &&
                Other->bMadeAfterOverridePinRemoval == Make->bMadeAfterOverridePinRemoval;
        }
        if (const auto* Var = Cast<UK2Node_Variable>(&Source))
        {
            const auto* Other = CastChecked<UK2Node_Variable>(&Copy);
            return Other->VariableReference.IsSameReference(Var->VariableReference);
        }
        return false;
    }

    bool AuditHook(const FPlan& Plan, const FHook& Hook, UK2Node_Event* Event)
    {
        if (!Event || Event->GetDesiredEnabledState() != ENodeEnabledState::Enabled ||
            !Event->bOverrideFunction || Event->EventReference.GetMemberName() != Hook.Name ||
            Event->EventReference.GetMemberParentClass() != Hook.Signature->GetOwnerClass())
        {
            return false;
        }
        TMap<UEdGraphNode*, UEdGraphNode*> Copies;
        for (UEdGraphNode* Source : Hook.Nodes)
        {
            auto* Copy = HookCopy(*Plan.Events, *Source);
            if (!Copy || !SameNode(*Source, *Copy))
            {
                return false;
            }
            Copies.Add(Source, Copy);
        }
        auto* Exec = CopyPin(Copies, Hook.Exec);
        auto* Argument = Hook.Argument ? CopyPin(Copies, Hook.Argument) : nullptr;
        if (!ExactEdge(Out(Event, Then), Exec) ||
            (Hook.Argument && !ExactEdge(Out(Event, Hook.ArgumentName), Argument)))
        {
            return false;
        }
        for (const auto* Pin : Event->Pins)
        {
            if (Pin != Out(Event, Then) && (!Hook.Argument || Pin != Out(Event, Hook.ArgumentName)) &&
                !Pin->LinkedTo.IsEmpty())
            {
                return false;
            }
        }
        for (UEdGraphNode* Source : Hook.Nodes)
        {
            for (const auto* Pin : Source->Pins)
            {
                const auto* Copy = CopyPin(Copies, Pin);
                if (!Copy || Pin->PinType != Copy->PinType || Pin->DefaultValue != Copy->DefaultValue ||
                    Pin->DefaultObject != Copy->DefaultObject || !Pin->DefaultTextValue.EqualTo(Copy->DefaultTextValue))
                {
                    return false;
                }
                int32 Expected = (Pin == Hook.Exec || Pin == Hook.Argument) ? 1 : 0;
                for (const auto* Link : Pin->LinkedTo)
                {
                    if (const auto* CopiedLink = CopyPin(Copies, Link))
                    {
                        ++Expected;
                        if (!Copy->LinkedTo.Contains(CopiedLink))
                        {
                            return false;
                        }
                    }
                }
                if (Copy->LinkedTo.Num() != Expected)
                {
                    return false;
                }
            }
        }
        return true;
    }

    bool AuditCensus(const FPlan& Plan, bool bInstalled)
    {
        TMap<FName, int32> Counts;
        for (const UEdGraphNode* Node : Plan.Modes->Nodes)
        {
            ++Counts.FindOrAdd(Node->GetClass()->GetFName());
        }
        const auto Count = [&Counts](const TCHAR* Name) { return Counts.FindRef(FName(Name)); };
        const int32 Extra = bInstalled ? 1 : 0;
        if (Plan.bChild)
        {
            return Count(TEXT("K2Node_CallParentFunction")) == 1 && Count(TEXT("K2Node_ExecutionSequence")) == 1 &&
                Count(TEXT("K2Node_SwitchName")) == 2 && Count(TEXT("K2Node_CallFunction")) == 2 + Extra &&
                Count(TEXT("K2Node_IfThenElse")) == Extra && Plan.Modes->Nodes.Num() == 10 + 2 * Extra;
        }
        // Retained GUIDs from the pre-crouch graph export. Eleven replaced nodes are
        // deliberately excluded; the two native crouch calls are verified by topology.
        static const TCHAR* const RetainedIds[] = {
            TEXT("9912AD1640BF3D10000A48957A11E619"), TEXT("22A5ECE44BC0ACA3837C898409F40E2F"),
            TEXT("86A7EE1546DC8BC481CD57B8582A1FE4"), TEXT("26B3099B43711DEFB38C4791EB784D75"),
            TEXT("33288634447C90CD6ADC3B89B16A1849"), TEXT("2D4BE0AB4C33A72B0DD6F0835CD646A5"),
            TEXT("E5014C1A4E405564E8B713996779EA11"), TEXT("18FF65F34B6A35B2663867A0F8228768"),
            TEXT("427FC28643BF1D5FFBD4B69D37F9827F"), TEXT("709C3E1845B7E9DF539FD48049DAB0F6"),
            TEXT("76DB28664D80D7E1526FD19651C75D7E"), TEXT("8A83E17444180220C899798F972ADBE7"),
            TEXT("A882486643C0FD0BC78E3481B5797000"), TEXT("C618151542BFFC72F0C6CB81BF6C3125"),
            TEXT("FEF1E5E14BBA6B892F43D3B69E347471"), TEXT("F89DFE0343E5A049A2C350BD220A8BDA"),
            TEXT("CCDFF6684DB648D7D4AEFA9F39893E7D"), TEXT("AAB5E5CB4BE7312D4239629F667E6A88"),
            TEXT("B2AC4AA64C517CAD246D9BBC74823A6D"), TEXT("BD8945814AA0F1C33D6638A414BA9775"),
            TEXT("7D1D460E4298C49C0754DCAF3B07CF48"), TEXT("D5F07B7F4ADC65C44A2F90829A3D7C79"),
            TEXT("D4002CD7435CF61A83D143BADC066BBE"), TEXT("F4890A7048AD1E9097B581B191202567"),
            TEXT("E73E964C400806F8C7D2408A57C244FE"), TEXT("7B0C03014E3C5A537E2A048AB5B3444F"),
            TEXT("83B215CD43D89B71AF1CFEB7A64A3987"), TEXT("2BB0F84F405E8F0594FB90B73CB561A6"),
            TEXT("91E8E0EC4A480BFEF3FFFC9B1E99C244"), TEXT("2EAB12E64196837725F3329D054BF0F3"),
            TEXT("BFD2A04D4E53956A5CC7DCBEF0F175D8"), TEXT("AC34ADE749CD23B6564CBF966002656D"),
            TEXT("5FE193804C12D65D6FC1AA9EF1EFB9FD"), TEXT("0EB2454648D1332B1234CFB98B4EE520"),
            TEXT("44293F644788AA3B654EC99C8C4E6FD3"), TEXT("D323DA6842EFA620B41CACAF65096400"),
            TEXT("D045B41E4E70FB315D63D3BB7A81B8F9"), TEXT("A7C7FBAE45B17B6DF91CE1BF8A4C660A"),
            TEXT("2C73F2E740A484402B2BE2B4FFB2A733"), TEXT("F54E427847513DDC19A925BD50E79C03"),
            TEXT("54B1C54149BC2BAE46D08386FDF63813"), TEXT("679B04D1431DE01F3BC70087B2DB8415"),
            TEXT("E4E6AD2E4E8F42CAF3D158AFA1488306")
        };
        for (const TCHAR* Id : RetainedIds)
        {
            if (!ByGuid(*Plan.Modes, Id))
            {
                return false;
            }
        }
        return Count(TEXT("K2Node_ExecutionSequence")) == 2 && Count(TEXT("K2Node_SwitchEnum")) == 4 &&
            Count(TEXT("K2Node_IfThenElse")) == 1 + Extra && Count(TEXT("K2Node_MakeStruct")) == 3 &&
            Count(TEXT("K2Node_VariableSet")) == 3 && Count(TEXT("K2Node_BreakStruct")) == 1 &&
            Count(TEXT("K2Node_CallFunction")) == 13 + Extra && Count(TEXT("K2Node_VariableGet")) == 15 &&
            Count(TEXT("K2Node_FunctionEntry")) == 1 && Count(TEXT("K2Node_FunctionResult")) == 1 &&
            Count(TEXT("K2Node_Knot")) == 1 && Plan.Modes->Nodes.Num() == 45 + 2 * Extra;
    }

    int32 Inspect(UBlueprint* Blueprint, FPlan& Plan)
    {
        if (!Blueprint || !Blueprint->ParentClass || !Blueprint->GeneratedClass ||
            (Blueprint->GetPathName() != BaseAsset && Blueprint->GetPathName() != ChildAsset))
        {
            return Reject(TEXT("unexpected Blueprint"));
        }
        Plan.bChild = Blueprint->GetPathName() == ChildAsset;
        if (Blueprint->ParentClass->GetPathName() != (Plan.bChild ? BaseClass : NativeClass))
        {
            return Reject(TEXT("unexpected parent class"));
        }
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetFName() == ModeGraph)
            {
                if (Plan.Modes)
                {
                    return Reject(TEXT("duplicate mode function graph"));
                }
                Plan.Modes = Graph;
            }
        }
        for (UEdGraph* Graph : Blueprint->UbergraphPages)
        {
            if (Graph && Graph->GetFName() == TEXT("EventGraph"))
            {
                if (Plan.Events)
                {
                    return Reject(TEXT("duplicate event graph"));
                }
                Plan.Events = Graph;
            }
        }
        if (!Plan.Modes || !Plan.Events || !GraphPinsValid(*Plan.Modes) || !GraphPinsValid(*Plan.Events))
        {
            return Reject(TEXT("missing or invalid graph"));
        }
        Plan.Entry = UniqueNode<UK2Node_FunctionEntry>(*Plan.Modes);
        if (!Plan.Entry || !Parameter(Out(Plan.Entry, Previous), UEdGraphSchema_K2::PC_Name) ||
            !Parameter(Out(Plan.Entry, Next), UEdGraphSchema_K2::PC_Name))
        {
            return Reject(TEXT("unexpected transition entry signature"));
        }
        const bool bLifecycle = Plan.bChild
            ? Lifecycle(*Plan.Events, TEXT("01960E22468E0DFA13CED28426C6C369"), TEXT("410713A84426E098BCFDCCB0EC6B295E"),
                TEXT("203D7E5940D2F9CCF38BC684F646B280"), TEXT("BA2216DD4119BA8C6E4F7781C0B9ACD8"), true)
            : Lifecycle(*Plan.Events, TEXT("A420535E4528240C45CBCEAB08539E1B"),
                TEXT("51333B5947322850A055D384B36CDB1A"), TEXT("8B0AB6044F86BDEEA72B32AE4DD92BDA"),
                TEXT("96A45A11471B9538B5682A9D7E6F9749"), false);
        if (!bLifecycle || !(Plan.bChild ? ChildSource(Plan) : ParentSource(Plan)))
        {
            return Reject(TEXT("lifecycle or authored fallback graph differs from the recorded topology"));
        }
        Plan.Native = NativeFunction(*Blueprint, Plan.bChild ? TEXT("TryHandleRagdollModeTransition") : TEXT("TryHandleMoverModeTransition"));
        if (!Plan.Native || !Plan.Native->HasAnyFunctionFlags(FUNC_BlueprintCallable) ||
            Plan.Native->HasAnyFunctionFlags(FUNC_BlueprintPure) || Plan.Native->NumParms != 3 ||
            !CastField<FBoolProperty>(Plan.Native->GetReturnProperty()) ||
            !FindFProperty<FNameProperty>(Plan.Native, Previous) || !FindFProperty<FNameProperty>(Plan.Native, Next))
        {
            return Reject(TEXT("native transition signature mismatch"));
        }
        const bool bOriginal = ExactEdge(Plan.Dispatch, Plan.Fallback);
        auto* NativeCall = Cast<UK2Node_CallFunction>(Owner(Peer(Plan.Dispatch)));
        auto* Gate = Cast<UK2Node_IfThenElse>(NativeCall ? Owner(Peer(Out(NativeCall, Then))) : nullptr);
        if (!bOriginal && (!NativeCall || !Gate || NativeCall->GetTargetFunction() != Plan.Native || !ImplicitSelf(NativeCall) ||
            NativeCall->GetDesiredEnabledState() != ENodeEnabledState::Enabled ||
            Gate->GetDesiredEnabledState() != ENodeEnabledState::Enabled ||
            !ExactEdge(Plan.Dispatch, In(NativeCall, Execute)) ||
            !Edge(Out(Plan.Entry, Previous), In(NativeCall, Previous)) ||
            !Edge(Out(Plan.Entry, Next), In(NativeCall, Next)) ||
            !ExactEdge(Out(NativeCall, Then), In(Gate, Execute)) ||
            !ExactEdge(Out(NativeCall, TEXT("ReturnValue")), In(Gate, TEXT("Condition"))) ||
            !Open(Out(Gate, Then)) || !ExactEdge(Out(Gate, TEXT("else")), Plan.Fallback)))
        {
            return Reject(TEXT("incomplete or unsupported native gate"));
        }
        if (!AuditCensus(Plan, !bOriginal))
        {
            return Reject(TEXT("unexpected mode graph census"));
        }
        for (FHook& Hook : Plan.Hooks)
        {
            Hook.Signature = NativeFunction(*Blueprint, Hook.Name);
            bool bDuplicate = false;
            auto* Event = FindHook(*Plan.Events, Hook.Name, bDuplicate);
            if (!HookSignature(Hook) || !SourceHookBoundary(Hook) || bDuplicate ||
                Blueprint->FunctionGraphs.ContainsByPredicate([&Hook](const UEdGraph* Graph)
                {
                    return Graph && Graph->GetFName() == Hook.Name;
                }))
            {
                return Reject(TEXT("missing or duplicate presentation hook"));
            }
            if (bOriginal)
            {
                if (Event || Hook.Nodes.ContainsByPredicate([&Plan](const UEdGraphNode* Node) { return HookCopy(*Plan.Events, *Node) != nullptr; }))
                {
                    return Reject(TEXT("partial presentation hook installation"));
                }
            }
            else if (!AuditHook(Plan, Hook, Event))
            {
                return Reject(TEXT("installed presentation hook differs from its authored source"));
            }
        }
        return bOriginal ? 2 : 1;
    }

    UEdGraphNode* CloneNode(UEdGraph& Graph, UEdGraphNode& Source)
    {
        UEdGraphNode* Result = nullptr;
        if (auto* Call = Cast<UK2Node_CallFunction>(&Source))
        {
            FGraphNodeCreator<UK2Node_CallFunction> Creator(Graph);
            auto* New = Creator.CreateNode();
            New->SetFromFunction(Call->GetTargetFunction());
            New->FunctionReference = Call->FunctionReference;
            Creator.Finalize();
            Result = New;
        }
        else if (auto* Make = Cast<UK2Node_MakeStruct>(&Source))
        {
            FGraphNodeCreator<UK2Node_MakeStruct> Creator(Graph);
            auto* New = Creator.CreateNode();
            New->StructType = Make->StructType;
            New->ShowPinForProperties = Make->ShowPinForProperties;
            Creator.Finalize();
            Result = New;
        }
        else if (auto* Get = Cast<UK2Node_VariableGet>(&Source))
        {
            FGraphNodeCreator<UK2Node_VariableGet> Creator(Graph);
            auto* New = Creator.CreateNode();
            New->VariableReference = Get->VariableReference;
            Creator.Finalize();
            Result = New;
        }
        else if (auto* Set = Cast<UK2Node_VariableSet>(&Source))
        {
            FGraphNodeCreator<UK2Node_VariableSet> Creator(Graph);
            auto* New = Creator.CreateNode();
            New->VariableReference = Set->VariableReference;
            Creator.Finalize();
            Result = New;
        }
        if (Result)
        {
            Result->NodeGuid = CopyId(Source.NodeGuid);
        }
        return Result;
    }

    bool Connect(UEdGraphPin* Source, UEdGraphPin* Target)
    {
        if (!Source || !Target || !Target->LinkedTo.IsEmpty())
        {
            return false;
        }
        const auto* Schema = Owner(Source)->GetGraph()->GetSchema();
        return Schema && Schema->CanCreateConnection(Source, Target).Response == CONNECT_RESPONSE_MAKE &&
            Schema->TryCreateConnection(Source, Target) && Edge(Source, Target);
    }

    bool CreateHook(FPlan& Plan, const FHook& Hook, int32 Y, TArray<UEdGraphNode*>& Created)
    {
        FGraphNodeCreator<UK2Node_Event> Creator(*Plan.Events);
        auto* Event = Creator.CreateNode();
        Event->EventReference.SetExternalMember(Hook.Signature->GetFName(), Hook.Signature->GetOwnerClass());
        Event->bOverrideFunction = true;
        Event->NodePosX = 0;
        Event->NodePosY = Y;
        Creator.Finalize();
        Created.Add(Event);
        TMap<UEdGraphNode*, UEdGraphNode*> Copies;
        for (UEdGraphNode* Source : Hook.Nodes)
        {
            auto* Copy = CloneNode(*Plan.Events, *Source);
            if (!Copy)
            {
                return false;
            }
            Created.Add(Copy);
            Copy->NodePosX = 420 + 340 * Copies.Num();
            Copy->NodePosY = Y;
            Copies.Add(Source, Copy);
            if (!SameNode(*Source, *Copy))
            {
                return false;
            }
            for (const auto* Pin : Source->Pins)
            {
                auto* New = CopyPin(Copies, Pin);
                if (!New || New->PinType != Pin->PinType || !New->LinkedTo.IsEmpty() || !Pin->SubPins.IsEmpty())
                {
                    return false;
                }
                New->DefaultValue = Pin->DefaultValue;
                New->DefaultObject = Pin->DefaultObject;
                New->DefaultTextValue = Pin->DefaultTextValue;
            }
        }
        for (UEdGraphNode* Source : Hook.Nodes)
        {
            for (const auto* Pin : Source->Pins)
            {
                if (Pin->Direction != EGPD_Output)
                {
                    continue;
                }
                for (const auto* Link : Pin->LinkedTo)
                {
                    if (auto* Target = CopyPin(Copies, Link); Target && !Connect(CopyPin(Copies, Pin), Target))
                    {
                        return false;
                    }
                }
            }
        }
        return Connect(Out(Event, Then), CopyPin(Copies, Hook.Exec)) &&
            (!Hook.Argument || Connect(Out(Event, Hook.ArgumentName), CopyPin(Copies, Hook.Argument))) &&
            AuditHook(Plan, Hook, Event);
    }

    void RemoveCreated(TArray<UEdGraphNode*>& Created)
    {
        for (int32 Index = Created.Num() - 1; Index >= 0; --Index)
        {
            Created[Index]->DestroyNode();
        }
    }
}

int32 USampleMoverBlueprintMigrationLibrary::ClassifyMoverModeTransitions(UBlueprint* Blueprint)
{
    FPlan Plan;
    return Inspect(Blueprint, Plan);
}

int32 USampleMoverBlueprintMigrationLibrary::InstallMoverModeTransitions(UBlueprint* Blueprint)
{
    FPlan Plan;
    const int32 State = Inspect(Blueprint, Plan);
    if (State != 2)
    {
        return State;
    }
    if (!GEditor || GEditor->PlayWorld || Blueprint->GetOutermost()->IsDirty())
    {
        return Reject(TEXT("PIE is active or the package is dirty"));
    }
    if (Plan.bChild)
    {
        auto* Parent = Cast<UBlueprint>(Blueprint->ParentClass->ClassGeneratedBy);
        if (!Parent || Parent->GetOutermost()->IsDirty() || !VerifyCompiledMoverModeTransitions(Parent))
        {
            return Reject(TEXT("the parent Mover must have a clean, compiled installation"));
        }
    }

    Blueprint->Modify();
    Plan.Events->Modify();
    Plan.Modes->Modify();
    int32 Y = 0;
    for (const UEdGraphNode* Node : Plan.Events->Nodes)
    {
        Y = FMath::Max(Y, Node->NodePosY + 800);
    }
    TArray<UEdGraphNode*> Created;
    for (const FHook& Hook : Plan.Hooks)
    {
        if (!CreateHook(Plan, Hook, Y, Created))
        {
            RemoveCreated(Created);
            return Reject(TEXT("could not construct the verified presentation hook"));
        }
        Y += 800;
    }

    FGraphNodeCreator<UK2Node_CallFunction> CallCreator(*Plan.Modes);
    auto* Call = CallCreator.CreateNode();
    Call->SetFromFunction(Plan.Native);
    Call->NodePosX = Owner(Plan.Dispatch)->NodePosX + 300;
    Call->NodePosY = Owner(Plan.Dispatch)->NodePosY - 300;
    CallCreator.Finalize();
    Created.Add(Call);
    FGraphNodeCreator<UK2Node_IfThenElse> GateCreator(*Plan.Modes);
    auto* Gate = GateCreator.CreateNode();
    Gate->NodePosX = Call->NodePosX + 350;
    Gate->NodePosY = Call->NodePosY;
    GateCreator.Finalize();
    Created.Add(Gate);

    if (!Connect(Out(Plan.Entry, Previous), In(Call, Previous)) ||
        !Connect(Out(Plan.Entry, Next), In(Call, Next)) ||
        !Connect(Out(Call, Then), In(Gate, Execute)) ||
        !Connect(Out(Call, TEXT("ReturnValue")), In(Gate, TEXT("Condition"))))
    {
        RemoveCreated(Created);
        return Reject(TEXT("native gate pin contract differs"));
    }

    Owner(Plan.Dispatch)->Modify();
    Owner(Plan.Fallback)->Modify();
    Plan.Dispatch->BreakLinkTo(Plan.Fallback);
    if (!Connect(Plan.Dispatch, In(Call, Execute)) ||
        !Connect(Out(Gate, TEXT("else")), Plan.Fallback) ||
        ClassifyMoverModeTransitions(Blueprint) != 1)
    {
        RemoveCreated(Created);
        Plan.Dispatch->MakeLinkTo(Plan.Fallback);
        return Reject(TEXT("native gate failed verification; original execution edge restored"));
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return ClassifyMoverModeTransitions(Blueprint);
}

bool USampleMoverBlueprintMigrationLibrary::VerifyCompiledMoverModeTransitions(UBlueprint* Blueprint)
{
    FPlan Plan;
    if (!Blueprint || !Blueprint->IsUpToDate() || Inspect(Blueprint, Plan) != 1 || !Blueprint->GeneratedClass)
    {
        return false;
    }
    const UFunction* ModeFunction = Blueprint->GeneratedClass->FindFunctionByName(ModeGraph);
    if (!ModeFunction || ModeFunction->GetOwnerClass() != Blueprint->GeneratedClass ||
        !Blueprint->GeneratedClass->IsFunctionImplementedInScript(ModeGraph) ||
        Blueprint->GeneratedClass->FindFunctionByName(Plan.Native->GetFName()) != Plan.Native)
    {
        return false;
    }
    for (const FHook& Hook : Plan.Hooks)
    {
        const UFunction* Function = Blueprint->GeneratedClass->FindFunctionByName(Hook.Name);
        if (!Function || Function->GetOwnerClass() != Blueprint->GeneratedClass ||
            !Blueprint->GeneratedClass->IsFunctionImplementedInScript(Hook.Name))
        {
            return false;
        }
    }
    return true;
}

bool USampleMoverBlueprintMigrationLibrary::CompileAndVerifyMoverModeTransitions(UBlueprint* Blueprint)
{
    if (!GEditor || GEditor->PlayWorld || ClassifyMoverModeTransitions(Blueprint) != 1)
    {
        return false;
    }
    FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave);
    return VerifyCompiledMoverModeTransitions(Blueprint);
}
