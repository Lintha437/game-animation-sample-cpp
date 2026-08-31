#include "SampleBlueprintMigrationLibrary.h"

#include "EdGraph/EdGraph.h"
#include "K2Node_CallFunction.h"

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
