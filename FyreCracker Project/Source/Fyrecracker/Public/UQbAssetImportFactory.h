#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"

#include "UQbAssetImportFactory.generated.h"

UCLASS()
class FYRECRACKER_API UQbAssetImportFactory : public UFactory
{
	GENERATED_BODY()
public:
	UQbAssetImportFactory(
		const FObjectInitializer& ObjectInitializer
	);

	virtual bool FactoryCanImport(
		const FString& Filename
	) override;
	virtual UObject* FactoryCreateFile(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags Flags,
		const FString& Filename,
		const TCHAR* Parms,
		FFeedbackContext* Warn,
		bool& bOutOperationCanceled
	) override;
};



