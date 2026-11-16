#include "SampleControllerInputDisplayMigration.h"

#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Select.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "UObject/Package.h"

namespace
{
    constexpr TCHAR WidgetPath[] = TEXT("/Game/Widgets/WidgetData/WB_SimpleController.WB_SimpleController");
    constexpr TCHAR NativeClassPath[] = TEXT("/Script/GameAnimationSampleCpp.SampleControllerInputDisplay");
    const FGuid OwnerGuid(0x06CC4715, 0x488F9232, 0x071B29B8, 0x21D9DB73);
    const FGuid AnalogValueGuid(0x061CAA59, 0x40A2BF74, 0x6256D0AF, 0x400A1EB4);

    struct FEdge
    {
        const TCHAR* Source;
        const TCHAR* Output;
        const TCHAR* Destination;
        const TCHAR* Input;
    };

    struct FNodeValue
    {
        const TCHAR* Node;
        const TCHAR* Value;
    };

    struct FPinDefault
    {
        const TCHAR* Node;
        const TCHAR* Pin;
        const TCHAR* Value;
    };

    struct FStickBoundary
    {
        const TCHAR* ContributionGetter = nullptr;
        const TCHAR* ContributionName = nullptr;
        const TCHAR* Transform = nullptr;
        const TCHAR* Render = nullptr;
        const TCHAR* Select = nullptr;
        const TCHAR* PositionScale = nullptr;
        const TCHAR* Magnitude = nullptr;
        const TCHAR* Threshold = nullptr;
    };

    struct FRewrite
    {
        const TCHAR* Graph;
        const TCHAR* NativeFunction;
        const TCHAR* Target;
        const TCHAR* TargetPin;
        const TCHAR* OwnerGetter;
        TArray<const TCHAR*> RemovedNodes;
        TArray<FEdge> Edges;
        TArray<FNodeValue> Functions;
        TArray<FPinDefault> Defaults;
        FStickBoundary Stick;
    };

