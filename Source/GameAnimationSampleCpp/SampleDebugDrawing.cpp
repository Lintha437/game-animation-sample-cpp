#include "SampleDebugDrawing.h"

#include "DrawDebugLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"
#include "UObject/UnrealType.h"

namespace
{
    FDrawDebugStringSettings MakeRowSettings()
    {
        FDrawDebugStringSettings Settings;
        Settings.Height = 5.0f;
        return Settings;
    }

    FDebugDrawer MakeHistoryDrawer(const UObject* Owner)
    {
        return UDrawDebugLibrary::MakeVisualLoggerDebugDrawer(Owner, TEXT("Draw Debug Library"));
    }

    void DrawImportedSeries(const UObject* Owner, FVector Origin, FRotator Rotation, FVector2D Size,
        float Minimum, float Maximum, const FString& XLabel, const FString& YLabel,
        const FArrayProperty& SeriesProperty, const FStructProperty& Row, void* Series)
    {
        const auto* Name = FindFProperty<FStrProperty>(Row.Struct, TEXT("Name_10_3306FBD140B223C953EF4AACC1AC6176"));
        const auto* Color = FindFProperty<FStructProperty>(Row.Struct, TEXT("Color_9_8B0F933E47F28862F20A129A698F0BD0"));
        const auto* Values = FindFProperty<FArrayProperty>(Row.Struct, TEXT("Values_8_8E822734407F401431999D9111E5F344"));
        const auto* Value = Values ? CastField<FDoubleProperty>(Values->Inner) : nullptr;
        if (!Name || !Color || Color->Struct != TBaseStructure<FLinearColor>::Get() || !Value)
        {
            return;
        }

        const FDebugDrawer Drawer = MakeHistoryDrawer(Owner);
        const FDrawDebugLineStyle TextStyle(FLinearColor::Black, 0.5f);
        const FDrawDebugStringSettings Labels = MakeRowSettings();
        FDrawDebugGraphAxesSettings Axes;
        Axes.XaxisLabel = XLabel;
        Axes.YaxisLabel = YLabel;
        Axes.AxisLabelSettings = Labels;
        UDrawDebugLibrary::DrawDebugGraphAxes(Drawer, Origin, Rotation, static_cast<float>(Size.X),
            static_cast<float>(Size.Y), TextStyle, FDrawDebugLineStyle(FLinearColor(1, 0, 1, 0), 0.01f), false, Axes);
        UDrawDebugLibrary::DrawDebugBox(Drawer, Origin + Rotation.RotateVector(FVector(0, Size.X * 0.5, Size.Y * 0.5)),
            FRotator(Rotation.Pitch + 90.0, Rotation.Yaw, Rotation.Roll), TextStyle, false,
            FVector((Size.X + 1.0) * 0.5, (Size.Y + 1.0) * 0.5, 0));

        FScriptArrayHelper Rows(&SeriesProperty, Series);
        TArray<FString> Names;
        TArray<FLinearColor> Colors;
        Names.Reserve(Rows.Num());
        Colors.Reserve(Rows.Num());
        TArray<float> X;
        TArray<float> Y;
        for (int32 Index = Rows.Num() - 1; Index >= 0; --Index)
        {
            const void* Data = Rows.GetRawPtr(Index);
            Names.Add(Name->GetPropertyValue_InContainer(Data));
            const FLinearColor LineColor = *Color->ContainerPtrToValuePtr<FLinearColor>(Data);
            Colors.Add(LineColor);
            FScriptArrayHelper Samples(Values, Values->ContainerPtrToValuePtr<void>(Data));
            Y.Reset(Samples.Num());
            for (int32 Sample = 0; Sample < Samples.Num(); ++Sample)
            {
                Y.Add(static_cast<float>(Value->GetPropertyValue(Samples.GetRawPtr(Sample))));
            }
            const float Width = static_cast<float>(Size.X);
            UDrawDebugLibrary::MakeLinearlySpacedFloatArray(X, 0.0f, Width, Y.Num());
            UDrawDebugLibrary::DrawDebugGraphLine(Drawer, Origin, Rotation, X, Y, 0.0f, Width, Minimum,
                Maximum, Width, static_cast<float>(Size.Y),
                FDrawDebugLineStyle(LineColor, 0.5f, EDrawDebugLineType::Solid, 1.0f), false);
        }
        UDrawDebugLibrary::DrawDebugGraphLegend(Drawer, Origin, Rotation, Colors, Names, 0.0f,
            static_cast<float>(2.0 * Size.Y + 6.0), FDrawDebugLineStyle(FLinearColor(1, 0, 1), 0.5f),
            FDrawDebugLineStyle(), 0.0f, false, Labels);
    }
}

