#include "AnimGraphNode_KawaiiPhysics.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "KawaiiPhysicsLimitsDataAsset.h"
#include "Selection.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Materials/MaterialInstanceDynamic.h"

#define LOCTEXT_NAMESPACE "KawaiiPhysics"

// ----------------------------------------------------------------------------
UAnimGraphNode_KawaiiPhysics::UAnimGraphNode_KawaiiPhysics(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

FText UAnimGraphNode_KawaiiPhysics::GetControllerDescription() const
{
	return LOCTEXT("Kawaii Physics", "Kawaii Physics");
}


// ----------------------------------------------------------------------------
FText UAnimGraphNode_KawaiiPhysics::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if ((TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle))
	{
		return GetControllerDescription();
	}
	// @TODO: the bone can be altered in the property editor, so we have to 
	//        choose to mark this dirty when that happens for this to properly work
	else //if (!CachedNodeTitles.IsTitleCached(TitleType, this))
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("ControllerDescription"), GetControllerDescription());
		Args.Add(TEXT("RootBoneName"), FText::FromName(Node.RootBone.BoneName));

		// FText::Format() is slow, so we cache this to save on performance
		if (TitleType == ENodeTitleType::ListView || TitleType == ENodeTitleType::MenuTitle)
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_KawaiiPhysics_ListTitle", "{ControllerDescription} - Root: {RootBoneName}"), Args), this);
		}
		else
		{
			CachedNodeTitles.SetCachedTitle(TitleType, FText::Format(LOCTEXT("AnimGraphNode_KawaiiPhysics_Title", "{ControllerDescription}\nRoot: {RootBoneName} "), Args), this);
		}
	}
	return CachedNodeTitles[TitleType];
}

void UAnimGraphNode_KawaiiPhysics::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	Node.ModifyBones.Empty();
	ReconstructNode();
}

FEditorModeID UAnimGraphNode_KawaiiPhysics::GetEditorMode() const
{
	return "AnimGraph.SkeletalControl.KawaiiPhysics";
}

void UAnimGraphNode_KawaiiPhysics::Draw(FPrimitiveDrawInterface* PDI, USkeletalMeshComponent* SkelMeshComp) const
{
	if (!SkelMeshComp)
	{
		return;
	}

	FAnimNode_KawaiiPhysics* ActiveNode = GetActiveInstanceNode<FAnimNode_KawaiiPhysics>(SkelMeshComp->GetAnimInstance());
	if (!ActiveNode)
	{
		return;
	}

	if (bEnableDebugDrawBone)
	{
		for (auto& Bone : ActiveNode->ModifyBones)
		{
			PDI->DrawPoint(Bone.Location, FLinearColor::White, 5.0f, SDPG_Foreground);

			if (Bone.PhysicsSettings.Radius > 0)
			{
				auto Color = Bone.bDummy ? FColor::Red : FColor::Yellow;
				DrawWireSphere(PDI, Bone.Location, Color, Bone.PhysicsSettings.Radius, 16, SDPG_Foreground);
			}

			for (const int32 ChildIndex : Bone.ChildIndexs)
			{
				DrawDashedLine(PDI, Bone.Location, ActiveNode->ModifyBones[ChildIndex].Location,
				               FLinearColor::White, 1, SDPG_Foreground);
			}
		}
	}

	if (bEnableDebugDrawAngleLimit)
	{
		for (auto& Bone : ActiveNode->ModifyBones)
		{
			if (Bone.ParentIndex < 0 || Bone.PhysicsSettings.LimitAngle <= 0)
			{
				continue;
			}

			auto& ParentBone = ActiveNode->ModifyBones[Bone.ParentIndex];
			FTransform ParentBoneTransform =
				FTransform(FQuat::FindBetween(FVector::ForwardVector, Bone.PoseLocation - ParentBone.PoseLocation), ParentBone.Location);
			TArray<FVector> Verts;
			DrawWireCone(PDI, Verts, ParentBoneTransform, (Bone.PoseLocation - ParentBone.PoseLocation).Size(),
			             Bone.PhysicsSettings.LimitAngle, 16, FColor::Green, SDPG_World);
		}
	}
}

