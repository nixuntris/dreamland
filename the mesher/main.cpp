//TODO:
//IMPLEMENT VOXELIZATION FOR MODELS
//ADD LOADING GLTF OBJECTS

#include "raylib.h"
#include "imgui_impl_glfw.h"
#include "raymath.h"
#include <float.h>
#include <map>
#include "SoftwareRasterizer.h"
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "NoiseGen.hpp"
#include "Chunk.hpp"
#include "Misc.hpp"
#include <thread>
#include <chrono>
#include "Structure.hpp"
#include "ParticleManager.hpp"
#include "Viewport.hpp"
bool spawnAnimatedObjects = true;
const int maxGPUBatch = 1024 * 1024;
const int maxGrassCount = 1449303 * 2;
const int treeAnimationUpdate = 150;
class App {
	Anubis::Window window;
	Chunk* chunks;
	ShaderManager shaders;
	World world;
	float* particles;
	bool firstGen = true;
	int totalTriangleCount;
	Math::IVector2 resolution = { 3840,2160 };
	int currentResolutionWindow = 3;
	int grass = 0;
	int totalMeshletCount = 0;
	int editSize = 5;
	Math::IVector3* softwareRaster;
	Settings graphicsSettings;
	float timeSpeed = 0.0004;
	bool mainMenu = false;
	int alreadySpawnedGrass = 0;
	int sampleCount = 1;
	ViewPort viewPort;
	ParticleRender particleRenderer;
	CHUNKTYPE selectedType = MOUNTAIN;
	StructureManager structureManager;
	int maxMeshUpdate = 2;
	int renderDistance = 500;
	int drd = 0;
public:
	App() {
		//Implement HiZ for meshlets
		//Optimize ray traversal
		//Speed up chunk generation
		//Fix the editing in meshlets
		resolution.width = graphicsSettings.resolutions[currentResolutionWindow].width;
		resolution.height = graphicsSettings.resolutions[currentResolutionWindow].height;

		matProj = MatrixPerspective(90 * DEG2RAD, ((double)resolution.width / (double)resolution.height), 0.1, 100000.0);

		window.Init(resolution.width,resolution.height,4,6,"software rasterization");
		window.DisableCursor();
		window.Resize(600, 800);
		
		omp_set_num_threads(32);
		viewPort.camera.target = { 0,1,0 };
		SetRandomSeed(100);
		shaders.Load();
		shaders.display.CreateSSBO(0, sizeof(uint32_t) * 3840 * 2160 * 6);
		int maxSSBOCount = 0;
		glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxSSBOCount);
		shaders.display.CreatePersistentSSBO(9, sizeof(int) * 2+1024*1024*2*sizeof(int));
		shaders.display.CreateSSBO(10, maxGPUBatch * sizeof(Math::IVector3));
		softwareRaster = (Math::IVector3*)(MemAlloc(1024 * 1024 * sizeof(Math::IVector3)));
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();

