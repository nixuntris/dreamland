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
#define IDX(x,y,z,size) ((x) + (y) * size + (z) * size * size)
#define INDEX(x, y, z, width, height) ((x) + (y) * (width) + (z) * (width) * (height))
#define FACE_FRONT  (1 << 0)
#define FACE_BACK   (1 << 1)
#define FACE_LEFT   (1 << 2)
#define FACE_RIGHT  (1 << 3)
#define FACE_TOP    (1 << 4)
#define FACE_BOTTOM (1 << 5)
const int FACES[6] = { FACE_FRONT,FACE_BACK,FACE_LEFT,FACE_RIGHT,FACE_TOP,FACE_BOTTOM };

struct PhysicsObject {
	Math::Vector3 position;
	Math::Vector3 velocity;
	Math::Vector3 rotation;
	Math::Vector3 angularVelocity;
	Math::Vector3 size;
	Anubis::GPUONLYShortMap byte;
	Anubis::HWVIrGeomMesh mesh;
	uint16_t* values;
	int* valuesFull;
	bool* visibility;
	void Clean() {

		for (int x = 0; x < 16; x++)
		{
			for (int y = 0; y < 16; y++)
			{
				for (int z = 0; z < 16; z++) {
					values[IDX(x, y, z, 16)] = 0;
					visibility[IDX(x, y, z, 16)] = false;

				}
			}
		}

	}
	void BuildHWMesh(const std::vector<Color>& palette)
	{
		for (int x = 0; x < 16; x++) {
			for (int y = 0; y < 16; y++) {
				for (int z= 0; z < 16; z++) {
					if (values[IDX(x, y, z, 16)] != 0) {
						size.x = std::max((double)x, size.x);
						size.y = std::max((double)y, size.y);
						size.z = std::max((double)z, size.z);

					}
					
				}
			}
		}
		std::vector<float> verts;
		std::vector<float> colors;

		verts.reserve(4096 * 6 * 6 * 3);
		colors.reserve(4096 * 6 * 6 * 3);

		const int S = 16;

		for (int x = 0; x < S; x++)
			for (int y = 0; y < S; y++)
				for (int z = 0; z < S; z++)
				{
					int idx = IDX(x, y, z, S);
					if (!visibility[idx]) continue;
					if (values[idx] == 0) continue;

					int type = values[idx];
					Color c = palette[type];

					float bx = (float)x;
					float by = (float)y;
					float bz = (float)z;

					auto emitFace = [&](int dx, int dy, int dz,
						float v0[3], float v1[3], float v2[3],
						float v3[3], float v4[3], float v5[3])
						{
							int nx = x + dx, ny = y + dy, nz = z + dz;
							bool exposed =
								(nx < 0 || nx >= S || ny < 0 || ny >= S || nz < 0 || nz >= S) ||
								(values[IDX(nx, ny, nz, S)] == 0);
							if (!exposed) return;

							float shade = 1.0f;
							if (dy > 0) shade = 1.00f;
							else if (dy < 0) shade = 0.50f;
							else if (dx != 0) shade = 0.75f;
							else              shade = 0.85f;

							// FIX: Normalize to 0.0-1.0 range
							float r = (c.r / 255.0f) * shade;
							float g = (c.g / 255.0f) * shade;
							float b = (c.b / 255.0f) * shade;

							const float* vs[6] = { v0, v1, v2, v3, v4, v5 };
							for (int i = 0; i < 6; i++) {
								verts.insert(verts.end(), vs[i], vs[i] + 3);
								colors.push_back(r);
								colors.push_back(g);
								colors.push_back(b);
							}
						};

					float p[8][3] = {
						{bx + 0, by + 0, bz + 0}, {bx + 0, by + 0, bz + 1},
						{bx + 0, by + 1, bz + 0}, {bx + 0, by + 1, bz + 1},
						{bx + 1, by + 0, bz + 0}, {bx + 1, by + 0, bz + 1},
						{bx + 1, by + 1, bz + 0}, {bx + 1, by + 1, bz + 1}
					};

					emitFace(-1, 0, 0, p[0], p[1], p[3], p[3], p[2], p[0]);
					emitFace(1, 0, 0, p[4], p[6], p[7], p[7], p[5], p[4]);
					emitFace(0, -1, 0, p[0], p[4], p[5], p[5], p[1], p[0]);
					emitFace(0, 1, 0, p[2], p[3], p[7], p[7], p[6], p[2]);
					emitFace(0, 0, -1, p[0], p[2], p[6], p[6], p[4], p[0]);
					emitFace(0, 0, 1, p[1], p[5], p[7], p[7], p[3], p[1]);
				}

		if (colors.empty()) {
			mesh.Create(verts.data(), verts.size());
		}
		else {
			mesh.CreateWithColor(verts.data(), verts.size(),
				colors.data(), colors.size());
		}
	}
	void CheckVisibility()
	{
		for (int x = 0; x < 16; x++)
		{
			for (int y = 0; y < 16; y++)
			{
				for (int z = 0; z < 16; z++)
				{
					if (values[IDX(x, y, z, 16)] == 0)
					{
						visibility[IDX(x, y, z, 16)] = false;
						continue;
					}

					visibility[IDX(x, y, z, 16)] =
						(x == 0 || values[IDX(x - 1, y, z, 16)] == 0) ||
						(x == 15 || values[IDX(x + 1, y, z, 16)] == 0) ||
						(y == 0 || values[IDX(x, y - 1, z, 16)] == 0) ||
						(y == 15 || values[IDX(x, y + 1, z, 16)] == 0) ||
						(z == 0 || values[IDX(x, y, z - 1, 16)] == 0) ||
						(z == 15 || values[IDX(x, y, z + 1, 16)] == 0);
				}
			}
		}
	}
	void Upload(const std::vector<Color>& palette) {
		byte.Clear({ 16,16,16 });
		byte.Upload(values);
		byte.GetBindlessHandle();
		valuesFull = (int*)MemAlloc(16 * 16 * 16 * sizeof(int));
		for (int i = 0; i < 16 * 16 * 16; i++) {
			valuesFull[i] = values[i];
		}
		BuildHWMesh(palette);
	}
};
struct Faces {
	uint8_t w;
	uint8_t d;
	uint8_t mask; //6
	uint8_t x; //6
	uint8_t y; //6
	uint8_t z; //6
};
struct ChunkData {
	bool skipped = false;
	int lod;
	Anubis::GPUONLYShortMap fluidByteMap;
	uint8_t* bitMap;
	uint16_t* terrain;
	uint16_t* palette;
	int paletteSize;
	uint8_t* compressedTerrain;
	bool compressed;
	int voxelCount;
	bool updated;
};
#define READ_BITMAP(x,y,z) ((chunkdata->bitMap[(((x) + (y) * size + (z) * size * size))>>3] >> ((((x) + (y) * size + (z) * size * size))&7)) & 1u)
struct PhysicsAccelerationChunk {
	int* idReferences;
	uint16_t objectsInChunk=0;
	bool allocated=false;
};
const int mwx = 300;
const int mwy = 300;
const int mwz = 300;

