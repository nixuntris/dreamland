#pragma once
#include <cinttypes>
#include "Anubis/AnubisShaders.hpp"
#include "Anubis/Anubis.hpp"
#include "NoiseGen.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iostream>
#include <omp.h>
#include "Structure.hpp"

#include "Misc.hpp"
#define IDX(x,y,z,size) ((x) + (y) * size + (z) * size * size)
#define INDEX(x, y, z, width, height) ((x) + (y) * (width) + (z) * (width) * (height))
#define FACE_FRONT  (1 << 0)
#define FACE_BACK   (1 << 1)
#define FACE_LEFT   (1 << 2)
#define FACE_RIGHT  (1 << 3)
#define FACE_TOP    (1 << 4)
#define FACE_BOTTOM (1 << 5)
const int FACES[6] = { FACE_FRONT,FACE_BACK,FACE_LEFT,FACE_RIGHT,FACE_TOP,FACE_BOTTOM };

struct ChunkData {
	bool skipped = false;
	int lod;
	Anubis::GPUONLYShortMap fluidShortMap;
	Anubis::GPUONLYByteMap fluidByteMap;
	Anubis::GPUONLYByteMap probeMap;
	uint16_t* terrain;
	uint16_t* palette;
	int paletteSize;
	uint8_t* compressedTerrain;
	bool compressed;
	int voxelCount = 0;
	bool updated = false;
};
const int mwx = 300;
const int mwy = 32;
const int mwz = 300;

struct World {
	std::vector<AnimatedObject> animatedObjects;
	bool array[mwx][mwz];
	uint8_t setLod[mwx][mwz];
	struct GlobalHandle {
		struct Region {
			GLuint64 handle = 0;
			GLuint64 lightStorageHandle = 0;
			int lod;
			int filled; 
		};
		Region handle[mwx * mwy * mwz];
	};
	GlobalHandle handle;
	std::vector<ChunkData> chunkData;
	void Setup() {
		chunkData.resize(wx * wz * wy);
		memset(handle.handle, 0, wx * wy * wz * sizeof(GLuint64));
		for (int x = 0; x < wx; x++) {
			for (int y = 0; y < wz; y++) {
				array[x][y] = false;
				setLod[x][y] = 0;
			}
		}
	}
	~World() {
		chunkData.clear();
		chunkData.reserve(0);
	}
};
long globalMemUsage = 0;
long afterGlobalMemUsage = 0;
enum CHUNKTYPE {
	DESERT = 0,
	SAVANNA = 1,
	BEACH = 2,
	MOUNTAIN = 3,
	CAVES = 4
};	
float caveScale = 0.025f;
float detailRatio = 2.1428f;

const float caveDetailScale = caveScale * detailRatio;