    // Original dataflow in the three function bodies, two inline assignments and two stick outputs.
    const FRewrite Rewrites[] =
    {
        {TEXT("GetKeysTotalAnalogState"), TEXT("SampleAnalogKeys"), TEXT("FunctionResult_0"), TEXT("ReturnValue"), TEXT("VariableGet_1"),
         {
             TEXT("CallFunction_6"),
             TEXT("MacroInstance_0"),
             TEXT("VariableGet_0"),
             TEXT("VariableSet_0"),
             TEXT("VariableGet_2"),
             TEXT("PromotableOperator_0")
         },
         {
             {TEXT("FunctionEntry_0"), TEXT("then"), TEXT("MacroInstance_0"), TEXT("Exec")},
             {TEXT("FunctionEntry_0"), TEXT("Keys"), TEXT("MacroInstance_0"), TEXT("Array")},
             {TEXT("CallFunction_6"), TEXT("ReturnValue"), TEXT("PromotableOperator_0"), TEXT("B")},
             {TEXT("VariableGet_1"), TEXT("PlayerController"), TEXT("CallFunction_6"), TEXT("self")},
             {TEXT("MacroInstance_0"), TEXT("LoopBody"), TEXT("VariableSet_0"), TEXT("execute")},
             {TEXT("MacroInstance_0"), TEXT("Array Element"), TEXT("CallFunction_6"), TEXT("Key")},
             {TEXT("MacroInstance_0"), TEXT("Completed"), TEXT("FunctionResult_0"), TEXT("execute")},
             {TEXT("VariableGet_0"), TEXT("AnalogValue"), TEXT("FunctionResult_0"), TEXT("ReturnValue")},
             {TEXT("VariableGet_2"), TEXT("AnalogValue"), TEXT("PromotableOperator_0"), TEXT("A")},
             {TEXT("PromotableOperator_0"), TEXT("ReturnValue"), TEXT("VariableSet_0"), TEXT("AnalogValue")}
         },
         {
             {TEXT("CallFunction_6"), TEXT("GetInputAnalogKeyState")},
             {TEXT("PromotableOperator_0"), TEXT("Add_DoubleDouble")}
         },
         {}},
        {TEXT("SetWSADValues"), TEXT("SampleWSAD"), TEXT("VariableSet_0"), TEXT("WSAD"), TEXT("VariableGet_29"),
         {
             TEXT("CallFunction_5"),
             TEXT("CallFunction_6"),
             TEXT("CallFunction_7"),
             TEXT("CallFunction_4"),
             TEXT("CallFunction_8"),
             TEXT("PromotableOperator_0"),
             TEXT("PromotableOperator_1"),
             TEXT("PromotableOperator_2"),
             TEXT("PromotableOperator_3")
         },
         {
             {TEXT("VariableGet_29"), TEXT("PlayerController"), TEXT("CallFunction_5"), TEXT("self")},
             {TEXT("VariableGet_29"), TEXT("PlayerController"), TEXT("CallFunction_6"), TEXT("self")},
             {TEXT("VariableGet_29"), TEXT("PlayerController"), TEXT("CallFunction_7"), TEXT("self")},
             {TEXT("VariableGet_29"), TEXT("PlayerController"), TEXT("CallFunction_4"), TEXT("self")},
             {TEXT("CallFunction_5"), TEXT("ReturnValue"), TEXT("PromotableOperator_1"), TEXT("A")},
             {TEXT("CallFunction_6"), TEXT("ReturnValue"), TEXT("PromotableOperator_0"), TEXT("A")},
             {TEXT("CallFunction_7"), TEXT("ReturnValue"), TEXT("PromotableOperator_2"), TEXT("A")},
             {TEXT("CallFunction_4"), TEXT("ReturnValue"), TEXT("PromotableOperator_3"), TEXT("A")},
             {TEXT("CallFunction_8"), TEXT("ReturnValue"), TEXT("VariableSet_0"), TEXT("WSAD")},
             {TEXT("PromotableOperator_0"), TEXT("ReturnValue"), TEXT("PromotableOperator_1"), TEXT("B")},
             {TEXT("PromotableOperator_1"), TEXT("ReturnValue"), TEXT("CallFunction_8"), TEXT("Y")},
             {TEXT("PromotableOperator_2"), TEXT("ReturnValue"), TEXT("PromotableOperator_3"), TEXT("B")},
             {TEXT("PromotableOperator_3"), TEXT("ReturnValue"), TEXT("CallFunction_8"), TEXT("X")}
         },
         {
             {TEXT("CallFunction_5"), TEXT("GetInputAnalogKeyState")},
             {TEXT("CallFunction_6"), TEXT("GetInputAnalogKeyState")},
             {TEXT("CallFunction_7"), TEXT("GetInputAnalogKeyState")},
             {TEXT("CallFunction_4"), TEXT("GetInputAnalogKeyState")},
             {TEXT("CallFunction_8"), TEXT("MakeVector")},
             {TEXT("PromotableOperator_0"), TEXT("Multiply_DoubleDouble")},
             {TEXT("PromotableOperator_1"), TEXT("Add_DoubleDouble")},
             {TEXT("PromotableOperator_2"), TEXT("Multiply_DoubleDouble")},
             {TEXT("PromotableOperator_3"), TEXT("Add_DoubleDouble")}
         },
         {
             {TEXT("CallFunction_5"), TEXT("Key"), TEXT("W")},
             {TEXT("CallFunction_6"), TEXT("Key"), TEXT("S")},
             {TEXT("CallFunction_7"), TEXT("Key"), TEXT("A")},
             {TEXT("CallFunction_4"), TEXT("Key"), TEXT("D")},
             {TEXT("CallFunction_8"), TEXT("Z"), TEXT("0.0")},
             {TEXT("PromotableOperator_0"), TEXT("B"), TEXT("-1.0")},
             {TEXT("PromotableOperator_2"), TEXT("B"), TEXT("-1.0")}
         }},
        {TEXT("SetMouseValues"), TEXT("SampleMouse"), TEXT("VariableSet_0"), TEXT("MOUSE"), TEXT("VariableGet_29"),
         {
             TEXT("CallFunction_1"),
             TEXT("CallFunction_10"),
             TEXT("PromotableOperator_3"),
             TEXT("PromotableOperator_2"),
             TEXT("CallFunction_9"),
             TEXT("CallFunction_8"),
             TEXT("CallFunction_12"),
             TEXT("PromotableOperator_4")
         },
         {
             {TEXT("VariableGet_29"), TEXT("PlayerController"), TEXT("CallFunction_1"), TEXT("self")},
             {TEXT("CallFunction_1"), TEXT("DeltaX"), TEXT("PromotableOperator_3"), TEXT("A")},
             {TEXT("CallFunction_1"), TEXT("DeltaY"), TEXT("PromotableOperator_2"), TEXT("A")},
             {TEXT("CallFunction_10"), TEXT("ReturnValue"), TEXT("PromotableOperator_3"), TEXT("B")},
             {TEXT("CallFunction_10"), TEXT("ReturnValue"), TEXT("PromotableOperator_2"), TEXT("B")},
             {TEXT("PromotableOperator_3"), TEXT("ReturnValue"), TEXT("CallFunction_12"), TEXT("A_X")},
             {TEXT("PromotableOperator_2"), TEXT("ReturnValue"), TEXT("PromotableOperator_4"), TEXT("A")},
             {TEXT("VariableSet_0"), TEXT("Output_Get"), TEXT("CallFunction_9"), TEXT("Current")},
             {TEXT("CallFunction_9"), TEXT("ReturnValue"), TEXT("VariableSet_0"), TEXT("MOUSE")},
             {TEXT("CallFunction_8"), TEXT("ReturnValue"), TEXT("CallFunction_9"), TEXT("DeltaTime")},
             {TEXT("CallFunction_12"), TEXT("ReturnValue"), TEXT("CallFunction_9"), TEXT("Target")},
             {TEXT("PromotableOperator_4"), TEXT("ReturnValue"), TEXT("CallFunction_12"), TEXT("A_Y")}
         },
         {
             {TEXT("CallFunction_1"), TEXT("GetInputMouseDelta")},
             {TEXT("CallFunction_10"), TEXT("GetWorldDeltaSeconds")},
             {TEXT("PromotableOperator_3"), TEXT("Divide_DoubleDouble")},
             {TEXT("PromotableOperator_2"), TEXT("Divide_DoubleDouble")},
             {TEXT("CallFunction_9"), TEXT("VInterpTo")},
             {TEXT("CallFunction_8"), TEXT("GetWorldDeltaSeconds")},
             {TEXT("CallFunction_12"), TEXT("ClampVectorSize")},
             {TEXT("PromotableOperator_4"), TEXT("Multiply_DoubleDouble")}
         },
         {
             {TEXT("PromotableOperator_3"), TEXT("C"), TEXT("100.0")},
             {TEXT("PromotableOperator_2"), TEXT("C"), TEXT("100.0")},
             {TEXT("CallFunction_9"), TEXT("InterpSpeed"), TEXT("10.0")},
             {TEXT("CallFunction_12"), TEXT("A_Z"), TEXT("0.0")},
             {TEXT("CallFunction_12"), TEXT("Min"), TEXT("0.0")},
             {TEXT("CallFunction_12"), TEXT("Max"), TEXT("1.0")},
             {TEXT("PromotableOperator_4"), TEXT("B"), TEXT("-1.0")}
         }},
        {TEXT("EventGraph"), TEXT("SampleWSAD"), TEXT("VariableSet_3"), TEXT("WSAD"), TEXT("VariableGet_46"),
         {
             TEXT("PromotableOperator_11"),
             TEXT("PromotableOperator_12"),
             TEXT("PromotableOperator_13"),
             TEXT("PromotableOperator_14"),
             TEXT("CallFunction_87"),
             TEXT("CallFunction_88"),
             TEXT("CallFunction_89"),
             TEXT("CallFunction_6"),
             TEXT("CallFunction_5")
         },
         {
             {TEXT("PromotableOperator_11"), TEXT("ReturnValue"), TEXT("CallFunction_87"), TEXT("X")},
             {TEXT("PromotableOperator_12"), TEXT("ReturnValue"), TEXT("PromotableOperator_11"), TEXT("B")},
             {TEXT("PromotableOperator_13"), TEXT("ReturnValue"), TEXT("CallFunction_87"), TEXT("Y")},
             {TEXT("PromotableOperator_14"), TEXT("ReturnValue"), TEXT("PromotableOperator_13"), TEXT("B")},
             {TEXT("CallFunction_87"), TEXT("ReturnValue"), TEXT("VariableSet_3"), TEXT("WSAD")},
             {TEXT("CallFunction_88"), TEXT("ReturnValue"), TEXT("PromotableOperator_11"), TEXT("A")},
             {TEXT("CallFunction_89"), TEXT("ReturnValue"), TEXT("PromotableOperator_12"), TEXT("A")},
             {TEXT("CallFunction_6"), TEXT("ReturnValue"), TEXT("PromotableOperator_14"), TEXT("A")},
             {TEXT("CallFunction_5"), TEXT("ReturnValue"), TEXT("PromotableOperator_13"), TEXT("A")},
             {TEXT("VariableGet_46"), TEXT("PlayerController"), TEXT("CallFunction_5"), TEXT("self")},
             {TEXT("VariableGet_46"), TEXT("PlayerController"), TEXT("CallFunction_6"), TEXT("self")},
             {TEXT("VariableGet_46"), TEXT("PlayerController"), TEXT("CallFunction_89"), TEXT("self")},
             {TEXT("VariableGet_46"), TEXT("PlayerController"), TEXT("CallFunction_88"), TEXT("self")}
         },
         {
             {TEXT("PromotableOperator_11"), TEXT("Add_DoubleDouble")},
             {TEXT("PromotableOperator_12"), TEXT("Multiply_DoubleDouble")},
             {TEXT("PromotableOperator_13"), TEXT("Add_DoubleDouble")},
             {TEXT("PromotableOperator_14"), TEXT("Multiply_DoubleDouble")},
             {TEXT("CallFunction_87"), TEXT("MakeVector")},
             {TEXT("CallFunction_88"), TEXT("GetInputAnalogKeyState")},
             {TEXT("CallFunction_89"), TEXT("GetInputAnalogKeyState")},
             {TEXT("CallFunction_6"), TEXT("GetInputAnalogKeyState")},
             {TEXT("CallFunction_5"), TEXT("GetInputAnalogKeyState")}
         },
         {
             {TEXT("PromotableOperator_12"), TEXT("B"), TEXT("-1.0")},
             {TEXT("PromotableOperator_14"), TEXT("B"), TEXT("-1.0")},
             {TEXT("CallFunction_87"), TEXT("Z"), TEXT("0.0")},
             {TEXT("CallFunction_88"), TEXT("Key"), TEXT("D")},
             {TEXT("CallFunction_89"), TEXT("Key"), TEXT("A")},
             {TEXT("CallFunction_6"), TEXT("Key"), TEXT("S")},
             {TEXT("CallFunction_5"), TEXT("Key"), TEXT("W")}
         }},
        {TEXT("EventGraph"), TEXT("SampleMouse"), TEXT("VariableSet_2"), TEXT("MOUSE"), TEXT("VariableGet_45"),
         {
             TEXT("PromotableOperator_8"),
             TEXT("CallFunction_83"),
             TEXT("CallFunction_84"),
             TEXT("CallFunction_85"),
             TEXT("PromotableOperator_9"),
             TEXT("PromotableOperator_10"),
             TEXT("CallFunction_86"),
             TEXT("CallFunction_1")
         },
         {
             {TEXT("PromotableOperator_8"), TEXT("ReturnValue"), TEXT("CallFunction_83"), TEXT("A_Y")},
             {TEXT("CallFunction_83"), TEXT("ReturnValue"), TEXT("CallFunction_85"), TEXT("Target")},
             {TEXT("CallFunction_84"), TEXT("ReturnValue"), TEXT("CallFunction_85"), TEXT("DeltaTime")},
             {TEXT("CallFunction_85"), TEXT("ReturnValue"), TEXT("VariableSet_2"), TEXT("MOUSE")},
             {TEXT("VariableSet_2"), TEXT("Output_Get"), TEXT("CallFunction_85"), TEXT("Current")},
             {TEXT("PromotableOperator_9"), TEXT("ReturnValue"), TEXT("PromotableOperator_8"), TEXT("A")},
             {TEXT("PromotableOperator_10"), TEXT("ReturnValue"), TEXT("CallFunction_83"), TEXT("A_X")},
             {TEXT("CallFunction_86"), TEXT("ReturnValue"), TEXT("PromotableOperator_10"), TEXT("B")},
             {TEXT("CallFunction_86"), TEXT("ReturnValue"), TEXT("PromotableOperator_9"), TEXT("B")},
             {TEXT("CallFunction_1"), TEXT("DeltaX"), TEXT("PromotableOperator_10"), TEXT("A")},
             {TEXT("CallFunction_1"), TEXT("DeltaY"), TEXT("PromotableOperator_9"), TEXT("A")},
             {TEXT("VariableGet_45"), TEXT("PlayerController"), TEXT("CallFunction_1"), TEXT("self")}
         },
         {
             {TEXT("PromotableOperator_8"), TEXT("Multiply_DoubleDouble")},
             {TEXT("CallFunction_83"), TEXT("ClampVectorSize")},
             {TEXT("CallFunction_84"), TEXT("GetWorldDeltaSeconds")},
             {TEXT("CallFunction_85"), TEXT("VInterpTo")},
             {TEXT("PromotableOperator_9"), TEXT("Divide_DoubleDouble")},
             {TEXT("PromotableOperator_10"), TEXT("Divide_DoubleDouble")},
             {TEXT("CallFunction_86"), TEXT("GetWorldDeltaSeconds")},
             {TEXT("CallFunction_1"), TEXT("GetInputMouseDelta")}
         },
         {
             {TEXT("PromotableOperator_8"), TEXT("B"), TEXT("-1.0")},
             {TEXT("CallFunction_83"), TEXT("A_Z"), TEXT("0.0")},
             {TEXT("CallFunction_83"), TEXT("Min"), TEXT("0.0")},
             {TEXT("CallFunction_83"), TEXT("Max"), TEXT("1.0")},
             {TEXT("CallFunction_85"), TEXT("InterpSpeed"), TEXT("10.0")},
             {TEXT("PromotableOperator_9"), TEXT("C"), TEXT("100.0")},
             {TEXT("PromotableOperator_10"), TEXT("C"), TEXT("100.0")}
         }},
        {TEXT("EventGraph"), TEXT("SampleLeftStick"), TEXT("CallFunction_37"), TEXT("InOpacity"), TEXT("VariableGet_43"),
         {
             TEXT("CallFunction_80"), TEXT("PromotableOperator_7"), TEXT("CallFunction_41"), TEXT("PromotableOperator_1"), TEXT("Select_1"), TEXT("CallFunction_17"), TEXT("PromotableOperator_4")
         },
         {
             {TEXT("VariableGet_43"), TEXT("PlayerController"), TEXT("CallFunction_80"), TEXT("self")},
             {TEXT("VariableGet_44"), TEXT("WSAD"), TEXT("PromotableOperator_7"), TEXT("B")},
             {TEXT("CallFunction_80"), TEXT("ReturnValue"), TEXT("PromotableOperator_7"), TEXT("A")},
             {TEXT("PromotableOperator_7"), TEXT("ReturnValue"), TEXT("CallFunction_41"), TEXT("A")},
             {TEXT("PromotableOperator_7"), TEXT("ReturnValue"), TEXT("CallFunction_17"), TEXT("A")},
             {TEXT("CallFunction_41"), TEXT("ReturnValue"), TEXT("PromotableOperator_1"), TEXT("A")},
             {TEXT("CallFunction_41"), TEXT("ReturnValue"), TEXT("Select_1"), TEXT("Option 1")},
             {TEXT("PromotableOperator_1"), TEXT("ReturnValue"), TEXT("Select_1"), TEXT("Index")},
             {TEXT("Select_1"), TEXT("ReturnValue"), TEXT("CallFunction_37"), TEXT("InOpacity")},
             {TEXT("CallFunction_17"), TEXT("ReturnValue"), TEXT("PromotableOperator_4"), TEXT("A")},
             {TEXT("PromotableOperator_4"), TEXT("ReturnValue_X"), TEXT("MakeStruct_3"), TEXT("Translation_X")},
             {TEXT("PromotableOperator_4"), TEXT("ReturnValue_Y"), TEXT("MakeStruct_3"), TEXT("Translation_Y")}
         },
         {
             {TEXT("CallFunction_80"), TEXT("GetInputVectorKeyState")},
             {TEXT("PromotableOperator_7"), TEXT("Add_VectorVector")},
             {TEXT("CallFunction_41"), TEXT("VSizeXY")},
             {TEXT("PromotableOperator_1"), TEXT("Greater_DoubleDouble")},
             {TEXT("CallFunction_17"), TEXT("ClampVectorSize")},
             {TEXT("PromotableOperator_4"), TEXT("Multiply_VectorVector")}
         },
         {
             {TEXT("CallFunction_80"), TEXT("Key"), TEXT("Gamepad_Left2D")},
             {TEXT("PromotableOperator_1"), TEXT("B"), TEXT("0.200000")},
             {TEXT("Select_1"), TEXT("Option 0"), TEXT("0.0")},
             {TEXT("CallFunction_17"), TEXT("Min"), TEXT("0.0")},
             {TEXT("CallFunction_17"), TEXT("Max"), TEXT("1.000000")},
             {TEXT("PromotableOperator_4"), TEXT("B"), TEXT("40.000000,-40.000000,0.000000")}
         },
         {TEXT("VariableGet_44"), TEXT("WSAD"), TEXT("MakeStruct_3"), TEXT("CallFunction_42"), TEXT("Select_1"), TEXT("PromotableOperator_4"), TEXT("CallFunction_41"), TEXT("PromotableOperator_1")}},
        {TEXT("EventGraph"), TEXT("SampleRightStick"), TEXT("CallFunction_27"), TEXT("InOpacity"), TEXT("VariableGet_21"),
         {
             TEXT("CallFunction_28"), TEXT("PromotableOperator_6"), TEXT("CallFunction_30"), TEXT("PromotableOperator_0"), TEXT("Select_0"), TEXT("CallFunction_15"), TEXT("PromotableOperator_2")
         },
         {
             {TEXT("VariableGet_21"), TEXT("PlayerController"), TEXT("CallFunction_28"), TEXT("self")},
             {TEXT("VariableGet_1"), TEXT("MOUSE"), TEXT("PromotableOperator_6"), TEXT("B")},
             {TEXT("CallFunction_28"), TEXT("ReturnValue"), TEXT("PromotableOperator_6"), TEXT("A")},
             {TEXT("PromotableOperator_6"), TEXT("ReturnValue"), TEXT("CallFunction_30"), TEXT("A")},
             {TEXT("PromotableOperator_6"), TEXT("ReturnValue"), TEXT("CallFunction_15"), TEXT("A")},
             {TEXT("CallFunction_30"), TEXT("ReturnValue"), TEXT("PromotableOperator_0"), TEXT("A")},
             {TEXT("CallFunction_30"), TEXT("ReturnValue"), TEXT("Select_0"), TEXT("Option 1")},
             {TEXT("PromotableOperator_0"), TEXT("ReturnValue"), TEXT("Select_0"), TEXT("Index")},
             {TEXT("Select_0"), TEXT("ReturnValue"), TEXT("CallFunction_27"), TEXT("InOpacity")},
             {TEXT("CallFunction_15"), TEXT("ReturnValue"), TEXT("PromotableOperator_2"), TEXT("A")},
             {TEXT("PromotableOperator_2"), TEXT("ReturnValue_X"), TEXT("MakeStruct_2"), TEXT("Translation_X")},
             {TEXT("PromotableOperator_2"), TEXT("ReturnValue_Y"), TEXT("MakeStruct_2"), TEXT("Translation_Y")}
         },
         {
             {TEXT("CallFunction_28"), TEXT("GetInputVectorKeyState")},
             {TEXT("PromotableOperator_6"), TEXT("Add_VectorVector")},
             {TEXT("CallFunction_30"), TEXT("VSizeXY")},
             {TEXT("PromotableOperator_0"), TEXT("Greater_DoubleDouble")},
             {TEXT("CallFunction_15"), TEXT("ClampVectorSize")},
             {TEXT("PromotableOperator_2"), TEXT("Multiply_VectorVector")}
         },
         {
             {TEXT("CallFunction_28"), TEXT("Key"), TEXT("Gamepad_Right2D")},
             {TEXT("PromotableOperator_0"), TEXT("B"), TEXT("0.200000")},
             {TEXT("Select_0"), TEXT("Option 0"), TEXT("0.0")},
             {TEXT("CallFunction_15"), TEXT("Min"), TEXT("0.0")},
             {TEXT("CallFunction_15"), TEXT("Max"), TEXT("1.000000")},
             {TEXT("PromotableOperator_2"), TEXT("B"), TEXT("40.000000,40.000000,0.000000")}
         },
         {TEXT("VariableGet_1"), TEXT("MOUSE"), TEXT("MakeStruct_2"), TEXT("CallFunction_31"), TEXT("Select_0"), TEXT("PromotableOperator_2"), TEXT("CallFunction_30"), TEXT("PromotableOperator_0")}},

    };

