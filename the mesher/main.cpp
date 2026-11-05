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
const bool gpuMeshing = true;
struct Particle {
	Math::Vector3 position;
	uint32_t color;
	Math::Vector3 targetPosition;
	void (*update)(Particle&);
};
Math::Vector3 Vector3MoveTowards(Math::Vector3 v, Math::Vector3 target, float maxDistance)
{
	Math::Vector3 result = { 0 };
	float dx = target.x - v.x;
	float dy = target.y - v.y;
	float dz = target.z - v.z;
	float sqrDist = dx * dx + dy * dy + dz * dz;
	if ((maxDistance < 0.0f) || (sqrDist > (maxDistance * maxDistance)))
	{
		float dist = sqrtf(sqrDist);
		float inv = maxDistance / dist;

		result.x = v.x + dx * inv;
		result.y = v.y + dy * inv;
		result.z = v.z + dz * inv;
	}
	else
	{
		result = target;
	}

	return result;
}
void ExampleUpdate(Particle& p) {
	p.position = Vector3MoveTowards(p.position, p.targetPosition, 3);
	if (Vector3Distance({ (float)p.position.x, (float)p.position.y,(float)p.position.z }, { (float)p.targetPosition.x,(float)p.targetPosition.y,(float)p.targetPosition.z }) < 1) {
		p.targetPosition.x = GetRandomValue(0, wx * 32);
		p.targetPosition.y = GetRandomValue(150, 250);
		p.targetPosition.z = GetRandomValue(0, wz * 32);
	}
}
const int MAX_PARTICLES = 1000000;
const bool DEBUG_FEATURES_ON = true;
class ParticleRender {
public:
	Particle *particles;
	int renderQueue;
	
	void Create() {
		renderQueue = 0;
		particles = new Particle[MAX_PARTICLES];
	}
	
	void PushRenderParticle(Math::Vector3 position, Color color, void (*update)(Particle&)) {
		Particle particle;
		particle.position = position;
		particle.color = 0;
		particle.update = update;
		particles[renderQueue++] = particle;
	}
	
	void Render(ShaderManager *shaders, Matrix matView, Matrix matProj, Vector3 forward, Vector3 pos, int wWidth, int wHeight, int frameIndex) {
		Math::Vector3 billboardPoses[1024];
		int renderPassAmount = 0;
		for (int i = 0; i <renderQueue; i++) {
			billboardPoses[renderPassAmount] = particles[i].position;
			renderPassAmount++;
			if (renderPassAmount == 1024-1) {

				Anubis::SwitchShaderStage(shaders->bilboards.programID);
				shaders->bilboards.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders->bilboards.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
				shaders->bilboards.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders->bilboards.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders->bilboards.SetInt(wx, "wx");
				shaders->bilboards.SetInt(wy, "wy");
				shaders->bilboards.SetInt(wz, "wz");
				shaders->bilboards.SetInt(wWidth, "renderWidth");
				shaders->bilboards.SetInt(wHeight, "renderHeight");
				shaders->bilboards.SetVector3Array(billboardPoses, renderPassAmount, "billboards");
				shaders->bilboards.SetInt(frameIndex, "frameIndex");
				shaders->bilboards.SetFloat(1, "billboardSize");
				shaders->bilboards.SetInt(0, "billboardColor");
				shaders->bilboards.Use(renderQueue / 64, 1, 1);
				renderPassAmount = 0;
				//dispatch
			}
		}
		if (renderPassAmount > 0) {
			//dispatch

			Anubis::SwitchShaderStage(shaders->bilboards.programID);
			shaders->bilboards.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shaders->bilboards.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shaders->bilboards.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shaders->bilboards.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
			shaders->bilboards.SetInt(wx, "wx");
			shaders->bilboards.SetInt(wy, "wy");
			shaders->bilboards.SetInt(wz, "wz");
			shaders->bilboards.SetInt(wWidth, "renderWidth");
			shaders->bilboards.SetInt(wHeight, "renderHeight");
			shaders->bilboards.SetVector3Array(billboardPoses, renderPassAmount, "billboards");
			shaders->bilboards.SetInt(frameIndex, "frameIndex");
			shaders->bilboards.SetInt(0, "billboardColor");
			shaders->bilboards.SetFloat(1, "billboardSize");;

			shaders->bilboards.Use(renderQueue/64, 1, 1);
		}
	}
	
	void Update() {
		for (int i = 0; i < renderQueue; i++) {
			particles[i].update(particles[i]);
		}
	}
	
	~ParticleRender() {
		delete[] particles;
	}
	
};
const int maxGPUBatch = 1024 * 1024;
const int maxGrassCount = 1449303 * 2;
struct ViewPort {
	ShaderManager* shaders;