struct World {

	struct GlobalHandle {
		struct Region {
			GLuint64 handle;
			int lod;
		};
		Region handle[mwx * mwy * mwz];
	};
	GlobalHandle handle;
	struct VoxelObject {
		int x;
		int y;
		int z;
		int type;
	};
	std::vector<ChunkData> chunkData;
	int lightCount = 0;
	Math::Vector3 lightPositions[16];
	Math::Vector3 lightValues[16];
	std::map<int, std::vector<VoxelObject>> models;
	std::vector<VoxelObject> loadVoxelPositionsFromFile(const char* path, int typeOffset)
	{
		std::vector<VoxelObject> positions;
		FILE* f = fopen(path, "r");
		if (!f) { TraceLog(LOG_ERROR, "Could not open %s", path); return positions; }

		char section[64] = {};
		char line[128] = {};
		while (fgets(line, sizeof(line), f))
		{
			if (strncmp(line, "palette:", 8) == 0) { strcpy(section, "palette"); continue; }
			if (strncmp(line, "voxel:", 6) == 0) { strcpy(section, "voxel");   continue; }

			if (strcmp(section, "voxel") == 0)
			{
				int x, y, z, type;
				if (sscanf(line, "%d %d %d %d", &x, &y, &z, &type) == 4)
					positions.push_back({ x, y, z, type + typeOffset });
			}
		}

		fclose(f);
		return positions;
	}
	PhysicsAccelerationChunk* physicsAccelerationChunks;
	void Save() {
		std::cout << "Save\n";
		FILE* f = fopen("world.bin", "wb");
		if (!f) { TraceLog(LOG_ERROR, "Save: cannot open world.bin"); return; }

		const int voxelsPerChunk = chunkSize * chunkSize * chunkSize;

		uint32_t magic = 0x574C4431u; // 'WLD1'
		uint32_t version = 1;
		int32_t  swx = wx, swy = wy, swz = wz, sChunkSize = chunkSize;
		uint32_t modelCount = (uint32_t)models.size();
		uint32_t chunkCount = (uint32_t)chunkData.size();

		fwrite(&magic, sizeof(magic), 1, f);
		fwrite(&version, sizeof(version), 1, f);
		fwrite(&swx, sizeof(swx), 1, f);
		fwrite(&swy, sizeof(swy), 1, f);
		fwrite(&swz, sizeof(swz), 1, f);
		fwrite(&sChunkSize, sizeof(sChunkSize), 1, f);
		fwrite(&modelCount, sizeof(modelCount), 1, f);
		fwrite(&chunkCount, sizeof(chunkCount), 1, f);

		for (std::map<int, std::vector<VoxelObject>>::iterator it = models.begin();
			it != models.end(); ++it)
		{
			uint32_t modelId = (uint32_t)it->first;
			uint32_t n = (uint32_t)it->second.size();
			fwrite(&modelId, sizeof(modelId), 1, f);
			fwrite(&n, sizeof(n), 1, f);
			if (n) fwrite(it->second.data(), sizeof(VoxelObject), n, f);
		}
		for (int i = 0; i < (int)chunkData.size(); i++) {
			ChunkData& cd = chunkData[i];

			fwrite(&i, sizeof(i), 1, f);
			fwrite(&cd.skipped, sizeof(cd.skipped), 1, f);
			fwrite(&cd.lod, sizeof(cd.lod), 1, f);
			fwrite(&cd.voxelCount, sizeof(cd.voxelCount), 1, f);
			fwrite(&cd.paletteSize, sizeof(cd.paletteSize), 1, f);
			fwrite(&cd.compressed, sizeof(cd.compressed), 1, f);
			fwrite(&cd.updated, sizeof(cd.updated), 1, f);

			if (cd.skipped || cd.voxelCount == 0) continue;

			if (cd.compressed) {
				fwrite(cd.palette, sizeof(uint16_t), cd.paletteSize, f);
				fwrite(cd.compressedTerrain, 1, voxelsPerChunk, f);
			}
			else {
				fwrite(cd.terrain, sizeof(uint16_t), voxelsPerChunk, f);
			}

			size_t bmBytes = (size_t)voxelsPerChunk / 8;
			fwrite(cd.bitMap, 1, bmBytes, f);
		}

		fclose(f);
		TraceLog(LOG_INFO, "World saved");
	}
	void Setup() {
		chunkData.resize(wx * wz * wy);
		memset(handle.handle, 0, wx * wy * wz * sizeof(GLuint64));
		physicsAccelerationChunks = (PhysicsAccelerationChunk*)MemAlloc(wx * wy * wz * sizeof(PhysicsAccelerationChunk));
	}
	~World() {
		free(physicsAccelerationChunks);
		chunkData.clear();
		chunkData.reserve(0);
	}
};
long globalMemUsage = 0;
long afterGlobalMemUsage = 0;
class Chunk {
	int tallestY = 0;
public:
	bool hasFluid = false;
	int gx;
	int gy;
	int gz;
	int oldTri;
	bool updated = false;
	int allocatedTriSize = 0;
	int size;
	int lod;
	int voxelCount = 0;
	int beginningMeshlet;
	int beginningTri;
	std::vector<Vector3> grassList;
	Anubis::HWVIrGeomMesh hwvMesh;
	bool hwOrSoftwareRaster = false;
	CPUMesh cpumesh;