    UEdGraphNode* FindNode(const UEdGraph& Graph, const TCHAR* Suffix)
    {
        const FName Name(*(FString(TEXT("K2Node_")) + Suffix));
        for (UEdGraphNode* Node : Graph.Nodes)
        {
            if (Node && Node->GetFName() == Name)
            {
                return Node;
            }
        }
        return nullptr;
    }

    UEdGraphPin* FindPin(const UEdGraph& Graph, const TCHAR* NodeName, const TCHAR* PinName, EEdGraphPinDirection Direction)
    {
        UEdGraphNode* Node = FindNode(Graph, NodeName);
        return Node ? Node->FindPin(FName(PinName), Direction) : nullptr;
    }

    bool IsSingleLink(const UEdGraphPin* Output, const UEdGraphPin* Input)
    {
        return Output && Input && Output->LinkedTo.Num() == 1 && Input->LinkedTo.Num() == 1
            && Output->LinkedTo.Contains(Input) && Input->LinkedTo.Contains(Output);
    }

    bool HasExecLink(const UEdGraph& Graph, const TCHAR* Source, const TCHAR* Output, const TCHAR* Destination, const TCHAR* Input)
    {
        const UEdGraphPin* From = FindPin(Graph, Source, Output, EGPD_Output);
        const UEdGraphPin* To = FindPin(Graph, Destination, Input, EGPD_Input);
        return IsSingleLink(From, To) && From->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec
            && To->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
    }

