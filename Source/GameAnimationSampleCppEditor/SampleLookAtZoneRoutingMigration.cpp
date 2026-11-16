#include "SampleLookAtZoneRoutingMigration.h"

#include "Editor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_Knot.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "PackageTools.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogSampleLookAtZoneRoutingMigration, Log, All);

namespace
{
    constexpr TCHAR ControllerPath[] =
        TEXT("/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Controllers/PC_LookAtPOI.PC_LookAtPOI");
    constexpr TCHAR NativePath[] = TEXT("/Script/GameAnimationSampleCpp.SampleLookAtController");
    constexpr TCHAR AnimPath[] =
        TEXT("/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Characters/ABP_LookAtPOI_Char.ABP_LookAtPOI_Char_C");
    constexpr TCHAR GizmoPath[] =
        TEXT("/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Props/BP_LookAtPOI_TargetGizmo.BP_LookAtPOI_TargetGizmo_C");
    constexpr TCHAR WidgetBlueprintPath[] =
        TEXT("/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Widget/WB_LookAtPOI__GUI.WB_LookAtPOI__GUI");
    constexpr TCHAR WidgetPath[] =
        TEXT("/Game/IsolatedExamples/LookAtPOI/TestLevelContent/Widget/WB_LookAtPOI__GUI.WB_LookAtPOI__GUI_C");
    const FName SelectionName(TEXT("Zone Was Selected"));
    const FName CaptureName(TEXT("CaptureLookAtZoneSelection"));
    const FName RouteName(TEXT("RouteLookAtZoneSelection"));
    const FName ApplyName(TEXT("ApplyLookAtZoneSelection"));
    const FName NotifyName(TEXT("NotifyLookAtZoneSelected"));
    const FName WidgetEventName(TEXT("Zone Selected"));
    const FName TargetName(TEXT("Set Look At Target On Anim Instance"));
    const FName ResolveGizmoName(TEXT("ResolveLookAtGizmoTarget"));

    void LogAuditRejection(const UBlueprint* Blueprint, const TCHAR* Stage, const TCHAR* Reason)
    {
        UE_LOG(LogSampleLookAtZoneRoutingMigration, Warning,
            TEXT("Look At zone routing audit rejected %s at %s: %s"),
            *GetPathNameSafe(Blueprint), Stage, Reason);
    }

    int32 RejectAudit(const UBlueprint* Blueprint, const TCHAR* Stage, const TCHAR* Reason)
    {
        LogAuditRejection(Blueprint, Stage, Reason);
        return 0;
    }

    bool RejectTail(const UBlueprint* Blueprint, const TCHAR* Stage, const TCHAR* Reason)
    {
        LogAuditRejection(Blueprint, Stage, Reason);
        return false;
    }

    bool IsTarget(const UBlueprint* Blueprint)
    {
        return Blueprint && Blueprint->GetPathName() == ControllerPath &&
            Blueprint->ParentClass && Blueprint->ParentClass->GetPathName() == NativePath;
    }

    UEdGraphPin* Pin(const UEdGraphNode* Node, FName Name, EEdGraphPinDirection Direction)
    {
        return Node ? Node->FindPin(Name, Direction) : nullptr;
    }

    UEdGraphPin* Then(const UEdGraphNode* Node)
    {
        return Pin(Node, UEdGraphSchema_K2::PN_Then, EGPD_Output);
    }

    UEdGraphPin* Exec(const UEdGraphNode* Node)
    {
        return Pin(Node, UEdGraphSchema_K2::PN_Execute, EGPD_Input);
    }

    template<typename T>
    T* PeerNode(const UEdGraphPin* From)
    {
        return From && From->LinkedTo.Num() == 1 ? Cast<T>(From->LinkedTo[0]->GetOwningNode()) : nullptr;
    }

    bool Linked(const UEdGraphPin* A, const UEdGraphPin* B)
    {
        return A && B && !A->bOrphanedPin && !B->bOrphanedPin &&
            A->GetOwningNode()->GetGraph() == B->GetOwningNode()->GetGraph() &&
            A->LinkedTo.Contains(B) && B->LinkedTo.Contains(A);
    }

    bool Links(const UEdGraphNode* Node, int32 Expected)
    {
        if (!Node)
        {
            return false;
        }
        int32 Total = 0;
        for (const UEdGraphPin* GraphPin : Node->Pins)
        {
            if (GraphPin->bOrphanedPin)
            {
                return false;
            }
            Total += GraphPin->LinkedTo.Num();
        }
        return Total == Expected;
    }

    bool HasType(const UEdGraphPin* GraphPin, FName Category, const TCHAR* ObjectPath = nullptr)
    {
        return GraphPin && GraphPin->PinType.PinCategory == Category &&
            GraphPin->PinType.ContainerType == EPinContainerType::None &&
            (!ObjectPath || GetPathNameSafe(GraphPin->PinType.PinSubCategoryObject.Get()) == ObjectPath);
    }

    bool TrueLiteral(const UEdGraphPin* Value)
    {
        return HasType(Value, UEdGraphSchema_K2::PC_Boolean) &&
            Value->LinkedTo.IsEmpty() && Value->DefaultValue == TEXT("true");
    }

    bool OnSelf(const UK2Node_CallFunction* Call)
    {
        const UEdGraphPin* Self = Pin(Call, UEdGraphSchema_K2::PN_Self, EGPD_Input);
        return Self && Self->LinkedTo.IsEmpty() && !Self->DefaultObject;
    }