void USampleDebugDrawing::DrawDirection(const UObject* WorldContextObject, bool DrawCircle, bool DrawAxis,
    bool DrawArrow, bool DrawString, const FString& String, FVector StringOffset,
    FVector Center, FVector Direction, FVector Offset, double Radius, double Length, double Size,
    int32 Segments, FLinearColor Color, double Duration, double Thickness)
{
    const FVector Origin = Center + Offset;
    if (DrawCircle)
    {
        const FRotator Rotation = UKismetMathLibrary::MakeRotFromX(Direction);
        UKismetSystemLibrary::DrawDebugCircle(WorldContextObject, Origin, static_cast<float>(Radius), Segments, Color,
            static_cast<float>(Duration), static_cast<float>(Thickness), Rotation.Vector(),
            FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y), DrawAxis);
    }
    FVector ArrowEnd = FVector::ZeroVector;
    if (DrawArrow)
    {
        const FVector Axis = Direction.GetSafeNormal(0.0001);
        const FVector ArrowStart = Origin + Axis * Radius;
        ArrowEnd = ArrowStart + Axis * Length;
        UKismetSystemLibrary::DrawDebugArrow(WorldContextObject, ArrowStart, ArrowEnd, static_cast<float>(Size),
            Color, static_cast<float>(Duration), static_cast<float>(Thickness));
    }
    if (DrawString)
    {
        UKismetSystemLibrary::DrawDebugString(WorldContextObject, ArrowEnd + StringOffset, String, nullptr,
            Color * FLinearColor(0.5f, 0.5f, 0.5f, 1.0f), static_cast<float>(Duration));
    }
}

void USampleDebugDrawing::DrawHeadingThresholds(const UObject* WorldContextObject, FVector Center, FVector Offset,
    FRotator InRotation, const TArray<double>& YawAngles, double StartRadius, double EndRadius,
    FLinearColor Color, float Duration, float Thickness)
{
    const FVector Origin = Center + Offset;
    for (double Angle : YawAngles)
    {
        const FVector Direction = FRotator(InRotation.Pitch, InRotation.Yaw + Angle, InRotation.Roll).Vector();
        UKismetSystemLibrary::DrawDebugLine(WorldContextObject, Origin + Direction * StartRadius,
            Origin + Direction * EndRadius, Color, Duration, Thickness);
    }
}

void USampleDebugDrawing::DrawBooleanRows(const UObject* WorldContextObject, FVector Location, FRotator Rotation,
    FVector Offset, const TArray<FString>& BoolNames, const TArray<bool>& BoolValues)
{
    const FVector Origin = Location + Rotation.RotateVector(Offset);
    const FDrawDebugStringSettings Settings = MakeRowSettings();
    const FDebugDrawer Drawer = MakeHistoryDrawer(WorldContextObject);
    const FDrawDebugLineStyle NameStyle(FLinearColor::Black, 0.5f);
    for (int32 Index = 0; Index < BoolNames.Num(); ++Index)
    {
        const FString& Name = BoolNames[Index];
        const FVector Line = Origin - Rotation.RotateVector(FVector(0, 0, 5.0 * Index));
        UDrawDebugLibrary::DrawDebugString(Drawer, Name, Line, Rotation, NameStyle, false, Settings);
        const double TextWidth = UDrawDebugLibrary::DrawDebugStringDimensions(Name, Settings).Y;
        const bool State = BoolValues.IsValidIndex(Index) && BoolValues[Index];
        UDrawDebugLibrary::DrawDebugString(Drawer, State ? TEXT("True") : TEXT("False"),
            Line + Rotation.RotateVector(FVector(0, TextWidth + 1.0, 0)), Rotation,
            FDrawDebugLineStyle(State ? FLinearColor::Green : FLinearColor::Red, 0.5f), false, Settings);
    }
}

