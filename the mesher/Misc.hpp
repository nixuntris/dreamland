#pragma once

#include "Anubis/Anubis.hpp"
#include "Anubis/AnubisShaders.hpp"
#include "Anubis/AnubisMath.hpp"
struct ShaderManager {
	Anubis::DisplayShader display;
	Anubis::DisplayShader displayNoFXAA;
	Anubis::DisplayShader hwRender;
	Anubis::DisplayShader hwRenderForObjects;
	Anubis::ComputeShader rasterizer;
	Anubis::ComputeShader frustumCull;
	Anubis::ComputeShader hizBuilder;
	Anubis::ComputeShader meshletRenderer;
	Anubis::ComputeShader grassRenderer;
	Anubis::ComputeShader secondaryRaysRender;
	Anubis::ComputeShader fluidRender;
	Anubis::ComputeShader fluidSim;
	Anubis::ComputeShader fillOut;
	Anubis::ComputeShader clouds;
	Anubis::ComputeShader bilboards;

	Anubis::ComputeShader renderPhysicalObjects;
	Anubis::ComputeShader clearPhysicalObjects;
	void Load() {
		display.LoadFromFile("quad_display_shader.fs", "Anubis/BaseShaders/quad_display_shader.vs");
		displayNoFXAA.LoadFromFile("quad_display_no_fxaa.glsl", "Anubis/BaseShaders/quad_display_shader.vs");
		rasterizer.Setup("Rasterize.glsl");
		frustumCull.Setup("frustumCull.glsl");
		hizBuilder.Setup("HizBuilder.glsl");
		meshletRenderer.Setup("renderAABB.glsl");
		grassRenderer.Setup("grassParticle.glsl");
		hwRender.LoadFromFile("fes.fs", "ver.vs");
		hwRenderForObjects.LoadFromFile("hwMesh.fs", "hwMesh.vs");
		secondaryRaysRender.Setup("SecondaryRays.glsl");
		fluidRender.Setup("RenderFluids.glsl");
		fluidSim.Setup("SimulateFluids.glsl");
		fillOut.Setup("FillOutPhysicsData.glsl");
		renderPhysicalObjects.Setup("PhysicsObject.glsl");
		clearPhysicalObjects.Setup("ClearPhysicalObject.glsl");
		clouds.Setup("Clouds.glsl");
		bilboards.Setup("bilboard.glsl");
	}

};
inline Matrix GetStaticMVPMatrix(Vector3 position, Matrix matView) {
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