	Plane planes[6];
	Math::Vector3 planeNormals[6];
	Anubis::Camera camera;
	float planeDs[6];
	bool raytracePrimaryRays = false;
	void FrustumCull(Matrix matView, Vector3 pos, Vector3 forward, int totalMeshletCount) {

		ExtractFrustumPlanes(matView, planes);

		for (int i = 0; i < 6; i++) {
			planeNormals[i].x = planes[i].normal.x;
			planeNormals[i].y = planes[i].normal.y;
			planeNormals[i].z = planes[i].normal.z;
			planeDs[i] = planes[i].d;
		}
		Anubis::SwitchShaderStage(shaders->frustumCull.programID);
		shaders->frustumCull.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
		shaders->frustumCull.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
		shaders->frustumCull.SetVector3Array(planeNormals, 6, "planeNormals");
		shaders->frustumCull.SetFloatArray(planeDs, 6, "planeDs");
		shaders->frustumCull.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
		shaders->frustumCull.SetVector3({ forward.x,forward.y,forward.z }, "camForward");


		shaders->frustumCull.Use(totalMeshletCount / 32, 1, 1);
	}
	void RenderClouds(Matrix matView, int frame, Vector3 pos, Vector3 forward, int wWidth, int wHeight, float timeSpeed) {
		Anubis::SwitchShaderStage(shaders->clouds.programID);
		shaders->clouds.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
		shaders->clouds.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
		shaders->clouds.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
		shaders->clouds.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
		shaders->clouds.SetInt(wx, "wx");
		shaders->clouds.SetInt(wy, "wy");
		shaders->clouds.SetInt(wz, "wz");
		shaders->clouds.SetFloat(timeSpeed, "timeSpeed");
		shaders->clouds.SetInt(wWidth, "renderWidth");
		shaders->clouds.SetInt(wHeight, "renderHeight");
		shaders->clouds.SetInt(frame, "frameIndex");
		shaders->clouds.Use(wWidth / 32, wHeight / 32, 1);
	}
	void SecondaryRays(Matrix matView, Vector3 pos, Vector3 forward, const std::vector<uint32_t> palette, int frame, float timeSpeed, int wWidth, int wHeight, World *world) {

		Anubis::SwitchShaderStage(shaders->secondaryRaysRender.programID);
		shaders->secondaryRaysRender.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
		shaders->secondaryRaysRender.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
		shaders->secondaryRaysRender.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
		shaders->secondaryRaysRender.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
		shaders->secondaryRaysRender.SetInt(wx, "wx");
		shaders->secondaryRaysRender.SetInt(wy, "wy");
		shaders->secondaryRaysRender.SetInt(wz, "wz");
		shaders->secondaryRaysRender.SetInt(world->lightCount, "lightCount");
		shaders->secondaryRaysRender.SetVector3Array(world->lightPositions, 16, "lightPositions");
		shaders->secondaryRaysRender.SetVector3Array(world->lightValues, 16, "lightValues");
		shaders->secondaryRaysRender.SetIntArray(world->lightRadiusList, 16, "lightRadiusList");

		shaders->secondaryRaysRender.SetFloat(timeSpeed, "timeSpeed");
		shaders->secondaryRaysRender.SetInt(wWidth, "renderWidth");
		shaders->secondaryRaysRender.SetInt(wHeight, "renderHeight");
		shaders->secondaryRaysRender.SetInt(frame, "frameIndex");
		shaders->secondaryRaysRender.Use(wWidth / 32, wHeight / 32, 1);
		shaders->secondaryRaysRender.SetUIntArray(palette.data(), palette.size(), "packedColors");
	}
	void RenderPhysicsObject(ShaderManager* shaders, Anubis::Camera camera, Matrix matView, Anubis::HWVIrGeomMesh& renderSphere, Vector3 vv, int frame, int voxelType, int size, uint32_t* pack, int packCount, GLuint handle, Vector3 rotation, int wHeight) {
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
		shaders->hwRenderForObjects.SetInt(wHeight, "vHeight");
		shaders->hwRenderForObjects.SetInt(voxelType, "voxelType");
		shaders->hwRenderForObjects.SetUVec2(handle, "handle");

		shaders->hwRenderForObjects.SetVector3({ 1,0,1 }, "colorr");
		shaders->hwRenderForObjects.SetMatrix({ m.m0 ,m.m1,m.m2,m.m3,m.m4,m.m5,m.m6,m.m7,m.m8,m.m9,m.m10,m.m11,m.m12,m.m13,m.m14,m.m15 }, "MVP");

		shaders->hwRenderForObjects.Use();

		renderSphere.Draw();
	};
	void RasterizeMeshes(Matrix matView, Vector3 pos, Vector3 forward, const std::vector<uint32_t> palette, int frame, int triangleCount, int wWidth, int wHeight) {
		if (raytracePrimaryRays) {
			Anubis::SwitchShaderStage(shaders->primaryRays.programID);
			shaders->primaryRays.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shaders->primaryRays.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shaders->primaryRays.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shaders->primaryRays.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
			shaders->primaryRays.SetInt(wx, "wx");
			shaders->primaryRays.SetInt(wy, "wy");
			shaders->primaryRays.SetInt(wz, "wz");
			shaders->primaryRays.SetUIntArray(palette.data(), palette.size(), "packedColors");
			shaders->primaryRays.SetInt(wWidth, "renderWidth");
			shaders->primaryRays.SetInt(wHeight, "renderHeight");
			shaders->primaryRays.SetInt(frame, "frameIndex");
			shaders->primaryRays.SetInt(0, "offset");

			shaders->primaryRays.Use(wWidth/32, wHeight/16, 1);

		}
		else {
			Anubis::SwitchShaderStage(shaders->rasterizer.programID);
			shaders->rasterizer.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shaders->rasterizer.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shaders->rasterizer.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shaders->rasterizer.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
			shaders->rasterizer.SetInt(wx, "wx");
			shaders->rasterizer.SetInt(wy, "wy");
			shaders->rasterizer.SetInt(wz, "wz");
			shaders->rasterizer.SetUIntArray(palette.data(), palette.size(), "packedColors");
			shaders->rasterizer.SetInt(wWidth, "renderWidth");
			shaders->rasterizer.SetInt(wHeight, "renderHeight");
			shaders->rasterizer.SetInt(frame, "frameIndex");
			shaders->rasterizer.SetInt(0, "offset");

			shaders->rasterizer.Use(floor(triangleCount), 1, 1);
		}
	}
	void RenderGrass(Matrix matView, Vector3 pos, Vector3 forward, int frame, int wWidth, int wHeight, int grass) {

		Anubis::SwitchShaderStage(shaders->grassRenderer.programID);
		shaders->grassRenderer.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
		shaders->grassRenderer.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
		shaders->grassRenderer.SetInt(wWidth, "renderWidth");
		shaders->grassRenderer.SetInt(wHeight, "renderHeight");
		shaders->grassRenderer.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
		shaders->grassRenderer.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
		shaders->grassRenderer.SetInt(wx, "wx");
		shaders->grassRenderer.SetInt(wy, "wy");
		shaders->grassRenderer.SetInt(wz, "wz");
		shaders->grassRenderer.SetVector3Array(planeNormals, 6, "planeNormals");
		shaders->grassRenderer.SetFloatArray(planeDs, 6, "planeDs");
		shaders->grassRenderer.SetInt(frame, "frameIndex");
		shaders->grassRenderer.Use(grass / 64, 1, 1);
	}
	/*
	void RenderPhysicalObjects(Matrix matView, Vector3 pos, Vector3 forward, std::vector<uint32_t> palette, int frame, std::vector<std::unique_ptr<PhysicsObject>> physicalObjects, int wWidth, int wHeight) {

		for (auto& k : physicalObjects) {
			bool downardsCollision = false;
			bool rightLeftCollision = false;
			bool frontBackCollision = false;


			if (Vector3Distance({ (float)k->position.x,(float)k->position.y,(float)k->position.z }, { (float)camera.position.x,(float)camera.position.y,(float)camera.position.z }) < 20) {
				RenderPhysicsObject(shaders, camera, matView, k->mesh, { (float)k->position.x,(float)k->position.y,(float)k->position.z }, frame, 1, 1, palette.data(), palette.size(), k->byte.handle, { (float)k->rotation.x,(float)k->rotation.y,(float)k->rotation.z },wHeight);

				Anubis::SwitchShaderStage(shaders->fillOut.programID);
				shaders->fillOut.SetVector3(k->position, "objectPos");
				shaders->fillOut.SetUVec2(k->byte.handle, "handle");
				shaders->fillOut.SetInt(wx, "wx");
				shaders->fillOut.SetInt(wy, "wy");
				shaders->fillOut.SetInt(wz, "wz");
				shaders->fillOut.Use(4, 4, 4);
			}
			else {
				Anubis::SwitchShaderStage(shaders->renderPhysicalObjects.programID);
				shaders->renderPhysicalObjects.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders->renderPhysicalObjects.SetVector3(k->rotation, "objectRotation");
				shaders->renderPhysicalObjects.SetVector3(k->position, "objectPos");
				shaders->renderPhysicalObjects.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
				shaders->renderPhysicalObjects.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders->renderPhysicalObjects.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders->renderPhysicalObjects.SetUIntArray(palette.data(), palette.size(), "packedColors");
				shaders->renderPhysicalObjects.SetInt(wWidth, "renderWidth");
				shaders->renderPhysicalObjects.SetInt(wHeight, "renderHeight");
				shaders->renderPhysicalObjects.SetInt(frame, "frameIndex");
				shaders->renderPhysicalObjects.SetInt(0, "offset");
				shaders->renderPhysicalObjects.SetInt(wx, "wx");
				shaders->renderPhysicalObjects.SetInt(wy, "wy");
				shaders->renderPhysicalObjects.SetInt(wz, "wz");
				shaders->renderPhysicalObjects.SetUVec2(k->byte.handle, "handle");
				shaders->renderPhysicalObjects.Use(4, 4, 4);
			}
		}
	}*/
};
class App {
	Anubis::Window window;
	Chunk* chunks;
	ShaderManager shaders;
	World world;
	float* particles;
	bool firstGen = true;
	int totalTriangleCount;
	int renderDistance = 500;
	int wWidth = 3840;
	int wHeight = 2160;
	int currentResolutionWindow = 3;
	int grass = 0;
	int totalMeshletCount = 0;
	int editSize = 5;
	Math::IVector3* vv;
	Settings graphicsSettings;
	std::vector<std::unique_ptr<PhysicsObject>> physicalObjects;
	std::vector<std::string> worlds = { "Mountains", "Desert", "Caves", "Sky" };
	float timeSpeed = 0.0004;
	bool mainMenu = false;
	int alreadySpawnedGrass = 0;
	ViewPort viewPort;
	ParticleRender particleRenderer;
	CHUNKTYPE selectedType = MOUNTAIN;
public:
	App() {
		//IMPLEMENT THE HARDWARE PIPELINE
		//39873 - voxels
		//239238 - quads
		//478476 - triangles
		wWidth = graphicsSettings.resolutions[currentResolutionWindow].width;
		wHeight = graphicsSettings.resolutions[currentResolutionWindow].height;

		matProj = MatrixPerspective(90 * DEG2RAD, ((double)wWidth / (double)wHeight), 0.1, 100000.0);

		window.Init(wWidth,wHeight,4,6,"software rasterization");
		window.DisableCursor();
		window.Resize(600, 800);
		
		omp_set_num_threads(32);
		viewPort.camera.target = { 0,1,0 };
		SetRandomSeed(100);
		shaders.Load();
		shaders.display.CreateSSBO(0, sizeof(uint32_t) * 3840 * 2160 *6);
		int maxSSBOCount = 0;
		glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxSSBOCount);
		if (gpuMeshing) {
			shaders.display.CreatePersistentSSBO(9, sizeof(int) * 2+1024*1024*2*sizeof(int));
			shaders.display.CreateSSBO(10, maxGPUBatch * sizeof(Math::IVector3));
			vv = (Math::IVector3*)(MemAlloc(1024 * 1024 * sizeof(Math::IVector3)));
		}
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();