    bool IsSelfVariable(const UK2Node_Variable* Variable, FName Name)
    {
        return Variable && Variable->VariableReference.IsSelfContext()
            && !Variable->VariableReference.IsLocalScope() && Variable->VariableReference.GetMemberName() == Name
            && (!Variable->FindPin(UEdGraphSchema_K2::PN_Self)
                || Variable->FindPin(UEdGraphSchema_K2::PN_Self)->LinkedTo.IsEmpty());
    }

    bool IsReal(const FEdGraphPinType& Type, FName Subcategory)
    {
        return Type.PinCategory == UEdGraphSchema_K2::PC_Real && Type.PinSubCategory == Subcategory
            && Type.ContainerType == EPinContainerType::None && !Type.bIsReference;
    }

    bool HasAnalogLocal(const UK2Node_FunctionEntry& Entry, bool bForwarded)
    {
        if (bForwarded)
        {
            return Entry.LocalVariables.IsEmpty();
        }
        if (Entry.LocalVariables.Num() != 1)
        {
            return false;
        }
        const FBPVariableDescription& Local = Entry.LocalVariables[0];
        return Local.VarName == TEXT("AnalogValue") && Local.VarGuid == AnalogValueGuid
            && IsReal(Local.VarType, UEdGraphSchema_K2::PC_Float)
            && (Local.DefaultValue.IsEmpty() || (Local.DefaultValue.IsNumeric() && FCString::Atod(*Local.DefaultValue) == 0.0));
    }