    bool ReadsSelf(const UK2Node_VariableGet* Get, FName Name, const UEdGraphPin* Consumer)
    {
        return Get && Get->GetVarName() == Name && Get->VariableReference.IsSelfContext() &&
            Links(Get, 1) && Linked(Pin(Get, Name, EGPD_Output), Consumer);
    }

    const UBlueprint* ExactWidgetBlueprint(const UClass* Class)
    {
        const auto* Owner = Class ? Cast<UBlueprint>(Class->ClassGeneratedBy) : nullptr;
        if (!Owner || GetPathNameSafe(Owner) != WidgetBlueprintPath ||
            GetPathNameSafe(Owner->GeneratedClass) != WidgetPath ||
            (Class != Owner->GeneratedClass && Class != Owner->SkeletonGeneratedClass))
        {
            return nullptr;
        }
        return Owner;
    }

    bool HasWidgetType(const UEdGraphPin* GraphPin, const UBlueprint* Owner)
    {
        return Owner && HasType(GraphPin, UEdGraphSchema_K2::PC_Object) &&
            ExactWidgetBlueprint(Cast<UClass>(GraphPin->PinType.PinSubCategoryObject.Get())) == Owner;
    }

    const TCHAR* WidgetContractFailure(const UFunction* Function)
    {
        if (!Function)
        {
            return TEXT("the Level Widget class does not resolve a Zone Selected function");
        }
        if (GetPathNameSafe(Function->GetOwnerClass()) != WidgetPath)
        {
            return TEXT("Zone Selected is not owned by WB_LookAtPOI__GUI");
        }
        if (Function->NumParms != 2)
        {
            return TEXT("Zone Selected must declare exactly two parameters");
        }
        if (!Function->HasAnyFunctionFlags(FUNC_BlueprintEvent))
        {
            return TEXT("Zone Selected must be a Blueprint event");
        }
        if (Function->HasAnyFunctionFlags(FUNC_Static | FUNC_Net | FUNC_Delegate | FUNC_MulticastDelegate))
        {
            return TEXT("Zone Selected has unsupported static, network, or delegate flags");
        }
        const auto IsInput = [](const FProperty* Property)
        {
            return Property && Property->ArrayDim == 1 && Property->HasAnyPropertyFlags(CPF_Parm) &&
                !Property->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm | CPF_ConstParm);
        };
        if (!IsInput(FindFProperty<FStrProperty>(Function, TEXT("Zone Type"))))
        {
            return TEXT("Zone Type must be a non-reference input FString");
        }
        const auto* Anim = FindFProperty<FObjectProperty>(Function, TEXT("Anim Instance"));
        if (!IsInput(Anim))
        {
            return TEXT("Anim Instance must be a non-reference input object parameter");
        }
        if (GetPathNameSafe(Anim->PropertyClass) != AnimPath)
        {
            return TEXT("Anim Instance must reference ABP_LookAtPOI_Char");
        }
        return nullptr;
    }

    bool AuditAuthoredTail(const UBlueprint* Blueprint, const UK2Node_CallFunction* Target,
        const UClass* NativeClass, bool bNativeNotify, UK2Node_CallFunction*& Notify)
    {
        const auto* AnimCast = PeerNode<UK2Node_DynamicCast>(Then(Target));
        const auto* Enable = PeerNode<UK2Node_VariableSet>(Then(AnimCast));
        Notify = PeerNode<UK2Node_CallFunction>(Then(Enable));
        if (!Target || Target->FunctionReference.GetMemberName() != TargetName ||
            !Target->FunctionReference.IsSelfContext() || !OnSelf(Target) || !Links(Target, 2) ||
            !AnimCast || GetPathNameSafe(AnimCast->TargetType.Get()) != AnimPath || !Links(AnimCast, 5) ||
            !Enable || Enable->GetVarName() != TEXT("LookAtPOI Enabled") || !Links(Enable, 3) ||
            !TrueLiteral(Pin(Enable, TEXT("LookAtPOI Enabled"), EGPD_Input)) ||
            GetPathNameSafe(Enable->VariableReference.GetMemberParentClass()) != AnimPath || !Links(Notify, 4))
        {
            return RejectTail(Blueprint, TEXT("target/cast/enable tail"),
                TEXT("the target call, animation cast, enable setter, or notify node failed its identity or pin-link checks"));
        }

        const UFunction* Function = Notify->GetTargetFunction();
        if (bNativeNotify && (!Function || Function->GetFName() != NotifyName ||
            Function->GetOwnerClass() != NativeClass || !OnSelf(Notify)))
        {
            return RejectTail(Blueprint, TEXT("native function"),
                TEXT("the notify node does not resolve to the expected controller function"));
        }

        const auto* Source = AnimCast->GetCastSourcePin();
        const auto* Result = AnimCast->GetCastResultPin();
        const auto* Widget = Pin(Notify, bNativeNotify ? FName(TEXT("Widget")) : UEdGraphSchema_K2::PN_Self, EGPD_Input);
        const auto* ZoneType = Pin(Notify, bNativeNotify ? TEXT("ZoneType") : TEXT("Zone Type"), EGPD_Input);
        const auto* Anim = Pin(Notify, bNativeNotify ? TEXT("AnimInstance") : TEXT("Anim Instance"), EGPD_Input);
        const auto* WidgetGet = PeerNode<UK2Node_VariableGet>(Widget);
        const auto* WidgetValue = Pin(WidgetGet, TEXT("Level Widget"), EGPD_Output);
        const auto* WidgetClass = WidgetValue ? Cast<UClass>(WidgetValue->PinType.PinSubCategoryObject.Get()) : nullptr;
        const auto* WidgetOwner = ExactWidgetBlueprint(WidgetClass);
        if (!WidgetOwner)
        {
            return RejectTail(Blueprint, TEXT("widget identity"),
                *FString::Printf(TEXT("Level Widget has unexpected class %s"), *GetPathNameSafe(WidgetClass)));
        }
        const auto* WidgetFunction = WidgetOwner->GeneratedClass->FindFunctionByName(WidgetEventName);
        if (const TCHAR* Failure = WidgetContractFailure(WidgetFunction))
        {
            return RejectTail(Blueprint, TEXT("widget signature"), Failure);
        }
        if (!bNativeNotify)
        {
            const UClass* FunctionOwner = Function ? Function->GetOwnerClass() : nullptr;
            const UClass* MemberParent = Notify->FunctionReference.GetMemberParentClass();
            if (!Function || Function->GetFName() != WidgetEventName ||
                Notify->FunctionReference.GetMemberName() != WidgetEventName ||
                ExactWidgetBlueprint(FunctionOwner) != WidgetOwner ||
                ExactWidgetBlueprint(MemberParent) != WidgetOwner ||
                Function != FunctionOwner->FindFunctionByName(WidgetEventName))
            {
                return RejectTail(Blueprint, TEXT("widget function identity"),
                    *FString::Printf(TEXT("Zone Selected resolution mismatch: function=%s, owner=%s, member=%s, parent=%s"),
                        *GetPathNameSafe(Function), *GetPathNameSafe(FunctionOwner),
                        *Notify->FunctionReference.GetMemberName().ToString(), *GetPathNameSafe(MemberParent)));
            }
        }

        if (!HasWidgetType(WidgetValue, WidgetOwner) ||
            !(bNativeNotify ? HasType(Widget, UEdGraphSchema_K2::PC_Object, TEXT("/Script/UMG.UserWidget"))
                : HasWidgetType(Widget, WidgetOwner)) ||
            !HasType(ZoneType, UEdGraphSchema_K2::PC_String) ||
            !HasType(Anim, UEdGraphSchema_K2::PC_Object,
                bNativeNotify ? TEXT("/Script/Engine.AnimInstance") : AnimPath) ||
            !Linked(Then(Target), Exec(AnimCast)) || !Linked(Then(AnimCast), Exec(Enable)) ||
            !Linked(Then(Enable), Exec(Notify)) ||
            !Linked(Result, Pin(Enable, UEdGraphSchema_K2::PN_Self, EGPD_Input)) || !Linked(Result, Anim) ||
            !ReadsSelf(PeerNode<UK2Node_VariableGet>(Source), TEXT("AnimInstance"), Source) ||
            !ReadsSelf(WidgetGet, TEXT("Level Widget"), Widget) ||
            !ReadsSelf(PeerNode<UK2Node_VariableGet>(ZoneType), TEXT("Selected Zone Type"), ZoneType))
        {
            return RejectTail(Blueprint, TEXT("final link checks"),
                TEXT("the notify pin types, execution flow, cast result, or controller variable links are inconsistent"));
        }
        return true;
    }

    struct FZoneChain
    {
        UEdGraph* Graph = nullptr;
        UK2Node_CustomEvent* Selection = nullptr;
        UK2Node_CallFunction* Call = nullptr;
        UK2Node_IfThenElse* Gate = nullptr;
        UK2Node_VariableSet* Enable = nullptr;
        UK2Node_CallFunction* Target = nullptr;
        UK2Node_CallFunction* Notify = nullptr;
    };

    bool MoveInput(UEdGraph& Graph, const UEdGraphNode& OldNode, FName OldName,
        const UEdGraphNode& NewNode, FName NewName)
    {
        auto* FromInput = Pin(&OldNode, OldName, EGPD_Input);
        auto* ToInput = Pin(&NewNode, NewName, EGPD_Input);
        if (!FromInput || FromInput->LinkedTo.Num() != 1 || !ToInput)
        {
            return false;
        }
        auto* Source = FromInput->LinkedTo[0];
        FromInput->BreakLinkTo(Source);
        return Graph.GetSchema()->TryCreateConnection(Source, ToInput);
    }

    int32 AuditRouting(UBlueprint* Blueprint, FZoneChain& Chain)
    {
        if (GEditor && GEditor->IsPlaySessionInProgress())
        {
            return RejectAudit(Blueprint, TEXT("target/class"), TEXT("a Play In Editor session is active"));
        }
        if (!IsTarget(Blueprint) || !Blueprint->GeneratedClass ||
            !FindFProperty<FBoolProperty>(Blueprint->GeneratedClass, TEXT("Look At Enabled")) ||
            !Blueprint->FunctionGraphs.ContainsByPredicate([](const UEdGraph* Graph)
            {
                return Graph && Graph->GetFName() == TargetName;
            }))
        {
            return RejectAudit(Blueprint, TEXT("target/class"),
                TEXT("expected canonical PC_LookAtPOI derived from SampleLookAtController with Look At Enabled and the target function graph"));
        }

        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        UK2Node_Event* Callback = nullptr;
        int32 RouteCalls = 0;
        int32 NotifyCalls = 0;
        for (UEdGraph* Graph : Graphs)
        {
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (auto* Custom = Cast<UK2Node_CustomEvent>(Node); Custom && Custom->CustomFunctionName == SelectionName)
                {
                    if (Chain.Selection || Graph->GetOuter() != Blueprint || Graph->GetFName() != TEXT("EventGraph"))
                    {
                        return RejectAudit(Blueprint, TEXT("chain"),
                            TEXT("Zone Was Selected must be unique and authored in the controller Event Graph"));
                    }
                    Chain.Selection = Custom;
                    Chain.Graph = Graph;
                }
                if (auto* Event = Cast<UK2Node_Event>(Node); Event &&
                    (Event->EventReference.GetMemberName() == ApplyName || Event->CustomFunctionName == ApplyName))
                {
                    if (Callback || !Event->bOverrideFunction || Event->EventReference.GetMemberName() != ApplyName ||
                        Event->EventReference.GetMemberParentClass() != Blueprint->ParentClass)
                    {
                        return RejectAudit(Blueprint, TEXT("chain"),
                            TEXT("ApplyLookAtZoneSelection must be a unique override of the controller native function"));
                    }
                    Callback = Event;
                }
                if (const auto* Call = Cast<UK2Node_CallFunction>(Node))
                {
                    RouteCalls += Call->FunctionReference.GetMemberName() == RouteName;
                    NotifyCalls += Call->FunctionReference.GetMemberName() == NotifyName;
                }
            }
        }

        const auto* Pawn = Pin(Chain.Selection, TEXT("Active  Pawn"), EGPD_Output);
        const auto* Type = Pin(Chain.Selection, TEXT("Type"), EGPD_Output);
        const auto* Gizmo = Pin(Chain.Selection, TEXT("Gizmo Onj"), EGPD_Output);
        Chain.Call = PeerNode<UK2Node_CallFunction>(Then(Chain.Selection));
        if (!Chain.Call || !Links(Chain.Selection, 4) || Chain.Selection->Pins.Num() != 5 ||
            (Chain.Selection->FunctionFlags & FUNC_Net) != 0 ||
            !HasType(Pawn, UEdGraphSchema_K2::PC_Object, TEXT("/Script/Engine.Pawn")) ||
            !HasType(Type, UEdGraphSchema_K2::PC_String) || !HasType(Gizmo, UEdGraphSchema_K2::PC_Object, GizmoPath) ||
            !OnSelf(Chain.Call) || !Linked(Then(Chain.Selection), Exec(Chain.Call)) ||
            !Linked(Pawn, Pin(Chain.Call, TEXT("SelectedPawn"), EGPD_Input)) ||
            !Linked(Type, Pin(Chain.Call, TEXT("ZoneType"), EGPD_Input)))
        {
            return RejectAudit(Blueprint, TEXT("chain"),
                TEXT("the Zone Was Selected event, native call, or Pawn/ZoneType/Gizmo connection failed its structural checks"));
        }

        const auto* Knot = PeerNode<UK2Node_Knot>(Gizmo);
        if (!Knot || !Links(Knot, 2) || !Linked(Gizmo, Knot->GetInputPin()) ||
            !Linked(Knot->GetOutputPin(), Pin(Chain.Call, TEXT("Gizmo"), EGPD_Input)))
        {
            return RejectAudit(Blueprint, TEXT("knot"),
                TEXT("the Gizmo output must connect through one reroute knot to the native function input"));
        }

        const UFunction* Native = Chain.Call->GetTargetFunction();
        if (!Native || Native->GetOwnerClass() != Blueprint->ParentClass)
        {
            return RejectAudit(Blueprint, TEXT("native function"),
                TEXT("the selection call must resolve to a function owned by SampleLookAtController"));
        }
        if (Native->GetFName() == RouteName)
        {
            Chain.Target = PeerNode<UK2Node_CallFunction>(Then(Callback));
            if (RouteCalls != 1 || NotifyCalls > 1 || !Links(Chain.Call, 4) || !Callback ||
                Callback->GetGraph() != Chain.Graph || Callback->Pins.Num() != 2 || !Links(Callback, 1))
            {
                return RejectAudit(Blueprint, TEXT("chain"),
                    TEXT("the route entry and ApplyLookAtZoneSelection override failed their graph and pin-link checks"));
            }
            if (!Linked(Then(Callback), Exec(Chain.Target)))
            {
                return RejectAudit(Blueprint, TEXT("final link checks"),
                    TEXT("ApplyLookAtZoneSelection must execute the target call"));
            }
            if (!AuditAuthoredTail(Blueprint, Chain.Target, Blueprint->ParentClass, NotifyCalls == 1, Chain.Notify))
            {
                return 0;
            }
            return NotifyCalls == 1 ? 1 : 3;
        }

        Chain.Gate = PeerNode<UK2Node_IfThenElse>(Then(Chain.Call));
        Chain.Enable = PeerNode<UK2Node_VariableSet>(Then(Chain.Gate));
        Chain.Target = PeerNode<UK2Node_CallFunction>(Then(Chain.Enable));
        if (Native->GetFName() != CaptureName)
        {
            return RejectAudit(Blueprint, TEXT("native function"),
                TEXT("the selection call must invoke RouteLookAtZoneSelection or CaptureLookAtZoneSelection"));
        }
        if (RouteCalls != 0 || NotifyCalls != 0 || Callback || !Links(Chain.Call, 6) ||
            !Links(Chain.Gate, 3) || !Links(Chain.Enable, 2) ||
            Chain.Enable->GetVarName() != TEXT("Look At Enabled") ||
            !Chain.Enable->VariableReference.IsSelfContext() ||
            !TrueLiteral(Pin(Chain.Enable, TEXT("Look At Enabled"), EGPD_Input)))
        {
            return RejectAudit(Blueprint, TEXT("chain"),
                TEXT("the capture branch, controller enable setter, or route/notify exclusion checks failed"));
        }
        if (!Linked(Then(Chain.Call), Exec(Chain.Gate)) ||
            !Linked(Chain.Call->GetReturnValuePin(), Chain.Gate->GetConditionPin()) ||
            !Linked(Then(Chain.Gate), Exec(Chain.Enable)) || !Linked(Then(Chain.Enable), Exec(Chain.Target)))
        {
            return RejectAudit(Blueprint, TEXT("final link checks"),
                TEXT("the capture call, branch, enable setter, and target call execution links are inconsistent"));
        }
        if (!AuditAuthoredTail(Blueprint, Chain.Target, Blueprint->ParentClass, false, Chain.Notify))
        {
            return 0;
        }
        return 2;
    }

    struct FGizmoTargetChain
    {
        UEdGraph* Graph = nullptr;
        UEdGraphPin* Gizmo = nullptr;
        UEdGraphPin* Target = nullptr;
        TArray<UEdGraphNode*> OldNodes;
        bool bSafe = false;
    };

    bool IsGizmoResolver(const UFunction* Function, const UClass* NativeClass)
    {
        const auto* Input = Function ? FindFProperty<FObjectProperty>(Function, TEXT("Gizmo")) : nullptr;
        const auto* Output = Function ? CastField<FStructProperty>(Function->GetReturnProperty()) : nullptr;
        return Function && Function->GetOwnerClass() == NativeClass && Function->NumParms == 2 &&
            Function->HasAllFunctionFlags(FUNC_BlueprintPure | FUNC_Const) &&
            Input && Input->ArrayDim == 1 && Input->PropertyClass == AActor::StaticClass() &&
            Input->HasAnyPropertyFlags(CPF_Parm) &&
            !Input->HasAnyPropertyFlags(CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm) &&
            Output && Output->ArrayDim == 1 && Output->Struct == TBaseStructure<FVector>::Get();
    }

    // The two authored branches evaluate their pure inputs only after the corresponding cast.
    // Audit the exact gizmo expression before replacing it, leaving the player socket branch intact.
    bool AuditGizmoTarget(UBlueprint* Blueprint, FGizmoTargetChain& Chain)
    {
        for (UEdGraph* Graph : Blueprint->FunctionGraphs)
        {
            if (Graph && Graph->GetFName() == TargetName)
            {
                if (Chain.Graph || Graph->GetOuter() != Blueprint)
                {
                    return false;
                }
                Chain.Graph = Graph;
            }
        }
        if (!Chain.Graph)
        {
            return false;
        }
        const auto Node = [&Chain](const TCHAR* Guid) -> UEdGraphNode*
        {
            for (UEdGraphNode* Candidate : Chain.Graph->Nodes)
            {
                if (Candidate && Candidate->NodeGuid.ToString(EGuidFormats::Digits) == Guid)
                {
                    return Candidate;
                }
            }
            return nullptr;
        };
        const auto Edge = [](const UEdGraphNode* A, FName Out, const UEdGraphNode* B, FName In)
        {
            return Linked(Pin(A, Out, EGPD_Output), Pin(B, In, EGPD_Input));
        };
        const auto PureCall = [](const UEdGraphNode* N, FName Name, int32 Count)
        {
            const auto* Call = Cast<UK2Node_CallFunction>(N);
            return Call && Call->FunctionReference.GetMemberName() == Name &&
                Call->IsNodePure() && Links(Call, Count);
        };
        const auto MathCall = [&PureCall](const UEdGraphNode* N, FName Name, int32 Count)
        {
            const auto* Call = Cast<UK2Node_CallFunction>(N);
            return PureCall(N, Name, Count) &&
                GetPathNameSafe(Call->FunctionReference.GetMemberParentClass()) == TEXT("/Script/Engine.KismetMathLibrary");
        };

        const auto* Entry = Cast<UK2Node_FunctionEntry>(Node(TEXT("82684D0D4AC70C6634D54EAEDE545DE1")));
        const auto* InitialGate = Cast<UK2Node_IfThenElse>(Node(TEXT("5D62DE724D4EF73CFCEE5D9174AB51AC")));
        const auto* ModeGate = Cast<UK2Node_IfThenElse>(Node(TEXT("B0D38EE74A938400151918AE5D97B425")));
        const auto* Mode = Cast<UK2Node_VariableGet>(Node(TEXT("F3E78F474B78BC446CD1E78FAC564DB3")));
        const auto* GizmoCast = Cast<UK2Node_DynamicCast>(Node(TEXT("A8A040394FE8BCCFA7612DB0D2021C22")));
        const auto* PlayerCast = Cast<UK2Node_DynamicCast>(Node(TEXT("6D2958DD4FCA0EDBFB8152A312DD2BDF")));
        const auto* GizmoSet = Cast<UK2Node_VariableSet>(Node(TEXT("F4336AB94070811177E3D88A07A742FA")));
        const auto* PlayerSet = Cast<UK2Node_VariableSet>(Node(TEXT("806A7AB144EFC0D430AD91BFF51E0E28")));
        const auto* GizmoGet = Cast<UK2Node_VariableGet>(Node(TEXT("B179B6DE4234CF5B5B03B7BDBC364DCF")));
        const auto* PlayerGet = Cast<UK2Node_VariableGet>(Node(TEXT("F3C782DA4C301B1975FC83A4358E5CA1")));
        const auto* Mesh = Cast<UK2Node_CallFunction>(Node(TEXT("D41D859B44287CF46EA31BAAE32F1C26")));
        const auto* Socket = Cast<UK2Node_CallFunction>(Node(TEXT("86627BDE4848D0FB8F44049C361E372F")));
        Chain.Gizmo = Pin(GizmoGet, TEXT("Gizmo Obj"), EGPD_Output);
        Chain.Target = Pin(GizmoSet, TEXT("LookAtPOI Target"), EGPD_Input);

        const auto AuditBranch = [&](const UK2Node_DynamicCast* CastNode, const UK2Node_VariableSet* Set,
            const TCHAR* AnimGetGuid, FName Branch)
        {
            const auto* AnimGet = Cast<UK2Node_VariableGet>(Node(AnimGetGuid));
            return CastNode && GetPathNameSafe(CastNode->TargetType.Get()) == AnimPath && Links(CastNode, 4) &&
                Set && Set->GetVarName() == TEXT("LookAtPOI Target") && Links(Set, 3) &&
                GetPathNameSafe(Set->VariableReference.GetMemberParentClass()) == AnimPath &&
                ReadsSelf(AnimGet, TEXT("AnimInstance"), CastNode->GetCastSourcePin()) &&
                Edge(ModeGate, Branch, CastNode, UEdGraphSchema_K2::PN_Execute) &&
                Linked(Then(CastNode), Exec(Set)) &&
                Linked(CastNode->GetCastResultPin(), Pin(Set, UEdGraphSchema_K2::PN_Self, EGPD_Input));
        };
        const auto* ComponentClass = Pin(Mesh, TEXT("ComponentClass"), EGPD_Input);
        const auto* SocketName = Pin(Socket, TEXT("InSocketName"), EGPD_Input);
        if (!Entry || !Links(Entry, 1) || !Links(InitialGate, 2) ||
            !TrueLiteral(Pin(InitialGate, TEXT("Condition"), EGPD_Input)) ||
            !Linked(Then(Entry), Exec(InitialGate)) || !Linked(Then(InitialGate), Exec(ModeGate)) ||
            !Links(ModeGate, 4) || !ReadsSelf(Mode, TEXT("Look At Player"), Pin(ModeGate, TEXT("Condition"), EGPD_Input)) ||
            !AuditBranch(PlayerCast, PlayerSet, TEXT("501CDD604390C8683D1069A4DCFEE01A"), UEdGraphSchema_K2::PN_Then) ||
            !AuditBranch(GizmoCast, GizmoSet, TEXT("A9AA69BF41874D4F7E6298BBB938238E"), UEdGraphSchema_K2::PN_Else) ||
            !GizmoGet || !GizmoGet->VariableReference.IsSelfContext() || GizmoGet->GetVarName() != TEXT("Gizmo Obj") ||
            !HasType(Chain.Gizmo, UEdGraphSchema_K2::PC_Object, GizmoPath) ||
            !HasType(Chain.Target, UEdGraphSchema_K2::PC_Struct, TEXT("/Script/CoreUObject.Vector")) ||
            !PureCall(Mesh, TEXT("GetComponentByClass"), 2) ||
            !ReadsSelf(PlayerGet, TEXT("Player Pawn"), Pin(Mesh, UEdGraphSchema_K2::PN_Self, EGPD_Input)) ||
            !ComponentClass || !ComponentClass->LinkedTo.IsEmpty() ||
            GetPathNameSafe(ComponentClass->DefaultObject) != TEXT("/Script/Engine.SkeletalMeshComponent") ||
            !PureCall(Socket, TEXT("GetSocketLocation"), 2) || !SocketName ||
            !SocketName->LinkedTo.IsEmpty() || SocketName->DefaultValue != TEXT("head") ||
            !Edge(Mesh, TEXT("ReturnValue"), Socket, UEdGraphSchema_K2::PN_Self) ||
            !Edge(Socket, TEXT("ReturnValue"), PlayerSet, TEXT("LookAtPOI Target")))
        {
            return false;
        }

        auto* Location = Node(TEXT("C3172AF643D3364E4FBFEDA781C481B4"));
        auto* Rotation = Node(TEXT("D9082F164CD4E5EC8733A8967444130F"));
        auto* Scale = Node(TEXT("86F32B1449C3DA29BF1E5FBBD099D389"));
        auto* Right = Node(TEXT("EF137FA74B28025FBBCF0BA29C75C6BE"));
        auto* Offset = Node(TEXT("AA0358FD40FCBBA77BF88E83B039AD36"));
        auto* Multiply = Node(TEXT("24C7CFED423E83F4D216F2983866ABF7"));
        auto* Add = Node(TEXT("99A39569459352CB2E68A0914C0458EB"));

        int32 ResolveCalls = 0;
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (const UEdGraph* Graph : Graphs)
        {
            for (const UEdGraphNode* GraphNode : Graph->Nodes)
            {
                if (const auto* Call = Cast<UK2Node_CallFunction>(GraphNode))
                {
                    ResolveCalls += Call->FunctionReference.GetMemberName() == ResolveGizmoName;
                }
            }
        }
        if (ResolveCalls == 1)
        {
            const auto* Resolve = PeerNode<UK2Node_CallFunction>(Chain.Target);
            const auto* Function = Resolve ? Resolve->GetTargetFunction() : nullptr;
            Chain.bSafe = !Location && !Rotation && !Scale && !Right && !Offset && !Multiply && !Add &&
                Links(GizmoGet, 1) && PureCall(Resolve, ResolveGizmoName, 2) && OnSelf(Resolve) &&
                IsGizmoResolver(Function, Blueprint->ParentClass) &&
                HasType(Pin(Resolve, TEXT("Gizmo"), EGPD_Input), UEdGraphSchema_K2::PC_Object, TEXT("/Script/Engine.Actor")) &&
                HasType(Pin(Resolve, TEXT("ReturnValue"), EGPD_Output), UEdGraphSchema_K2::PC_Struct, TEXT("/Script/CoreUObject.Vector")) &&
                Edge(GizmoGet, TEXT("Gizmo Obj"), Resolve, TEXT("Gizmo")) &&
                Edge(Resolve, TEXT("ReturnValue"), GizmoSet, TEXT("LookAtPOI Target"));
            return Chain.bSafe;
        }

        const auto* Factor = Pin(Offset, TEXT("B"), EGPD_Input);
        const auto* ScaleX = Pin(Scale, TEXT("ReturnValue_X"), EGPD_Output);
        const auto IsDouble = [](const UEdGraphPin* Value)
        {
            return HasType(Value, UEdGraphSchema_K2::PC_Real) && Value->PinType.PinSubCategory == TEXT("double");
        };
        if (ResolveCalls != 0 || !Links(GizmoGet, 3) || !PureCall(Location, TEXT("K2_GetActorLocation"), 2) ||
            !PureCall(Rotation, TEXT("K2_GetActorRotation"), 2) || !PureCall(Scale, TEXT("GetActorScale3D"), 2) ||
            !MathCall(Right, TEXT("GetRightVector"), 2) || !MathCall(Offset, TEXT("Multiply_DoubleDouble"), 2) ||
            !MathCall(Multiply, TEXT("Multiply_VectorVector"), 3) || !MathCall(Add, TEXT("Add_VectorVector"), 3) ||
            !IsDouble(Factor) || !Factor->LinkedTo.IsEmpty() || Factor->DefaultValue != TEXT("50.000000") ||
            !IsDouble(ScaleX) || ScaleX->ParentPin != Pin(Scale, TEXT("ReturnValue"), EGPD_Output) ||
            !IsDouble(Pin(Offset, TEXT("A"), EGPD_Input)) ||
            !IsDouble(Pin(Offset, TEXT("ReturnValue"), EGPD_Output)) ||
            !IsDouble(Pin(Multiply, TEXT("B"), EGPD_Input)) ||
            !Edge(GizmoGet, TEXT("Gizmo Obj"), Location, UEdGraphSchema_K2::PN_Self) ||
            !Edge(GizmoGet, TEXT("Gizmo Obj"), Rotation, UEdGraphSchema_K2::PN_Self) ||
            !Edge(GizmoGet, TEXT("Gizmo Obj"), Scale, UEdGraphSchema_K2::PN_Self) ||
            !Edge(Rotation, TEXT("ReturnValue"), Right, TEXT("InRot")) ||
            !Edge(Scale, TEXT("ReturnValue_X"), Offset, TEXT("A")) ||
            !Edge(Offset, TEXT("ReturnValue"), Multiply, TEXT("B")) ||
            !Edge(Right, TEXT("ReturnValue"), Multiply, TEXT("A")) ||
            !Edge(Location, TEXT("ReturnValue"), Add, TEXT("A")) ||
            !Edge(Multiply, TEXT("ReturnValue"), Add, TEXT("B")) ||
            !Edge(Add, TEXT("ReturnValue"), GizmoSet, TEXT("LookAtPOI Target")))
        {
            return false;
        }
        Chain.OldNodes = {Location, Rotation, Scale, Right, Offset, Multiply, Add};
        return true;
    }

    int32 Audit(UBlueprint* Blueprint, FZoneChain& Chain, FGizmoTargetChain& GizmoTarget)
    {
        const int32 RoutingState = AuditRouting(Blueprint, Chain);
        if (!RoutingState)
        {
            return 0;
        }
        if (!AuditGizmoTarget(Blueprint, GizmoTarget) || (GizmoTarget.bSafe && RoutingState != 1))
        {
            return RejectAudit(Blueprint, TEXT("target expression"),
                TEXT("the authored player/gizmo branches or native gizmo resolver failed the graph audit"));
        }
        return RoutingState == 1 && !GizmoTarget.bSafe ? 4 : RoutingState;
    }

    bool InstallGizmoTarget(UBlueprint* Blueprint, const FGizmoTargetChain& Chain, const UFunction* Resolve)
    {
        Chain.Graph->Modify();
        FGraphNodeCreator<UK2Node_CallFunction> Creator(*Chain.Graph);
        auto* Call = Creator.CreateNode();
        Call->SetFromFunction(Resolve);
        Call->NodePosX = Chain.OldNodes.Last()->NodePosX;
        Call->NodePosY = Chain.OldNodes.Last()->NodePosY;
        Creator.Finalize();

        auto* Input = Pin(Call, TEXT("Gizmo"), EGPD_Input);
        auto* Output = Pin(Call, TEXT("ReturnValue"), EGPD_Output);
        if (!Input || !Output || !Chain.Graph->GetSchema()->TryCreateConnection(Chain.Gizmo, Input))
        {
            return false;
        }
        Chain.Target->BreakLinkTo(Pin(Chain.OldNodes.Last(), TEXT("ReturnValue"), EGPD_Output));
        if (!Chain.Graph->GetSchema()->TryCreateConnection(Output, Chain.Target))
        {
            return false;
        }
        for (UEdGraphNode* Node : Chain.OldNodes)
        {
            FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
        }
        return true;
    }
}