		if (firstGen && gpuMeshing) {
			std::cout << "gpu meshing on\n";
			totalTriangleCount = 59259259;
			totalMeshletCount = totalTriangleCount/64;
			shaders.display.CreatePersistentSSBO(5, (totalMeshletCount + 1) * sizeof(int) * 8);
			shaders.display.CreatePersistentSSBO(2, totalTriangleCount * sizeof(uint32_t) * 8);
			shaders.display.CreatePersistentSSBO(6, totalTriangleCount * sizeof(uint32_t) * 8);
			shaders.display.CreatePersistentSSBO(7, totalMeshletCount * 6 * sizeof(float) * 8);
			firstGen = false;
		}
		shaders.display.CreateSSBO(20, maxGrassCount * 3 * sizeof(float));
		ImGui_ImplGlfw_InitForOpenGL(window.GetWindowHandle(), true);
		ImGui_ImplOpenGL3_Init("#version 460");
		viewPort.shaders = &shaders;
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
		for (int y = 0; y < gen.highestChunkY; y++) {
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
			if (chunks[neighbourId].voxelCount ==
				chunks[neighbourId].size * chunks[neighbourId].size * chunks[neighbourId].size)
				continue;

			int size = chunkSize / chunks[neighbourId].lod;
			if (world.chunkData[neighbourId].skipped) {
				world.handle.handle[neighbourId].lod = 3000;
			}
			if (world.chunkData[neighbourId].voxelCount != 0 &&
				!world.chunkData[neighbourId].skipped) {

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
		int drd = 0;
		

		int lod = 1;
		float dist = Vector2Distance({ (float)nx * chunkSize + chunkSize / 2,(float)nz * chunkSize + chunkSize / 2 }, { float(viewPort.camera.position.x),float(viewPort.camera.position.z) });
		bool grass = false;

		if (dist < 1920) grass = true;
		if (dist > 960) lod = 2;
		if (dist > 960 * 2) lod = 4;
		if (dist > 960 * 4) lod = 8;
		if (dist > 960 * 8) lod = 16;
		if (dist > 960 * 12) lod = 32;
		if (dist > 960 * 24) lod = 64;
		if (world.array[nx][nz]) {
			world.setLod[nx][nz] = lod;
			for (int ny = 0; ny < wy; ny++) {
				int neighbourId = ny * (wx * wz) + nz * wx + nx;
				ChunkDeleteMeshData(nx, ny, nz);
				vv[drd] = { nx,ny,nz };
				drd++;
				if (clearChunkData) {
					chunks[neighbourId].ClearChunkData(&world.chunkData[neighbourId]);
					chunks[neighbourId].grassList.clear();
				}
				if (uploadData) uploadChunkSSBO(neighbourId);
			}
		}

		if (drd > 0) {
			shaders.display.UpdateSSBO(10, vv, drd * sizeof(Math::IVector3), 0);

			auto batchUploadEnd = std::chrono::high_resolution_clock::now();
			Anubis::GpuTimer mesh;
			mesh.Start();
			Anubis::SwitchShaderStage(shaders.generateMesh.programID);
			shaders.generateMesh.SetInt(wx, "wx");
			shaders.generateMesh.SetInt(wy, "wy");
			shaders.generateMesh.SetInt(wz, "wz");
			shaders.generateMesh.SetFloat(timeSpeed, "timeSpeed");
			shaders.generateMesh.SetInt(wWidth, "renderWidth");
			shaders.generateMesh.SetInt(wHeight, "renderHeight");
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


				int id = vv[i].y * (wx * wz) + vv[i].z * wx + vv[i].x;
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
			for (int i = 0; i < 5; ++i) {
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
				wWidth = graphicsSettings.resolutions[currentResolutionWindow].width;
				wHeight = graphicsSettings.resolutions[currentResolutionWindow].height;
				window.Resize(wWidth,wHeight);
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
#pragma omp for collapse(2)
		for (int nx = 0; nx < wx; nx++) {
			for (int nz = 0; nz < wz; nz++) {
				world.array[nx][nz] = true;
				float dist = Vector2Distance({ (float)nx * chunkSize + chunkSize / 2,(float)nz * chunkSize + chunkSize / 2 }, { float(viewPort.camera.position.x),float(viewPort.camera.position.z) });
				bool grass = false;

				if (dist < 1920) grass = true;


				int lod = 1;
				if (setLod != 0) {
					lod = setLod;
				}
				else {

					if (dist > 960) lod = 2;
					if (dist > 960 * 2) lod = 4;
					if (dist > 960 * 4) lod = 8;
					if (dist > 960 * 8) lod = 16;
					if (dist > 960 * 12) lod = 32;
					if (dist > 960 * 24) lod = 64;
				}
				GenerationData gen;
				gen.GenerateMountains(nx, nz);
				gen.highestChunkY += 2;

				for (int y = 0; y < wy; y++) {
					world.chunkData[y * (wx * wz) + nz * wx + nx].lod = 0;
				}
				for (int y = 0; y < gen.highestChunkY && y < wy; y++) {
					chunks[y * (wx * wz) + nz * wx + nx].GeneratePerType(selectedType,&world.chunkData[y * (wx * wz) + nz * wx + nx], gen, nx, y, nz, lod, grass);
				};
				int base = 0;
				int cy = gen.highestChunkY - 2;
				#define SET_VOXEL(x, y, z, block) do { \
					int _x = (x), _y = (y), _z = (z); \
					if (_x >= 0 && _x < 32 && _z >= 0 && _z < 32 && _y >= 0 && _y < wy * 32) { \
						ChunkData& _c = world.chunkData[(_y / 32) * (wx * wz) + nz * wx + nx]; \
						if (!_c.skipped && _c.terrain) _c.terrain[IDX(_x, _y % 32, _z, 32)] = (block); \
					} \
				} while (0)

				auto spawnTree = [&](int dx, int dy, int dz, int trunkHeight) {
					//trunk height should be at least 24
					const int firstTierHeight = 6;
					const int tierSpacing = 4;

					const int straight[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
					const int diagonal[4][2] = { {1,1}, {-1,1}, {1,-1}, {-1,-1} };

					for (int y = 0; y < trunkHeight; y++)
						SET_VOXEL(dx, dy + y, dz, WOOD);

					for (int tier = 0, h = firstTierHeight; h < trunkHeight - 1; h += tierSpacing, tier++) {
						const int (*dirs)[2] = (tier % 2 == 0) ? straight : diagonal;

						int length = 1 + (trunkHeight - h) * 6 / (trunkHeight - firstTierHeight);

						for (int i = 0; i < 4; i++) {
							int dirX = dirs[i][0], dirZ = dirs[i][1];
							int sideX = -dirZ, sideZ = dirX;

							for (int step = 1; step <= length; step++) {
								int x = dx + dirX * step;
								int z = dz + dirZ * step;
								int y = dy + h - step / 4;

								if (step <= length - 3) {
									SET_VOXEL(x, y, z, WOOD);
								}
								else {
									SET_VOXEL(x, y, z, LEAVES);
									SET_VOXEL(x + sideX, y, z + sideZ, LEAVES);
									SET_VOXEL(x - sideX, y, z - sideZ, LEAVES);
									SET_VOXEL(x, y + 1, z, LEAVES);
								}
							}

							SET_VOXEL(dx + dirX * length, dy + h - length / 4 - 1, dz + dirZ * length, LEAVES);
						}

						for (int i = 0; i < 4; i++)
							SET_VOXEL(dx + straight[i][0], dy + h + 1, dz + straight[i][1], LEAVES);
					}

					for (int i = 0; i < 4; i++) {
						SET_VOXEL(dx + straight[i][0], dy + trunkHeight - 2, dz + straight[i][1], LEAVES);
						SET_VOXEL(dx + straight[i][0], dy + trunkHeight - 1, dz + straight[i][1], LEAVES);
					}
					SET_VOXEL(dx, dy + trunkHeight, dz, LEAVES);
					SET_VOXEL(dx, dy + trunkHeight + 1, dz, LEAVES);
					};

				auto spawnRock = [&](int cx, int cy, int cz, int size) {
					float rx = size + Math::GetRandomValueStreamlined(0, size / 2);
					float rz = size + Math::GetRandomValueStreamlined(0, size / 2);
					float ry = size * 0.6f;
					cy -= (int)(ry * 0.4f);

					for (int y = -(int)ry; y <= (int)ry; y++) {
						for (int x = -(int)rx; x <= (int)rx; x++) {
							for (int z = -(int)rz; z <= (int)rz; z++) {
								float d = (x * x) / (rx * rx) + (y * y) / (ry * ry) + (z * z) / (rz * rz);
								float jitter = Math::GetRandomValueStreamlined(-25, 25) / 100.0f;
								if (d <= 0.75f || d <= 1.0f + jitter)
									SET_VOXEL(cx + x, cy + y, cz + z, STONE);
							}
						}
					}
					};
				if (GetRandomValue(0, 10) == 5 && selectedType==MOUNTAIN) {
					for (int i = 0; i < 3; i++) {
						int rx = Math::GetRandomValueStreamlined(8, 23);
						int rz = Math::GetRandomValueStreamlined(8, 23);
						int groundY = gen.heightMap[rx + rz * chunkSize];
						spawnRock(rx, groundY, rz, Math::GetRandomValueStreamlined(2, 5));
					}

				}
				if (selectedType == MOUNTAIN) {
					for (int i = 0; i < Math::GetRandomValueStreamlined(0, 4); i++) {
						int dx = Math::GetRandomValueStreamlined(10, 20);
						int dz = Math::GetRandomValueStreamlined(10, 20);
						int baseY = gen.heightMap[dx + dz * chunkSize];
						if (baseY > 250) continue;
						spawnTree(dx, baseY, dz, Math::GetRandomValueStreamlined(24, 48));

					}

				}
				
				for (int y = 0; y < gen.highestChunkY && y < wy; y++) {
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
		int byteMapCount = 0;

		Anubis::GPUONLYShortMap** shortMapBatch = new Anubis::GPUONLYShortMap * [maxCount];
		uint16_t** shortMapData = new uint16_t * [maxCount];
		Math::IVector3* shortMapSizes = new Math::IVector3[maxCount];
		int shortMapCount = 0;

		for (int nx = 0; nx < wx; nx++) {
			for (int nz = 0; nz < wz; nz++) {
				for (int ny = 0; ny < wy; ny++) {

					int neighbourId = ny * (wx * wz) + nz * wx + nx;
					if (chunks[neighbourId].lod == 0 || chunks[neighbourId].lod==3000) continue;

					int size = chunkSize / chunks[neighbourId].lod;

					if (world.chunkData[neighbourId].skipped ||
						world.chunkData[neighbourId].voxelCount >= chunkSize * chunkSize * chunkSize) {
						world.handle.handle[neighbourId].lod = 3000;
					}


					if (world.chunkData[neighbourId].voxelCount != 0 &&
						!world.chunkData[neighbourId].skipped) {

						if (chunks[neighbourId].shortenedData) {
							auto& map = world.chunkData[neighbourId].fluidByteMap;

							map.Clear({ size, size, size });

							byteMapBatch[byteMapCount] = &map;
							byteMapData[byteMapCount] = world.chunkData[neighbourId].compressedTerrain;
							byteMapSizes[byteMapCount] = { size, size, size };

							byteMapCount++;
						}
						else {
							auto& map = world.chunkData[neighbourId].fluidShortMap;

							map.Clear({ size, size, size });

							shortMapBatch[shortMapCount] = &map;
							shortMapData[shortMapCount] = chunks[neighbourId].DecompressChunk(
								&world.chunkData[neighbourId]
							);
							shortMapSizes[shortMapCount] = { size, size, size };

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

				for (int nx = 0; nx < wx; nx++) {
					for (int nz = 0; nz < wz; nz++) {
						for (int ny = 0; ny < wy; ny++) {
							int id = ny * (wx * wz) + nz * wx + nx;
							if (&world.chunkData[id].fluidByteMap == byteMapBatch[i]) {
								world.handle.handle[id].handle = byteMapBatch[i]->handle;
								world.handle.handle[id].lod = world.chunkData[id].lod;
							}
						}
					}
				}
			}
		}

		if (shortMapCount > 0) {
			UploadShortMaps(shortMapBatch, shortMapData, shortMapSizes, shortMapCount);

			for (int i = 0; i < shortMapCount; i++) {
				shortMapBatch[i]->GetBindlessHandle();

				for (int nx = 0; nx < wx; nx++) {
					for (int nz = 0; nz < wz; nz++) {
						for (int ny = 0; ny < wy; ny++) {
							int id = ny * (wx * wz) + nz * wx + nx;
							if (&world.chunkData[id].fluidShortMap == shortMapBatch[i]) {
								world.handle.handle[id].handle = shortMapBatch[i]->handle;
								world.handle.handle[id].lod = world.chunkData[id].lod;
							}
						}
					}
				}

				free(shortMapData[i]);
			}
		}
		delete[] byteMapBatch;
		delete[] shortMapBatch;
		delete[] shortMapSizes;
		delete[] byteMapSizes;
		END_TIMER(upload)
		shaders.display.UpdateSSBO(3, &world.handle, sizeof(world.handle), 0);
		RecalculateParticlesFullDebug();
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

			const float radius = 0.5f;

			for (size_t i = 0; i < physicalObjects.size(); i++) {
				auto& a = physicalObjects[i];
				int offx = ceil((a->size.x / 2) / chunkSize) + 2;
				int offy = ceil((a->size.y / 2) / chunkSize) + 2;
				int offz = ceil((a->size.z / 2) / chunkSize) + 2;
				for (int x = -offx; x <= offx; x++) {
					for (int y = -offy; y <= offy; y++) {
						for (int z = -offz; z <= offz; z++) {
							int chunkX = int(floor(a->position.x / chunkSize)) + x;
							int chunkY = int(floor(a->position.y / chunkSize)) + y;
							int chunkZ = int(floor(a->position.z / chunkSize)) + z;
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
			float t = 0.5f + 0.5f * sinf((float)GetTime() * 2.0f);
			t *= t;

			world.lightValues[0] = { 1.0f,  0.15f + 0.35f * t, 0.0f + 0.05f * t };
			world.lightRadiusList[0] = 90 * (0.75f + 0.35f * t);


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
			//viewPort.RenderPhysicalObjects(matView, pos, forward, ColorPallete, frame, physicalObjects, wWidth, wHeight);
			Math::Vector3 pp[2] = { viewPort.camera.target,{wx * 32,150,wy * 32} };
			double rasterTimeMs;
			int trianglesRendered = 0;
			Anubis::GpuTimer raster;
			raster.Start();
				
			viewPort.RasterizeMeshes(matView, pos, forward, ColorPallete, frame, triangleCount, wWidth, wHeight);
			trianglesRendered = triangleCount;
			std::cout << triangleCount << "\n";
			rasterTimeMs = raster.Stop();

			Anubis::GpuTimer grassT;
			grassT.Start();
			if (graphicsSettings.grassRender && grass > 0) {
				viewPort.RenderGrass(matView, pos, forward, frame, wWidth, wHeight, grass);
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
						if (dist > 960 * 24) lod = 64;
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
			int drd = 0;
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
									chunks[id].GenerateBitMap(&world.chunkData[id]);
									chunks[id].Compress(&world.chunkData[id]);

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
							if (chunks[id].lod != 0) {
								ChunkDeleteMeshData(dx, dy, dz);
								vv[drd] = { dx,dy,dz };
								drd++;
								uploadChunkSSBO(id);

							}
						}
						chunkCount = 0;
					}
					releasedMouseButton = false;


				}
			}
			else if (window.GetMouseButtonPressed(0)) {

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
								chunks[id].GenerateBitMap(&world.chunkData[id]);
								chunks[id].Compress(&world.chunkData[id]);

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
			const float halfAngle = tanf((160 * 0.5f) * DEG2RAD);
			float cosHalf = cosf(halfAngle);
			float sinHalf = sinf(halfAngle);
			Vector2 camPos2D = { viewPort.camera.position.x, viewPort.camera.position.z };
			Vector2 camTarget2D = { viewPort.camera.target.x,   viewPort.camera.target.z };
			Vector2 baseForward2D = { viewPort.camera.GetForward().x, viewPort.camera.GetForward().z };
			Vector2 right = { -baseForward2D.y, baseForward2D.x };
			float fLen = sqrtf(camTarget2D.x * camTarget2D.x + camTarget2D.y * camTarget2D.y);
			baseForward2D.x /= fLen;
			baseForward2D.y /= fLen;
			bool alwaysInFrustum = false;
			if (fLen < 0.001f) {
				alwaysInFrustum = true;
			}

			for (int i = 0; i < 2; i++) {
				float lastDistance = renderDistance*chunkSize*2;
				int chunkX = -1;
				int chunkZ = -1;
				for (int x = 0; x < wx; x++) {
					for (int z = 0; z < wz; z++) {
						Vector2 point = { x * chunkSize+ chunkSize/2,z * chunkSize + chunkSize /2};
						float dd = Vector2Distance(point, camPos2D);
						if (dd < renderDistance*chunkSize) {

							bool inFrustum = false;
							if (!alwaysInFrustum) {

								Vector2 toPoint = { point.x - viewPort.camera.position.x, point.y - viewPort.camera.position.z };

								float alongForward = toPoint.x * forward.x + toPoint.y * forward.y;
								float alongRight = toPoint.x * right.x + toPoint.y * right.y;

								float halfWidth = alongForward * halfAngle;

								if (fabsf(alongRight) <= halfWidth) {
									inFrustum = true;
								}
							}


							if (inFrustum || alwaysInFrustum) {

								if (!world.array[x][z]) {
									if (lastDistance > dd*chunkSize || chunkX == -1) {
										lastDistance = dd * chunkSize;
										chunkX = x;
										chunkZ = z;
									}
								}
							}
						}
					}
				}
				if (chunkX != -1) {
					int nz = chunkZ;
					int nx = chunkX;
					if (nz < 0 || nx < 0 || nx >= wx || nz >= wz) continue;

					if (!world.array[nx][nz]) {
						world.array[nx][nz] = true;
						world.setLod[nx][nz] = chunks[nz * wx + nx].lod;
						generateArrayId[generateCount++] = { nx,nz };
					}
				}
			}
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
				if (dist > 960 * 8) lod = 16;
				if (dist > 960 * 12) lod = 32;
				if (dist > 960 * 24) lod = 64;
				GenerationData gen;
				gen.GenerateMountains(nx, nz);
				gen.highestChunkY += 2;

				for (int y = 0; y < gen.highestChunkY && y < wy; y++) {
					chunks[y * (wx * wz) + nz * wx + nx].GeneratePerType(selectedType, &world.chunkData[y * (wx * wz) + nz * wx + nx], gen, nx, y, nz, lod, grass);
					RecalculateParticles(nx,y,nz);
				};
				for (int y = 0; y < gen.highestChunkY && y < wy; y++) {
					chunks[y * (wx * wz) + nz * wx + nx].GenerateLod(&world.chunkData[y * (wx * wz) + nz * wx + nx], nx, y, nz);
					chunks[y * (wx * wz) + nz * wx + nx].Compress(&world.chunkData[y * (wx * wz) + nz * wx + nx]);
				};
				free(gen.heightMap);
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
					if (world.chunkData[neighbourId].voxelCount != 0 && !world.chunkData[neighbourId].skipped)
					{

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
			for (int i = 0; i < 2; i++) {
				for (int x = 0; x < wx; x++) {
					for (int z = 0; z < wz; z++) {
						if (world.array[x][z]) {
							bool neighborsLoaded =
								x > 0 && x < wx - 1 && z > 0 && z < wz - 1 &&
								world.array[x - 1][z] && world.array[x + 1][z] && world.array[x][z - 1] && world.array[x][z + 1];
							if (neighborsLoaded) {

								for (int ny = 0; ny < wy; ny++) {
									int id = ny * (wx * wz) + z * wx + x;
									if (!world.chunkData[id].updated && world.chunkData[id].voxelCount != 0) {

										vv[drd].x = chunks[id].gx;
										vv[drd].y = chunks[id].gy;
										vv[drd].z = chunks[id].gz;

										if (world.chunkData[id].voxelCount != 0 && !world.chunkData[id].skipped) {
											world.chunkData[id].updated = true;
											drd++;
										}
									}
								}
							}
						}

					}
				}
			}
			if (drd > 0 && !viewPort.raytracePrimaryRays) {
				shaders.display.UpdateSSBO(10, vv, drd * sizeof(Math::IVector3), 0);

				auto batchUploadEnd = std::chrono::high_resolution_clock::now();
				Anubis::GpuTimer mesh;
				mesh.Start();
				Anubis::SwitchShaderStage(shaders.generateMesh.programID);
				shaders.generateMesh.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
				shaders.generateMesh.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
				shaders.generateMesh.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
				shaders.generateMesh.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
				shaders.generateMesh.SetInt(wx, "wx");
				shaders.generateMesh.SetInt(wy, "wy");
				shaders.generateMesh.SetInt(wz, "wz");
				shaders.generateMesh.SetFloat(timeSpeed, "timeSpeed");
				shaders.generateMesh.SetInt(wWidth, "renderWidth");
				shaders.generateMesh.SetInt(wHeight, "renderHeight");
				shaders.generateMesh.SetInt(frame, "frameIndex");
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
					

					int id = vv[i].y * (wx * wz) + vv[i].z * wx + vv[i].x;
					chunks[id].beginningTri = begin[i];
					chunks[id].endTri = end[i];
				}
			}
			offfr++;
			if (window.GetMouseButtonPressed(1) && offfr>5) {
				offfr = 0;
				Math::Vector3 angularVelocity = Math::NormalizeVector3(Math::SubtractVector3(viewPort.camera.target, viewPort.camera.position));
				angularVelocity.x *= 4;
				angularVelocity.y *= 4;
				angularVelocity.z *= 4;
				auto pp = std::make_unique<PhysicsObject>();
				pp->values = (uint16_t*)MemAlloc(16 * 16 * 16 * sizeof(uint16_t));
				pp->visibility = (bool*)MemAlloc(16 * 16 * 16 * sizeof(bool));
				pp->position = viewPort.camera.target;
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
				viewPort.SecondaryRays(matView, pos, forward, ColorPallete, frame, timeSpeed,wWidth,wHeight, &world);
			}
			auto secondaryRaysMs = secondaryRays.Stop();
			if (graphicsSettings.renderClouds) {
				viewPort.RenderClouds(matView, frame, pos, forward, wWidth,wHeight,timeSpeed);
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
			
			shaders.display.Use();
			shaders.display.SetInt(frame, "frameIndex");

			shaders.display.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shaders.display.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shaders.display.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shaders.display.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
			shaders.display.SetInt(wx, "wx");
			shaders.display.SetInt(wy, "wy");
			shaders.display.SetInt(wz, "wz");
			shaders.display.SetUIntArray(ColorPallete.data(), ColorPallete.size(), "packedColors");
			shaders.display.SetInt(wHeight, "resY");
			shaders.display.SetVector4({ 1.0f, 1.0f, 1.0f, 1.0f }, "color");
			Anubis::Quad quad;
			quad.Create();
			window.DrawQuad(quad);
			particleRenderer.Update();
			particleRenderer.Render(&shaders, matView, matProj, forward, pos, wWidth, wHeight, frame);
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
						matProj = MatrixPerspective(90 * DEG2RAD, ((double)wWidth / (double)wHeight), 0.1, 100000.0);
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
				ImGui::SliderInt("Render Distance", &renderDistance, 1, 1000);

				CustomToggle("Render grass", &graphicsSettings.grassRender);
				CustomToggle("Hardware/Software rendering", &graphicsSettings.hardwareSoftwareRendering);
				CustomToggle("Secondary rays", &graphicsSettings.secondaryRays);

				if (ImGui::Button("Recompile shaders")) {
					shaders.Load();
				}
				CustomToggle("God mode", &graphicsSettings.godMode);
				CustomToggle("Simulate Physics", &graphicsSettings.simulatePhysics);
				CustomToggle("Render Fluids", &graphicsSettings.renderFluids);
				CustomToggle("Render Clouds", &graphicsSettings.renderClouds);
				CustomToggle("Raytrace/Rasterize", &viewPort.raytracePrimaryRays);

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
			std::cout << viewPort.camera.GetForward().x << " " << viewPort.camera.GetForward().y << " " << viewPort.camera.GetForward().z << "\n";
			std::cout << window.getFPS() << "\n";
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

			double elapsed = glfwGetTime() - frameStart;
			double remaining = (1.0 / 150.0) - elapsed;
			if (remaining > 0.0) {
				std::this_thread::sleep_for(std::chrono::duration<double>(remaining));
			}
			frame = frame % 6;
			frame++;
		}
	}
};
int main() {
	
	App* app = new App;
	app->MainMenu();

}