	void GenerateMountains(ChunkData* chunkData, GenerationData generationData, int gx, int gy, int gz, int lod = 1, bool grass = false) {
		this->gx = gx;
		this->gy = gy;
		this->gz = gz;
		START_TIMER(total)
			this->size = chunkSize / lod;
		chunkData->terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));
		this->lod = lod;
		START_TIMER(voxel_fill)
			if (gy * chunkSize + chunkSize< generationData.lowestChunkY * chunkSize || gy * chunkSize> generationData.highestChunkY * chunkSize) {
				//		chunkData->skipped = true;
						//return;
			}
		for (uint8_t x = 0; x < chunkSize; x += 1) {
			for (uint8_t z = 0; z < chunkSize; z += 1) {
				int terrainHeight = generationData.heightMap[x + z * chunkSize];
				int grassLineOffset = Math::GetRandomValueStreamlined(-50, 50);
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
								if ((x + y + Math::GetRandomValueStreamlined(-1, 1)) % 2 == 0 && grass) grassList.push_back({ (float)x + gx * 64,(float)y + gy * 64,(float)z + gz * 64 });
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
	}
	void GenerateDebug(ChunkData* chunkData, GenerationData generationData, int gx, int gy, int gz, int lod = 1, bool grass = false) {
		this->gx = gx;
		this->gy = gy;
		this->gz = gz;
		START_TIMER(total)
			this->size = chunkSize / lod;
		chunkData->terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));
		this->lod = lod;
		START_TIMER(voxel_fill)
			if (gy * chunkSize + chunkSize< generationData.lowestChunkY * chunkSize || gy * chunkSize> generationData.highestChunkY * chunkSize) {
				//		chunkData->skipped = true;
						//return;
			}
		for (uint8_t x = 0; x < chunkSize; x += 1) {
			for (uint8_t z = 0; z < chunkSize; z += 1) {
				int terrainHeight = 50;
				int grassLineOffset = 0;
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
								if ((x + y + Math::GetRandomValueStreamlined(-1, 1)) % 2 == 0 && grass) grassList.push_back({ (float)x + gx * 64,(float)y + gy * 64,(float)z + gz * 64 });
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
	}
	void GenLowerResolution(ChunkData* chunkData) {

		uint16_t* lodVals = (uint16_t*)MemAlloc(this->size / 2 * this->size / 2 * this->size / 2 * sizeof(uint16_t));

		memset(lodVals, 0, this->size * this->size * this->size * sizeof(uint8_t));
		for (int x = 0; x < this->size / 2; x++) {
			for (int y = 0; y < this->size / 2; y++) {
				for (int z = 0; z < this->size / 2; z++) {
					for (int cx = 0; cx < 2; cx++) {
						for (int cy = 0; cy < 2; cy++) {
							for (int cz = 0; cz < 2; cz++) {
								int dx = x * 2 + cx;
								int dy = y * 2 + cy;
								int dz = z * 2 + cz;
								lodVals[IDX(x, y, z, this->size / 2)] = chunkData->terrain[IDX(dx, dy, dz, this->size)];

							}
						}
					}
				}
			}
		}
		free(chunkData->terrain);
		chunkData->terrain = lodVals;
	}
	void Compress(ChunkData* chunkData) {

		if (voxelCount == 0) {
			return;
		}
		globalMemUsage += size * size * size * sizeof(uint16_t);
		chunkData->compressed = false;
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
		if (originalVoxels < 15) {
			//do the same but 4 bit
		}
		else  if (originalVoxels < 255) {
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
			afterGlobalMemUsage += size * size * size + originalVoxels * sizeof(uint16_t);
		}
	}
	uint16_t* DecompressChunk(ChunkData* chunkData) {
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
	void GenerateLod(ChunkData* chunkData, int gx, int gy, int gz) {
		voxelCount = 0;
		for (int i = 0; i < 64 * 64 * 64; i++) {
			if (chunkData->compressed) {
				voxelCount += chunkData->compressedTerrain[i] != 0;
			}
			else {
				voxelCount += chunkData->terrain[i] != 0;
			}
		}
		chunkData->voxelCount = voxelCount;
		if (voxelCount == 0) {
			free(chunkData->terrain);
			voxelCount = 0;
			return;
		}
		if (lod != 1) {
			START_TIMER(lod_generation)
			if (chunkData->compressed) {

				uint8_t* lodVals = (uint8_t*)MemAlloc(this->size * this->size * this->size * sizeof(uint8_t));

				for (int x = 0; x < this->size; x++) {
					for (int z = 0; z < this->size; z++) {
						for (int y = 0; y < this->size; y++) {
							for (int cx = 0; cx < this->lod; cx++) {
								for (int cy = 0; cy < this->lod; cy++) {
									for (int cz = 0; cz < this->lod; cz++) {
										if (chunkData->compressedTerrain[IDX(x * this->lod + cx, y * this->lod + cy, z * this->lod + cz, chunkSize)] != 0) {
											lodVals[IDX(x, y, z, this->size)] = chunkData->compressedTerrain[IDX(x * this->lod + cx, y * this->lod + cy, z * this->lod + cz, chunkSize)];
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
				MemFree(chunkData->compressedTerrain);

				chunkData->compressedTerrain = lodVals;
			}
			else {
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
		}
		chunkData->lod = lod;
		GenerateBitMap(chunkData);
	}
	void GenerateBitMap(ChunkData* chunkData) {
		chunkData->bitMap = (uint8_t*)MemAlloc(size * size * size / 8 * sizeof(uint8_t));
		for (int x = 0; x < size; x++) {
			for (int y = 0; y < size; y++) {
				for (int z = 0; z < size; z++) {

					int id = (x)+(y)*size + (z)*size * size;
					chunkData->bitMap[(id / 8)] &= ~(1u << (id % 8));
					if (chunkData->terrain[id] != 0) chunkData->bitMap[(id / 8)] |= (1 << (id % 8));

				}
			}
		}

	}
	inline bool SetTerrain(ChunkData* chunkdata, int x, int y, int z, uint16_t t) {
		if (voxelCount == 0 || lod != 1) {
			return false;
		}

		int id = x + y * size + z * size * size;
		bool set = false;

		if (chunkdata->compressed) {
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
			uint16_t oldValue = chunkdata->terrain[id];
			set = oldValue != t;
			if (set) {
				chunkdata->terrain[id] = t;
			}
		}

		if (set) {
			voxelCount += set ? 1 : 0;
		}
		return set;
	}
	inline bool ReadBitMap(ChunkData* chunkdata, int x, int y, int z) {
		if (chunkdata->skipped) return true;
		if (chunkdata->voxelCount == 0) return false;
		int ss = 64 / chunkdata->lod;
		if (ss * ss * ss == chunkdata->voxelCount) return true;
		if (chunkdata->lod == lod) {
			int id = x + (y * size) + (z * size * size);
			return (chunkdata->bitMap[id / 8] >> (id % 8)) & 1u;
		}
		return false;
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
		if (chunkData->voxelCount == 0) return 0;
		if (chunkData->skipped) return 0;
		
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
	inline void HardwareMesher(World* world, int gx, int gy, int gz)
	{
		int id = 0;
		int gg = gy * (wx * wz) + gz * wx + gx;
		ChunkData* chunkdata = &world->chunkData[gg];
		if (chunkdata->skipped) return;
		if (voxelCount == size * size * size) return;
		if (voxelCount == 0) return;
		Faces* scroll = (Faces*)MemAlloc(voxelCount * sizeof(Faces));

		for (int y = 0; y < size; y++) {
			for (int z = 0; z < size; z++) {
				int baseIndex = y * size + z * size * size;
				uint64_t xstrip = *(uint64_t*)(world->chunkData[gg].bitMap + (baseIndex >> 3)) >> (baseIndex & 7);
				xstrip &= (1ULL << size) - 1;

				while (xstrip) {
					unsigned long x;
					_BitScanForward64(&x, xstrip);
					xstrip &= xstrip - 1;

					scroll[id].x = x;
					scroll[id].y = y;
					scroll[id].z = z;

					uint8_t mask = 0;
					mask |= ((scroll[id].z == 0 && gz - 1 >= 0 && !ReadBitMap(&world->chunkData[gy * (wx * wz) + (gz - 1) * wx + gx], scroll[id].x, scroll[id].y, size - 1)) ||
						(z > 0 && !READ_BITMAP(x, y, z - 1))) * FACE_FRONT;
					mask |= ((scroll[id].z == size - 1 && gz + 1 < wz && !ReadBitMap(&world->chunkData[gy * (wx * wz) + (gz + 1) * wx + gx], scroll[id].x, scroll[id].y, 0)) ||
						(z < size - 1 && !READ_BITMAP(x, y, z + 1))) * FACE_BACK;
					mask |= ((scroll[id].x == 0 && gx - 1 >= 0 && !ReadBitMap(&world->chunkData[gy * (wx * wz) + gz * wx + (gx - 1)], size - 1, scroll[id].y, scroll[id].z)) ||
						(x > 0 && !READ_BITMAP(x - 1, y, z))) * FACE_LEFT;
					mask |= ((scroll[id].x == size - 1 && gx + 1 < wx && !ReadBitMap(&world->chunkData[gy * (wx * wz) + gz * wx + (gx + 1)], 0, scroll[id].y, scroll[id].z)) ||
						(x < size - 1 && !READ_BITMAP(x + 1, y, z))) * FACE_RIGHT;
					mask |= ((scroll[id].y == size - 1 && gy + 1 < wy && !ReadBitMap(&world->chunkData[(gy + 1) * (wx * wz) + gz * wx + gx], scroll[id].x, 0, scroll[id].z)) ||
						(y < size - 1 && !READ_BITMAP(x, y + 1, z))) * FACE_TOP;
					mask |= ((scroll[id].y == 0 && gy - 1 >= 0 && !ReadBitMap(&world->chunkData[(gy - 1) * (wx * wz) + gz * wx + gx], scroll[id].x, size - 1, scroll[id].z)) ||
						(y > 0 && !READ_BITMAP(x, y - 1, z))) * FACE_BOTTOM;


					scroll[id].mask = mask;
					if (mask != 0) {
						id += 1;
					}

				}
			}

		}
		int vertexId = 0;
		int colorId = 0;
		cpumesh = { 0 };
		int faceCount = 0;
		for (int i = 0; i < id; i++) {
			faceCount += __popcnt(scroll[i].mask);
		}
		int totalTriangleCount = faceCount * 2;
		int totalVertexCount = totalTriangleCount * 3;
		float* vertices = (float*)MemAlloc(totalVertexCount * 3 * sizeof(float));
		uint32_t* colors = (uint32_t*)MemAlloc(totalVertexCount * sizeof(uint32_t));

		int renderX = gx * chunkSize;
		int renderY = gy * chunkSize;
		int renderZ = gz * chunkSize;

		static const int faceVerts[6][4][3] = {
			{{0,0,0},{1,0,0},{1,1,0},{0,1,0}},
			{{1,0,1},{0,0,1},{0,1,1},{1,1,1}},
			{{0,0,1},{0,0,0},{0,1,0},{0,1,1}},
			{{1,0,0},{1,0,1},{1,1,1},{1,1,0}},
			{{0,1,0},{1,1,0},{1,1,1},{0,1,1}},
			{{0,0,1},{1,0,1},{1,0,0},{0,0,0}},
		};

		for (int i = 0; i < id; i++) {
			uint8_t x = scroll[i].x;
			uint8_t y = scroll[i].y;
			uint8_t z = scroll[i].z;
			uint8_t mask = scroll[i].mask;

			uint8_t type = ReadVoxel(&world->chunkData[gg], x, y, z) - 1;
			uint32_t packed = PackMetaData(mask, lod, type);

			int baseX = x * lod + renderX;
			int baseY = y * lod + renderY;
			int baseZ = z * lod + renderZ;

			for (int f = 0; f < 6; f++) {
				if (!(mask & (1 << f))) continue;

				const int(*quad)[3] = faceVerts[f];

				int triIndices[2][3] = { {0,1,2}, {0,2,3} };

				for (int t = 0; t < 2; t++) {
					for (int v = 0; v < 3; v++) {
						const int* corner = quad[triIndices[t][v]];
						vertices[vertexId++] = (float)(baseX + corner[0] * lod);
						vertices[vertexId++] = (float)(baseY + corner[1] * lod);
						vertices[vertexId++] = (float)(baseZ + corner[2] * lod);
						colors[colorId++] = packed;
					}
				}
			}
		}

		hwvMesh.Create(Anubis::cubeVertices, (size_t)Anubis::cubeVertices/sizeof(float));
		
		free(vertices);
		free(colors);
		MemFree(scroll);
	}
	inline void SoftwareMesher(World* world, int gx, int gy, int gz)
	{
		int id = 0;
		int gg = gy * (wx * wz) + gz * wx + gx;
		ChunkData* chunkdata = &world->chunkData[gg];
		if (chunkdata->skipped) return;
		if (voxelCount == size * size * size) return;
		if (voxelCount == 0) return;
			Faces* scroll = (Faces*)MemAlloc(voxelCount * sizeof(Faces));

		for (int y = 0; y < size; y++) {
			for (int z = 0; z < size; z++) {
				int baseIndex = y * size + z * size * size;
				uint64_t xstrip = *(uint64_t*)(world->chunkData[gg].bitMap + (baseIndex >> 3)) >> (baseIndex & 7);
				xstrip &= (1ULL << size) - 1;

				while (xstrip) {
					unsigned long x;
					_BitScanForward64(&x, xstrip);
					xstrip &= xstrip - 1;

					scroll[id].x = x;
					scroll[id].y = y;
					scroll[id].z = z;

					uint8_t mask = 0;
					mask |= ((scroll[id].z == 0 && gz - 1 >= 0 && !ReadBitMap(&world->chunkData[gy * (wx * wz) + (gz - 1) * wx + gx], scroll[id].x, scroll[id].y, size - 1)) ||
						(z > 0 && !READ_BITMAP(x, y, z - 1))) * FACE_FRONT;
					mask |= ((scroll[id].z == size - 1 && gz + 1 < wz && !ReadBitMap(&world->chunkData[gy * (wx * wz) + (gz + 1) * wx + gx], scroll[id].x, scroll[id].y, 0)) ||
						(z < size - 1 && !READ_BITMAP(x, y, z + 1))) * FACE_BACK;
					mask |= ((scroll[id].x == 0 && gx - 1 >= 0 && !ReadBitMap(&world->chunkData[gy * (wx * wz) + gz * wx + (gx - 1)], size - 1, scroll[id].y, scroll[id].z)) ||
						(x > 0 && !READ_BITMAP(x - 1, y, z))) * FACE_LEFT;
					mask |= ((scroll[id].x == size - 1 && gx + 1 < wx && !ReadBitMap(&world->chunkData[gy * (wx * wz) + gz * wx + (gx + 1)], 0, scroll[id].y, scroll[id].z)) ||
						(x < size - 1 && !READ_BITMAP(x + 1, y, z))) * FACE_RIGHT;
					mask |= ((scroll[id].y == size - 1 && gy + 1 < wy && !ReadBitMap(&world->chunkData[(gy + 1) * (wx * wz) + gz * wx + gx], scroll[id].x, 0, scroll[id].z)) ||
						(y < size - 1 && !READ_BITMAP(x, y + 1, z))) * FACE_TOP;
					mask |= ((scroll[id].y == 0 && gy - 1 >= 0 && !ReadBitMap(&world->chunkData[(gy - 1) * (wx * wz) + gz * wx + gx], scroll[id].x, size - 1, scroll[id].z)) ||
						(y > 0 && !READ_BITMAP(x, y - 1, z))) * FACE_BOTTOM;


					scroll[id].mask = mask;
					if (mask != 0) {
						id += 1;
					}

				}
			}

		}
		int vertexId = 0;
		int colorId = 0;
		cpumesh = { 0 };

		cpumesh.triangleCount = ceil(float(id) / 64.0f) * 64.0f;
		cpumesh.vertexCount = cpumesh.triangleCount * 3;
		cpumesh.realTriangleCount = id;
		cpumesh.vertices = (float*)MemAlloc(cpumesh.vertexCount * sizeof(float));
		memset(cpumesh.vertices, 0, cpumesh.vertexCount * sizeof(float));
		cpumesh.gpuBasedColoring = (uint32_t*)MemAlloc(cpumesh.triangleCount * sizeof(uint32_t));
		memset(cpumesh.gpuBasedColoring, 0, cpumesh.triangleCount * sizeof(uint32_t));


		int renderX = gx * chunkSize;
		int renderY = gy * chunkSize;
		int renderZ = gz * chunkSize;
		for (int i = 0; i < id; i++) {

			uint8_t x = scroll[i].x;
			uint8_t y = scroll[i].y;
			uint8_t z = scroll[i].z;

			uint8_t mask = scroll[i].mask;
			
			uint8_t type = ReadVoxel(&world->chunkData[gg], x, y, z) - 1;
			cpumesh.gpuBasedColoring[colorId++] = PackMetaData(mask, lod, type);

			cpumesh.vertices[vertexId++] = x * lod + renderX;
			cpumesh.vertices[vertexId++] = y * lod + renderY;
			cpumesh.vertices[vertexId++] = z * lod + renderZ;

		}
		PreGenMeshCullingData(&cpumesh, false);
		cpumesh.vertexCount /= 3;
		free(cpumesh.vertices);
		MemFree(scroll);
	}
};
