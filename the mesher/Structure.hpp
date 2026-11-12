#pragma once
#include <cinttypes>
#include "Anubis/AnubisShaders.hpp"
#include "Anubis/Anubis.hpp"
#include "Misc.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <iostream>
#include <omp.h>

struct StructureDataHolder {
	uint16_t* values;
	uint8_t* positions;
	uint8_t width, height, depth;
	int voxelCount;
	Anubis::GPUONLYShortMap map;
	inline void Create(uint16_t* values, uint8_t* positions, int voxelCount, uint8_t width, uint8_t height, uint8_t depth) {
		this->values = values;
		this->positions = positions;
		this->voxelCount = voxelCount;
		this->width = width;
		this->height = height;
		this->depth = depth;
		
		map.Clear({ this->width,this->height,this->depth });
		map.Upload();
		map.GetBindlessHandle();

		for (int i = 0; i < voxelCount; i++) {
			
			
			uint16_t val = values[i];

			int x = (uint8_t)(positions[i * 3 + 0]);
			int y = (uint8_t)(positions[i * 3 + 1]);
			int z = (uint8_t)(positions[i * 3 + 2]);
			map.ChangePixel(x, y, z, val);
		}
	}
};

struct Structure {
	int variantCount;
	std::vector<StructureDataHolder*> variants;
	std::string name;
	