class Chunk {
	int tallestY = 0;
public:
	int gx;
	int gy;
	int gz;
	int oldTri;
	bool updated = false;
	int allocatedTriSize = 0;
	bool hardwareRasterize = false;
	int size;
	int lod;
	int voxelCount = 0;
	int endTri;
	int beginningTri;
	std::vector<Vector3> grassList;
	bool shortenedData = false;
	inline void GeneratePerType(CHUNKTYPE type, ChunkData* chunkData, GenerationData generationData, int gx, int gy, int gz, int lod = 1, bool grass = false) {
		switch (type) {
		case DESERT:
			GenerateDesert(chunkData, generationData, gx, gy, gz, lod, grass);
			break;
		case SAVANNA:
			GenerateSavanna(chunkData, generationData, gx, gy, gz, lod, grass);
			break;
		case MOUNTAIN:
			GenerateMountains(chunkData, generationData, gx, gy, gz,lod, grass); //the problem is grass, specifically if it's set to true, it makes the program randomly hang after generation
			break;	
		}
	}
	inline void GenerateSavanna(ChunkData* chunkData, GenerationData generationData, int gx, int gy, int gz, int lod = 1, bool grass = false) {
		this->gx = gx;
		this->gy = gy;
		this->gz = gz;
		this->size = chunkSize / lod;
		this->lod = lod;
		chunkData->terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));

		const int maxSavannaY = 250;
		if (gy * chunkSize > maxSavannaY) {
			chunkData->skipped = true;
			return;
		}

		const int baseWX = gx * chunkSize;
		const int baseWY = gy * chunkSize;
		const int baseWZ = gz * chunkSize;

		for (int x = 0; x < chunkSize; x++) {
			for (int z = 0; z < chunkSize; z++) {
				float wxf = (float)(baseWX + x);
				float wzf = (float)(baseWZ + z);
				float broad = perlin_noise_2d(wxf * 0.004f, wzf * 0.004f) * 10.0f;
				float small = perlin_noise_2d(wxf * 0.020f, wzf * 0.020f) * 2.5f; 
				int terrainHeight = 135 + (int)(broad + small) + 17;

				float dryness = perlin_noise_2d(wxf * 0.03f, wzf * 0.03f);
				float redPatch = perlin_noise_2d(wxf * 0.012f + 50.0f, wzf * 0.012f + 50.0f);
				float fine = SavRand01(baseWX + x, baseWZ + z, 7);

				uint16_t topBlock;
				if (redPatch > 0.45f) topBlock = 18;
				else if (dryness > 0.15f) topBlock = (fine > 0.8f ? 10 : 9);
				else topBlock = (fine > 0.7f ? 9 : 10);

				for (int y = chunkSize - 1; y >= 0; y--) {
					int worldY = baseWY + y;
					if (worldY >= terrainHeight) continue;

					uint16_t v;
					if (worldY == terrainHeight - 1) v = topBlock;
					else if (worldY >= terrainHeight - 4) v = 18;
					else if (worldY >= terrainHeight - 7) v = 4;
					else v = 2;

					chunkData->terrain[IDX(x, y, z, chunkSize)] = v;
				}

				int tuftY = terrainHeight - baseWY;
				if (tuftY >= 0 && tuftY < chunkSize - 2 && topBlock != 18) {
					float r = SavRand01(baseWX + x, baseWZ + z, 31);
					if (r < 0.14f) {
						int h = 1 + (int)(SavRand01(baseWX + x, baseWZ + z, 32) * 2.0f);
						for (int t = 0; t < h; t++)
							chunkData->terrain[IDX(x, tuftY + t, z, chunkSize)] = (r < 0.05f ? 9 : 8);
					}
					else if (r > 0.995f) {
						chunkData->terrain[IDX(x, tuftY, z, chunkSize)] = 21;
						if (x + 1 < chunkSize) chunkData->terrain[IDX(x + 1, tuftY, z, chunkSize)] = 27;
						chunkData->terrain[IDX(x, tuftY + 1, z, chunkSize)] = 20;
					}
				}
			}
		}

			
		
	}

	inline void GenerateCaves(ChunkData* chunkData, int gx, int gy, int gz, int lod = 1) {
		this->gx = gx;
		this->gy = gy;
		this->gz = gz;
		this->size = chunkSize / lod;
		this->lod = lod;
		chunkData->terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));

		const int baseWX = gx * chunkSize;
		const int baseWY = gy * chunkSize;
		const int baseWZ = gz * chunkSize;
		float noiseValues[chunkSize/4][chunkSize/4][chunkSize / 4];
		for (int x = 0; x < chunkSize / 4; x ++) {

			int worldX = baseWX + x * 4;
			for (int y = 0; y < chunkSize/4; y++) {
				int worldY = baseWY + y * 4;
				for (int z = 0; z < chunkSize / 4; z++) {

					int worldZ = baseWZ + z * 4;

					float broad = perlin_noise_3d(
						worldX * caveScale,
						worldY * caveScale,
						worldZ * caveScale
					);
					float detail = perlin_noise_3d(
						worldX * caveDetailScale,
						worldY * caveDetailScale,
						worldZ * caveDetailScale
					);
					noiseValues[x][y][z] = broad * 0.75f + detail * 0.25f;
				}
			}
		}
		for (int x = 0; x < chunkSize; x ++) {
			for (int y = 0; y < chunkSize; y ++) {
				int worldY = baseWY + y;
				for (int z = 0; z < chunkSize; z ++) {
					int worldX = baseWX + x;
					int worldZ = baseWZ + z;

					float cave = noiseValues[x/4][y/4][z/4];

					chunkData->terrain[IDX(x, y, z, chunkSize)] = (cave > 0.32f)*STONE;
				}
			}
		}
	}

	inline void GenerateDesert(ChunkData* chunkData, GenerationData generationData, int gx, int gy, int gz, int lod = 1, bool grass = false) {
		this->gx = gx;
		this->gy = gy;
		this->gz = gz;
		START_TIMER(total)
		this->size = chunkSize / lod;
		chunkData->terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));
		this->lod = lod;
		START_TIMER(voxel_fill)
		if ((gy  + 1< generationData.lowestChunkY)/5 || (gy > generationData.highestChunkY)/5) {
			chunkData->skipped = true;
			return;
		}
		for (uint8_t x = 0; x < chunkSize; x += 1) {
			for (uint8_t z = 0; z < chunkSize; z += 1) {
				int terrainHeight = generationData.heightMap[x + z * chunkSize]/5+125;
				int grassLineOffset = Math::GetRandomValueStreamlinedFast(-50, 50);
				for (int y = chunkSize - 1; y >= 0; y -= 1) {
					int worldY = gy * chunkSize + y;
					if (worldY < terrainHeight - 6) {
						chunkData->terrain[
							IDX(x, y, z, chunkSize)
						] = 9; // sand
						if (chunkData->terrain[IDX(x, y + 1, z,chunkSize)] == 0) {
							if ((x + y + Math::GetRandomValueStreamlinedFast(-1, 1)) % 2 == 0 && grass && Math::GetRandomValueStreamlinedFast(0,1000)==150) grassList.push_back({ (float)x + gx * chunkSize,(float)y + gy * chunkSize,(float)z + gz * chunkSize});
						}
					}
				}
			}
		}
	}
	inline void GenerateMountains(ChunkData* chunkData, GenerationData generationData, int gx, int gy, int gz, int lod = 1, bool grass = false) {
		this->gx = gx;
		this->gy = gy;
		this->gz = gz;
		START_TIMER(total)
		if (lod == 0) {
			chunkData->skipped = true;
			return;
		}
		this->size = chunkSize / lod;
		this->lod = lod;
		START_TIMER(voxel_fill)
		if (gy * chunkSize + chunkSize< generationData.lowestChunkY * chunkSize || gy * chunkSize> generationData.highestChunkY * chunkSize) {
			chunkData->skipped = true;
			return;
		}
		chunkData->terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));

		for (uint8_t x = 0; x < chunkSize; x += 1) {
			for (uint8_t z = 0; z < chunkSize; z += 1) {
				int terrainHeight = generationData.heightMap[x + z * chunkSize];
				int grassLineOffset = Math::GetRandomValueStreamlinedFast(-50, 50);
				for (int y = chunkSize - 1; y >= 0; y -= 1) {
					int worldY = gy * chunkSize + y;
					if (worldY < terrainHeight - 6) {
						chunkData->terrain[
							IDX(x, y, z, chunkSize)
						] = 2; // stone

					}
					else if (worldY < terrainHeight) {
						if (worldY < 200 + grassLineOffset) {
							chunkData->terrain[
								IDX(x, y, z, chunkSize)
							] = 5; //dirt
							if (chunkData->terrain[
								IDX(x, y + 1, z, chunkSize)
							] == 0) {
								if ((x + y + Math::GetRandomValueStreamlinedFast(-1, 1)) % 2 == 0 && grass) grassList.push_back({ (float)x + gx * chunkSize,(float)y + gy * chunkSize,(float)z + gz * chunkSize });
							}
						}
						else if (worldY < 400 + grassLineOffset) {

							chunkData->terrain[
								IDX(x, y, z, chunkSize)
							] = 2; //stone

						}
						else {
							chunkData->terrain[
								IDX(x, y, z, chunkSize)
							] = 6; //snow


						}
					}
				}
			}
		}

