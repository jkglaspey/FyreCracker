#pragma once

#include "CoreMinimal.h"

/*
* Limited Support Qubicle Binary (.qb) file reader. Requires a certain header configuration.
*/
class QbReader
{
public:
	static bool ReadQbFile(
		const TArray<uint8>& Data,
		const TFunctionRef<void(
			uint32 SizeX,
			uint32 SizeY,
			uint32 SizeZ
			)> SetSize,
		const TFunctionRef<void(
			uint32 x,
			uint32 y,
			uint32 z,
			uint8 red,
			uint8 green,
			uint8 blue,
			uint8 alpha
			)> SetColor
	);
};