int32 USampleLookAtZoneRoutingMigration::ClassifyZoneRouting(UBlueprint* Blueprint)
{
    FZoneChain Chain;
    FGizmoTargetChain GizmoTarget;
    return Audit(Blueprint, Chain, GizmoTarget);
}

static bool InstallSelectionEntry(UBlueprint* Blueprint, const FZoneChain& Chain,
    const UFunction* Route, const UFunction* Apply)
{
    FGraphNodeCreator<UK2Node_CallFunction> RouteCreator(*Chain.Graph);
    auto* Call = RouteCreator.CreateNode();
    Call->SetFromFunction(Route);
    Call->NodePosX = Chain.Call->NodePosX;
    Call->NodePosY = Chain.Call->NodePosY;
    RouteCreator.Finalize();

    FGraphNodeCreator<UK2Node_Event> ApplyCreator(*Chain.Graph);
    auto* Event = ApplyCreator.CreateNode();
    Event->EventReference.SetExternalMember(ApplyName, Apply->GetOwnerClass());
    Event->bOverrideFunction = true;
    Event->NodePosX = Chain.Enable->NodePosX;
    Event->NodePosY = Chain.Enable->NodePosY;
    ApplyCreator.Finalize();

    const auto* Schema = Chain.Graph->GetSchema();
    for (const FName Input : {UEdGraphSchema_K2::PN_Execute, FName(TEXT("SelectedPawn")),
        FName(TEXT("ZoneType")), FName(TEXT("Gizmo"))})
    {
        if (!MoveInput(*Chain.Graph, *Chain.Call, Input, *Call, Input))
        {
            return false;
        }
    }
    auto* TargetExec = Exec(Chain.Target);
    TargetExec->BreakLinkTo(Then(Chain.Enable));
    if (!Then(Event) || !Schema->TryCreateConnection(Then(Event), TargetExec))
    {
        return false;
    }

    for (UEdGraphNode* Node : {static_cast<UEdGraphNode*>(Chain.Call), static_cast<UEdGraphNode*>(Chain.Gate),
        static_cast<UEdGraphNode*>(Chain.Enable)})
    {
        FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
    }
    return true;
}