void UAnimGraphNode_KawaiiPhysics::ValidateAnimNodePostCompile(FCompilerResultsLog& MessageLog, UAnimBlueprintGeneratedClass* CompiledClass, int32 CompiledNodeIndex)
{
	UAnimGraphNode_SkeletalControlBase::ValidateAnimNodePostCompile(MessageLog, CompiledClass, CompiledNodeIndex);

	Node.RootBone.Initialize(CompiledClass->TargetSkeleton);
	if (Node.RootBone.BoneIndex >= 0)
	{
		if (Node.ExcludeBones.Contains(Node.RootBone))
		{
			MessageLog.Warning(TEXT("@@ ExcludeBones should NOT has RootBone."), this);
		}
	}
	else
	{
		MessageLog.Warning(TEXT("@@ RootBone is empty."), this);
	}
	
}

void UAnimGraphNode_KawaiiPhysics::CopyNodeDataToPreviewNode(FAnimNode_Base* AnimNode)
{
	FAnimNode_KawaiiPhysics* KawaiiPhysics = static_cast<FAnimNode_KawaiiPhysics*>(AnimNode);

	// pushing properties to preview instance, for live editing
	// Default
	KawaiiPhysics->RootBone = Node.RootBone;
	KawaiiPhysics->ExcludeBones = Node.ExcludeBones;
	KawaiiPhysics->TargetFramerate = Node.TargetFramerate;
	KawaiiPhysics->OverrideTargetFramerate = Node.OverrideTargetFramerate;

	// Physics Settings
	KawaiiPhysics->DampingCurveData = Node.DampingCurveData;
	KawaiiPhysics->WorldDampingLocationCurveData = Node.WorldDampingLocationCurveData;
	KawaiiPhysics->WorldDampingRotationCurveData = Node.WorldDampingRotationCurveData;
	KawaiiPhysics->StiffnessCurveData = Node.StiffnessCurveData;
	KawaiiPhysics->RadiusCurveData = Node.RadiusCurveData;
	KawaiiPhysics->LimitAngleCurveData = Node.LimitAngleCurveData;
	KawaiiPhysics->bUpdatePhysicsSettingsInGame = Node.bUpdatePhysicsSettingsInGame;
	KawaiiPhysics->PlanarConstraint = Node.PlanarConstraint;
	KawaiiPhysics->ResetBoneTransformWhenBoneNotFound = Node.ResetBoneTransformWhenBoneNotFound;

	// DummyBone
	KawaiiPhysics->DummyBoneLength = Node.DummyBoneLength;
	KawaiiPhysics->BoneForwardAxis = Node.BoneForwardAxis;

	// Limits
	KawaiiPhysics->SphericalLimits = Node.SphericalLimits;
	KawaiiPhysics->CapsuleLimits = Node.CapsuleLimits;
	KawaiiPhysics->PlanarLimits = Node.PlanarLimits;
	KawaiiPhysics->LimitsDataAsset = Node.LimitsDataAsset;

	// ExternalForce
	KawaiiPhysics->Gravity = Node.Gravity;

	// Wind
	KawaiiPhysics->bEnableWind = Node.bEnableWind;
	KawaiiPhysics->WindScale = Node.WindScale;

	// BoneConstraint
	KawaiiPhysics->BoneConstraintGlobalComplianceType = Node.BoneConstraintGlobalComplianceType;
	KawaiiPhysics->BoneConstraintIterationCountBeforeCollision = Node.BoneConstraintIterationCountBeforeCollision;
	KawaiiPhysics->BoneConstraintIterationCountAfterCollision = Node.BoneConstraintIterationCountAfterCollision;
	KawaiiPhysics->bAutoAddChildDummyBoneConstraint = Node.bAutoAddChildDummyBoneConstraint;
	KawaiiPhysics->BoneConstraints = Node.BoneConstraints;
	KawaiiPhysics->BoneConstraintsDataAsset = Node.BoneConstraintsDataAsset;

	// Reset for sync without compile
	KawaiiPhysics->ModifyBones.Empty();
}

void UAnimGraphNode_KawaiiPhysics::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	Super::CustomizeDetails(DetailBuilder);

	IDetailCategoryBuilder& ViewportCategory = DetailBuilder.EditCategory(TEXT("Kawaii Physics Tools"));
	FDetailWidgetRow& WidgetRow = ViewportCategory.AddCustomRow(LOCTEXT("KawaiiPhysics", "KawaiiPhysicsTools"));

	WidgetRow
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.OnClicked_Lambda([this]()
			             {
				             this->ExportLimitsDataAsset();
				             return FReply::Handled();
			             })
				.Content()
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Export Limits Data Asset")))
			]
		]
	];
}