//		CarveCaves(chunkData, gx, gy, gz, lod);
	}
	inline void CarveCaves(ChunkData* chunkData, int gx, int gy, int gz, int lod = 1) {
		const int baseWX = gx * chunkSize;
		const int baseWY = gy * chunkSize;
		const int baseWZ = gz * chunkSize;

		const float surfaceThreshold = 0.45f; 
		const float deepThreshold = 0.28f;
		const int   surfaceWorldY = 220;
		const int   deepWorldY = 40; 

		float noiseValues[chunkSize / 4][chunkSize / 4][chunkSize / 4];
		for (int x = 0; x < chunkSize / 4; x++) {

			int worldX = baseWX + x * 4;
			for (int y = 0; y < chunkSize / 4; y++) {
				int worldY = baseWY + y * 4;
				for (int z = 0; z < chunkSize / 4; z++) {

					int worldZ = baseWZ + z * 4;

					float broad = perlin_noise_3d(
						worldX * caveScale,
						worldY * caveScale,
						worldZ * caveScale
					);
					float detail = perlin_noise_3d(
						worldX * caveDetailScale,
						worldY * caveDetailScale,
						worldZ * caveDetailScale
					);
					noiseValues[x][y][z] = broad * 0.75f + detail * 0.25f;
				}
			}
		}
		for (int x = 0; x < chunkSize; x += 1) {
			for (int y = 0; y < chunkSize; y += 1) {
				int worldY = baseWY + y;
				for (int z = 0; z < chunkSize; z += 1) {
					uint16_t& cell = chunkData->terrain[IDX(x, y, z, chunkSize)];

					if (cell != 2 && cell != 5) continue;

					int worldX = baseWX + x;
					int worldZ = baseWZ + z;

					float wx = (float)worldX;
					float wy = (float)worldY;
					float wz = (float)worldZ;

					float cave = noiseValues[x/4][y/4][z/4];
					float t = Math::Clamp((float)(surfaceWorldY - worldY) /
						(float)(surfaceWorldY - deepWorldY), 0.f, 1.f);
					float threshold = Lerp(surfaceThreshold, deepThreshold, t);

					if (cave > threshold) {
						cell = 0;
					}
				}
			}
		}
	}
	inline void GenerateLod(ChunkData* chunkData, int gx, int gy, int gz) {
		
		voxelCount = 0;
		for (int i = 0; i < chunkSize * chunkSize * chunkSize; i++) {
			voxelCount += chunkData->terrain[i] != 0;
		}
		chunkData->voxelCount = voxelCount;
		if (voxelCount == 0) {
			free(chunkData->terrain);
			voxelCount = 0;
			return;
		}
		if (lod != 1) {
			START_TIMER(lod_generation)
			uint16_t* lodVals = (uint16_t*)MemAlloc(this->size * this->size * this->size * sizeof(uint16_t));

			for (int x = 0; x < this->size; x++) {
				for (int z = 0; z < this->size; z++) {
					for (int y = 0; y < this->size; y++) {
						for (int cx = 0; cx < this->lod; cx++) {
							for (int cy = 0; cy < this->lod; cy++) {
								for (int cz = 0; cz < this->lod; cz++) {
									if (chunkData->terrain[IDX(x * this->lod + cx, y * this->lod + cy, z * this->lod + cz, chunkSize)] != 0) {
										lodVals[IDX(x, y, z, this->size)] = chunkData->terrain[IDX(x * this->lod + cx, y * this->lod + cy, z * this->lod + cz, chunkSize)];
										break;
										break;
										break;
									}
								}
							}
						}

					}
				}
			}
			MemFree(chunkData->terrain);

			chunkData->terrain = lodVals;
		}
		chunkData->lod = lod;
	}
	inline void Compress(ChunkData* chunkData) {

		if (voxelCount == 0) {
			return;
		}
		chunkData->compressed = false;
		bool over255 = true;
		for (int i = 0; i < size * size * size; i++) {
			if (chunkData->terrain[i] > 255) {
				over255 = false;
				break;
			}
		}
		if (over255) {
			chunkData->compressedTerrain = (uint8_t*)MemAlloc(size * size * size);

			for (int i = 0; i < size * size * size; i++) {
				chunkData->compressedTerrain[i] = (uint8_t)chunkData->terrain[i];
			}

			shortenedData = true;

			free(chunkData->terrain);
			chunkData->terrain = NULL;
			return;
		}
		uint16_t voxels[65535];
		for (int i = 0; i < 65535; i++) {
			voxels[i] = 0;
		}
		for (int i = 0; i < size * size * size; i++) {
			voxels[chunkData->terrain[i]] += 1;
		}
		int originalVoxels = 0;
		uint16_t palette[255]; 
		for (int i = 1; i < 65535; i++) {
			if (voxels[i] != 0) {
				palette[originalVoxels] = i;
				originalVoxels++;
			}
		}
		if (originalVoxels < 255) {
			chunkData->compressed = true;
			chunkData->compressedTerrain = (uint8_t*)MemAlloc(size * size * size);
			uint8_t lookup[65535] = { 0 }; 
			for (int i = 0; i < originalVoxels; i++) {
				lookup[palette[i]] = i + 1;
			}

			for (int i = 0; i < size * size * size; i++) {
				uint16_t originalValue = chunkData->terrain[i];
				chunkData->compressedTerrain[i] = lookup[originalValue];
			}

			chunkData->paletteSize = originalVoxels;
			chunkData->palette = (uint16_t*)MemAlloc(originalVoxels * sizeof(uint16_t));
			memcpy(chunkData->palette, palette, originalVoxels * sizeof(uint16_t));

			free(chunkData->terrain);
			chunkData->terrain = NULL; 
		}
	}
	inline uint16_t* DecompressChunk(ChunkData* chunkData) {
		int totalVoxels = size * size * size;
		uint16_t* decompressedData = (uint16_t*)MemAlloc(totalVoxels * sizeof(uint16_t));
		if (chunkData->compressed) {
			for (int i = 0; i < totalVoxels; i++) {
				uint8_t compressedValue = chunkData->compressedTerrain[i];
				decompressedData[i] = (compressedValue == 0) ? 0 : chunkData->palette[compressedValue - 1];
			}
		}
		else {
			memcpy(decompressedData, chunkData->terrain, totalVoxels * sizeof(uint16_t));
		}

		return decompressedData;
	}
	inline bool SetTerrain(ChunkData* chunkdata, int x, int y, int z, uint16_t t, bool setGPUDATA=true) {
		int id = x + y * size + z * size * size;
		bool set = false;
		
		if (shortenedData) {
			if (chunkdata->compressedTerrain[id] == 0) {
				voxelCount++;
			}
			chunkdata->compressedTerrain[id] = (uint8_t)t;
			if (setGPUDATA) chunkdata->fluidByteMap.ChangePixel(x, y, z, t);
			//edit byte map
			return true;
		}
		if (chunkdata->compressed) {
			//edit byte map
			uint8_t compressedValue = 0;
			for (int i = 0; i < chunkdata->paletteSize; i++) {
				if (chunkdata->palette[i] == t) {
					compressedValue = i + 1;
					break;
				}
			}
			if (compressedValue == 0 && t != 0) {
				if (chunkdata->paletteSize < 255) {
					chunkdata->palette[chunkdata->paletteSize] = t;
					compressedValue = chunkdata->paletteSize + 1;
					chunkdata->paletteSize++;
				}
				else {
					return false;
				}
			}

			uint8_t oldCompressed = chunkdata->compressedTerrain[id];
			set = oldCompressed != compressedValue;
			if (set) {
				chunkdata->compressedTerrain[id] = compressedValue;
			}
		}
		else {
			//edit short map
			uint16_t oldValue = chunkdata->terrain[id];
			set = oldValue != t;
			if (set) {
				chunkdata->terrain[id] = t;
			}
			if (setGPUDATA) chunkdata->fluidShortMap.ChangePixel(x, y, z, t);
		}

		if (set) {
			voxelCount += set ? 1 : 0;
		}
		return set;
	}
	inline void DownscaleLoD(ChunkData* chunkData, int targetLod)
	{
		if (chunkData->voxelCount == 0) return;

		START_TIMER(downscale_lod)

			while (lod < targetLod) {
				const int srcSize = size;
				const int dstSize = size / 2;
				if (dstSize == 0) break;

				const bool srcIsByte = shortenedData || chunkData->compressed;

				auto sampleSource = [&](int sx, int sy, int sz) -> uint16_t {
					int id = sx + sy * srcSize + sz * srcSize * srcSize;
					if (shortenedData) {
						return chunkData->compressedTerrain[id];
					}
					if (chunkData->compressed) {
						uint8_t cv = chunkData->compressedTerrain[id];
						return cv == 0 ? 0 : chunkData->palette[cv - 1];
					}
					return chunkData->terrain[id];
					};

				auto downsample = [&](int dx, int dy, int dz) -> uint16_t {
					for (int cz = 0; cz < 2; cz++) {
						int sz = dz * 2 + cz; if (sz >= srcSize) continue;
						for (int cy = 0; cy < 2; cy++) {
							int sy = dy * 2 + cy; if (sy >= srcSize) continue;
							for (int cx = 0; cx < 2; cx++) {
								int sx = dx * 2 + cx; if (sx >= srcSize) continue;
								uint16_t v = sampleSource(sx, sy, sz);
								if (v != 0) return v;
							}
						}
					}
					return 0;
					};

				if (srcIsByte) {
					uint8_t* dst = chunkData->compressedTerrain;

					if (chunkData->compressed) {
						uint8_t lookup[65536];
						memset(lookup, 0, sizeof(lookup));
						for (int i = 0; i < chunkData->paletteSize; i++)
							lookup[chunkData->palette[i]] = (uint8_t)(i + 1);

						std::vector<uint8_t> scratch(dstSize * dstSize * dstSize);
						for (int z = 0; z < dstSize; z++)
							for (int y = 0; y < dstSize; y++)
								for (int x = 0; x < dstSize; x++) {
									uint16_t v = downsample(x, y, z);
									scratch[x + y * dstSize + z * dstSize * dstSize] =
										(v == 0) ? 0 : lookup[v];
								}
						memcpy(dst, scratch.data(), scratch.size());
					}
					else {
						std::vector<uint8_t> scratch(dstSize * dstSize * dstSize);
						for (int z = 0; z < dstSize; z++)
							for (int y = 0; y < dstSize; y++)
								for (int x = 0; x < dstSize; x++)
									scratch[x + y * dstSize + z * dstSize * dstSize] =
									(uint8_t)downsample(x, y, z);
						memcpy(dst, scratch.data(), scratch.size());
					}
				}
				else {
					uint16_t* dst = chunkData->terrain;
					std::vector<uint16_t> scratch(dstSize * dstSize * dstSize);
					for (int z = 0; z < dstSize; z++)
						for (int y = 0; y < dstSize; y++)
							for (int x = 0; x < dstSize; x++)
								scratch[x + y * dstSize + z * dstSize * dstSize] =
								downsample(x, y, z);
					memcpy(dst, scratch.data(), scratch.size() * sizeof(uint16_t));
				}

				size = dstSize;
				lod *= 2;
				chunkData->lod = lod;

				voxelCount = 0;
				const int total = dstSize * dstSize * dstSize;
				if (shortenedData || chunkData->compressed) {
					uint8_t* buf = chunkData->compressedTerrain;
					for (int i = 0; i < total; i++) voxelCount += (buf[i] != 0);
				}
				else {
					uint16_t* buf = chunkData->terrain;
					for (int i = 0; i < total; i++) voxelCount += (buf[i] != 0);
				}
				chunkData->voxelCount = voxelCount;
			}

	}
	inline uint32_t PackMetaData(uint8_t mask, uint8_t lod, uint16_t type)
	{
		return
			((uint32_t)(mask & 0x3Fu)) |          // Bits 0-5   (6 bits)
			((uint32_t)(lod & 0x3Fu) << 6) |      // Bits 6-11  (6 bits)
			((uint32_t)(type & 0xFFFFu) << 12);   // Bits 12-27 (16 bits)
	}
	uint16_t ReadVoxel(ChunkData* chunkData, int x, int y, int z) {
		int index = x + y * size + z * size * size; 
		if (index<0 || index>size * size * size) return 0;
		if (chunkData->voxelCount == 0) return 0;
		if (chunkData->skipped) return 0;
		if (shortenedData) {
			return chunkData->compressedTerrain[index];;

		}
		if (chunkData->compressed) {
			uint8_t compressedValue = chunkData->compressedTerrain[index];
			if (compressedValue == 0) {
				return 0; 
			}
			return chunkData->palette[compressedValue - 1];
		}
		else {
			return chunkData->terrain[index];
		}
	}
	inline void ClearChunkData(ChunkData* cd)
	{
		if (!cd) return;
		if (cd->voxelCount != 0) {

			if (cd->terrain && !cd->compressed && cd->lod!=32) {
				MemFree(cd->terrain);
				cd->terrain = NULL;
			}
			if (cd->compressedTerrain && (cd->compressed || shortenedData)) {
				MemFree(cd->compressedTerrain);
				cd->compressedTerrain = NULL;
			}
			if (cd->palette && cd->compressed) {
				MemFree(cd->palette);
				cd->palette = NULL;
			}
		}

		lod = 0;
		voxelCount = 0;
		cd->fluidShortMap.Delete();
		cd->fluidByteMap.Delete();

		cd->skipped = false;
		cd->lod = 0;
		cd->paletteSize = 0;
		cd->compressed = false;
		cd->voxelCount = 0;
		cd->updated = false;

		shortenedData = false;
		updated = false;
		voxelCount = 0;
		allocatedTriSize = 0;
		beginningTri = 0;
		endTri = 0;
		oldTri = 0;

		tallestY = 0;
		grassList.clear();
	}
};