bool USampleLookAtZoneRoutingMigration::MigrateZoneRouting(UBlueprint* Blueprint)
{
    FZoneChain Chain;
    FGizmoTargetChain GizmoTarget;
    const int32 State = Audit(Blueprint, Chain, GizmoTarget);
    if ((State != 2 && State != 3 && State != 4) || Blueprint->GetOutermost()->IsDirty())
    {
        return false;
    }
    const UFunction* Route = Blueprint->ParentClass->FindFunctionByName(RouteName);
    const UFunction* Apply = Blueprint->ParentClass->FindFunctionByName(ApplyName);
    const UFunction* Notify = Blueprint->ParentClass->FindFunctionByName(NotifyName);
    const UFunction* Resolve = Blueprint->ParentClass->FindFunctionByName(ResolveGizmoName);
    if (!IsGizmoResolver(Resolve, Blueprint->ParentClass) ||
        !Notify || (State == 2 && (!Route || !Apply || Apply->NumParms != 0 ||
        !Apply->HasAnyFunctionFlags(FUNC_BlueprintEvent))))
    {
        return false;
    }

    Blueprint->Modify();
    Chain.Graph->Modify();
    if (State == 2 && !InstallSelectionEntry(Blueprint, Chain, Route, Apply))
    {
        return false;
    }

    if (State != 4)
    {
        FGraphNodeCreator<UK2Node_CallFunction> NotifyCreator(*Chain.Graph);
        auto* Call = NotifyCreator.CreateNode();
        Call->SetFromFunction(Notify);
        Call->NodePosX = Chain.Notify->NodePosX;
        Call->NodePosY = Chain.Notify->NodePosY;
        NotifyCreator.Finalize();

        auto& Graph = *Chain.Graph;
        const auto& Old = *Chain.Notify;
        if (!MoveInput(Graph, Old, UEdGraphSchema_K2::PN_Self, *Call, TEXT("Widget")) ||
            !MoveInput(Graph, Old, TEXT("Zone Type"), *Call, TEXT("ZoneType")) ||
            !MoveInput(Graph, Old, TEXT("Anim Instance"), *Call, TEXT("AnimInstance")) ||
            !MoveInput(Graph, Old, UEdGraphSchema_K2::PN_Execute, *Call, UEdGraphSchema_K2::PN_Execute))
        {
            return false;
        }
        FBlueprintEditorUtils::RemoveNode(Blueprint, Chain.Notify, true);
    }
    if (!InstallGizmoTarget(Blueprint, GizmoTarget, Resolve))
    {
        return false;
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    return ClassifyZoneRouting(Blueprint) == 1;
}

bool USampleLookAtZoneRoutingMigration::ReloadZoneRoutingPackage(UBlueprint* Blueprint)
{
    if (!IsTarget(Blueprint) || (GEditor && GEditor->IsPlaySessionInProgress()))
    {
        return false;
    }
    UPackage* Package = Blueprint->GetOutermost();
    const TWeakObjectPtr<UPackage> OriginalPackage(Package);
    const bool bWasDirty = Package->IsDirty();
    Package->SetDirtyFlag(false);
    FText Error;
    if (UPackageTools::ReloadPackages({Package}, Error, EReloadPackagesInteractionMode::AssumeNegative))
    {
        return true;
    }
    if (UPackage* StillLoaded = OriginalPackage.Get())
    {
        StillLoaded->SetDirtyFlag(bWasDirty);
    }
    UE_LOG(LogSampleLookAtZoneRoutingMigration, Error,
        TEXT("Look At zone routing package reload failed: %s"), *Error.ToString());
    return false;
}