struct FKawaiiPhysicsVersion
{
	enum Type
	{
		BeforeCustomVersionWasAdded,
		UseRuntimeFloatCurve,
		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	// The GUID for this custom version number
	const static FGuid GUID;

private:
	FKawaiiPhysicsVersion() {};
};

const FGuid FKawaiiPhysicsVersion::GUID(0x4B2D3E25, 0xCD681D29, 0x2DB298D7, 0xAD3E55FA);

const FCustomVersionRegistration GRegisterKawaiiPhysCustomVersion(FKawaiiPhysicsVersion::GUID, FKawaiiPhysicsVersion::LatestVersion, TEXT("Kawaii-Phys"));

void UAnimGraphNode_KawaiiPhysics::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FKawaiiPhysicsVersion::GUID);

	if (Ar.CustomVer(FKawaiiPhysicsVersion::GUID) < FKawaiiPhysicsVersion::UseRuntimeFloatCurve)
	{
		Node.DampingCurveData.ExternalCurve = Node.DampingCurve_DEPRECATED;
		Node.WorldDampingLocationCurveData.ExternalCurve = Node.WorldDampingLocationCurve_DEPRECATED;
		Node.WorldDampingRotationCurveData.ExternalCurve = Node.WorldDampingRotationCurve_DEPRECATED;
		Node.StiffnessCurveData.ExternalCurve = Node.StiffnessCurve_DEPRECATED;
		Node.RadiusCurveData.ExternalCurve = Node.RadiusCurve_DEPRECATED;
		Node.LimitAngleCurveData.ExternalCurve = Node.LimitAngleCurve_DEPRECATED;
	}
}

void UAnimGraphNode_KawaiiPhysics::ExportLimitsDataAsset()
{
	const FString DefaultAsset = FPackageName::GetLongPackagePath(GetOutermost()->GetName()) + TEXT("/") + GetName() + TEXT("_Collision");

	const TSharedRef<SDlgPickAssetPath> NewAssetDlg =
		SNew(SDlgPickAssetPath)
			.Title(LOCTEXT("NewDataAssetDialogTitle", "Choose Location for Collision Data Asset"))
			.DefaultAssetPath(FText::FromString(DefaultAsset));

	if (NewAssetDlg->ShowModal() == EAppReturnType::Cancel)
	{
		return;
	}
	
	const FString Package(NewAssetDlg->GetFullAssetPath().ToString());
	const FString Name(NewAssetDlg->GetAssetName().ToString());

	UPackage* Pkg = CreatePackage(*Package);
	
	if (UKawaiiPhysicsLimitsDataAsset* NewDataAsset =
		NewObject<UKawaiiPhysicsLimitsDataAsset>(Pkg, UKawaiiPhysicsLimitsDataAsset::StaticClass(), FName(Name),
		RF_Public | RF_Standalone))
	{
		// copy data
		NewDataAsset->SphericalLimitsData.SetNum(Node.SphericalLimits.Num());
		for(int32 i=0; i<Node.SphericalLimits.Num(); i++)
		{
			NewDataAsset->SphericalLimitsData[i].Update(&Node.SphericalLimits[i]);
		}

		NewDataAsset->CapsuleLimitsData.SetNum(Node.CapsuleLimits.Num());
		for(int32 i=0; i<Node.CapsuleLimits.Num(); i++)
		{
			NewDataAsset->CapsuleLimitsData[i].Update(&Node.CapsuleLimits[i]);
		}

		NewDataAsset->PlanarLimitsData.SetNum(Node.PlanarLimits.Num());
		for(int32 i=0; i<Node.PlanarLimits.Num(); i++)
		{
			NewDataAsset->PlanarLimitsData[i].Update(&Node.PlanarLimits[i]);
		}

		NewDataAsset->Sync();
		
		// select new asset
		USelection* SelectionSet = GEditor->GetSelectedObjects();
		SelectionSet->DeselectAll();
		SelectionSet->Select(NewDataAsset);

		FAssetRegistryModule::AssetCreated(NewDataAsset);
		Pkg->MarkPackageDirty();
	}
}

#undef LOCTEXT_NAMESPACE