	void Create(std::string name) {
		this->name = name;
	}
	inline StructureDataHolder* GetRandomVariant() {
		int variant = Math::GetRandomValueStreamlinedFast(0, variantCount - 1);
		return this->variants[variant];
	}
	inline StructureDataHolder* GetSpecificVariant(int variant) {
		if (variant < 0 || variant >= variantCount) return nullptr;
		return this->variants[variant];
	}
	inline void AddVariant(uint16_t* values, uint8_t* positions, int voxelCount, uint8_t width, uint8_t height, uint8_t depth) {
		StructureDataHolder* variant = new StructureDataHolder;
		variant->Create(values, positions, voxelCount, width, height, depth);
		this->variants.push_back(variant);
		variantCount++;
	}
	~Structure() {
		for (auto t : variants) {
			delete[] t->values;
			delete[] t->positions;
			delete t;
		}
		variants.resize(0);
		variantCount = 0;
		name = "";
	}
};
auto addTreeVariants = [&](Structure* structure, int minTrunkHeight, int maxTrunkHeight) {
	const int firstTierHeight = 6;
	const int tierSpacing = 4;

	const int straight[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
	const int diagonal[4][2] = { {1,1}, {-1,1}, {1,-1}, {-1,-1} };

	const int variantCount = 3;
	for (int v = 0; v < variantCount; v++) {
		std::vector<uint16_t> rawValues;
		std::vector<int> rawPositions;

		int minX = 0, minY = 0, minZ = 0;
		int maxX = 0, maxY = 0, maxZ = 0;

		auto addVoxel = [&](int x, int y, int z, uint16_t type) {
			rawValues.push_back(type);
			rawPositions.push_back(x);
			rawPositions.push_back(y);
			rawPositions.push_back(z);
			if (x < minX) minX = x;
			if (y < minY) minY = y;
			if (z < minZ) minZ = z;
			if (x > maxX) maxX = x;
			if (y > maxY) maxY = y;
			if (z > maxZ) maxZ = z;
		};

		int trunkHeight = minTrunkHeight + (maxTrunkHeight - minTrunkHeight) * v / (variantCount - 1);
		int variation = v * 2;

		for (int y = 0; y < trunkHeight; y++)
			addVoxel(0, y, 0, WOOD);

		for (int tier = 0, h = firstTierHeight; h < trunkHeight - 1; h += tierSpacing, tier++) {
			const int (*dirs)[2] = (tier % 2 == 0) ? straight : diagonal;

			int length = 1 + (trunkHeight - h) * 6 / (trunkHeight - firstTierHeight) + (variation % 3) - 1;
			if (length < 1) length = 1;

			for (int i = 0; i < 4; i++) {
				int dirX = dirs[i][0], dirZ = dirs[i][1];
				int sideX = -dirZ, sideZ = dirX;

				for (int step = 1; step <= length; step++) {
					int x = dirX * step;
					int z = dirZ * step;
					int y = h - step / 4;

					if (step <= length - 3) {
						addVoxel(x, y, z, WOOD);
					}
					else {
						addVoxel(x, y, z, LEAVES);
						addVoxel(x + sideX, y, z + sideZ, LEAVES);
						addVoxel(x - sideX, y, z - sideZ, LEAVES);
						addVoxel(x, y + 1, z, LEAVES);
					}
				}

				addVoxel(dirX * length, h - length / 4 - 1, dirZ * length, LEAVES);
			}

			for (int i = 0; i < 4; i++)
				addVoxel(straight[i][0], h + 1, straight[i][1], LEAVES);
		}

		for (int i = 0; i < 4; i++) {
			addVoxel(straight[i][0], trunkHeight - 2, straight[i][1], LEAVES);
			addVoxel(straight[i][0], trunkHeight - 1, straight[i][1], LEAVES);
		}
		addVoxel(0, trunkHeight, 0, LEAVES);
		addVoxel(0, trunkHeight + 1 + (variation % 2), 0, LEAVES);

		int voxelCount = (int)rawValues.size();
		uint16_t* valArray = new uint16_t[voxelCount];
		uint8_t* posArray = new uint8_t[voxelCount * 3];

		for (int i = 0; i < voxelCount; i++) {
			valArray[i] = rawValues[i];
			posArray[i * 3 + 0] = (uint8_t)(rawPositions[i * 3 + 0] - minX);
			posArray[i * 3 + 1] = (uint8_t)(rawPositions[i * 3 + 1] - minY);
			posArray[i * 3 + 2] = (uint8_t)(rawPositions[i * 3 + 2] - minZ);
		}

		structure->AddVariant(valArray, posArray, voxelCount,
			(uint8_t)(maxX - minX + 1),
			(uint8_t)(maxY - minY + 1),
			(uint8_t)(maxZ - minZ + 1));
	}

	structure->variantCount = structure->variants.size();
};
auto addRockVariants = [&](Structure* structure, int minSize, int maxSize) {
	const int variantCount = 3;

	for (int v = 0; v < variantCount; v++) {
		std::vector<uint16_t> rawValues;
		std::vector<int> rawPositions;

		int minX = 0, minY = 0, minZ = 0;
		int maxX = 0, maxY = 0, maxZ = 0;

		auto addVoxel = [&](int x, int y, int z, uint16_t type) {
			rawValues.push_back(type);
			rawPositions.push_back(x);
			rawPositions.push_back(y);
			rawPositions.push_back(z);

			if (x < minX) minX = x;
			if (y < minY) minY = y;
			if (z < minZ) minZ = z;
			if (x > maxX) maxX = x;
			if (y > maxY) maxY = y;
			if (z > maxZ) maxZ = z;
		};

		int size = minSize + (maxSize - minSize) * v / (variantCount - 1);

		int radiusX = size;
		int radiusZ = size - (v == 1 ? 1 : 0);
		int height = size / 2 + 1 + v;
		for (int y = 0; y < height; y++) {
			float vertical = (float)(y + 1) / height;

			for (int x = -radiusX; x <= radiusX; x++) {
				for (int z = -radiusZ; z <= radiusZ; z++) {
					float nx = (float)x / radiusX;
					float nz = (float)z / radiusZ;

					float distance = nx * nx + nz * nz;
					float noise = ((x * 17 + z * 31 + y * 13 + v * 47) % 7) * 0.025f;

					float threshold = 1.0f - vertical * 0.25f + noise;

					if (distance > threshold) continue;
					if (y == height - 1 && distance > 0.35f + v * 0.1f) continue;

					addVoxel(x, y, z, STONE);
				}
			}
		}

		int voxelCount = (int)rawValues.size();

		uint16_t* valArray = new uint16_t[voxelCount];
		uint8_t* posArray = new uint8_t[voxelCount * 3];

		for (int i = 0; i < voxelCount; i++) {
			valArray[i] = rawValues[i];

			posArray[i * 3 + 0] = (uint8_t)(rawPositions[i * 3 + 0] - minX);
			posArray[i * 3 + 1] = (uint8_t)(rawPositions[i * 3 + 1] - minY);
			posArray[i * 3 + 2] = (uint8_t)(rawPositions[i * 3 + 2] - minZ);
		}

		structure->AddVariant(valArray,posArray,voxelCount,(uint8_t)(maxX - minX + 1),(uint8_t)(maxY - minY + 1),(uint8_t)(maxZ - minZ + 1));
	}

	structure->variantCount = structure->variants.size();
};
auto addSphereVariants = [&](Structure* structure) {
	const int minRadius = 0;
	const int maxRadius = 16;

	const int variantCount = maxRadius - minRadius + 1;
	for (int v = 0; v < variantCount; v++) {
		std::vector<uint16_t> rawValues;
		std::vector<int> rawPositions;

		int minX = 0, minY = 0, minZ = 0;
		int maxX = 0, maxY = 0, maxZ = 0;

		auto addVoxel = [&](int x, int y, int z, uint16_t type) {
			rawValues.push_back(type);
			rawPositions.push_back(x);
			rawPositions.push_back(y);
			rawPositions.push_back(z);
			if (x < minX) minX = x;
			if (y < minY) minY = y;
			if (z < minZ) minZ = z;
			if (x > maxX) maxX = x;
			if (y > maxY) maxY = y;
			if (z > maxZ) maxZ = z;
			};

		int radius = minRadius + v;

		if (radius == 0) {
			addVoxel(0, 0, 0, 20);
		}
		else {
			int r2 = radius * radius;
			for (int x = -radius; x <= radius; x++) {
				for (int y = -radius; y <= radius; y++) {
					for (int z = -radius; z <= radius; z++) {
						if (x * x + y * y + z * z <= r2) {
							bool isShell = (x * x + y * y + z * z) > (radius - 1) * (radius - 1);
							addVoxel(x, y, z, isShell ? 0 : 7);
						}
					}
				}
			}
		}

		int voxelCount = (int)rawValues.size();
		uint16_t* valArray = new uint16_t[voxelCount];
		uint8_t* posArray = new uint8_t[voxelCount * 3];

		for (int i = 0; i < voxelCount; i++) {
			valArray[i] = rawValues[i];
			posArray[i * 3 + 0] = (uint8_t)(rawPositions[i * 3 + 0] - minX);
			posArray[i * 3 + 1] = (uint8_t)(rawPositions[i * 3 + 1] - minY);
			posArray[i * 3 + 2] = (uint8_t)(rawPositions[i * 3 + 2] - minZ);
		}

		structure->AddVariant(valArray, posArray, voxelCount,
			(uint8_t)(maxX - minX + 1),
			(uint8_t)(maxY - minY + 1),
			(uint8_t)(maxZ - minZ + 1));
	}

	structure->variantCount = structure->variants.size();
};
class StructureManager { //should store all the data but also handle the structure editor and viewer, which it currently doesn't 
public:
	int selectedStructure = 0;
	Anubis::Window* windowHandle;
	Structure structureList[1024];
	int structureCount = 0;
	inline void Init() {
		structureList[0].Create("tree");
		addTreeVariants(&structureList[0], 24, 48);
		structureList[1].Create("rock");
		addRockVariants(&structureList[1], 3, 5);
		structureList[2].Create("Sphere");
		addSphereVariants(&structureList[2]);
		ImGui::SetNextWindowPos(ImVec2(40, 40), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(520, 480), ImGuiCond_FirstUseEver);
		structureCount = 2;
	}

	inline void EditorView() {

		ImGui::Begin("Structure Editor", nullptr, ImGuiWindowFlags_NoCollapse);

		ImGui::BeginChild("StructureList", ImVec2(200, 0), true);
		ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Structures");
		ImGui::Separator();
		for (int i = 0; i < structureCount; ++i) {
			ImGui::PushID(i);
			bool selected = (selectedStructure == i);
			if (selected) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
			}
			if (ImGui::Button(structureList[i].name.c_str(), ImVec2(-1, 0))) {
				selectedStructure = i;
				std::cout << i << "\n";
			}
			if (selected) ImGui::PopStyleColor();
			ImGui::PopID();
		}
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("Preview", ImVec2(0, 0), true);
		ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Preview");
		ImGui::Separator();

		ImVec2 p = ImGui::GetCursorScreenPos();
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 rectMin = ImVec2(p.x, p.y);
		ImVec2 rectMax = ImVec2(p.x + avail.x, p.y + avail.y);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(rectMin, rectMax, IM_COL32(30, 30, 36, 255), 6.0f);
		dl->AddRect(rectMin, rectMax, IM_COL32(80, 80, 90, 255), 6.0f, 0, 2.0f);

		ImGui::EndChild();

		ImGui::End();
	}
	
};

struct AnimatedObject {
	int animationFrameTick; //how many frames need to happen for the animation tick to update
	int currentAnimationTick;
	Math::Vector3 position;
	int objectType;
	int oldAnimationTick;
	bool animationFrameUpdated;
	AnimatedObject(Math::Vector3 position, int animationFrameTick, int object) {
		this->position = position;
		this->animationFrameTick = animationFrameTick;
		this->currentAnimationTick = 2;
		this->oldAnimationTick = this->currentAnimationTick;
		this->objectType = object;
		animationFrameUpdated = true;
	}
	void Update(int frame, StructureManager *structureManager) {
		this->oldAnimationTick = this->currentAnimationTick;
		this->currentAnimationTick++;

		if (currentAnimationTick >= structureManager->structureList[objectType].variants.size()) currentAnimationTick = 0;

	}
};