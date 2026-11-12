#pragma once

#include "Anubis/Anubis.hpp"
#include "Anubis/AnubisShaders.hpp"
#include "Anubis/AnubisMath.hpp"
#include <sys/stat.h> 

enum VOXEL_ENUM_NAMES {
	GRASS = 1,   // palette[0]
	STONE = 2,   // palette[1]
	DIRT = 3,   // palette[2]
	WOOD = 4,   // palette[3]
	LEAVES = 5,   // palette[4]
	SNOW = 6,   // palette[5]
	WATER = 7,   // palette[6]
	SAND = 9,   // palette[8]
	OLIVE = 10,   // palette[9]
	LIGHT_BARRIER = 255
};
struct ShaderManager {
	Anubis::DisplayShader display;
	Anubis::DisplayShader hwRender;
	Anubis::DisplayShader hwRenderForObjects;
	Anubis::ComputeShader rasterizer;
	Anubis::ComputeShader frustumCull;
	Anubis::ComputeShader hizBuilder;
	Anubis::ComputeShader meshletRenderer;
	Anubis::ComputeShader grassRenderer;
	Anubis::ComputeShader secondaryRaysRender;
	Anubis::ComputeShader generateMesh;
	Anubis::ComputeShader clouds;
	Anubis::ComputeShader bilboards;
	Anubis::ComputeShader primaryRays;
	Anubis::ComputeShader prepassPrimaryRays;
	Anubis::ComputeShader rasterizeAnimatedObjects;
	Anubis::ComputeShader setAnimatedObjects;
	Anubis::ComputeShader clearAnimatedObjects;
	void Load() {

		std::string shaderDir = "";
		struct stat info;
		if (stat("shaders", &info) == 0 && (info.st_mode & S_IFDIR)) {
			shaderDir = "shaders/";
			std::cout << "Found shader dir\n";
		}

		auto path = [&](const std::string& name) -> std::string {
			return shaderDir + name;
			};

		display.LoadFromFile(
			path("quad_display_shader.fs").c_str(),
			"Anubis/BaseShaders/quad_display_shader.vs"
		);
		primaryRays.Setup(path("PrimaryRays.glsl").c_str());
		rasterizer.Setup(path("Rasterize.glsl").c_str());
		frustumCull.Setup(path("frustumCull.glsl").c_str());
		hizBuilder.Setup(path("HizBuilder.glsl").c_str());
		meshletRenderer.Setup(path("renderAABB.glsl").c_str());
		grassRenderer.Setup(path("grassParticle.glsl").c_str());
		hwRender.LoadFromFile(path("fes.fs").c_str(), path("ver.vs").c_str());
		hwRenderForObjects.LoadFromFile(path("hwMesh.fs").c_str(), path("hwMesh.vs").c_str());
		secondaryRaysRender.Setup(path("SecondaryRays.glsl").c_str());
		generateMesh.Setup(path("GenerateMesh.glsl").c_str());
		clouds.Setup(path("Clouds.glsl").c_str());
		bilboards.Setup(path("bilboard.glsl").c_str());
		prepassPrimaryRays.Setup(path("PrePassPrimary.glsl").c_str());
		rasterizeAnimatedObjects.Setup(path("RasterizeAnimatedObjects.glsl"));
		setAnimatedObjects.Setup(path("SetAnimatedData.glsl"));
		clearAnimatedObjects.Setup(path("ClearAnimatedData.glsl"));
	}

};
const inline Matrix GetStaticMVPMatrix(Vector3 position, Matrix matView) {
	const float aspect = (float)3840 / (float)2160;
	Matrix matModel = MatrixTranslate(position.x, position.y, position.z);
	Matrix matViewProj = MatrixMultiply(matView, matProj);
	return MatrixMultiply(matModel, matViewProj);
}
void Render(ShaderManager* shaders, Anubis::Camera camera, Matrix matView, Anubis::HWVIrGeomMesh& renderSphere, Vector3 vv, int frame, int voxelType, int size, uint32_t* pack, int packCount, int vHeight) {

	Anubis::SwitchShaderStage(shaders->hwRender.shaderId);

	Matrix m = GetStaticMVPMatrix(vv, matView);
	shaders->hwRender.SetVector3(camera.position, "camPos");
	Math::Vector3 camFwd = Math::NormalizeVector3(
		Math::SubtractVector3(camera.target, camera.position)
	);
	shaders->hwRender.SetVector3(camFwd, "camForward");
	shaders->hwRender.SetUIntArray(pack, packCount, "packedColors");
	shaders->hwRender.SetFloat(size, "size");
	shaders->hwRender.SetInt(frame, "frame");
	shaders->hwRender.SetInt(voxelType, "voxelType");
	shaders->hwRender.SetInt(vHeight, "vHeight");
	shaders->hwRender.SetVector3({ 1,0,1 }, "colorr");
	shaders->hwRender.SetMatrix({ m.m0 ,m.m1,m.m2,m.m3,m.m4,m.m5,m.m6,m.m7,m.m8,m.m9,m.m10,m.m11,m.m12,m.m13,m.m14,m.m15 }, "MVP");
	shaders->hwRender.Use();

	renderSphere.Draw();
}
struct Settings {
	struct Resolution {
		int width;
		int height;
		std::string name;
	};
	bool renderClouds = true;
	bool grassRender = true;
	bool secondaryRays = true;
	bool simulatePhysics = true;
	bool renderFluids = true;
	bool globalIllumination = false;
	bool godMode = false;
	bool renderResolution = false;
	bool hardwareSoftwareRendering = true;
	bool FXAA = false;
	Resolution resolutions[10] = {
		{640,480,"SD.480p"},
		{1280,720,"HD/720p"},
		{1920,1080,"FHD/1080p"},
		{2560,1440, "QHD/1440P"},
		{3840,2160, "UHD/4k"}
	};
};

