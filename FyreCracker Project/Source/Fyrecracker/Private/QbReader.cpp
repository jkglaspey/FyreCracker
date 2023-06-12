#include "QbReader.h"

bool QbReader::ReadQbFile(
	const TArray<uint8>& Data,
	const TFunctionRef<void(uint32 SizeX, uint32 SizeY, uint32 SizeZ)> SetSize,
	const TFunctionRef<void(uint32 x, uint32 y, uint32 z, uint8 red, uint8 green, uint8 blue, uint8 alpha)> SetColor
) {
	const int CODEFLAG = 2;
	const int NEXTSLICEFLAG = 6;

	const uint32 RGBA_FORMAT = 0;
	const uint32 BGRA_FORMAT = 1;

	FMemoryReader FromBinary = FMemoryReader(Data, true); //true, free data after done
	FromBinary.Seek(0);

	uint32 version;
	FromBinary.Serialize(&version, 4);
	uint8 MajorVersion = (version >> 0) & 0xFF;
	uint8 MinorVersion = (version >> 8) & 0xFF;
	uint8 Release = (version >> 16) & 0xFF;
	uint8 Build = (version >> 24) & 0xFF;

	if ((MajorVersion > 1) || (MajorVersion == 1 && MinorVersion > 1)) {
		UE_LOG(LogTemp, Warning, TEXT("Reading from file version: %d.%d.%d.%d. File version 1.1.0.0 is the last supported version."), MajorVersion, MinorVersion, Release, Build);
	}

	uint32 colorFormat;
	FromBinary.Serialize(&colorFormat, 4);
	if (colorFormat != 0) {
		UE_LOG(LogTemp, Error, TEXT("Only supports .qb files that are RGBA. Given code %d."), colorFormat);
		FromBinary.FlushCache();
		FromBinary.Close();
		return false;
	}

	uint32 zAxisOrientation;
	FromBinary.Serialize(&zAxisOrientation, 4);
	if (zAxisOrientation != 1) {
		UE_LOG(LogTemp, Error, TEXT("Only supports .qb files that are right handed. Given code %d."), zAxisOrientation);
		FromBinary.FlushCache();
		FromBinary.Close();
		return false;
	}

	uint32 compressed;
	FromBinary.Serialize(&compressed, 4);
	// any compression is fine. Only uncompressed has been tested.

	uint32 visibilityMaskEncoded;
	FromBinary.Serialize(&visibilityMaskEncoded, 4);
	if (visibilityMaskEncoded != 0) {
		UE_LOG(LogTemp, Error, TEXT("Only supports .qb files that do not use a visibility mask. Given code %d."), visibilityMaskEncoded);
		FromBinary.FlushCache();
		FromBinary.Close();
		return false;
	}

	uint32 numMatrices;
	FromBinary.Serialize(&numMatrices, 4);
	if (numMatrices != 1) {
		UE_LOG(LogTemp, Error, TEXT("Only supports .qb files that have 1 voxel model. Given %d."), numMatrices);
		FromBinary.FlushCache();
		FromBinary.Close();
		return false;
	}

	// read matrix name
	uint8 nameLength;
	FromBinary.Serialize(&nameLength, 1);
	if (nameLength != 0) {
		UE_LOG(LogTemp, Error, TEXT("Only supports .qb files that an unnamed voxel model. Given name of length %d."), nameLength);
		FromBinary.FlushCache();
		FromBinary.Close();
		return false;
	}

	// Try uncommenting this if names should be supported. This code is not tested.
	// TArray<char> StringData;
	// StringData.AddUninitialized(nameLength + 1);
	// FromBinary.Serialize(StringData.GetData(), nameLength);
	// StringData[nameLength] = '\0';
	// FString name = FString(StringData.GetData());

	uint32 sizeX;
	uint32 sizeY;
	uint32 sizeZ;
	FromBinary.Serialize(&sizeX, 4);
	FromBinary.Serialize(&sizeY, 4);
	FromBinary.Serialize(&sizeZ, 4);

	int32 posX;
	int32 posY;
	int32 posZ;
	FromBinary.Serialize(&posX, 4);
	FromBinary.Serialize(&posY, 4);
	FromBinary.Serialize(&posZ, 4);

	if (posX != 0 || posY != 0 || posZ != 0) {
		// beware that this presents coordinates in xyz, not xzy
		UE_LOG(LogTemp, Error, TEXT("Only supports .qb files that a voxel model at (0, 0, 0). Given (%d, %d, %d)."), posX, posY, posZ);
		FromBinary.FlushCache();
		FromBinary.Close();
		return false;
	}

	SetSize(sizeX, sizeZ, sizeY);

	const auto SetRGB = [&](int32 x, int32 y, int32 z, uint32 rgba)
	{
		uint8 v1 = (rgba >> 0) & 0xFF;
		uint8 v2 = (rgba >> 8) & 0xFF;
		uint8 v3 = (rgba >> 16) & 0xFF;
		uint8 v4 = (rgba >> 24) & 0xFF;
		uint8 red, green, blue, alpha;
		if (colorFormat == RGBA_FORMAT) {
			red = v1;
			green = v2;
			blue = v3;
			alpha = v4;
		}
		else {
			blue = v1;
			green = v2;
			red = v3;
			alpha = v4;
		}
		SetColor(x, y, z, red, green, blue, alpha);
	};

	if (compressed == 0) // if uncompressd
	{
		for (uint32 z = 0; z < sizeZ; z++)
			for (uint32 y = 0; y < sizeY; y++)
				for (uint32 x = 0; x < sizeX; x++) {
					uint32 rgba;
					FromBinary.Serialize(&rgba, 4);
					SetRGB(x, y, z, rgba);
				}
	}
	else // if compressed
	{
		uint32 z = 0;

		while (z < sizeZ)
		{
			z++;
			uint32 index = 0;

			while (true)
			{
				uint32 data;
				FromBinary.Serialize(&data, 4);

				if (data == NEXTSLICEFLAG)
					break;
				else if (data == CODEFLAG)
				{
					uint32 count;
					FromBinary.Serialize(&count, 4);
					FromBinary.Serialize(&data, 4);

					for (uint32 j = 0; j < count; j++)
					{
						uint32 x = index % sizeX + 1; // mod = modulo e.g. 12 mod 8 = 4
						uint32 y = index / sizeX + 1; // div = integer division e.g. 12 div 8 = 1
						index++;
						SetRGB(x, y, z, data);
					}
				}
				else
				{
					uint32 x = index % sizeX + 1;
					uint32 y = index / sizeX + 1;
					index++;
					SetRGB(x, y, z, data);
				}
			}
		}
	}

	FromBinary.FlushCache();
	FromBinary.Close();
	return true;
}