    bool MatchesAnalogBody(const UEdGraph& Graph)
    {
        const auto* Loop = Cast<UK2Node_MacroInstance>(FindNode(Graph, TEXT("MacroInstance_0")));
        if (!Loop || !Loop->GetMacroGraph()
            || Loop->GetMacroGraph()->GetPathName() != TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros:ForEachLoop"))
        {
            return false;
        }
        for (const TCHAR* Name : {TEXT("VariableGet_0"), TEXT("VariableGet_2"), TEXT("VariableSet_0")})
        {
            const auto* Variable = Cast<UK2Node_Variable>(FindNode(Graph, Name));
            const UEdGraphPin* Pin = Variable ? Variable->FindPin(TEXT("AnalogValue")) : nullptr;
            if (!Pin || Variable->VariableReference.GetMemberGuid() != AnalogValueGuid
                || Variable->VariableReference.GetMemberName() != TEXT("AnalogValue")
                || Variable->VariableReference.GetMemberScopeName() != TEXT("GetKeysTotalAnalogState")
                || !IsReal(Pin->PinType, UEdGraphSchema_K2::PC_Float))
            {
                return false;
            }
        }
        return true;
    }

    bool HasRealPin(const UEdGraph& Graph, const TCHAR* Node, const TCHAR* Pin, EEdGraphPinDirection Direction, FName Type)
    {
        const UEdGraphPin* Value = FindPin(Graph, Node, Pin, Direction);
        return Value && IsReal(Value->PinType, Type);
    }

    bool MatchesStickTypes(const UEdGraph& Graph, const FStickBoundary& Stick)
    {
        const UEdGraphPin* Index = FindPin(Graph, Stick.Select, TEXT("Index"), EGPD_Input);
        return Cast<UK2Node_Select>(FindNode(Graph, Stick.Select))
            && Index && Index->PinType.PinCategory == UEdGraphSchema_K2::PC_Boolean
            && HasRealPin(Graph, Stick.Select, TEXT("Option 0"), EGPD_Input, UEdGraphSchema_K2::PC_Float)
            && HasRealPin(Graph, Stick.Select, TEXT("Option 1"), EGPD_Input, UEdGraphSchema_K2::PC_Float)
            && HasRealPin(Graph, Stick.Select, TEXT("ReturnValue"), EGPD_Output, UEdGraphSchema_K2::PC_Float)
            && HasRealPin(Graph, Stick.Magnitude, TEXT("ReturnValue"), EGPD_Output, UEdGraphSchema_K2::PC_Double)
            && HasRealPin(Graph, Stick.Threshold, TEXT("A"), EGPD_Input, UEdGraphSchema_K2::PC_Double)
            && HasRealPin(Graph, Stick.Threshold, TEXT("B"), EGPD_Input, UEdGraphSchema_K2::PC_Double)
            && HasRealPin(Graph, Stick.PositionScale, TEXT("ReturnValue_X"), EGPD_Output, UEdGraphSchema_K2::PC_Double)
            && HasRealPin(Graph, Stick.PositionScale, TEXT("ReturnValue_Y"), EGPD_Output, UEdGraphSchema_K2::PC_Double);
    }

    bool MatchesOriginal(const UEdGraph& Graph, const FRewrite& Rewrite)
    {
        // Count links on both sides, including the retained pins that will be rewired.
        TMap<const UEdGraphPin*, int32> ExpectedLinks;
        for (const TCHAR* Name : Rewrite.RemovedNodes)
        {
            const UEdGraphNode* Node = FindNode(Graph, Name);
            if (!Node)
            {
                return false;
            }
            for (const UEdGraphPin* Pin : Node->Pins)
            {
                if (Pin->bOrphanedPin)
                {
                    return false;
                }
                ExpectedLinks.Add(Pin, 0);
            }
        }
        for (const FEdge& Edge : Rewrite.Edges)
        {
            const auto* Output = FindPin(Graph, Edge.Source, Edge.Output, EGPD_Output);
            const auto* Input = FindPin(Graph, Edge.Destination, Edge.Input, EGPD_Input);
            if (!Output || !Input || !Output->LinkedTo.Contains(Input) || !Input->LinkedTo.Contains(Output))
            {
                return false;
            }
            ++ExpectedLinks.FindOrAdd(Output);
            ++ExpectedLinks.FindOrAdd(Input);
        }
        for (const auto& Pair : ExpectedLinks)
        {
            if (Pair.Key->LinkedTo.Num() != Pair.Value)
            {
                return false;
            }
        }
        for (const FNodeValue& Expected : Rewrite.Functions)
        {
            const auto* Call = Cast<UK2Node_CallFunction>(FindNode(Graph, Expected.Node));
            const UFunction* Function = Call ? Call->GetTargetFunction() : nullptr;
            const FString Name(Expected.Value);
            const TCHAR* Owner = Name.StartsWith(TEXT("GetInput")) ? TEXT("/Script/Engine.PlayerController")
                : Name == TEXT("GetWorldDeltaSeconds") ? TEXT("/Script/Engine.GameplayStatics")
                : TEXT("/Script/Engine.KismetMathLibrary");
            if (!Function || Function->GetFName() != FName(Expected.Value) || Function->GetOwnerClass()->GetPathName() != Owner)
            {
                return false;
            }
            if (FString(Expected.Node).StartsWith(TEXT("PromotableOperator_")))
            {
                // Verify every operand, including the unlinked third operand of division.
                int32 OperandCount = 0;
                for (const UEdGraphPin* Pin : Call->Pins)
                {
                    if (!Pin->bHidden && Pin->PinName != UEdGraphSchema_K2::PN_Self)
                    {
                        if (!Rewrite.Stick.ContributionGetter && !IsReal(Pin->PinType, UEdGraphSchema_K2::PC_Double))
                        {
                            return false;
                        }
                        OperandCount += Pin->Direction == EGPD_Input ? 1 : 0;
                    }
                }
                if (OperandCount != (Name == TEXT("Divide_DoubleDouble") ? 3 : 2))
                {
                    return false;
                }
            }
        }
        for (const FPinDefault& Expected : Rewrite.Defaults)
        {
            const auto* Pin = FindPin(Graph, Expected.Node, Expected.Pin, EGPD_Input);
            if (!Pin)
            {
                return false;
            }
            const FString Value = Pin->GetDefaultAsString();
            const FString ExpectedValue(Expected.Value);
            if (ExpectedValue.IsNumeric()
                ? !Value.IsNumeric() || FCString::Atod(*Value) != FCString::Atod(*ExpectedValue)
                : Value != ExpectedValue)
            {
                return false;
            }
        }
        return true;
    }

    struct FConnection
    {
        UEdGraphPin* Output;
        UEdGraphPin* Input;
    };

    struct FPlan
    {
        UEdGraph* Graph = nullptr;
        UFunction* Function = nullptr;
        UEdGraphPin* Owner = nullptr;
        UEdGraphPin* Target = nullptr;
        UEdGraphPin* Current = nullptr;
        UEdGraphPin* Keys = nullptr;
        UEdGraphPin* Contribution = nullptr;
        UEdGraphPin* TranslationX = nullptr;
        UEdGraphPin* TranslationY = nullptr;
        UK2Node_FunctionEntry* AnalogEntry = nullptr;
        UEdGraphPin* ReturnExec = nullptr;
        UK2Node_CallFunction* Forwarder = nullptr;

        TArray<FConnection> Connections() const
        {
            TArray<FConnection> Result{
                {Owner, Forwarder->FindPin(TEXT("PlayerController"), EGPD_Input)},
                {Forwarder->FindPin(Contribution ? FName(TEXT("Opacity")) : UEdGraphSchema_K2::PN_ReturnValue, EGPD_Output), Target}
            };
            if (Contribution)
            {
                Result.Add({Contribution, Forwarder->FindPin(TEXT("Contribution"), EGPD_Input)});
                Result.Add({Forwarder->FindPin(TEXT("TranslationX"), EGPD_Output), TranslationX});
                Result.Add({Forwarder->FindPin(TEXT("TranslationY"), EGPD_Output), TranslationY});
            }
            if (Current)
            {
                Result.Add({Current, Forwarder->FindPin(TEXT("Current"), EGPD_Input)});
            }
            if (Keys)
            {
                Result.Add({Keys, Forwarder->FindPin(TEXT("Keys"), EGPD_Input)});
                Result.Add({AnalogEntry->FindPin(UEdGraphSchema_K2::PN_Then, EGPD_Output), ReturnExec});
            }
            return Result;
        }
    };

    int32 Inspect(UBlueprint* Blueprint, const FRewrite& Rewrite, const TArray<UEdGraph*>& Graphs,
        UClass& NativeClass, FPlan& Plan)
    {
        UEdGraph* const* Graph = Graphs.FindByPredicate([&](const UEdGraph* Candidate)
        {
            return Candidate && Candidate->GetOuter() == Blueprint && Candidate->GetFName() == FName(Rewrite.Graph);
        });
        if (!Graph)
        {
            return 0;
        }
        Plan.Graph = *Graph;
        UEdGraph& Body = **Graph;
        Plan.Function = NativeClass.FindFunctionByName(FName(Rewrite.NativeFunction));
        const auto* Getter = Cast<UK2Node_VariableGet>(FindNode(Body, Rewrite.OwnerGetter));
        Plan.Owner = Getter ? Getter->FindPin(TEXT("PlayerController"), EGPD_Output) : nullptr;
        Plan.Target = FindPin(Body, Rewrite.Target, Rewrite.TargetPin, EGPD_Input);
        if (!Plan.Function || !Plan.Target || !Plan.Owner || !IsSelfVariable(Getter, TEXT("PlayerController"))
            || Getter->VariableReference.GetMemberGuid() != OwnerGuid)
        {
            return 0;
        }

        for (UEdGraphNode* Node : Body.Nodes)
        {
            auto* Call = Cast<UK2Node_CallFunction>(Node);
            if (Call && Call->GetTargetFunction() == Plan.Function)
            {
                if (Plan.Forwarder)
                {
                    return 0;
                }
                Plan.Forwarder = Call;
            }
        }

        const FName GraphName(Rewrite.Graph);
        const FName TargetName(Rewrite.TargetPin);
        const bool bForwarded = Plan.Forwarder != nullptr;
        const bool bTick = GraphName == TEXT("EventGraph");
        const bool bAnalog = TargetName == TEXT("ReturnValue");
        if (!bTick && Body.Nodes.Num() != (bForwarded ? 4 : Rewrite.RemovedNodes.Num() + 3))
        {
            return 0;
        }
        if (bAnalog)
        {
            Plan.AnalogEntry = Cast<UK2Node_FunctionEntry>(FindNode(Body, TEXT("FunctionEntry_0")));
            Plan.Keys = Plan.AnalogEntry ? Plan.AnalogEntry->FindPin(TEXT("Keys"), EGPD_Output) : nullptr;
            Plan.ReturnExec = FindPin(Body, Rewrite.Target, TEXT("execute"), EGPD_Input);
            if (!Plan.Keys || !Plan.ReturnExec || !Cast<UK2Node_FunctionResult>(FindNode(Body, Rewrite.Target))
                || !IsReal(Plan.Target->PinType, UEdGraphSchema_K2::PC_Float)
                || Plan.Keys->PinType.PinCategory != UEdGraphSchema_K2::PC_Struct
                || Plan.Keys->PinType.ContainerType != EPinContainerType::Array || Plan.Keys->PinType.bIsReference
                || !Plan.Keys->PinType.PinSubCategoryObject.IsValid()
                || Plan.Keys->PinType.PinSubCategoryObject->GetPathName() != TEXT("/Script/InputCore.Key")
                || !HasAnalogLocal(*Plan.AnalogEntry, bForwarded)
                || (!bForwarded && !MatchesAnalogBody(Body)))
            {
                return 0;
            }
        }
        else if (Rewrite.Stick.ContributionGetter)
        {
            const FStickBoundary& Stick = Rewrite.Stick;
            const auto* ContributionGetter = Cast<UK2Node_VariableGet>(FindNode(Body, Stick.ContributionGetter));
            Plan.Contribution = ContributionGetter ? ContributionGetter->FindPin(Stick.ContributionName, EGPD_Output) : nullptr;
            Plan.TranslationX = FindPin(Body, Stick.Transform, TEXT("Translation_X"), EGPD_Input);
            Plan.TranslationY = FindPin(Body, Stick.Transform, TEXT("Translation_Y"), EGPD_Input);
            if (!Plan.Contribution || !IsSelfVariable(ContributionGetter, FName(Stick.ContributionName))
                || !Plan.TranslationX || !IsReal(Plan.TranslationX->PinType, UEdGraphSchema_K2::PC_Double)
                || !Plan.TranslationY || !IsReal(Plan.TranslationY->PinType, UEdGraphSchema_K2::PC_Double)
                || !IsReal(Plan.Target->PinType, UEdGraphSchema_K2::PC_Float)
                || !HasExecLink(Body, Rewrite.Target, TEXT("then"), Stick.Render, TEXT("execute"))
                || !IsSingleLink(FindPin(Body, Stick.Transform, TEXT("WidgetTransform"), EGPD_Output),
                    FindPin(Body, Stick.Render, TEXT("InTransform"), EGPD_Input))
                || (!bForwarded && !MatchesStickTypes(Body, Stick)))
            {
                return 0;
            }
        }
        else
        {
            if (!IsSelfVariable(Cast<UK2Node_VariableSet>(FindNode(Body, Rewrite.Target)), TargetName))
            {
                return 0;
            }
            if (TargetName == TEXT("MOUSE"))
            {
                Plan.Current = FindPin(Body, Rewrite.Target, TEXT("Output_Get"), EGPD_Output);
                if (!Plan.Current)
                {
                    return 0;
                }
            }
            if (!bTick && !HasExecLink(Body, TEXT("FunctionEntry_0"), TEXT("then"), Rewrite.Target, TEXT("execute")))
            {
                return 0;
            }
            if (bTick && (!HasExecLink(Body, TEXT("Event_2"), TEXT("then"), TEXT("VariableSet_0"), TEXT("execute"))
                || !HasExecLink(Body, TEXT("VariableSet_0"), TEXT("then"), TEXT("MacroInstance_0"), TEXT("exec"))
                || !HasExecLink(Body, TEXT("MacroInstance_0"), TEXT("Is Valid"), TEXT("CallFunction_7"), TEXT("execute"))
                || !HasExecLink(Body, TEXT("CallFunction_8"), TEXT("then"), TEXT("VariableSet_3"), TEXT("execute"))
                || !HasExecLink(Body, TEXT("VariableSet_3"), TEXT("then"), TEXT("VariableSet_2"), TEXT("execute"))
                || !HasExecLink(Body, TEXT("VariableSet_2"), TEXT("then"), TEXT("CallFunction_37"), TEXT("execute"))
                || !HasExecLink(Body, TEXT("CallFunction_42"), TEXT("then"), TEXT("CallFunction_27"), TEXT("execute"))
                || !IsSelfVariable(Cast<UK2Node_VariableSet>(FindNode(Body, TEXT("VariableSet_0"))), TEXT("PlayerController"))
                || !IsSingleLink(FindPin(Body, TEXT("CallFunction_11"), TEXT("ReturnValue"), EGPD_Output),
                    FindPin(Body, TEXT("VariableSet_0"), TEXT("PlayerController"), EGPD_Input))
                || !IsSingleLink(FindPin(Body, TEXT("VariableSet_0"), TEXT("Output_Get"), EGPD_Output),
                    FindPin(Body, TEXT("MacroInstance_0"), TEXT("InputObject"), EGPD_Input))))
            {
                return 0;
            }
        }
        if (!bForwarded)
        {
            return MatchesOriginal(Body, Rewrite) ? 2 : 0;
        }

        for (const TCHAR* Name : Rewrite.RemovedNodes)
        {
            if (FindNode(Body, Name))
            {
                return 0;
            }
        }
        const auto* Schema = Cast<UEdGraphSchema_K2>(Body.GetSchema());
        for (const FConnection& Connection : Plan.Connections())
        {
            if (!IsSingleLink(Connection.Output, Connection.Input) || !Schema
                || !Schema->ArePinTypesCompatible(Connection.Output->PinType, Connection.Input->PinType, Blueprint->GeneratedClass))
            {
                return 0;
            }
        }
        for (const UEdGraphPin* Pin : Plan.Forwarder->Pins)
        {
            if (Pin->bOrphanedPin || ((Pin->PinName == UEdGraphSchema_K2::PN_Self
                || Pin->PinName == TEXT("WorldContextObject")) && !Pin->LinkedTo.IsEmpty()))
            {
                return 0;
            }
        }
        return 1;
    }

    int32 InspectAll(UBlueprint* Blueprint, TArray<FPlan>& Plans)
    {
        if (!Blueprint || Blueprint->GetPathName() != WidgetPath)
        {
            return 0;
        }
        UClass* NativeClass = FindObject<UClass>(nullptr, NativeClassPath);
        if (!NativeClass)
        {
            return 0;
        }
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        // Every slice must describe the same generation before any package backup or rewrite.
        int32 Classification = 0;
        for (const FRewrite& Rewrite : Rewrites)
        {
            const int32 State = Inspect(Blueprint, Rewrite, Graphs, *NativeClass, Plans.AddDefaulted_GetRef());
            if (State == 0 || (Classification != 0 && Classification != State))
            {
                UE_LOG(LogTemp, Warning, TEXT("Unsupported input-display graph at %s / %s."), Rewrite.Graph, Rewrite.NativeFunction);
                return 0;
            }
            Classification = State;
        }
        return Classification;
    }
}

int32 USampleControllerInputDisplayMigration::ClassifyInputDisplay(UBlueprint* Blueprint)
{
    TArray<FPlan> Plans;
    return InspectAll(Blueprint, Plans);
}

int32 USampleControllerInputDisplayMigration::ForwardInputDisplay(UBlueprint* Blueprint)
{
    TArray<FPlan> Plans;
    const int32 Classification = InspectAll(Blueprint, Plans);
    if (Classification != 2)
    {
        return Classification;
    }
    if (Blueprint->GetOutermost()->IsDirty())
    {
        return 0;
    }

    // Allocate all replacement pins and check compatibility before mutating the original bodies.
    for (FPlan& Plan : Plans)
    {
        Plan.Forwarder = NewObject<UK2Node_CallFunction>(Plan.Graph);
        Plan.Forwarder->SetFromFunction(Plan.Function);
        Plan.Forwarder->AllocateDefaultPins();
        const auto* Schema = Cast<UEdGraphSchema_K2>(Plan.Graph->GetSchema());
        for (const FConnection& Connection : Plan.Connections())
        {
            if (!Schema || !Connection.Output || !Connection.Input
                || !Schema->ArePinTypesCompatible(Connection.Output->PinType, Connection.Input->PinType, Blueprint->GeneratedClass))
            {
                return 0;
            }
        }
    }

    Blueprint->Modify();
    for (int32 Index = 0; Index < Plans.Num(); ++Index)
    {
        FPlan& Plan = Plans[Index];
        Plan.Graph->Modify();
        for (const TCHAR* Name : Rewrites[Index].RemovedNodes)
        {
            Plan.Graph->RemoveNode(FindNode(*Plan.Graph, Name));
        }
        if (Plan.AnalogEntry)
        {
            Plan.AnalogEntry->Modify();
            Plan.AnalogEntry->LocalVariables.Reset();
        }
        const UEdGraphNode* TargetNode = Plan.Target->GetOwningNode();
        Plan.Forwarder->NodePosX = TargetNode->NodePosX - 320;
        Plan.Forwarder->NodePosY = TargetNode->NodePosY + 96;
        Plan.Forwarder->SetFlags(RF_Transactional);
        Plan.Forwarder->CreateNewGuid();
        Plan.Graph->AddNode(Plan.Forwarder, false, false);
        for (const FConnection& Connection : Plan.Connections())
        {
            Connection.Output->MakeLinkTo(Connection.Input);
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    return ClassifyInputDisplay(Blueprint);
}
