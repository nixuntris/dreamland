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


class App {
	Anubis::Window window;
	Anubis::Camera camera;
	Chunk* chunks;
	ShaderManager shaders;
	World world;
	float* particles;
	bool firstGen = true;
	int totalTriangleCount;
	struct ChunkOffset {
		int vertice;
		int color;
		int meshlet;
		uint16_t x;
		uint16_t y;
		uint16_t z;
	};
	ChunkOffset* off;
	int wWidth = 3840;
	int wHeight = 2160;
	int currentResolutionWindow = 3;
	int grass = 0;
	int totalMeshletCount = 0;
	int editSize = 5;
	Anubis::Texture2DFloat texture;
	Settings graphicsSettings;
	std::vector<std::unique_ptr<PhysicsObject>> physicalObjects;
	std::vector<std::string> worlds = { "Mountains", "Desert", "Caves", "Sky" };
	float timeSpeed = 0.0004;
public:
	App() {
		//IMPLEMENT THE HARDWARE PIPELINE
		//39873 - voxels
		//239238 - quads
		//478476 - triangles
		wWidth = graphicsSettings.resolutions[currentResolutionWindow].width;
		wHeight = graphicsSettings.resolutions[currentResolutionWindow].height;
		window.Init(wWidth,wHeight,4,6,"software rasterization", 100);
		window.DisableCursor();
		omp_set_num_threads(32);
		camera.position = {(double)wx*32,200,(double)wz*32};
		camera.target = { 0,1,0 };
		SetRandomSeed(100);
		shaders.Load();
		shaders.display.CreateSSBO(0, sizeof(uint32_t) * 3840 * 2160 *6);
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();

		ImGui_ImplGlfw_InitForOpenGL(window.GetWindowHandle(), true);
		ImGui_ImplOpenGL3_Init("#version 460");
		glEnable(GL_DEBUG_OUTPUT);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		world.Setup();

	}
	void Update() {
		START_TIMER(triangle_counter);

		totalTriangleCount = 0;
		totalMeshletCount = 0;

		// ---- calculate totals ----
		for (int kk = 0; kk < wx * wy * wz; kk++) {
			if (chunks[kk].voxelCount == 0) continue;
			if (chunks[kk].updated) {

				if (chunks[kk].oldTri > chunks[kk].cpumesh.triangleCount) {
					//override old as empty
					//need new memory
					//look for pockets
					//if no pockets that fit, push to the end
					//nuclear option, if no memory that is already allocated can be used	
					chunks[kk].oldTri = chunks[kk].cpumesh.triangleCount;
				}
				else {
					//std::cout << "existing mem \n";
					//override existing

				}
			}
			chunks[kk].beginningTri = totalTriangleCount;
			chunks[kk].beginningMeshlet = totalMeshletCount;

			int triCount = chunks[kk].cpumesh.triangleCount;
			int meshletCount = (int)ceil((float)triCount / 64.0f);

			totalTriangleCount += triCount;
			totalMeshletCount += meshletCount;
		}

		END_TIMER(triangle_counter);

		if (firstGen) {
			shaders.display.CreatePersistentSSBO(5, (totalMeshletCount + 1) * sizeof(int)*8);
			shaders.display.CreatePersistentSSBO(2, totalTriangleCount * sizeof(uint32_t)*8);
			shaders.display.CreatePersistentSSBO(6, totalTriangleCount * sizeof(uint32_t)*8);
			shaders.display.CreatePersistentSSBO(7, totalMeshletCount * 6 * sizeof(float)*8);
			firstGen = false;
		}

		START_TIMER(stream_to_gpu);

		uint32_t* p_vertex = (uint32_t*)shaders.display.ssboPointers[2];
		uint32_t* p_color = (uint32_t*)shaders.display.ssboPointers[6];
		float* p_aabb = (float*)shaders.display.ssboPointers[7];

		size_t bytesVertex = 0;
		size_t bytesColor = 0;
		size_t bytesAABB = 0;

		bool previously = false;

		for (int kk = 0; kk < wx * wy * wz; kk++) {
//			if (Vector3Distance({ (float)camera.position.x ,(float)camera.position.y,(float)camera.position.z }, { (float)chunks[kk].gx * chunkSize,(float)chunks[kk].gy * chunkSize,(float)chunks[kk].gz * chunkSize }) > 100 * chunkSize) continue;
			if (chunks[kk].voxelCount == 0) continue;
			if (!chunks[kk].updated && !previously) continue;
			
			previously = true;
			int triCount = chunks[kk].cpumesh.triangleCount;
			int meshletCount = (int)ceil((float)triCount / 64.0f);

			size_t vBytes = triCount * sizeof(uint32_t);
			size_t cBytes = triCount * sizeof(uint32_t);
			size_t aBytes = meshletCount * 6 * sizeof(float);

			memcpy(p_vertex + chunks[kk].beginningTri,
				chunks[kk].cpumesh.packedVertex,
				vBytes);

			memcpy(p_color + chunks[kk].beginningTri,
				chunks[kk].cpumesh.gpuBasedColoring,
				cBytes);

			memcpy(p_aabb + chunks[kk].beginningMeshlet * 6,
				chunks[kk].cpumesh.MeshletAABB,
				aBytes);

			bytesVertex += vBytes;
			bytesColor += cBytes;
			bytesAABB += aBytes;

			previously = true;
			chunks[kk].updated = false;
		}

		END_TIMER(stream_to_gpu);

		size_t totalBytes = bytesVertex + bytesColor + bytesAABB;
	}
	void CheckLodsToUpdate() {
		std::vector<Vector3> ids;
		for (int x = 0; x < wx; x++) {
			for (int z = 0; z < wz; z++) {

				int lod = 1;
				float dist = Vector2Distance({ (float)x * chunkSize,(float)z * chunkSize}, { float(camera.position.x),float(camera.position.z) });
				
				if (dist > 640) lod = 2;
				if (dist > 640 * 2) lod = 4;
				if (dist > 640 * 4) lod = 8;
				if (dist > 640 * 8) lod = 16;
				if (dist > 640 * 12) lod = 32;
				if (dist > 640 * 24) lod = 64;
				if (chunks[0 * (wx * wz) + z * wx + x].lod != lod) {
					ids.push_back({ (float)x,0,(float)z });
				}
			}
		}
		for (auto t : ids) {
			int lod = 1;
			float dist = Vector2Distance({ (float)t.x * chunkSize,(float)t.z * chunkSize }, { float(camera.position.x),float(camera.position.z) });
			if (dist > 640) lod = 2;
			if (dist > 640 * 2) lod = 4;
			if (dist > 640 * 4) lod = 8;
			if (dist > 640 * 8) lod = 16;
			if (dist > 640 * 12) lod = 32;
			if (dist > 640 * 24) lod = 64;
			for (int y = 0; y < wy; y++) {
				if (chunks[y * (wx * wz) + (int)t.z * wx + (int)t.x].lod > lod) {
				//	std::cout << "Fuck\n";
				}
				else {
				//	std::cout << "Nah chill, downscale\n";
//					chunks[y * (wx * wz) + (int)t.z * wx + (int)t.x].GenLowerResolution(&world.chunkData[y * (wx * wz) + (int)t.z * wx + (int)t.x]);
	//				chunks[y * (wx * wz) + (int)t.z * wx + (int)t.x].size = chunkSize / lod;
				}
		//		chunks[y * (wx * wz) + (int)t.z * wx + (int)t.x].lod = lod;

			}
		}
		//std::cout << ids.size() << "\n";

	}
	void GenerateTerrain() {

		off = (ChunkOffset*)MemAlloc(wx * wy * wz * sizeof(ChunkOffset));
		chunks = (Chunk*)MemAlloc(wx * wz * wy * sizeof(Chunk));
		START_TIMER(terrain)
#pragma omp parallel for collapse(2)
			for (int x = 0; x < wx; x++) {
				for (int z = 0; z < wz; z++) {
					int lod = 1;
					float dist = Vector2Distance({ (float)x * chunkSize,(float)z * chunkSize }, { float(camera.position.x),float(camera.position.z) });
					bool grass = false;
					
					if (dist < 1920) grass = true;
					if (dist > 960) lod = 2;
					if (dist > 960 * 2) lod = 4;
					if (dist > 960 * 4) lod = 8;
					if (dist > 960 * 8) lod = 16;
					if (dist > 960 * 12) lod = 32;
					if (dist > 960 * 24) lod = 64;
					GenerationData gen;
					gen.GenerateMountains(x, z);
					gen.highestChunkY += 2;
					for (int y = 0; y < gen.highestChunkY && y < wy; y++) {
						chunks[y * (wx * wz) + z * wx + x].GenerateMountains(&world.chunkData[y * (wx * wz) + z * wx + x], gen, x, y, z, lod, grass);
						
					};
					for (int y = 0; y < gen.highestChunkY && y < wy; y++) {
						chunks[y * (wx * wz) + z * wx + x].GenerateLod(&world.chunkData[y * (wx * wz) + z * wx + x], x, y, z);
						chunks[y * (wx * wz) + z * wx + x].Compress(&world.chunkData[y * (wx * wz) + z * wx + x]);
						
					};
					free(gen.heightMap);

				}
			}
		END_TIMER(terrain)

	}
	void MeshWorld() {
		START_TIMER(mesher)
#pragma omp parallel for collapse(2)
			for (int x = 0; x < wx; x++) {
				for (int z = 0; z < wz; z++) {
					for (int y = 0; y < wy; y++) {
						int index = y * (wx * wz) + z * wx + x;
						if (chunks[index].voxelCount == 0) continue;
						if (chunks[index].voxelCount == chunks[index].size * chunks[index].size * chunks[index].size) continue;
						float dist = Vector2Distance({ (float)x * chunkSize,(float)z * chunkSize }, { float(camera.position.x),float(camera.position.z) });
						//if (dist < 256) {
							//hardware mesh
							//pretty much the same as software mesh but instead I store a hardware mesh
							//woah
						//	std::cout << "a\n";
						//	chunks[index].hwOrSoftwareRaster = false;
						//	chunks[index].HardwareMesher(&world, x, y, z);
						//}
						//else {
							//std::cout << "d\n";
							chunks[index].hwOrSoftwareRaster = true;
							chunks[index].SoftwareMesher(&world, x, y, z);
							chunks[index].updated = true;
						//}
						
						
					}
				}
			}
		END_TIMER(mesher)
	}
	void FreeTerrain() {

#pragma omp parallel for collapse(2)
		for (int x = 0; x < wx; x++) {
			for (int z = 0; z < wz; z++) {
				for (int y = 0; y < wy; y++) {
					if (chunks[y * (wx * wz) + z * wx + x].voxelCount == 0) continue;
					int index = y * (wx * wz) + z * wx + x;
					if (chunks[y * (wx * wz) + z * wx + x].lod != 1) {
						//free(world.chunkData[y * (wx * wz) + z * wx + x].terrain);
					}

				}
			}
		}
	}
	void RecalculateParticles() {
		
		grass = 0;
		int total = 0;
		int tt = 0;
		for (int x = 0; x < wx; x++) {
			for (int z = 0; z < wz; z++) {
				for (int y = 0; y < wy; y++) {

					tt+=chunks[y * (wx * wz) + z * wx + x].grassList.size();
				}
			}
		}
		particles = (float*)MemAlloc(tt*2 * 3 * sizeof(float));

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
		shaders.display.CreateSSBO(20, grass * 3 * sizeof(float));

		shaders.display.UpdateSSBO(20, particles, grass * 3 * sizeof(float), 0);
		free(particles);
	}
	bool CustomToggle(const char* label, bool* value) {
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
	uint32_t packRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
		return (static_cast<uint32_t>(r)) |
			(static_cast<uint32_t>(g) << 8) |
			(static_cast<uint32_t>(b) << 16) |
			(static_cast<uint32_t>(a) << 24);
	}
	void Run() {
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
					{ 45,  66,  33, 255 }   // 18: Alternative Leaf Green (Replaced Apple Green)
		};
		std::vector<uint32_t> ColorPallete;
		for (auto t : palette) {
			ColorPallete.push_back(packRGBA8(t.r, t.g, t.b, t.a));
		}
		int offfr = 0;
		int selectedColorIndex = 0; // Keep track of selection
		Math::Vector2 oldMouse = window.GetMousePos();
		GenerateTerrain();
		MeshWorld();
		FreeTerrain();
		RecalculateParticles();
		Update();
		const ImVec4 clearColor = { 0.1f, 0.1f, 0.12f, 1.0f };
		bool menuOpen = false;
		bool releasedMenu = false;
		int frame = 0;
		std::vector<int> chunksToUpdate;
		std::vector<Vector3> chunksToUpdateIds;
		std::vector<Vector3> decals;
		std::vector<int> decalTypes;
		Anubis::HWVIrGeomMesh mesh;
		size_t floatCount = sizeof(Anubis::cubeVertices) / sizeof(float);
		mesh.Create(Anubis::cubeVertices, floatCount);
		bool preallocated = false;
		while (window.IsOpen()) {
			CheckLodsToUpdate();
			Anubis::DisplayShader* shader = &shaders.display;
			if (graphicsSettings.FXAA) {
				shader = &shaders.displayNoFXAA;
			}
			int zero = 0;
			int* p_triangleCounter = (int*)shaders.display.ssboPointers[5];
			*p_triangleCounter = 0;
			if (!menuOpen) {
				camera.UpdateFREE(window.GetMousePos(), oldMouse, window);
			}
			//std::cout << afterGlobalMemUsage << " " << globalMemUsage << "\n";
			Vector3 tag = { (float)camera.target.x ,(float)camera.target.y,(float)camera.target.z };
			Vector3 pos = { (float)camera.position.x ,(float)camera.position.y,(float)camera.position.z };

			Vector3 forward = Vector3NormalizeInline(Vector3Subtract(tag, pos));
			window.Clear();
			oldMouse = window.GetMousePos();
			Plane planes[6];
			Matrix matView = MatrixLookAt(pos, tag, { 0,1,0 });
			ExtractFrustumPlanes(matView, planes);
			Math::Vector3 planeNormals[6];

			float planeDs[6];
			for (int i = 0; i < 6; i++) {
				planeNormals[i].x = planes[i].normal.x;
				planeNormals[i].y = planes[i].normal.y;
				planeNormals[i].z = planes[i].normal.z;
				planeDs[i] = planes[i].d;
			}

			int renderedChunks = 0;
			Anubis::GpuTimer frustum;
			Anubis::GpuTimer timerFrustum;
			Anubis::GpuTimer secondaryRays;
			timerFrustum.Start();
			if (totalMeshletCount > 0) {

				Anubis::SwitchShaderStage(shaders.frustumCull.programID);
				shaders.frustumCull.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders.frustumCull.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders.frustumCull.SetVector3Array(planeNormals, 6, "planeNormals");
				shaders.frustumCull.SetFloatArray(planeDs, 6, "planeDs");
				shaders.frustumCull.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders.frustumCull.SetVector3({ forward.x,forward.y,forward.z }, "camForward");

				frustum.Start();

				shaders.frustumCull.Use(totalMeshletCount / 32, 1, 1);
			}

			glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT | GL_CLIENT_MAPPED_BUFFER_BARRIER_BIT);
			double frustumTimeMs = timerFrustum.Stop(); // This will block until culling is done

			GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
			glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000);
			glDeleteSync(fence);
			int triangleCount = *p_triangleCounter;

			int r = 0;
			for (auto t : decals) {

				Render(&shaders, camera, matView, mesh, t, frame, decalTypes[r], editSize, ColorPallete.data(), ColorPallete.size());
				r++;
			}
			const float radius = 0.5f;

			for (size_t i = 0; i < physicalObjects.size(); i++) {
				auto& a = physicalObjects[i];
				int offx = ceil((a->size.x/2)/64)+1;
				int offy = ceil((a->size.y/2)/64)+1;
				int offz = ceil((a->size.z/2)/64)+1;
				for (int x = -offx; x <= offx; x++) {
					for (int y = -offy; y <= offy; y++) {
						for (int z = -offz; z <= offz; z++) {
							int chunkX = int(floor(a->position.x / chunkSize))+x;
							int chunkY = int(floor(a->position.y / chunkSize))+y;
							int chunkZ = int(floor(a->position.z / chunkSize))+z;
							int id = INDEX(chunkX, chunkY, chunkZ, wx, wy);
							if (!world.physicsAccelerationChunks[id].allocated) {
								world.physicsAccelerationChunks[id].objectsInChunk = 0;
								world.physicsAccelerationChunks[id].idReferences = (int*)MemAlloc(sizeof(int) * 1024);
								world.physicsAccelerationChunks[id].allocated = true;
							}
							if (world.physicsAccelerationChunks[id].objectsInChunk < 1024) {
								world.physicsAccelerationChunks[id].idReferences[world.physicsAccelerationChunks[id].objectsInChunk++] = i;
							}
							
						}
					}
				}
				
			}
			for (size_t i = 0; i < physicalObjects.size(); i++)
			{
				auto& a = physicalObjects[i];
				int chunkX = int(floor(a->position.x / chunkSize));
				int chunkY = int(floor(a->position.y / chunkSize));
				int chunkZ = int(floor(a->position.z / chunkSize));
				int id = INDEX(chunkX, chunkY, chunkZ, wx, wy);
				
				for (size_t f = i + 1; f < world.physicsAccelerationChunks[id].objectsInChunk; f++)
				{
					int j = world.physicsAccelerationChunks[id].idReferences[f];
					auto& b = physicalObjects[j];

					int ox = int(round(b->position.x - a->position.x));
					int oy = int(round(b->position.y - a->position.y));
					int oz = int(round(b->position.z - a->position.z));

					bool collided = false;

					for (int ax = 0; ax < 16 && !collided; ax++)
					{
						int bx = ax - ox;
						if (bx < 0 || bx >= 16)
							continue;

						for (int ay = 0; ay < 16 && !collided; ay++)
						{
							int by = ay - oy;
							if (by < 0 || by >= 16)
								continue;

							for (int az = 0; az < 16 && !collided; az++)
							{
								int bz = az - oz;
								if (bz < 0 || bz >= 16)
									continue;

								if (!a->visibility[IDX(ax, ay, az, 16)] ||
									!b->visibility[IDX(bx, by, bz, 16)])
									continue;

								collided = true;

								float dx = (b->position.x + bx) - (a->position.x + ax);
								float dy = (b->position.y + by) - (a->position.y + ay);
								float dz = (b->position.z + bz) - (a->position.z + az);

								float dist2 = dx * dx + dy * dy + dz * dz;

								if (dist2 < 0.0001f)
								{
									dx = 1.0f;
									dy = 0.0f;
									dz = 0.0f;
									dist2 = 1.0f;
								}

								float dist = sqrtf(dist2);

								float nx = dx / dist;
								float ny = dy / dist;
								float nz = dz / dist;

								float overlap = 1.0f - dist;

								a->position.x -= nx * overlap * 0.75f;
								a->position.y -= ny * overlap * 0.75f;
								a->position.z -= nz * overlap * 0.75f;

								b->position.z += nz * overlap * 0.75f;
								b->position.x += nx * overlap * 0.75f;
								b->position.y += ny * overlap * 0.75f;

								float rvx = b->velocity.x - a->velocity.x;
								float rvy = b->velocity.y - a->velocity.y;
								float rvz = b->velocity.z - a->velocity.z;

								float velAlongNormal =
									rvx * nx +
									rvy * ny +
									rvz * nz;

								if (velAlongNormal < 0.0f)
								{
									float restitution = 0.5f;

									float impulse = -(1.0f + restitution) * velAlongNormal;

									a->velocity.x -= nx * impulse * 0.75f;
									a->velocity.y -= ny * impulse * 0.75f;
									a->velocity.z -= nz * impulse * 0.75f;

									b->velocity.x += nx * impulse * 0.75f;
									b->velocity.y += ny * impulse * 0.75f;
									b->velocity.z += nz * impulse * 0.75f;
									
									
									
								}
							}
						}
					}
				}
			}

			for (size_t i = 0; i < physicalObjects.size(); i++) {
				auto& a = physicalObjects[i];
				int chunkX = int(floor(a->position.x / chunkSize));
				int chunkY = int(floor(a->position.y / chunkSize));
				int chunkZ = int(floor(a->position.z / chunkSize));
				int id = INDEX(chunkX, chunkY, chunkZ, wx, wy);
				if (world.physicsAccelerationChunks[id].allocated) {
					MemFree(world.physicsAccelerationChunks[id].idReferences);
					world.physicsAccelerationChunks[id].allocated = false;
				}

			}
			auto RenderPhysicsObject = [&](ShaderManager * shaders, Anubis::Camera camera, Matrix matView, Anubis::HWVIrGeomMesh & renderSphere, Vector3 vv, int frame, int voxelType, int size, uint32_t * pack, int packCount, GLuint handle, Vector3 rotation){
				Anubis::SwitchShaderStage(shaders->hwRenderForObjects.shaderId);

				Matrix m = GetStaticMVPMatrix(vv, matView);
				shaders->hwRenderForObjects.SetVector3(camera.position, "camPos");
				Math::Vector3 camFwd = Math::NormalizeVector3(
					Math::SubtractVector3(camera.target, camera.position)
				);
				shaders->hwRenderForObjects.SetVector3({ rotation.x,rotation.y,rotation.z }, "objectRotation");
				shaders->hwRenderForObjects.SetVector3({ vv.x,vv.y,vv.z }, "objectPos");
				shaders->hwRenderForObjects.SetVector3(camFwd, "camForward");
				shaders->hwRenderForObjects.SetUIntArray(pack, packCount, "packedColors");
				shaders->hwRenderForObjects.SetFloat(size, "size");
				shaders->hwRenderForObjects.SetInt(frame, "frame");
				shaders->hwRenderForObjects.SetInt(voxelType, "voxelType");
				shaders->hwRenderForObjects.SetUVec2(handle, "handle");
				
				shaders->hwRenderForObjects.SetVector3({ 1,0,1 }, "colorr");
				shaders->hwRenderForObjects.SetMatrix({ m.m0 ,m.m1,m.m2,m.m3,m.m4,m.m5,m.m6,m.m7,m.m8,m.m9,m.m10,m.m11,m.m12,m.m13,m.m14,m.m15 }, "MVP");
				
				shaders->hwRenderForObjects.Use();

				renderSphere.Draw();
			};
			for (auto& k : physicalObjects) {
				bool downardsCollision = false;
				bool rightLeftCollision = false;
				bool frontBackCollision = false;


				if (Vector3Distance({ (float)k->position.x,(float)k->position.y,(float)k->position.z }, { (float)camera.position.x,(float)camera.position.y,(float)camera.position.z })<200) {
					RenderPhysicsObject(&shaders, camera, matView, k->mesh, { (float)k->position.x,(float)k->position.y,(float)k->position.z }, frame, 1, 1, ColorPallete.data(), ColorPallete.size(), k->byte.handle, { (float)k->rotation.x,(float)k->rotation.y,(float)k->rotation.z });

					Anubis::SwitchShaderStage(shaders.fillOut.programID);
					shaders.fillOut.SetVector3(k->position, "objectPos");
					shaders.fillOut.SetUVec2(k->byte.handle, "handle");
					shaders.fillOut.SetInt(wx, "wx");
					shaders.fillOut.SetInt(wy, "wy");
					shaders.fillOut.SetInt(wz, "wz");
					shaders.fillOut.Use(4, 4, 4);
				}
				else {
					Anubis::SwitchShaderStage(shaders.renderPhysicalObjects.programID);
					shaders.renderPhysicalObjects.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
					shaders.renderPhysicalObjects.SetVector3(k->rotation, "objectRotation");
					shaders.renderPhysicalObjects.SetVector3(k->position, "objectPos");
					shaders.renderPhysicalObjects.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
					shaders.renderPhysicalObjects.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
					shaders.renderPhysicalObjects.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
					shaders.renderPhysicalObjects.SetUIntArray(ColorPallete.data(), ColorPallete.size(), "packedColors");
					shaders.renderPhysicalObjects.SetInt(wWidth, "renderWidth");
					shaders.renderPhysicalObjects.SetInt(wHeight, "renderHeight");
					shaders.renderPhysicalObjects.SetInt(frame, "frameIndex");
					shaders.renderPhysicalObjects.SetInt(0, "offset");
					shaders.renderPhysicalObjects.SetInt(wx, "wx");
					shaders.renderPhysicalObjects.SetInt(wy, "wy");
					shaders.renderPhysicalObjects.SetInt(wz, "wz");
					shaders.renderPhysicalObjects.SetUVec2(k->byte.handle, "handle");
					shaders.renderPhysicalObjects.Use(4, 4, 4);
				}
			}
			Math::Vector3 pp[2] = { camera.target,{wx * 32,150,wy * 32} };
			double rasterTimeMs;
			int trianglesRendered = 0;
			int count = 0;
			for (int x = 0; x < wx; x++) {
				for (int z = 0; z < wz; z++) {
					for (int y = 0; y < wy; y++) {
						int index = y * (wx * wz) + z * wx + x;
						count += chunks[index].cpumesh.triangleCount;

					}
	
				}
			}
			std::cout<<count<<' '<<totalMeshletCount<< '\n';
			if (graphicsSettings.hardwareSoftwareRendering && triangleCount>0) {
				Anubis::GpuTimer raster;
				raster.Start();
				Anubis::SwitchShaderStage(shaders.rasterizer.programID);
				shaders.rasterizer.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders.rasterizer.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
				shaders.rasterizer.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders.rasterizer.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders.rasterizer.SetInt(wx, "wx");
				shaders.rasterizer.SetInt(wy, "wy");
				shaders.rasterizer.SetInt(wz, "wz");
				shaders.rasterizer.SetUIntArray(ColorPallete.data(), ColorPallete.size(), "packedColors");
				shaders.rasterizer.SetInt(wWidth, "renderWidth");
				shaders.rasterizer.SetInt(wHeight, "renderHeight");
				shaders.rasterizer.SetInt(frame, "frameIndex");
				shaders.rasterizer.SetInt(0, "offset");
				
				shaders.rasterizer.Use(floor(triangleCount), 1, 1);
				rasterTimeMs = raster.Stop(); // This will block until raster is done
				trianglesRendered = triangleCount;
			}

			Anubis::GpuTimer grassT;
			grassT.Start();
			if (graphicsSettings.grassRender && grass>0) {

				Anubis::SwitchShaderStage(shaders.grassRenderer.programID);
				shaders.grassRenderer.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders.grassRenderer.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
				shaders.grassRenderer.SetInt(wWidth, "renderWidth");
				shaders.grassRenderer.SetInt(wHeight, "renderHeight");
				shaders.grassRenderer.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders.grassRenderer.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders.grassRenderer.SetInt(wx, "wx");
				shaders.grassRenderer.SetInt(wy, "wy");
				shaders.grassRenderer.SetInt(wz, "wz");

				shaders.grassRenderer.SetVector3Array(planeNormals, 6, "planeNormals");
				shaders.grassRenderer.SetFloatArray(planeDs, 6, "planeDs");
				shaders.grassRenderer.SetInt(frame, "frameIndex");
				shaders.grassRenderer.Use(grass/64, 1, 1);

			}

			auto grassTimeMs = grassT.Stop();
			auto allocateChunk = [&](int nx, int ny, int nz)
				{
					if (ny < 0) return;

					int neighbourId = ny * (wx * wz) + nz * wx + nx;
					if (nx < 0 || ny < 0 || nz < 0 || nx >= wx || ny >= wy || nz >= wz) return;
					if (chunks[neighbourId].lod == 0) return;
					if (chunks[neighbourId].hasFluid) return;
					if (chunks[neighbourId].voxelCount == chunks[neighbourId].size * chunks[neighbourId].size * chunks[neighbourId].size) return;

					chunks[neighbourId].hasFluid = true;
					int size = chunkSize / chunks[neighbourId].lod;
					world.chunkData[neighbourId].fluidByteMap.Clear({ size,size,size });

					if (world.chunkData[neighbourId].voxelCount != 0)
					{
						uint16_t* tt = chunks[neighbourId].DecompressChunk(&world.chunkData[neighbourId]);
						world.chunkData[neighbourId].fluidByteMap.Upload(tt);
						free(tt);
						world.chunkData[neighbourId].fluidByteMap.GetBindlessHandle();
						world.handle.handle[neighbourId].handle = world.chunkData[neighbourId].fluidByteMap.handle;
						world.handle.handle[neighbourId].lod = world.chunkData[neighbourId].lod;
					}
				};
			START_TIMER(physicsAllocator)
			if (!preallocated) {

				for (int x = 0; x < wx; x++) {
					for (int y = 0; y < wx; y++) {
						for (int z = 0; z < wx; z++) {

							allocateChunk(x, y, z);
							
						}
					}
				}
				shaders.display.CreateSSBO(3, sizeof(world.handle));
				shaders.display.UpdateSSBO(3, &world.handle, sizeof(world.handle), 0);
				preallocated = true;
			}
			offfr++;
			if (window.GetMouseButtonPressed(1) && offfr>5) {
				offfr = 0;
				Math::Vector3 angularVelocity = Math::NormalizeVector3(Math::SubtractVector3(camera.target, camera.position));
				angularVelocity.x *= 4;
				angularVelocity.y *= 4;
				angularVelocity.z *= 4;
				auto pp = std::make_unique<PhysicsObject>();
				pp->values = (uint16_t*)MemAlloc(16 * 16 * 16 * sizeof(uint16_t));
				pp->visibility = (bool*)MemAlloc(16 * 16 * 16 * sizeof(bool));
				pp->position = camera.target;
				pp->velocity = angularVelocity;
				pp->angularVelocity = { 0,0,0 };
				pp->rotation = { 0,0,0 };
				double radius = 7.0;
				double centerX = 7.5;
				double centerY = 7.5;
				double centerZ = 7.5;
				pp->Clean();
				int type = GetRandomValue(2,10);
				for (int x = 0; x < 16; x++) {
					for (int y = 0; y < 16; y++) {
						for (int z = 0; z < 16; z++) {
							double dx = x - centerX;
							double dy = y - centerY;
							double dz = z - centerZ;
							double distance = sqrt(dx * dx + dy * dy + dz * dz);

							if (distance <= radius) {
								pp->values[IDX(x, y, z, 16)] = type;
							}
							else {
								pp->values[IDX(x, y, z, 16)] = 0;
							}
						}
					}
				}
				pp->CheckVisibility();
				pp->Upload(palette);
				
				physicalObjects.push_back(std::move(pp));
			}

			if (graphicsSettings.secondaryRays) {
				secondaryRays.Start();
				Anubis::SwitchShaderStage(shaders.secondaryRaysRender.programID);
				shaders.secondaryRaysRender.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders.secondaryRaysRender.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
				shaders.secondaryRaysRender.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders.secondaryRaysRender.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders.secondaryRaysRender.SetInt(wx, "wx");
				shaders.secondaryRaysRender.SetInt(wy, "wy");
				shaders.secondaryRaysRender.SetInt(wz, "wz");
				shaders.secondaryRaysRender.SetFloat(timeSpeed, "timeSpeed");
				shaders.secondaryRaysRender.SetInt(wWidth, "renderWidth");
				shaders.secondaryRaysRender.SetInt(wHeight, "renderHeight");
				shaders.secondaryRaysRender.SetInt(frame, "frameIndex");
				shaders.secondaryRaysRender.Use(wWidth / 32, wHeight / 32, 1);
				shaders.secondaryRaysRender.SetUIntArray(ColorPallete.data(), ColorPallete.size(), "packedColors");

				Anubis::SwitchShaderStage(shaders.clouds.programID);
				shaders.clouds.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders.clouds.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
				shaders.clouds.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders.clouds.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders.clouds.SetInt(wx, "wx");
				shaders.clouds.SetInt(wy, "wy");
				shaders.clouds.SetInt(wz, "wz");
				shaders.clouds.SetFloat(timeSpeed, "timeSpeed");
				shaders.clouds.SetInt(wWidth, "renderWidth");
				shaders.clouds.SetInt(wHeight, "renderHeight");
				shaders.clouds.SetInt(frame, "frameIndex");
				shaders.clouds.Use(wWidth / 32, wHeight / 32, 1);
			}

			for (auto& k : physicalObjects) {
				bool downardsCollision = false;
				bool rightLeftCollision = false;
				bool frontBackCollision = false;

				Anubis::SwitchShaderStage(shaders.clearPhysicalObjects.programID);
				shaders.clearPhysicalObjects.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders.clearPhysicalObjects.SetVector3(k->position, "objectPos");
				shaders.clearPhysicalObjects.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
				shaders.clearPhysicalObjects.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders.clearPhysicalObjects.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders.clearPhysicalObjects.SetUIntArray(ColorPallete.data(), ColorPallete.size(), "packedColors");
				shaders.clearPhysicalObjects.SetInt(wWidth, "renderWidth");
				shaders.clearPhysicalObjects.SetInt(wHeight, "renderHeight");
				shaders.clearPhysicalObjects.SetInt(frame, "frameIndex");
				shaders.clearPhysicalObjects.SetInt(0, "offset");
				shaders.clearPhysicalObjects.SetInt(wx, "wx");
				shaders.clearPhysicalObjects.SetInt(wy, "wy");
				shaders.clearPhysicalObjects.SetInt(wz, "wz");

				shaders.clearPhysicalObjects.SetUVec2(k->byte.handle, "handle");

				shaders.clearPhysicalObjects.Use(4, 4, 4);

				for (int x = 0; x < 16; x++) {
					for (int y = 0; y < 16; y++) {
						for (int z = 0; z < 16; z++) {
							if (k->visibility[IDX(x, y, z, 16)]) {

								int wx_ = int(k->position.x) + x;
								int wy_ = int(k->position.y) + y;
								int wz_ = int(k->position.z) + z;

								int chunkx = wx_ / chunkSize;
								int chunky = wy_ / chunkSize;
								int chunkz = wz_ / chunkSize;

								// bounds check — non-negotiable
								if (chunkx < 0 || chunky < 0 || chunkz < 0 ||
									chunkx >= wx || chunky >= wy || chunkz >= wz)
									continue;

								int id = chunky * (wx * wz) + chunkz * wx + chunkx;

								if (world.chunkData[id].lod != 0) {
									int lod = world.chunkData[id].lod;
									int cs = chunkSize;

									downardsCollision |= chunks[id].ReadVoxel(&world.chunkData[id],
										(wx_ % cs) / lod,
										((wy_ - 1) % cs) / lod,
										(wz_ % cs) / lod) != 0;

									rightLeftCollision |=
										chunks[id].ReadVoxel(&world.chunkData[id],
											((wx_ - 1) % cs) / lod, (wy_ % cs) / lod, (wz_ % cs) / lod) != 0 ||
										chunks[id].ReadVoxel(&world.chunkData[id],
											((wx_ + 1) % cs) / lod, (wy_ % cs) / lod, (wz_ % cs) / lod) != 0;

									frontBackCollision |=
										chunks[id].ReadVoxel(&world.chunkData[id],
											(wx_ % cs) / lod, (wy_ % cs) / lod, ((wz_ + 1) % cs) / lod) != 0 ||
										chunks[id].ReadVoxel(&world.chunkData[id],
											(wx_ % cs) / lod, (wy_ % cs) / lod, ((wz_ - 1) % cs) / lod) != 0;
								}
							}
						}
					}
				}

				if (rightLeftCollision)
				{
					k->velocity.x *= -0.15f;
					if (fabs(k->velocity.x) < 0.02f)
						k->velocity.x = 0.0f;
				}

				if (frontBackCollision)
				{
					k->velocity.z *= -0.15f;
					if (fabs(k->velocity.z) < 0.02f)
						k->velocity.z = 0.0f;
				}

				if (downardsCollision)
				{
					if (k->velocity.y < 0.0f)
					{
						k->velocity.y *= -0.2f;    // small bounce
						if (fabs(k->velocity.y) < 0.05f)
							k->velocity.y = 0.0f;

						if (!rightLeftCollision)
							k->velocity.x += k->velocity.y * 0.25f;

						// Slide in Z
						if (!frontBackCollision)
							k->velocity.z += k->velocity.y * 0.25f;
					}
				}
				else
				{
					k->velocity.y -= 0.1f;
				}
				k->velocity.x *= 0.98;
				k->velocity.y *= 0.98;
				k->velocity.z *= 0.98;
				k->position.y += k->velocity.y;
				k->position.x += k->velocity.x;
				k->position.z += k->velocity.z;

			}
			auto secondaryRaysMs = secondaryRays.Stop();

			shader->Use();
			shader->SetInt(frame, "frameIndex");

			shader->SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shader->SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shader->SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shader->SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
			shader->SetInt(wx, "wx");
			shader->SetInt(wy, "wy");
			shader->SetInt(wz, "wz");
			shader->SetUIntArray(ColorPallete.data(), ColorPallete.size(), "packedColors");
			shader->SetInt(wHeight, "resY");
			shader->SetVector4({ 1.0f, 1.0f, 1.0f, 1.0f }, "color");
			Anubis::Quad quad;
			quad.Create();
			window.DrawQuad(quad);
			Vector3 rayCastPos = pos;
			for (int i = 0; i < 100; i++) {
				rayCastPos.x += forward.x * 2;
				rayCastPos.y += forward.y * 2;
				rayCastPos.z += forward.z*2;
				int x = rayCastPos.x / chunkSize;
				int y = rayCastPos.y / chunkSize;
				int z = rayCastPos.z / chunkSize;

				int id = y * (wx * wz) + z * wx + x;
				if (chunks[id].ReadVoxel(&world.chunkData[id], (int)rayCastPos.x % chunkSize, (int)rayCastPos.y % chunkSize, (int)rayCastPos.z%chunkSize) != 0) {
					break;
				}
			
			}


	//		Render(&shaders, camera, matView, mesh, rayCastPos, frame, 4, editSize, ColorPallete.data(), ColorPallete.size());
			if (window.GetMouseButtonPressed(0) && !menuOpen && selectedColorIndex>=0) {

				Vector3 ppp = rayCastPos;

				bool doneChanges = false;
				for (int dx = -editSize; dx < editSize; dx++) {
					for (int dy = -editSize; dy < editSize; dy++) {
						for (int dz = -editSize; dz < editSize; dz++) {

							Vector3 pp = ppp;
							pp.x += dx;
							pp.y += dy;
							pp.z += dz;

							int x = pp.x / chunkSize;
							int y = pp.y / chunkSize;
							int z = pp.z / chunkSize;

							int id = y * (wx * wz) + z * wx + x;

							if (id >= wx * wy * wz || id < 0) continue;
							if (chunks[id].lod == 1 || chunks[id].lod==0) {

								bool alreadyOnList = false;
								for (auto t : chunksToUpdate) {
									if (t == id) {
										alreadyOnList = true;
									}
								}

								chunks[id].lod = 1;
								if (!alreadyOnList) {
									chunksToUpdate.push_back(id);
									chunksToUpdateIds.push_back({ float(x), (float)y, float(z) });

									if (chunks[id].voxelCount == 0) {
										chunks[id].gx = x;
										chunks[id].gy = y;
										chunks[id].gz = z;
										chunks[id].size = chunkSize/chunks[id].lod;

										world.chunkData[id].terrain = (uint16_t*)MemAlloc(chunkSize * chunkSize * chunkSize * sizeof(uint16_t));
										for (int i = 0; i < chunkSize * chunkSize * chunkSize; i++) {
											world.chunkData[id].terrain[i] = 0;
										}
										chunks[id].voxelCount = 1;
										chunks[id].updated = true;
										world.chunkData[id].fluidByteMap.Clear({ chunks[id].size ,chunks[id].size ,chunks[id].size });
										world.chunkData[id].fluidByteMap.Upload(world.chunkData[id].terrain);
										world.chunkData[id].fluidByteMap.GetBindlessHandle();
										world.handle.handle[id].handle = world.chunkData[id].fluidByteMap.handle;
										world.handle.handle[id].lod = 1;
									}
								}
								if (chunks[id].hasFluid) {
									if (selectedColorIndex == 0) world.chunkData[id].fluidByteMap.EditGPURegion({ int(fabs(pp.x)) % chunkSize, int(fabs(pp.y)) % chunkSize, int(fabs(pp.z)) % chunkSize }, { 1,1,1 }, 0);

									
									else world.chunkData[id].fluidByteMap.EditGPURegion({ int(fabs(pp.x)) % chunkSize, int(fabs(pp.y)) % chunkSize, int(fabs(pp.z)) % chunkSize }, { 1,1,1 }, selectedColorIndex + 1);

								}
								if (selectedColorIndex == 0) {
									if (chunks[id].SetTerrain(&world.chunkData[id], int(fabs(pp.x)) % chunkSize, int(fabs(pp.y)) % chunkSize, int(fabs(pp.z)) % chunkSize, 0) && !doneChanges) {
										doneChanges = true;

									}
								}
								else {
									if (chunks[id].SetTerrain(&world.chunkData[id], int(fabs(pp.x)) % chunkSize, int(fabs(pp.y)) % chunkSize, int(fabs(pp.z)) % chunkSize, selectedColorIndex + 1) && !doneChanges) {
										doneChanges = true;

									}
								}
								if (chunks[id].updated) {
									chunks[id].voxelCount += 1;
								}
							}
						}
					}
				}
				if (doneChanges) {
					Vector3 snappedPos = {
						std::floor(ppp.x) + 2.5,
						std::floor(ppp.y) + 2.5,
						std::floor(ppp.z) + 2.5
					};
					decals.push_back(snappedPos);
					decalTypes.push_back(selectedColorIndex+1);
				}
			}
			else if (chunksToUpdate.size() != 0)
			{
				 
				int r = 0;
				for (auto t : chunksToUpdate) {
					if (t >= wx * wy * wz || t < 0) continue;

					if (!chunks[t].updated) {
						chunks[t].cpumesh.colorCount = 0;
						chunks[t].cpumesh.vertexCount = 0;
						chunks[t].cpumesh.triangleCount = 0;
						chunks[t].cpumesh.realTriangleCount = 0;
						free(world.chunkData[t].bitMap);
						free(chunks[t].cpumesh.packedVertex);

						free(chunks[t].cpumesh.gpuBasedColoring);

						free(chunks[t].cpumesh.MeshletAABB);

						free(chunks[t].cpumesh.meshletPosition);

						chunks[t].cpumesh.realTriangleCount = 0;
						chunks[t].cpumesh.vertexCount = 0;
						chunks[t].cpumesh.colorCount = 0;
					}


					chunks[t].GenerateLod(&world.chunkData[t], chunks[t].gx, chunks[t].gy, chunks[t].gz);

					chunks[t].GenerateBitMap(&world.chunkData[t]);
					chunks[t].updated = true;
					r++;
				}
				for (auto t : chunksToUpdate) {

					if (t >= wx * wy * wz || t < 0) continue;
					chunks[t].lod = 1;
					chunks[t].SoftwareMesher(&world, chunks[t].gx, chunks[t].gy, chunks[t].gz);

				}


				Update();
				chunksToUpdate.clear();
				decals.clear();
				decalTypes.clear();
				chunksToUpdate.resize(0);
				decals.resize(0);
				decalTypes.resize(0);
				shaders.display.UpdateSSBO(3, &world.handle, sizeof(world.handle), 0);
			}
			
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

				float padding = 10.0f;

				float resWidth = 250.0f;
				ImGui::SetNextWindowPos(ImVec2(wWidth - resWidth - padding, padding), ImGuiCond_Always);
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
						wWidth = graphicsSettings.resolutions[currentResolutionWindow].width;
						wHeight = graphicsSettings.resolutions[currentResolutionWindow].height;
					}

					if (currentResolutionWindow == i) {
						ImGui::PopStyleColor();
					}
				}
				ImGui::End();

				float debugWidth = 250.0f;
				ImGui::SetNextWindowPos(ImVec2(wWidth - debugWidth - padding, padding + 180.0f), ImGuiCond_Always);
				ImGui::SetNextWindowSize(ImVec2(debugWidth, wHeight - 180.0f - (padding * 2)), ImGuiCond_Always);

				ImGui::Begin("Debug Menu");

				ImGui::SliderInt("Edit Size", &editSize, 1, 300);

				CustomToggle("Render grass", &graphicsSettings.grassRender);
				CustomToggle("Hardware/Software rendering", &graphicsSettings.hardwareSoftwareRendering);
				CustomToggle("Secondary rays", &graphicsSettings.secondaryRays);

				if (ImGui::Button("Recompile shaders")) {
					shaders.Load();
				}
				CustomToggle("God mode", &graphicsSettings.godMode);
				CustomToggle("Simulate Physics", &graphicsSettings.simulatePhysics);
				CustomToggle("Render Fluids", &graphicsSettings.renderFluids);

				if (ImGui::Button("Reset entities")) {
				}

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
				ImGui::SetNextWindowPos(ImVec2(padding, wHeight - paletteHeight - padding), ImGuiCond_Always);
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
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			window.UpdateBuffer();
			Anubis::SwitchShaderStage(shaders.hizBuilder.programID);
			shaders.hizBuilder.SetInt(frame, "frameIndex");
			shaders.hizBuilder.SetInt(1, "buildLevel");
			shaders.hizBuilder.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shaders.hizBuilder.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shaders.hizBuilder.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shaders.hizBuilder.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");

			shaders.hizBuilder.Use(wWidth / 32, 1, wHeight / 32);
			frame++;
		}
	}
};
int main() {
	
	App* app = new App;
	app->Run();
}