		totalTriangleCount = 59259259/2;
		totalMeshletCount = totalTriangleCount/64;
		shaders.display.CreatePersistentSSBO(5, (totalMeshletCount + 1) * sizeof(int) * 8);
		shaders.display.CreatePersistentSSBO(2, totalTriangleCount * sizeof(uint32_t) * 8);
		shaders.display.CreatePersistentSSBO(6, totalTriangleCount * sizeof(uint32_t) * 8);
		shaders.display.CreatePersistentSSBO(7, totalMeshletCount * 6 * sizeof(float) * 8);
		firstGen = false;
		shaders.display.CreateSSBO(20, maxGrassCount * 3 * sizeof(float));
		ImGui_ImplGlfw_InitForOpenGL(window.GetWindowHandle(), true);
		ImGui_ImplOpenGL3_Init("#version 460");
		viewPort.shaders = &shaders;
		structureManager.Init();
	}
	inline void GenerateTerrain() {
		chunks = (Chunk*)MemAlloc(wx * wz * wy * sizeof(Chunk));
	}
	inline void uploadChunkSSBO(uint32_t neighbourId) {
		Anubis::SwitchShaderStage(shaders.display.shaderId);
		shaders.display.UpdateSSBO(
			3,
			&world.handle.handle[neighbourId],
			sizeof(World::GlobalHandle::Region),
			neighbourId * sizeof(World::GlobalHandle::Region)
		);
	};
	inline void SetLODChunks(int x, int z, int lod) {
		for (int y = 0; y < wy; y++) {
			int id = y * (wx * wz) + z * wx + x;

			ChunkDeleteMeshData(x, y, z);

			chunks[id].ClearChunkData(&world.chunkData[id]);

			world.chunkData[id].fluidShortMap.Delete();
			world.chunkData[id].fluidByteMap.Delete();

			chunks[id].endTri = 0;
			chunks[id].oldTri = 0;
			world.chunkData[id].updated = false;
		}

		world.setLod[x][z] = lod;
		world.array[x][z] = true;

		float dist = Vector2Distance(
			{ (float)x * chunkSize + chunkSize / 2, (float)z * chunkSize + chunkSize / 2 },
			{ float(viewPort.camera.position.x), float(viewPort.camera.position.z) });
		bool grass = false;
		if (dist < 1920) grass = true;
		GenerationData gen;
		gen.GenerateMountains(x, z);
#pragma omp parallel for num_threads(4)
		for (int y = gen.lowestChunkY; y < gen.highestChunkY; y++) {
			chunks[y * (wx * wz) + z * wx + x].GeneratePerType(selectedType,&world.chunkData[y * (wx * wz) + z * wx + x], gen, x, y, z, lod, grass);
			
			chunks[y * (wx * wz) + z * wx + x].GenerateLod(
				&world.chunkData[y * (wx * wz) + z * wx + x], x, y, z);
			chunks[y * (wx * wz) + z * wx + x].Compress(
				&world.chunkData[y * (wx * wz) + z * wx + x]);
		}

		free(gen.heightMap);

		for (int ny = 0; ny < wy; ny++) {
			int neighbourId = ny * (wx * wz) + z * wx + x;
			if (x < 0 || ny < 0 || z < 0 || x >= wx || ny >= wy || z >= wz) continue;
			if (chunks[neighbourId].lod == 0) continue;

			int size = chunkSize / chunks[neighbourId].lod;
			if (world.chunkData[neighbourId].skipped) {
				world.handle.handle[neighbourId].lod = 32;
				world.handle.handle[neighbourId].filled = 1;

				continue;
			}
			if (world.chunkData[neighbourId].voxelCount != 0 &&
				!world.chunkData[neighbourId].skipped && chunks[neighbourId].voxelCount > chunkSize* chunkSize* chunkSize) {

				if (chunks[neighbourId].shortenedData) {
					world.chunkData[neighbourId].fluidByteMap.Clear({ size, size, size });
					world.chunkData[neighbourId].fluidByteMap.Upload(
						world.chunkData[neighbourId].compressedTerrain);
					world.chunkData[neighbourId].fluidByteMap.GetBindlessHandle();
					world.handle.handle[neighbourId].handle =
						world.chunkData[neighbourId].fluidByteMap.handle;
				}
				else {
					world.chunkData[neighbourId].fluidShortMap.Clear({ size, size, size });
					uint16_t* tt = chunks[neighbourId].DecompressChunk(
						&world.chunkData[neighbourId]);
					world.chunkData[neighbourId].fluidShortMap.Upload(tt);
					free(tt);
					world.chunkData[neighbourId].fluidShortMap.GetBindlessHandle();
					world.handle.handle[neighbourId].handle =
						world.chunkData[neighbourId].fluidShortMap.handle;
				}

				world.handle.handle[neighbourId].lod = world.chunkData[neighbourId].lod;
			}
		}

	}
	inline void ChunkDeleteMeshData(int nx, int ny, int nz) {

		int neighbourId = ny * (wx * wz) + nz * wx + nx;

		int beginTri = chunks[neighbourId].beginningTri;
		int endTri = chunks[neighbourId].endTri;
		int triCount = endTri - beginTri;

		if (chunks[neighbourId].voxelCount != 0) {
			memset((uint8_t*)shaders.display.ssboPointers[2] + (size_t)beginTri * sizeof(uint32_t),
				0, (size_t)triCount * sizeof(uint32_t));

			memset((uint8_t*)shaders.display.ssboPointers[6] + (size_t)beginTri * sizeof(uint32_t),
				0, (size_t)triCount * sizeof(uint32_t));

			int beginMeshlet = beginTri / 64;
			int endMeshlet = (endTri + 63) / 64;
			int meshletCount = endMeshlet - beginMeshlet;

			if (meshletCount > 0) {
				memset((uint8_t*)shaders.display.ssboPointers[5] + (size_t)beginMeshlet * sizeof(int),
					0, (size_t)meshletCount * sizeof(int));

				memset((uint8_t*)shaders.display.ssboPointers[7] + (size_t)beginMeshlet * 6 * sizeof(float),
					0, (size_t)meshletCount * 6 * sizeof(float));
			}
		}
	}
	inline void DeleteColumn(int nx, int nz) {
		world.array[nx][nz] = false;

		for (int ny = 0; ny < wy; ny++) {
			int neighbourId = ny * (wx * wz) + nz * wx + nx;
			ChunkDeleteMeshData(nx, ny, nz);
			chunks[neighbourId].ClearChunkData(&world.chunkData[neighbourId]);
			chunks[neighbourId].grassList.clear();
			
			uploadChunkSSBO(neighbourId);
		}
	}
	inline void ChunkColumnRegenerate(int nx, int nz, bool clearChunkData = true, bool uploadData=true) {
		drd = 0;
		

		int lod = 1;
		float dist = Vector2Distance({ (float)nx * chunkSize + chunkSize / 2,(float)nz * chunkSize + chunkSize / 2 }, { float(viewPort.camera.position.x),float(viewPort.camera.position.z) });
		bool grass = false;

		if (dist < 1920) grass = true;
		if (dist > 960) lod = 2;
		if (dist > 960 * 2) lod = 4;
		if (dist > 960 * 4) lod = 8;
		if (dist > 960 * 8) lod = 16;
		if (dist > 960 * 12) lod = 32;
		if (world.array[nx][nz]) {
			world.setLod[nx][nz] = lod;
			for (int ny = 0; ny < wy; ny++) {
				int neighbourId = ny * (wx * wz) + nz * wx + nx;
				ChunkDeleteMeshData(nx, ny, nz);
				softwareRaster[drd] = { nx,ny,nz };
				drd++;
				if (clearChunkData) {
					chunks[neighbourId].ClearChunkData(&world.chunkData[neighbourId]);
					chunks[neighbourId].grassList.clear();
				}
				if (uploadData) uploadChunkSSBO(neighbourId);
			}
		}

		if (drd > 0) {
			shaders.display.UpdateSSBO(10, softwareRaster, drd * sizeof(Math::IVector3), 0);

			auto batchUploadEnd = std::chrono::high_resolution_clock::now();
			Anubis::GpuTimer mesh;
			mesh.Start();
			Anubis::SwitchShaderStage(shaders.generateMesh.programID);
			shaders.generateMesh.SetInt(wx, "wx");
			shaders.generateMesh.SetInt(wy, "wy");
			shaders.generateMesh.SetInt(wz, "wz");
			shaders.generateMesh.SetFloat(timeSpeed, "timeSpeed");
			shaders.generateMesh.SetInt(resolution.width, "renderWidth");
			shaders.generateMesh.SetInt(resolution.height, "renderHeight");
			shaders.generateMesh.Use(drd, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

			constexpr GLintptr N = 1024 * 1024;                       // array length in the shader
			const GLintptr beginOffset = sizeof(int);                  // after meshletCounter
			const GLintptr endOffset = sizeof(int) + N * sizeof(int);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, shaders.display.ssboIds[9]);

			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(int), &totalMeshletCount);

			std::vector<int> begin(drd), end(drd);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, beginOffset,
				drd * sizeof(int), begin.data());
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, endOffset,
				drd * sizeof(int), end.data());

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

			totalTriangleCount = totalMeshletCount * 64 + 64;

			for (int i = 0; i < drd; i++) {


				int id = softwareRaster[i].y * (wx * wz) + softwareRaster[i].z * wx + softwareRaster[i].x;
				chunks[id].beginningTri = begin[i];
				chunks[id].endTri = end[i];
			}
		}
	}
	inline void DeleteWorld() {

		for (int nx = 0; nx < wx; nx++) {
			for (int nz = 0; nz < wz; nz++) {

				world.array[nx][nz] = false;

				for (int ny = 0; ny < wy; ny++) {
					int neighbourId = ny * (wx * wz) + nz * wx + nx;
					chunks[neighbourId].ClearChunkData(&world.chunkData[neighbourId]);
					chunks[neighbourId].grassList.clear();
					uploadChunkSSBO(neighbourId);
				}
			}
		}
		chunks = (Chunk*)MemAlloc(wx * wz * wy * sizeof(Chunk));
		Anubis::SwitchShaderStage(shaders.display.shaderId);
		memset(shaders.display.ssboPointers[9], 0, (1024 * 1024 * 2 + 1) * sizeof(int));
		memset(shaders.display.ssboPointers[5], 0, (totalMeshletCount + 1) * sizeof(int) * 8);
		memset(shaders.display.ssboPointers[2], 0, totalTriangleCount * sizeof(uint32_t) * 8);
		memset(shaders.display.ssboPointers[6], 0, totalTriangleCount * sizeof(uint32_t) * 8);
		memset(shaders.display.ssboPointers[7], 0, totalMeshletCount * 6 * sizeof(float) * 8);
		shaders.display.UpdateSSBO(20, 0, maxGrassCount * 3 * sizeof(float), 0);
		grass = 0;
	}
	inline void RecalculateParticles(int x, int y, int z) {
		Anubis::SwitchShaderStage(shaders.display.shaderId);
		int total = 0;
		int tt = chunks[y * (wx * wz) + z * wx + x].grassList.size();
		if (tt <= 0) return;
		particles = (float*)MemAlloc(tt * 2 * 3 * sizeof(float));
		
		for (auto t : chunks[y * (wx * wz) + z * wx + x].grassList) {
			for (int i = 0; i < GetRandomValue(1, 2); i++) {
				particles[total * 3] = (t.x + float(GetRandomValue(-10, 10)) / 10.0f);
				particles[total * 3 + 1] = (t.y + 1);
				particles[total * 3 + 2] = (t.z + float(GetRandomValue(-10, 10)) / 10.0f);
				grass++;
				total++;
			}
		}
	
		if (grass > maxGrassCount) grass = maxGrassCount;
		if (grass > 0) {
			shaders.display.UpdateSSBO(20, particles, tt * 3 * sizeof(float), alreadySpawnedGrass*3*sizeof(float));
			alreadySpawnedGrass += tt;
		}
		free(particles); 
	}
	inline bool CustomToggle(const char* label, bool* value) {
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 size = ImVec2(32, 32);
		bool clicked = ImGui::InvisibleButton(label, size);
		if (clicked) *value = !*value;

		ImU32 color = *value ? IM_COL32(255, 255, 255, 255) : IM_COL32(50, 50, 50, 255);
		ImGui::GetWindowDrawList()->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), color, 4.0f);

		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s", label);
		return clicked;
	}
	inline uint32_t packRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		return (static_cast<uint32_t>(r)) |
			(static_cast<uint32_t>(g) << 8) |
			(static_cast<uint32_t>(b) << 16) |
			(static_cast<uint32_t>(a) << 24);
	}
	void MainMenu() {
		
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 8.0f;
		style.FrameRounding = 6.0f;
		style.GrabRounding = 6.0f;
		style.ItemSpacing = ImVec2(10, 10);
		style.WindowPadding = ImVec2(16, 16);
		Math::Vector2 oldMouse;
		auto DrawBackground = [&](ImDrawList* dl, ImVec2 size) {
			ImU32 top = IM_COL32(20, 24, 34, 255);
			ImU32 bottom = IM_COL32(8, 10, 16, 255);
			dl->AddRectFilledMultiColor(
				ImVec2(0, 0), size,
				top, top, bottom, bottom
			);
			ImVec2 c = ImVec2(size.x * 0.5f, size.y * 0.55f);
			for (int i = 12; i >= 0; --i) {
				float r = size.y * (0.25f + i * 0.05f);
				ImU32 col = IM_COL32(60, 90, 140, (int)(4 * i));
				dl->AddCircle(c, r, col, 96, 2.0f);
			}
			};
		int menuResolutionIndex = 0;
		int worldTypeIndex = 0; 
		int worldSize = 0;
		bool running = true;
		bool pregenerate = true;
		bool spawnCritters = false;
		while (running && window.IsOpen()) {
			window.EnableCursor();
			oldMouse = window.GetMousePos();

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			ImGuiIO& io = ImGui::GetIO();
			ImVec2 screen = ImGui::GetIO().DisplaySize;
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(screen);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
			ImGui::Begin("##bg", nullptr,
				ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
				ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
				ImGuiWindowFlags_NoBringToFrontOnFocus);
			DrawBackground(ImGui::GetWindowDrawList(), screen);
			ImGui::End();
			ImGui::PopStyleVar();

			const float menuW = 420.0f;
			ImGui::SetNextWindowPos(
				ImVec2(screen.x * 0.5f - menuW * 0.5f, screen.y * 0.20f),
				ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImVec2(menuW, 0), ImGuiCond_Always);
			ImGui::Begin("Voxel Engine", nullptr,
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
				ImGuiWindowFlags_NoCollapse);

			ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "Voxel Engine");
			ImGui::Separator();

			ImGui::Text("World Size");
			const char* worldSizes[] = { "10x10", "50x50", "100x100", "200x200", "300x300" };
			for (int i = 0; i < 4; ++i) {
				bool selected = (worldSize == i);
				char label[128];
				snprintf(label, sizeof(label), "%s",
					worldSizes[i]);

				if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
				if (ImGui::Button(label, ImVec2(-1, 0))) {
					worldSize = i;
				}
				if (ImGui::IsItemHovered() && pregenerate) {
					if (i > 2) {
						ImGui::SetTooltip("Very large worlds aren't recommended to be pre-generated");

					}
				}
				if (selected) ImGui::PopStyleColor();
			}


			ImGui::Spacing();
			ImGui::Text("Resolution");
			const char* resNames[] = { "Low", "Medium", "High", "Ultra", "Native" };
			for (int i = 0; i < 5; ++i) {
				bool selected = (menuResolutionIndex == i);
				char label[128];
				snprintf(label, sizeof(label), "%s  (%dx%d)",
					resNames[i],
					graphicsSettings.resolutions[i].width,
					graphicsSettings.resolutions[i].height);

				if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
				if (ImGui::Button(label, ImVec2(-1, 0))) {
					menuResolutionIndex = i;
				}

				if (selected) ImGui::PopStyleColor();
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			if (ImGui::CollapsingHeader("Advanced")) {
				ImGui::Indent();

				ImGui::Checkbox("Pregenerate World", &pregenerate);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Generate all chunks during loading before the game starts.\n");
				}
				ImGui::Checkbox("Spawn Critters", &spawnCritters);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("Currently it's just debug flies.\n");
				}
				ImGui::Checkbox("Rasterize/Raytrace", &viewPort.raytracePrimaryRays);
				ImGui::Unindent();
				ImGui::Spacing();
			}


			ImGui::Spacing();
			ImGui::Text("World Type");
			const char* typeNames[] = { "Mountains", "Savanna", "Desert"};
			for (int i = 0; i < 3; ++i) {
				bool selected = (worldTypeIndex == i);
				
				if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.55f, 0.9f, 1.0f));
				if (ImGui::Button(typeNames[i], ImVec2(-1, 0))) {
					worldTypeIndex = i;
					switch (i) {
						case 0:
							selectedType = MOUNTAIN;
							break;
						case 1:
							selectedType = SAVANNA;
							break;
						case 2:
							selectedType = DESERT;
							break;
					}

				}
				if (selected) ImGui::PopStyleColor();
			}

			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.65f, 0.25f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.80f, 0.30f, 1.0f));
			if (ImGui::Button("Play", ImVec2(-1, 40))) {
				running = false;
				switch (worldSize) {
				case 0:
					wx = 10;
					wz = 10;
					break;
				case 1:
					wx = 50;
					wz = 50;
					break;
				case 2:
					wx = 100;
					wz = 100;
					break;
				case 3:
					wx = 200;
					wz = 200;
					break;
				case 4:
					wx = 300;
					wz = 300;
					break;
				}

				particleRenderer.Create();
				if (spawnCritters) {
					for (int i = 0; i < 10000; i++) {
						particleRenderer.PushRenderParticle({ (float)GetRandomValue(wx * 16 + 32, wx * 16 - 32),300,(float)GetRandomValue(wx * 16 + 32, wx * 16 - 32) }, { 255,255,255,255 }, ExampleUpdate);
						particleRenderer.particles[i].targetPosition.x = wx * 16;
						particleRenderer.particles[i].targetPosition.z = wz * 16;
						particleRenderer.particles[i].targetPosition.y = 300;

					}

				}

				currentResolutionWindow = menuResolutionIndex;
				resolution.width = graphicsSettings.resolutions[currentResolutionWindow].width;
				resolution.height = graphicsSettings.resolutions[currentResolutionWindow].height;
				window.Resize(resolution.width,resolution.height);
				world.Setup();
				viewPort.camera.position = { (double)wx * chunkSize/2,200,(double)wz * chunkSize / 2 };
				if (pregenerate) {
					Run(true);
				}
				else {
					Run();
				}
				
			}

			ImGui::PopStyleColor(2);

			if (ImGui::Button("Quit", ImVec2(-1, 30))) {
				running = false;
			}

			ImGui::End();

			window.Clear();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			window.UpdateBuffer();
		}
	}
	void RecalculateParticlesFullDebug() {

		grass = 0;
		int total = 0;
		int tt = 0;
		for (int x = 0; x < wx; x++) {
			for (int z = 0; z < wz; z++) {
				for (int y = 0; y < wy; y++) {

					tt += chunks[y * (wx * wz) + z * wx + x].grassList.size();
				}
			}
		}
		particles = (float*)MemAlloc(tt * 2 * 3 * sizeof(float));

		for (int x = 0; x < wx; x++) {
			for (int z = 0; z < wz; z++) {
				for (int y = 0; y < wy; y++) {

					for (auto t : chunks[y * (wx * wz) + z * wx + x].grassList) {
						for (int i = 0; i < GetRandomValue(1, 2); i++) {
							particles[total * 3] = (t.x + float(GetRandomValue(-10, 10)) / 10.0f);
							particles[total * 3 + 1] = (t.y + 1);
							particles[total * 3 + 2] = (t.z + float(GetRandomValue(-10, 10)) / 10.0f);
							grass++;
							total++;
						}


					}
					chunks[y * (wx * wz) + z * wx + x].grassList.clear();
				}
			}
		}
		if (grass > 0) {
			std::cout << "grass: " << grass << "\n";
			shaders.display.UpdateSSBO(20, particles, grass * 3 * sizeof(float), 0);
		}
		free(particles);
	}
	void PreGenerateFullDebug(int setLod=0) {
		START_TIMER(data_gen)
		for (int nx = 0; nx < wx; nx++) {
			for (int nz = 0; nz < wz; nz++) {
				world.array[nx][nz] = true;
			}
		}
#pragma omp for collapse(2)
		for (int nx = 0; nx < wx; nx++) {
			for (int nz = 0; nz < wz; nz++) {
				float dist = Vector2Distance({ (float)nx * chunkSize + chunkSize / 2,(float)nz * chunkSize + chunkSize / 2 }, { float(viewPort.camera.position.x),float(viewPort.camera.position.z) });
				bool grass = false;

				int lod = 1;
				if (dist > 960) lod = 2;
				if (dist > 960 * 2) lod = 4;
				if (dist > 960 * 4) lod = 8;
				if (dist > 960 * 8) lod = 16;
				if (dist > 960 * 12) lod = 32;
				if (lod == 1) grass = true;

				GenerationData gen;
				gen.GenerateMountains(nx, nz);
				gen.highestChunkY += 3;
				for (int y = 0; y < wy; y++) {
					world.chunkData[y * (wx * wz) + nz * wx + nx].lod = 0;
				}
				for (int y = gen.lowestChunkY; y < gen.highestChunkY && y < wy; y++) {
					chunks[y * (wx * wz) + nz * wx + nx].GeneratePerType(selectedType,&world.chunkData[y * (wx * wz) + nz * wx + nx], gen, nx, y, nz, lod, grass);
				};
	//			for (int y = 0; y < gen.lowestChunkY; y++) {
		//			chunks[y * (wx * wz) + nz * wx + nx].GeneratePerType(CAVES, &world.chunkData[y * (wx * wz) + nz * wx + nx], gen, nx, y, nz, lod, false);
			//	}
				int base = 0;
				int cy = gen.highestChunkY - 2;
				#define SET_VOXEL(x, y, z, block) do { \
					int _x = (x), _y = (y), _z = (z); \
					if (_x >= 0 && _x < 32 && _z >= 0 && _z < 32 && _y >= 0 && _y < wy * 32) { \
						ChunkData& _c = world.chunkData[(_y / 32) * (wx * wz) + nz * wx + nx]; \
						if (!_c.skipped && _c.terrain) _c.terrain[IDX(_x, _y % 32, _z, 32)] = (block); \
					} \
				} while (0)

				auto spawnTree = [&](int dx, int dy, int dz) {
					
					if (lod != 1 || !spawnAnimatedObjects) {
						StructureDataHolder* tree = structureManager.structureList[0].GetRandomVariant();
						uint8_t* positions = tree->positions;
						uint16_t* values = tree->values;
						for (int i = 0; i < tree->voxelCount; i++) {

							int x = positions[i * 3 + 0];
							int y = positions[i * 3 + 1];
							int z = positions[i * 3 + 2];
							uint16_t type = values[i];
							SET_VOXEL(dx + x, dy + y, dz + z, type);
						}
					}
					else {
						StructureDataHolder* tree = structureManager.structureList[0].GetSpecificVariant(2);
						uint8_t* positions = tree->positions;
						uint16_t* values = tree->values;
						for (int i = 0; i < tree->voxelCount; i++) {

							int x = positions[i * 3 + 0];
							int y = positions[i * 3 + 1];
							int z = positions[i * 3 + 2];
							uint16_t type = values[i];
							SET_VOXEL(dx + x, dy + y, dz + z, 255);
						}
						AnimatedObject ao({ (float)dx + nx * 32,(float)dy + 32,(float)dz + nz * 32 }, treeAnimationUpdate, 0);
						world.animatedObjects.push_back(ao);
					}
				};
				if (selectedType == MOUNTAIN) {
					for (int i = 0; i < Math::GetRandomValueStreamlinedFast(0, 4); i++) {
						int dx = Math::GetRandomValueStreamlinedFast(10, 20);
						int dz = Math::GetRandomValueStreamlinedFast(10, 20);
						int baseY = gen.heightMap[dx + dz * chunkSize];
						if (baseY > 250) continue;
						
						spawnTree(dx, baseY, dz);

					}

				}
				for (int y = 0; y < gen.lowestChunkY; y++) {
					world.handle.handle[y * (wx * wz) + nz * wx + nx].lod = 32;
					world.handle.handle[y * (wx * wz) + nz * wx + nx].filled = 1;
				}
				for (int y = gen.lowestChunkY; y < gen.highestChunkY && y < wy; y++) {
					if (!world.chunkData[y * (wx * wz) + nz * wx + nx].skipped) {
						chunks[y * (wx * wz) + nz * wx + nx].GenerateLod(&world.chunkData[y * (wx * wz) + nz * wx + nx], nx, y, nz);
						chunks[y * (wx * wz) + nz * wx + nx].Compress(&world.chunkData[y * (wx * wz) + nz * wx + nx]);
					}
					
				};

				world.setLod[nx][nz] = lod;
				free(gen.heightMap);
			}
		}
		END_TIMER(data_gen)
		START_TIMER(upload)
		int maxCount = wx * wy * wz;

		Anubis::GPUONLYByteMap** byteMapBatch = new Anubis::GPUONLYByteMap * [maxCount];
		uint8_t** byteMapData = new uint8_t * [maxCount];
		Math::IVector3* byteMapSizes = new Math::IVector3[maxCount];
		int* byteMapChunkIds = new int[maxCount]; 
		int byteMapCount = 0;

		Anubis::GPUONLYShortMap** shortMapBatch = new Anubis::GPUONLYShortMap * [maxCount];
		uint16_t** shortMapData = new uint16_t * [maxCount];
		Math::IVector3* shortMapSizes = new Math::IVector3[maxCount];
		int* shortMapChunkIds = new int[maxCount]; 
		int shortMapCount = 0;

		for (int nx = 0; nx < wx; nx++) {
			for (int nz = 0; nz < wz; nz++) {
				for (int ny = 0; ny < wy; ny++) {

					int neighbourId = ny * (wx * wz) + nz * wx + nx;
					if (chunks[neighbourId].lod == 0 || chunks[neighbourId].lod == 3000 || chunks[neighbourId].lod == 32) continue;

					int size = chunkSize / chunks[neighbourId].lod;

					if (world.chunkData[neighbourId].voxelCount != 0 &&
						!world.chunkData[neighbourId].skipped &&
						!(world.chunkData[neighbourId].voxelCount >= chunkSize * chunkSize * chunkSize)) {

						world.chunkData[neighbourId].probeMap.Clear({ size,size,size });
						world.chunkData[neighbourId].probeMap.Upload(0);
						world.chunkData[neighbourId].probeMap.GetBindlessHandle();
						world.handle.handle[neighbourId].lightStorageHandle = world.chunkData[neighbourId].probeMap.handle;

						if (chunks[neighbourId].shortenedData) {
							auto& map = world.chunkData[neighbourId].fluidByteMap;
							map.Clear({ size, size, size });

							byteMapBatch[byteMapCount] = &map;
							byteMapData[byteMapCount] = world.chunkData[neighbourId].compressedTerrain;
							byteMapSizes[byteMapCount] = { size, size, size };
							byteMapChunkIds[byteMapCount] = neighbourId;
							byteMapCount++;
						}
						else {
							auto& map = world.chunkData[neighbourId].fluidShortMap;
							map.Clear({ size, size, size });

							shortMapBatch[shortMapCount] = &map;
							shortMapData[shortMapCount] = chunks[neighbourId].DecompressChunk(&world.chunkData[neighbourId]);
							shortMapSizes[shortMapCount] = { size, size, size };
							shortMapChunkIds[shortMapCount] = neighbourId; 
							shortMapCount++;
						}
					}
				}
			}
		}

		if (byteMapCount > 0) {
			UploadByteMaps(byteMapBatch, byteMapData, byteMapSizes, byteMapCount);

			for (int i = 0; i < byteMapCount; i++) {
				byteMapBatch[i]->GetBindlessHandle();

				int id = byteMapChunkIds[i];
				world.handle.handle[id].handle = byteMapBatch[i]->handle;
				world.handle.handle[id].lod = world.chunkData[id].lod;
			}
		}

		if (shortMapCount > 0) {
			UploadShortMaps(shortMapBatch, shortMapData, shortMapSizes, shortMapCount);

			for (int i = 0; i < shortMapCount; i++) {
				shortMapBatch[i]->GetBindlessHandle();

				int id = shortMapChunkIds[i];
				world.handle.handle[id].handle = shortMapBatch[i]->handle;
				world.handle.handle[id].lod = world.chunkData[id].lod;

				free(shortMapData[i]);
			}
		}

		delete[] byteMapChunkIds;
		delete[] shortMapChunkIds;
		delete[] byteMapBatch;
		delete[] shortMapBatch;
		delete[] shortMapSizes;
		delete[] byteMapSizes;
		END_TIMER(upload)
		shaders.display.UpdateSSBO(3, &world.handle, sizeof(world.handle), 0);
		RecalculateParticlesFullDebug();
		PreCalculateMesh();
	}
	void CallGPUMesh() {

		if (drd > 0 && !viewPort.raytracePrimaryRays) {
			shaders.display.UpdateSSBO(10, softwareRaster, drd * sizeof(Math::IVector3), 0);

			auto batchUploadEnd = std::chrono::high_resolution_clock::now();
			Anubis::GpuTimer mesh;
			mesh.Start();
			Anubis::SwitchShaderStage(shaders.generateMesh.programID);
			shaders.generateMesh.SetInt(wx, "wx");
			shaders.generateMesh.SetInt(wy, "wy");
			shaders.generateMesh.SetInt(wz, "wz");
			shaders.generateMesh.SetFloat(timeSpeed, "timeSpeed");
			shaders.generateMesh.SetInt(resolution.width, "renderWidth");
			shaders.generateMesh.SetInt(resolution.height, "renderHeight");
			shaders.generateMesh.Use(drd, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);

			constexpr GLintptr N = 1024 * 1024;
			const GLintptr endOffset = sizeof(int) + N * sizeof(int);

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, shaders.display.ssboIds[9]);

			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, sizeof(int), &totalMeshletCount);

			std::vector<int> begin(drd), end(drd);
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, sizeof(int),
				drd * sizeof(int), begin.data());
			glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, endOffset,
				drd * sizeof(int), end.data());

			glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
			if (totalMeshletCount * 64 > 59259259) {
				std::cout << "Buffer overflow \n";
			}

			totalTriangleCount = totalMeshletCount * 64 + 64;

			for (int i = 0; i < drd; i++) {


				int id = softwareRaster[i].y * (wx * wz) + softwareRaster[i].z * wx + softwareRaster[i].x;
				chunks[id].beginningTri = begin[i];
				chunks[id].endTri = end[i];
			}
		}
	}
	void PreCalculateMesh() {
		drd = 0;
		Math::IVector3 *queue = (Math::IVector3*)MemAlloc(wx * wz * wy*sizeof(Math::IVector3));
		int queueCounter = 0;
			for (int x = 0; x < wx; x++) {
				for (int z = 0; z < wz; z++) {
					if (world.array[x][z]) {
						for (int ny = 0; ny < wy; ny++) {
							int id = ny * (wx * wz) + z * wx + x;
							if (!world.chunkData[id].updated && world.chunkData[id].voxelCount != 0) {
								if (world.chunkData[id].voxelCount != 0 && !world.chunkData[id].skipped && !chunks[id].hardwareRasterize) {
									queue[queueCounter] = { chunks[id].gx,chunks[id].gy,chunks[id].gz };
									queueCounter++;
								}
							}
						}
					}

				}
			}
		drd = 0;
		for (int i = 0; i < queueCounter; i++) {
			softwareRaster[drd] = queue[drd];
			drd++;
			if (drd == 1024 * 1024-1) {
				CallGPUMesh();
				drd = 0;
			}
		}
		if (drd > 0) {
			CallGPUMesh();
			drd = 0;
		}
		free(queue);
	}
	void Run(bool pregenerate=false) {
		const std::vector<Color> palette = {
					{0, 255, 0, 255},       // 0: GREEN GRASS
					{128, 128, 128, 255},   // 1: GRAY
					{153, 76, 0, 255},      // 2: BROWN
					{92, 64, 51, 255},      // 3: DARKBROWN
					{0, 117, 44, 255},      // 4: DARKGREEN
					{255, 255, 255, 255},   // 5: WHITE
					{0, 0, 128, 255},       // 6: DARKBLUE
					{85, 158, 47, 255},     // 7: CUSTOM GREEN
					{238, 214, 175, 255},   // 8: DESERT SAND
					{ 97 ,112 ,44 ,255 },   // 9: OLIVE
					{0, 121, 36, 255},      // 10: Darker Grass Shade
					{0, 128, 49, 255},      // 11: Mid-Light Grass Variant
					{2, 137, 53, 255},      // 12: Brightest Grass Highlight
					{0, 110, 41, 255},      // 13: Medium Grass Blend
					{0, 106, 39, 255},       // 14: Deepest Grass Shadow
					{54, 34, 15, 255},      // 15
					{92, 64, 51, 255},      // 16
					{153, 77, 0, 255},      // 17
					{238, 214, 175, 255},    // 18
					{ 59,  87,  44, 255 },  // 10: Leaf Highlight (Replaced the neon yellow with a bright, natural leaf green)
					{ 48,  71,  36, 255 },  // 11: Main Canopy Leaf Green (Replaced Warm Sand)
					{ 40,  61,  30, 255 },  // 12: Medium Canopy Leaf Green (Replaced Light Camel)
					{ 33,  51,  25, 255 },  // 13: Dark/Shadow Leaf Green (Replaced Peach/Tan)
					{ 83,  62,  46, 255 },  // 14: Outer Branch Brown (Replaced Bright Olive)
					{ 69,  50,  36, 255 },  // 15: Inner Heavy Branch Brown (Replaced Intense Lemon)
					{ 56,  40,  29, 255 },  // 16: Deep Core Wood/Shadow Brown (Replaced Pale Yellow)
					{ 53,  76,  40, 255 },  // 17: Alternative Leaf Green (Replaced Pear Green)
					{ 45,  66,  33, 255 },   // 18: Alternative Leaf Green (Replaced Apple Green)
					{255,255,255,255},
					{0,0,0,255}
		};
		std::vector<uint32_t> ColorPallete;
		for (auto t : palette) {
			ColorPallete.push_back(packRGBA8(t.r, t.g, t.b, t.a));
		}
		int offfr = 0;
		int selectedColorIndex = 0; // Keep track of selection
		Math::Vector2 oldMouse = window.GetMousePos();
		std::cout << "Begun terrain generation \n";
		GenerateTerrain();
		const ImVec4 clearColor = { 0.1f, 0.1f, 0.12f, 1.0f };
		bool menuOpen = false;
		bool releasedMenu = false;
		int frame = 0;
		bool preallocated = false;
		bool generatedMesh = false;
		Anubis::SwitchShaderStage(shaders.display.shaderId);
		shaders.display.CreateSSBO(3, sizeof(world.handle));
		shaders.display.UpdateSSBO(9, 0, sizeof(int), 0);
		Math::IVector2 oldChunkPos = { viewPort.camera.position.x / chunkSize,viewPort.camera.position.z / chunkSize };
		if (pregenerate) PreGenerateFullDebug();
		bool releasedMouseButton = false;
		Math::IVector3 chunksUpdatedThisFrame[100];
		int chunkCount = 0;
		
		while (window.IsOpen()) {
			double frameStart = glfwGetTime();
			int zero = 0;
			int* p_triangleCounter = (int*)shaders.display.ssboPointers[5];
			*p_triangleCounter = 0;
			if (!menuOpen) {
				viewPort.camera.UpdateFREE(window.GetMousePos(), oldMouse, window);
			}
			Vector3 tag = { (float)viewPort.camera.target.x ,(float)viewPort.camera.target.y,(float)viewPort.camera.target.z };
			Vector3 pos = { (float)viewPort.camera.position.x ,(float)viewPort.camera.position.y,(float)viewPort.camera.position.z };

			Vector3 forward = Vector3Normalize(Vector3Subtract(tag, pos));
			window.Clear();
			oldMouse = window.GetMousePos();
			Matrix matView = MatrixLookAt(pos, tag, { 0,1,0 });

			Anubis::SwitchShaderStage(shaders.hizBuilder.programID);
			shaders.hizBuilder.SetInt(frame-1, "frameIndex");
			shaders.hizBuilder.SetInt(1, "buildLevel");
			shaders.hizBuilder.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shaders.hizBuilder.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shaders.hizBuilder.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shaders.hizBuilder.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");

			shaders.hizBuilder.Use(resolution.width / 32, 1, resolution.height / 32);
			int renderedChunks = 0;
			Anubis::GpuTimer frustum;
			Anubis::GpuTimer timerFrustum;
			Anubis::GpuTimer secondaryRays;
			timerFrustum.Start();

			frustum.Start();
			if (totalMeshletCount > 0) {
				viewPort.FrustumCull(matView, pos, forward, totalMeshletCount);
			}

			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
			double frustumTimeMs = timerFrustum.Stop(); // This will block until culling is done

			GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
			glDeleteSync(fence);
			int triangleCount = *p_triangleCounter;
			Math::Vector3 pp[2] = { viewPort.camera.target,{wx * 32,150,wy * 32} };
			double rasterTimeMs;
			int trianglesRendered = 0;
			Anubis::GpuTimer raster;
			raster.Start();
				
			viewPort.RasterizeMeshes(matView, pos, forward, ColorPallete, frame, triangleCount, resolution);
			trianglesRendered = triangleCount;
			rasterTimeMs = raster.Stop();

			for (auto &t : world.animatedObjects) { 

				viewPort.RenderAnimatedObject(matView, &t, pos, forward, frame, ColorPallete, resolution, &structureManager);
			}
			if (frame % treeAnimationUpdate==0) {
				std::cout << "Animation frame tick\n";
				for (auto& t : world.animatedObjects) {
					t.Update(frame, &structureManager);
				}

				for (auto& t : world.animatedObjects) {
					if (t.animationFrameUpdated) {
						viewPort.SetAnimatedObjectData(&t, &structureManager);
						t.animationFrameUpdated = false;
					}

				}
			}
			
			Anubis::GpuTimer grassT;
			grassT.Start();
			if (graphicsSettings.grassRender && grass > 0) {
				viewPort.RenderGrass(matView, pos, forward, frame, resolution, grass);
			}

			if (graphicsSettings.secondaryRays) {

				secondaryRays.Start();
				viewPort.SecondaryRays(sampleCount, matView, pos, forward, ColorPallete, frame, timeSpeed, resolution, &world);
			}
			auto secondaryRaysMs = secondaryRays.Stop();
			if (graphicsSettings.renderClouds) {
				viewPort.RenderClouds(matView, frame, pos, forward, resolution, timeSpeed);
			}
			if (frame % treeAnimationUpdate == 0) {

				for (auto& t : world.animatedObjects) {

					viewPort.ClearAnimatedObjectData(&t, &structureManager);
				}
			}
			auto grassTimeMs = grassT.Stop();
			START_TIMER(physicsAllocator)
			
			START_TIMER(chunkAllocator)
			
			Math::IVector2 currentChunkPos= { int(viewPort.camera.position.x / chunkSize),int(viewPort.camera.position.z / chunkSize )};
			if (oldChunkPos.x != currentChunkPos.x || oldChunkPos.y != currentChunkPos.y) {
				int lodchanged = 0;
				
				for (int x = 0; x < wx; x++) {
					for (int z = 0; z < wz; z++) {

						int lod = 1;
						float dist = Vector2Distance({ (float)x * chunkSize + chunkSize / 2,(float)z * chunkSize + chunkSize / 2 }, { float(viewPort.camera.position.x),float(viewPort.camera.position.z) });

						if (dist > 960) lod = 2;
						if (dist > 960 * 2) lod = 4;
						if (dist > 960 * 4) lod = 8;
						if (dist > 960 * 8) lod = 16;
						if (dist > 960 * 12) lod = 32;
						if (world.setLod[x][z] != lod && lodchanged<10) {
							SetLODChunks(x, z, lod);
							for (int y = 0; y < wy; y++) {
								int neighbourId = y * (wx * wz) + z * wx + x;
								if (world.chunkData[neighbourId].voxelCount!=0) uploadChunkSSBO(neighbourId);
								
							}
							lodchanged++;
						}

					}
				}

			}
			oldChunkPos = currentChunkPos;
			drd = 0;
			Vector3 rayCastPos = pos;
			bool hitDeadEnd = false;
			for (int i = 0; i < 100; i++) {
				rayCastPos.x += forward.x * 2;
				rayCastPos.y += forward.y * 2;
				rayCastPos.z += forward.z * 2;
				int x = rayCastPos.x / chunkSize;
				int y = rayCastPos.y / chunkSize;
				int z = rayCastPos.z / chunkSize;

				int id = y * (wx * wz) + z * wx + x;
				if (rayCastPos.x >= 0 && rayCastPos.y >= 0 && rayCastPos.z >= 0 && rayCastPos.x < wx * chunkSize && rayCastPos.y < wy * chunkSize && rayCastPos.z < wz * chunkSize) {
					if (chunks[id].lod != 0) {
						if (chunks[id].ReadVoxel(&world.chunkData[id], (int)rayCastPos.x % (chunkSize / chunks[id].lod), (int)rayCastPos.y % (chunkSize / chunks[id].lod), (int)rayCastPos.z % (chunkSize / chunks[id].lod)) != 0) {
							hitDeadEnd = true;
							break;
						}
					}

				}
				else break;
			}
			if (!hitDeadEnd) {
				rayCastPos = { pos.x + forward.x * 25,pos.y + forward.y * 25,pos.z + forward.z * 25 };
			}

			if (!viewPort.raytracePrimaryRays) {
				
				if (window.GetMouseButtonPressed(0) && !menuOpen && selectedColorIndex >= 0) {

					Vector3 ppp = rayCastPos;

					bool doneChanges = false;
					bool allocatedThisRun = false;
					for (int dx = 0; dx < editSize; dx++) {
						for (int dy = 0; dy < editSize; dy++) {
							for (int dz = 0; dz < editSize; dz++) {

								Vector3 pp = ppp;
								pp.x += dx;
								pp.y += dy;
								pp.z += dz;

								int x = pp.x / chunkSize;
								int y = pp.y / chunkSize;
								int z = pp.z / chunkSize;

								int id = y * (wx * wz) + z * wx + x;

								if (id >= wx * wy * wz || id < 0) continue;
								if (chunks[id].lod == 0 || chunks[id].voxelCount == 0 || world.chunkData[id].skipped) {
									world.chunkData[id].skipped = false;
									chunks[id].gx = x;
									chunks[id].gy = y;
									chunks[id].gz = z;
									chunks[id].lod = 1;
									chunks[id].size = chunkSize / chunks[id].lod;
									world.chunkData[id].lod = 1;
									world.chunkData[id].terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));
									int kx = int(fabs(pp.x)) % chunkSize;
									int ky = int(fabs(pp.y)) % chunkSize;
									int kz = int(fabs(pp.z)) % chunkSize;
									int blockid = kx + ky * chunkSize + kz * chunkSize * chunkSize;
									world.chunkData[id].terrain[blockid] = selectedColorIndex;
									int size = chunkSize / chunks[id].lod;
									chunks[id].Compress(&world.chunkData[id]);
									world.chunkData[id].probeMap.Clear({ size,size,size });
									world.chunkData[id].probeMap.Upload(0);
									world.chunkData[id].probeMap.GetBindlessHandle();
									world.handle.handle[id].lightStorageHandle = world.chunkData[id].probeMap.handle;

									if (chunks[id].shortenedData) {
										world.chunkData[id].fluidByteMap.Clear({ size,size,size });
										world.chunkData[id].fluidByteMap.Upload(world.chunkData[id].compressedTerrain);
										world.chunkData[id].fluidByteMap.GetBindlessHandle();
										world.handle.handle[id].handle = world.chunkData[id].fluidByteMap.handle;
									}
									else {
										world.chunkData[id].fluidShortMap.Clear({ size,size,size });
										uint16_t* tt = chunks[id].DecompressChunk(&world.chunkData[id]);
										world.chunkData[id].fluidShortMap.Upload(tt);
										free(tt);
										world.chunkData[id].fluidShortMap.GetBindlessHandle();
										world.handle.handle[id].handle = world.chunkData[id].fluidShortMap.handle;

									}
									allocatedThisRun = true;
									world.handle.handle[id].lod = world.chunkData[id].lod;
									for (int dx = -1; dx <= 1; dx++) {
										for (int dy = -1; dy <= 1; dy++) {
											for (int dz = -1; dz <= 1; dz++) {

												bool inGen = false;

												for (int d = 0; d < chunkCount; d++) {
													if (chunksUpdatedThisFrame[d].x == x + dx && chunksUpdatedThisFrame[d].y == y + dy && chunksUpdatedThisFrame[d].z == z + dz) {
														inGen = true;
													}
												}
												if (!inGen) {
													chunksUpdatedThisFrame[chunkCount] = { x + dx,y + dy,z + dz };
													chunkCount++;
												}
											}
										}
									}
								}
								if (chunks[id].lod == 1 && world.chunkData[id].lod == 1) {
									if (chunks[id].ReadVoxel(&world.chunkData[id], int(fabs(pp.x)) % chunkSize, int(fabs(pp.y)) % chunkSize, int(fabs(pp.z)) % chunkSize) == 0) {
										chunks[id].SetTerrain(&world.chunkData[id], int(fabs(pp.x)) % chunkSize, int(fabs(pp.y)) % chunkSize, int(fabs(pp.z)) % chunkSize, selectedColorIndex);
										doneChanges = true;
									}
									for (int dx = -1; dx <= 1; dx++) {
										for (int dy = -1; dy <= 1; dy++) {
											for (int dz = -1; dz <= 1; dz++) {
												bool inGen = false;
												for (int d = 0; d < chunkCount; d++) {
													if (chunksUpdatedThisFrame[d].x == x + dx && chunksUpdatedThisFrame[d].y == y + dy && chunksUpdatedThisFrame[d].z == z + dz) {
														inGen = true;
													}
												}
												if (!inGen) {
													chunksUpdatedThisFrame[chunkCount] = { x + dx,y + dy,z + dz };
													chunkCount++;
												}
											}
										}
									}
								}

							}
						}
					}
					releasedMouseButton = true;
				}
				if (!window.GetMouseButtonPressed(0) && releasedMouseButton) {

					if (chunkCount > 0) {
						for (int i = 0; i < chunkCount; i++) {
							int dx = chunksUpdatedThisFrame[i].x;
							int dy = chunksUpdatedThisFrame[i].y;
							int dz = chunksUpdatedThisFrame[i].z;
							int id = dy * (wx * wz) + dz * wx + dx;
							if (chunks[id].lod != 0 && !chunks[id].hardwareRasterize) {
								ChunkDeleteMeshData(dx, dy, dz);
								softwareRaster[drd] = { dx,dy,dz };
								drd++;
								uploadChunkSSBO(id);

							}
						}
						chunkCount = 0;
					}
					releasedMouseButton = false;


				}
			}
			else if (window.GetMouseButtonPressed(0) && selectedColorIndex > 0) {

				Vector3 ppp = rayCastPos;

				bool doneChanges = false;
				bool allocatedThisRun = false;
				for (int dx = 0; dx < editSize; dx++) {
					for (int dy = 0; dy < editSize; dy++) {
						for (int dz = 0; dz < editSize; dz++) {

							Vector3 pp = ppp;
							pp.x += dx;
							pp.y += dy;
							pp.z += dz;

							int x = pp.x / chunkSize;
							int y = pp.y / chunkSize;
							int z = pp.z / chunkSize;

							int id = y * (wx * wz) + z * wx + x;

							if (id >= wx * wy * wz || id < 0) continue;
							if (chunks[id].lod == 0 || chunks[id].voxelCount == 0 || world.chunkData[id].skipped) {
								world.chunkData[id].skipped = false;
								chunks[id].gx = x;
								chunks[id].gy = y;
								chunks[id].gz = z;
								chunks[id].lod = 1;
								chunks[id].size = chunkSize / chunks[id].lod;
								world.chunkData[id].lod = 1;
								world.chunkData[id].terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));
								int kx = int(fabs(pp.x)) % chunkSize;
								int ky = int(fabs(pp.y)) % chunkSize;
								int kz = int(fabs(pp.z)) % chunkSize;
								int blockid = kx + ky * chunkSize + kz * chunkSize * chunkSize;
								world.chunkData[id].terrain[blockid] = selectedColorIndex;
								int size = chunkSize / chunks[id].lod;
								chunks[id].Compress(&world.chunkData[id]);

								world.chunkData[id].probeMap.Clear({ size,size,size });
								world.chunkData[id].probeMap.Upload(0);
								world.chunkData[id].probeMap.GetBindlessHandle();
								world.handle.handle[id].lightStorageHandle = world.chunkData[id].probeMap.handle;
								if (chunks[id].shortenedData) {
									world.chunkData[id].fluidByteMap.Clear({ size,size,size });
									world.chunkData[id].fluidByteMap.Upload(world.chunkData[id].compressedTerrain);
									world.chunkData[id].fluidByteMap.GetBindlessHandle();
									world.handle.handle[id].handle = world.chunkData[id].fluidByteMap.handle;
								}
								else {
									world.chunkData[id].fluidShortMap.Clear({ size,size,size });
									uint16_t* tt = chunks[id].DecompressChunk(&world.chunkData[id]);
									world.chunkData[id].fluidShortMap.Upload(tt);
									free(tt);
									world.chunkData[id].fluidShortMap.GetBindlessHandle();
									world.handle.handle[id].handle = world.chunkData[id].fluidShortMap.handle;

								}
								allocatedThisRun = true;
								world.handle.handle[id].lod = world.chunkData[id].lod;
								for (int dx = -1; dx <= 1; dx++) {
									for (int dy = -1; dy <= 1; dy++) {
										for (int dz = -1; dz <= 1; dz++) {

											bool inGen = false;

											for (int d = 0; d < chunkCount; d++) {
												if (chunksUpdatedThisFrame[d].x == x + dx && chunksUpdatedThisFrame[d].y == y + dy && chunksUpdatedThisFrame[d].z == z + dz) {
													inGen = true;
												}
											}
											if (!inGen) {
												chunksUpdatedThisFrame[chunkCount] = { x + dx,y + dy,z + dz };
												chunkCount++;
											}
										}
									}
								}
							}
							if (chunks[id].lod == 1 && world.chunkData[id].lod == 1) {
								if (chunks[id].ReadVoxel(&world.chunkData[id], int(fabs(pp.x)) % chunkSize, int(fabs(pp.y)) % chunkSize, int(fabs(pp.z)) % chunkSize) == 0) {
									chunks[id].SetTerrain(&world.chunkData[id], int(fabs(pp.x)) % chunkSize, int(fabs(pp.y)) % chunkSize, int(fabs(pp.z)) % chunkSize, selectedColorIndex);
									doneChanges = true;
								}
								for (int dx = -1; dx <= 1; dx++) {
									for (int dy = -1; dy <= 1; dy++) {
										for (int dz = -1; dz <= 1; dz++) {
											bool inGen = false;
											for (int d = 0; d < chunkCount; d++) {
												if (chunksUpdatedThisFrame[d].x == x + dx && chunksUpdatedThisFrame[d].y == y + dy && chunksUpdatedThisFrame[d].z == z + dz) {
													inGen = true;
												}
											}
											if (!inGen) {
												chunksUpdatedThisFrame[chunkCount] = { x + dx,y + dy,z + dz };
												chunkCount++;
											}
										}
									}
								}
							}

						}
					}
				}
				if (chunkCount > 0) {
					for (int i = 0; i < chunkCount; i++) {
						int dx = chunksUpdatedThisFrame[i].x;
						int dy = chunksUpdatedThisFrame[i].y;
						int dz = chunksUpdatedThisFrame[i].z;
						int id = dy * (wx * wz) + dz * wx + dx;
						if (chunks[id].lod != 0) {
							uploadChunkSSBO(id);

						}
					}
					chunkCount = 0;
				}
			}
			Math::IVector2 generateArrayId[1024];
			int generateCount = 0;
			bool leftToGenerate = false;
			for (int x = 0; x < wx; x++) {
				for (int z = 0; z < wz; z++) {

					if (!world.array[x][z]) {
						leftToGenerate = true;
					}
				}
			}
			if (leftToGenerate) {
				for (int i = 0; i < maxMeshUpdate; i++) {
					float lastDistance = renderDistance * chunkSize * 2;
					int chunkX = -1;
					int chunkZ = -1;
					for (int x = 0; x < wx; x++) {
						for (int z = 0; z < wz; z++) {

							if (!world.array[x][z] && generateCount < 10) {
								world.array[x][z] = true;
								generateArrayId[generateCount++] = { x,z };
							}
						}
					}

				}
			}
			#pragma omp parallel for schedule(dynamic)
			for (int i = 0; i < generateCount; i++) {
				int nx = generateArrayId[i].x;
				int nz = generateArrayId[i].y;
				int lod = 1;
				float dist = Vector2Distance({ (float)nx * chunkSize + chunkSize / 2,(float)nz * chunkSize+chunkSize/2 }, { float(viewPort.camera.position.x),float(viewPort.camera.position.z) });
				bool grass = false;

				if (dist < 1920) grass = true;
				if (dist > 960) lod = 2;
				if (dist > 960 * 2) lod = 4;
				if (dist > 960 * 4) lod = 8;
				if (dist > 960 * 8) { lod = 16; }
				if (dist > 960 * 12) { lod = 32; }
				world.setLod[nx][nz] = lod;
				
				GenerationData gen;
				gen.GenerateMountains(nx, nz);
				gen.highestChunkY += 2;

				for (int y = gen.lowestChunkY; y < gen.highestChunkY && y < wy; y++) {
					chunks[y * (wx * wz) + nz * wx + nx].GeneratePerType(selectedType, &world.chunkData[y * (wx * wz) + nz * wx + nx], gen, nx, y, nz, lod, grass);
					
				};
				for (int y = gen.lowestChunkY; y < gen.highestChunkY && y < wy; y++) {
					if (!world.chunkData[y * (wx * wz) + nz * wx + nx].skipped) {
						chunks[y * (wx * wz) + nz * wx + nx].GenerateLod(&world.chunkData[y * (wx * wz) + nz * wx + nx], nx, y, nz);
						chunks[y * (wx * wz) + nz * wx + nx].Compress(&world.chunkData[y * (wx * wz) + nz * wx + nx]);
					}
					else {
						world.handle.handle[y * (wx * wz) + nz * wx + nx].lod = 32;
						world.handle.handle[y * (wx * wz) + nz * wx + nx].filled = 1;
					}
				};
				free(gen.heightMap);
			}
			for (int i = 0; i < generateCount; i++) {

				int nx = generateArrayId[i].x;
				int nz = generateArrayId[i].y;
				for (int y = 0; y < wy; y++) {
					RecalculateParticles(nx, y, nz);
				}
			}
			for (int i = 0; i < generateCount; i++) {
				int nx = generateArrayId[i].x;
				int nz = generateArrayId[i].y;

				for (int ny = 0; ny < wy; ny++) {


					int neighbourId = ny * (wx * wz) + nz * wx + nx;
					if (nx < 0 || ny < 0 || nz < 0 || nx >= wx || ny >= wy || nz >= wz) continue;
					if (chunks[neighbourId].lod == 0) continue;

					if (chunks[neighbourId].voxelCount == 0) {

						world.handle.handle[neighbourId].lod = 0;
						uploadChunkSSBO(neighbourId);
					}
					int size = chunkSize / chunks[neighbourId].lod;
					if (world.chunkData[neighbourId].skipped) {
						world.handle.handle[neighbourId].lod = 32;
						world.handle.handle[neighbourId].filled = 1;
						uploadChunkSSBO(neighbourId);
					}
					if (world.chunkData[neighbourId].voxelCount != 0 && !world.chunkData[neighbourId].skipped)
					{
						world.chunkData[neighbourId].probeMap.Clear({ size,size,size });
						world.chunkData[neighbourId].probeMap.Upload(0);
						world.chunkData[neighbourId].probeMap.GetBindlessHandle();
						world.handle.handle[neighbourId].lightStorageHandle = world.chunkData[neighbourId].probeMap.handle;

						if (chunks[neighbourId].shortenedData) {
							world.chunkData[neighbourId].fluidByteMap.Clear({ size,size,size });
							world.chunkData[neighbourId].fluidByteMap.Upload(world.chunkData[neighbourId].compressedTerrain);
							world.chunkData[neighbourId].fluidByteMap.GetBindlessHandle();
							world.handle.handle[neighbourId].handle = world.chunkData[neighbourId].fluidByteMap.handle;

						}
						else {
							world.chunkData[neighbourId].fluidShortMap.Clear({ size,size,size });
							uint16_t* tt = chunks[neighbourId].DecompressChunk(&world.chunkData[neighbourId]);
							world.chunkData[neighbourId].fluidShortMap.Upload(tt);
							free(tt);
							world.chunkData[neighbourId].fluidShortMap.GetBindlessHandle();
							world.handle.handle[neighbourId].handle = world.chunkData[neighbourId].fluidShortMap.handle;
						}
						world.handle.handle[neighbourId].lod = world.chunkData[neighbourId].lod;
						uploadChunkSSBO(neighbourId);
					}
				}
			}
			bool leftTomesh = false;
			bool keepRunning = true;
			for (int x = 0; x < wx && keepRunning; x++) {
				for (int z = 0; z < wz && keepRunning; z++) {
					for (int ny = 0; ny < wy && keepRunning; ny++) {
						bool neighborsLoaded =
							x > 0 && x < wx - 1 && z > 0 && z < wz - 1 &&
							world.array[x - 1][z] && world.array[x + 1][z] && world.array[x][z - 1] && world.array[x][z + 1];
						int id = ny * (wx * wz) + z * wx + x;

						if (neighborsLoaded && IsBoxInFrustum({ float(x) * 32,0,float(z) * 32 }, { 32,float(wy) * 32,32 }, viewPort.planes)) {
							if (!world.chunkData[id].updated && world.chunkData[id].voxelCount != 0) {
								if (world.chunkData[id].voxelCount != 0 && !world.chunkData[id].skipped && !chunks[id].hardwareRasterize) {
									leftTomesh = true;
									keepRunning = false;
								}
							}
						}
					}
				}
			}
			if (leftTomesh) {

				for (int i = 0; i < maxMeshUpdate; i++) {
					bool keepRunning = true;
					for (int x = 0; x < wx && keepRunning; x++) {
						for (int z = 0; z < wz && keepRunning; z++) {
							if (world.array[x][z]) {
								bool neighborsLoaded =
									x > 0 && x < wx - 1 && z > 0 && z < wz - 1 &&
									world.array[x - 1][z] && world.array[x + 1][z] && world.array[x][z - 1] && world.array[x][z + 1];

								if (neighborsLoaded && IsBoxInFrustum({ float(x) * 32,0,float(z) * 32 }, { 32,float(wy) * 32,32 }, viewPort.planes)) {

									for (int ny = 0; ny < wy && keepRunning; ny++) {
										int id = ny * (wx * wz) + z * wx + x;
										if (!world.chunkData[id].updated && world.chunkData[id].voxelCount != 0) {
											if (world.chunkData[id].voxelCount != 0 && !world.chunkData[id].skipped && !chunks[id].hardwareRasterize) {
												world.chunkData[id].updated = true;
												float dist = Vector2Distance({ (float)x * chunkSize + chunkSize / 2,(float)z * chunkSize + chunkSize / 2 }, { float(viewPort.camera.position.x),float(viewPort.camera.position.z) });
												bool grass = false;


												int lod = 1;
												softwareRaster[drd].x = chunks[id].gx;
												softwareRaster[drd].y = chunks[id].gy;
												softwareRaster[drd].z = chunks[id].gz;
												drd++;
												keepRunning = false;
											}
										}
									}
								}
							}

						}
					}
				}
			}
			if (drd > 0 && !viewPort.raytracePrimaryRays) {
				CallGPUMesh();
			}
			offfr++;
			
			viewPort.Display(frame,resolution.height);
			Anubis::Quad quad;
			quad.Create();
			window.DrawQuad(quad);
			particleRenderer.Update();
			particleRenderer.Render(&shaders, matView, matProj, forward, pos, resolution, frame);
			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();

			if (window.GetKeyPressed(KEY_K) && releasedMenu) {
				menuOpen = !menuOpen;
				releasedMenu = false;
			}
			if (!window.GetKeyPressed(KEY_K)) {
				releasedMenu = true;
			}
			if (menuOpen) {
				window.EnableCursor();
				structureManager.EditorView();

				float padding = 10.0f;

				float resWidth = 250.0f;
				ImGui::SetNextWindowPos(ImVec2(resolution.width - resWidth - padding, padding), ImGuiCond_Always);
				ImGui::SetNextWindowSize(ImVec2(resWidth, 0), ImGuiCond_Always);

				ImGui::Begin("Resolution Settings");
				ImGui::Text("Select Screen Resolution:");
				ImGui::Separator();

				for (int i = 0; i < 5; ++i) {
					std::string resLabel = std::string(graphicsSettings.resolutions[i].name) +
						" (" + std::to_string(graphicsSettings.resolutions[i].width) +
						"x" + std::to_string(graphicsSettings.resolutions[i].height) + ")";
					if (currentResolutionWindow == i) {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.2f, 1.0f));
					}

					if (ImGui::Button(resLabel.c_str(), ImVec2(-1, 0))) {
						currentResolutionWindow = i;
						window.Resize(graphicsSettings.resolutions[currentResolutionWindow].width, graphicsSettings.resolutions[currentResolutionWindow].height);
						resolution.width = graphicsSettings.resolutions[currentResolutionWindow].width;
						resolution.height = graphicsSettings.resolutions[currentResolutionWindow].height;
						matProj = MatrixPerspective(90 * DEG2RAD, ((double)resolution.width / (double)resolution.height), 0.1, 100000.0);
					}

					if (currentResolutionWindow == i) {
						ImGui::PopStyleColor();
					}
				}
				ImGui::End();

				float debugWidth = 250.0f;
				ImGui::SetNextWindowPos(ImVec2(resolution.width - debugWidth - padding, padding + 180.0f), ImGuiCond_Always);
				ImGui::SetNextWindowSize(ImVec2(debugWidth, resolution.height - 180.0f - (padding * 2)), ImGuiCond_Always);

				ImGui::Begin("Debug Menu");

				ImGui::SliderInt("Edit Size", &editSize, 1, 300);
				ImGui::SliderInt("Render Distance", &renderDistance, 1, 1000);
				ImGui::SliderFloat("Render Percentage", &viewPort.renderPercentage, 0.1f, 1.0f);
				ImGui::SliderInt("Max Mesh Gen Per Frame", &maxMeshUpdate, 1, 40);
				CustomToggle("Render grass", &graphicsSettings.grassRender);
				CustomToggle("Hardware/Software rendering", &graphicsSettings.hardwareSoftwareRendering);
				CustomToggle("Secondary rays", &graphicsSettings.secondaryRays);
				int oldSampleCount = sampleCount;
				ImGui::SliderInt("Sample Count", &sampleCount, 1, 9);
				if (oldSampleCount != sampleCount) {
					for (int x = 0; x < wx; x++) {
						for (int y = 0; y < wy; y++) {
							for (int z = 0; z < wz; z++) {
								int neighbourId = y * (wx * wz) + z * wx + x;
								if (chunks[neighbourId].lod == 0 || chunks[neighbourId].lod == 3000 || chunks[neighbourId].lod == 32) continue;

								int size = chunkSize / chunks[neighbourId].lod;

								if (world.chunkData[neighbourId].voxelCount != 0 &&
									!world.chunkData[neighbourId].skipped &&
									!(world.chunkData[neighbourId].voxelCount >= chunkSize * chunkSize * chunkSize)) {

									world.chunkData[neighbourId].probeMap.ClearTexture(0);
								}

							}
						}
					}
				}
				if (ImGui::Button("Recompile shaders")) {
					shaders.Load();
				}
				CustomToggle("God mode", &graphicsSettings.godMode);
				CustomToggle("Simulate Physics", &graphicsSettings.simulatePhysics);
				CustomToggle("Render Fluids", &graphicsSettings.renderFluids);
				CustomToggle("Render Clouds", &graphicsSettings.renderClouds);
				CustomToggle("Raytrace/Rasterize", &viewPort.raytracePrimaryRays);

				CustomToggle("Low Res/High res", &graphicsSettings.renderResolution);
				CustomToggle("FXAA/No FXAA", &graphicsSettings.FXAA);

				ImGui::Separator();
				ImGui::TextColored(ImVec4(1, 1, 0, 1), "Performance Metrics:");
				ImGui::Text("Grass: %.2f ms", grassTimeMs);
				ImGui::Text("Raster: %.2f ms", rasterTimeMs);
				ImGui::Text("Culling: %.2f ms", frustumTimeMs);
				ImGui::Text("Secondary Rays: %.2f ms", secondaryRaysMs);
				ImGui::Text("Total Triangles In Scene: %d", totalTriangleCount);
				ImGui::Text("Rendered Triangles: %d ", trianglesRendered);
				ImGui::Separator();

				ImGui::End();
				float paletteWidth = 200.0f;
				float paletteHeight = 200.0f;
				ImGui::SetNextWindowPos(ImVec2(padding, resolution.height - paletteHeight - padding), ImGuiCond_Always);
				ImGui::SetNextWindowSize(ImVec2(paletteWidth, paletteHeight), ImGuiCond_Always);

				ImGui::Begin("Color Palette");
				if (ImGui::BeginChild("PaletteScroll", ImVec2(0, 100), true)) {
					float buttonSize = 20.0f;
					float windowWidth = ImGui::GetContentRegionAvail().x;
					int buttonsPerRow = (int)(windowWidth / (buttonSize + ImGui::GetStyle().ItemSpacing.x));
					for (int i = 0; i < palette.size(); ++i) {
						ImVec4 color = {
							palette[i].r / 255.0f,
							palette[i].g / 255.0f,
							palette[i].b / 255.0f,
							palette[i].a / 255.0f
						};
						std::string label = "##" + std::to_string(i);
						if (selectedColorIndex == i) {
							ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 1, 1, 1));
						}

						if (ImGui::ColorButton(label.c_str(), color, 0, ImVec2(buttonSize, buttonSize))) {
							selectedColorIndex = i;
						}

						if (selectedColorIndex == i) ImGui::PopStyleColor();
						if ((i + 1) % buttonsPerRow != 0) ImGui::SameLine();
					}
				}
				ImGui::EndChild();

				ImGui::Text("Selected Color Index: %d", selectedColorIndex);
				ImGui::End();
			}
			else {
				window.DisableCursor();
			}
			ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.35f); 
			ImGui::Begin("##fps", nullptr,
				ImGuiWindowFlags_NoDecoration |
				ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoSavedSettings |
				ImGuiWindowFlags_NoFocusOnAppearing |
				ImGuiWindowFlags_NoNav |
				ImGuiWindowFlags_NoInputs);     
			ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
			ImGui::Text("Frame: %.2f ms", 1000.0f / ImGui::GetIO().Framerate);

			ImGui::End();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			window.UpdateBuffer();

			double elapsed = glfwGetTime() - frameStart;
			double remaining = (1.0 / 150.0) - elapsed;
			if (remaining > 0.0) {
				std::this_thread::sleep_for(std::chrono::duration<double>(remaining));
			}
			frame++;
		}
	}
};
int main() {
	
	App* app = new App;
	app->MainMenu();

}