void USampleDebugDrawing::DrawStringRows(const UObject* WorldContextObject, FVector Location, FRotator Rotation,
    FVector Offset, const FString& Label, const FString& Prefix, const TArray<FString>& Strings,
    const FString& HighlightedString, const FString& Highlight)
{
    FString Text = Label;
    for (const FString& Row : Strings)
    {
        if (Text.IsEmpty())
        {
            Text = Row;
            continue;
        }
        Text += TEXT("\r\n");
        Text += Prefix;
        Text += Row;
        if (Row.Equals(HighlightedString, ESearchCase::IgnoreCase))
        {
            Text += Highlight;
        }
    }
    UDrawDebugLibrary::DrawDebugString(MakeHistoryDrawer(WorldContextObject), Text, Location + Rotation.RotateVector(Offset),
        Rotation, FDrawDebugLineStyle(FLinearColor::Black, 0.5f), false, MakeRowSettings());
}

void USampleDebugDrawing::DrawObjectNames(const UObject* WorldContextObject, FVector Location, FRotator Rotation,
    FVector Offset, const FString& ArrayLabel, const TArray<UObject*>& Objects)
{
    FString Text = ArrayLabel;
    for (const UObject* Object : Objects)
    {
        Text += TEXT("\r\n");
        Text += UKismetSystemLibrary::GetDisplayName(Object);
    }
    UDrawDebugLibrary::DrawDebugString(MakeHistoryDrawer(WorldContextObject), Text, Location + Rotation.RotateVector(Offset),
        Rotation, FDrawDebugLineStyle(FLinearColor::Black, 0.5f), false, MakeRowSettings());
}

TArray<FString> USampleDebugDrawing::GetObjectNames(const TArray<UObject*>& Objects)
{
    TArray<FString> Names;
    Names.Reserve(Objects.Num());
    for (const UObject* Object : Objects)
    {
        Names.Add(UKismetSystemLibrary::GetDisplayName(Object));
    }
    return Names;
}

void USampleDebugDrawing::DrawHistoryGraph(const UObject*, FVector, FRotator, FVector, FVector2D, FVector2D,
    FVector2D, const FString&, const FString&, const TArray<int32>&)
{
    checkNoEntry();
}

DEFINE_FUNCTION(USampleDebugDrawing::execDrawHistoryGraph)
{
    P_GET_OBJECT(UObject, WorldContextObject);
    P_GET_STRUCT(FVector, Location);
    P_GET_STRUCT(FRotator, Rotation);
    P_GET_STRUCT(FVector, Offset);
    P_GET_STRUCT(FVector2D, GraphSize);
    P_GET_STRUCT(FVector2D, MinValue);
    P_GET_STRUCT(FVector2D, MaxValue);
    P_GET_PROPERTY(FStrProperty, XaxisLabel);
    P_GET_PROPERTY(FStrProperty, YaxisLabel);
    Stack.MostRecentProperty = nullptr;
    Stack.MostRecentPropertyAddress = nullptr;
    Stack.StepCompiledIn<FArrayProperty>(nullptr);
    const auto* Array = CastField<FArrayProperty>(Stack.MostRecentProperty);
    const auto* Row = Array ? CastField<FStructProperty>(Array->Inner) : nullptr;
    void* Data = Stack.MostRecentPropertyAddress;
    P_FINISH;

    P_NATIVE_BEGIN;
    if (Data && Row && Row->Struct && Row->Struct->GetFName() == TEXT("S_DebugGraphLineProperties"))
    {
        DrawImportedSeries(WorldContextObject, Location + Rotation.RotateVector(Offset), Rotation, GraphSize,
            static_cast<float>(MinValue.Y), static_cast<float>(MaxValue.Y), XaxisLabel, YaxisLabel, *Array, *Row, Data);
    }
    P_NATIVE_END;
}

void USampleDebugDrawing::AddStringHistory(TArray<FString>& InOutValues, const FString& NewValue, int32 MaxHistoryCount)
{
    const FString Value = NewValue;
    InOutValues.Insert(Value, 0);
    if (InOutValues.IsValidIndex(MaxHistoryCount))
    {
        InOutValues.RemoveAt(MaxHistoryCount);
    }
}
