#include "UQbAssetImportFactory.h"
#include "Misc/FileHelper.h"
#include "VoxelAssets/VoxelDataAsset.h"
#include "VoxelAssets/VoxelDataAssetData.inl"
#include "VoxelMaterial.h"
#include "VoxelValue.h"
#include "QbReader.h"

#include "AssetRegistry/AssetRegistryModule.h"

UQbAssetImportFactory::UQbAssetImportFactory(const FObjectInitializer& ObjectInitializer) : UFactory(ObjectInitializer)
{
	SupportedClass = UVoxelDataAsset::StaticClass();
	Formats.Add(TEXT("qb;Qubicle Binary parser for voxel game"));

	bCreateNew = false;
	bText = false;
	bEditorImport = true;
	bEditAfterNew = false;
}

bool UQbAssetImportFactory::FactoryCanImport(const FString& Filename)
{
	return FPaths::GetExtension(Filename) == TEXT("qb");
}

UObject* UQbAssetImportFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled
)
{
	// read from file
	TArray<uint8> inBinaryArray;
	if (!FFileHelper::LoadFileToArray(inBinaryArray, *Filename))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to load file '%s'"), *Filename);
		return nullptr;
	}

	// shared because that type is needed for SetData(...)
	const auto DataShared = MakeVoxelShared<FVoxelDataAssetData>();

	// get an reference to the shared data
	FVoxelDataAssetData& Data = *DataShared;

	// parse the file
	const auto SetSize = [&](uint32 SizeX, uint32 SizeY, uint32 SizeZ)
	{
		Data.SetSize(FIntVector(SizeX, SizeY, SizeZ), true);
	};
	const auto SetColor = [&](uint32 x, uint32 y, uint32 z, uint8 red, uint8 green, uint8 blue, uint8 alpha)
	{
		FVoxelMaterial Material(ForceInit);
		Material.SetColor(FColor(red, green, blue, alpha));
		Data.SetValue(x, z, y, alpha > 0 ? FVoxelValue::Full() : FVoxelValue::Empty());
		Data.SetMaterial(x, z, y, Material);
	};
	bool success = QbReader::ReadQbFile(
		inBinaryArray,
		SetSize,
		SetColor
	);

	// clean up the buffer
	inBinaryArray.Empty();

	if (!success) {
		return nullptr;
	}

	// create the asset
	UObject* Parent = GetTransientPackage();
	UVoxelDataAsset* Asset = NewObject<UVoxelDataAsset>(InParent, *FPaths::GetBaseFilename(Filename), Flags);
	Asset->SetData(DataShared);
	Asset->Paths = { Filename };

	return Asset;
}
