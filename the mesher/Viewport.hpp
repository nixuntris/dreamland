#pragma once
#include "Chunk.hpp"
#include "Misc.hpp"
#include "Anubis/AnubisMath.hpp"
#include "Anubis/Anubis.hpp"
#include "SoftwareRasterizer.h"
struct ViewPort {
	ShaderManager* shaders;

	Plane planes[6];
	Math::Vector3 planeNormals[6];
	Anubis::Camera camera;
	float planeDs[6];
	bool raytracePrimaryRays = false;
	float renderPercentage = 0.8f;
	void Display(int frame, int wHeight) {
		shaders->display.Use();
		shaders->display.SetInt(frame, "frameIndex");
		shaders->display.SetInt(wHeight, "resY");
		shaders->display.SetFloat(renderPercentage, "renderPercentage");
		shaders->display.SetVector4({ 1.0f, 1.0f, 1.0f, 1.0f }, "color");

	}
	inline void FrustumCull(Matrix matView, Vector3 pos, Vector3 forward, int totalMeshletCount) {

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
	inline void RenderClouds(Matrix matView, int frame, Vector3 pos, Vector3 forward, Math::IVector2 resolution, float timeSpeed) {
		Anubis::SwitchShaderStage(shaders->clouds.programID);
		shaders->clouds.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
		shaders->clouds.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
		shaders->clouds.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
		shaders->clouds.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
		shaders->clouds.SetInt(wx, "wx");
		shaders->clouds.SetInt(wy, "wy");
		shaders->clouds.SetInt(wz, "wz");
		shaders->clouds.SetFloat(timeSpeed, "timeSpeed");
		shaders->clouds.SetInt(resolution.width * renderPercentage, "renderWidth");
		shaders->clouds.SetInt(resolution.height * renderPercentage, "renderHeight");
		shaders->clouds.SetInt(frame, "frameIndex");
		shaders->clouds.Use(resolution.width / 32, resolution.height / 32, 1);
	}
	inline void SecondaryRays(int sampleCount, Matrix matView, Vector3 pos, Vector3 forward, const std::vector<uint32_t> palette, int frame, float timeSpeed, Math::IVector2 resolution, World* world) {

		Anubis::SwitchShaderStage(shaders->secondaryRaysRender.programID);
		shaders->secondaryRaysRender.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
		shaders->secondaryRaysRender.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
		shaders->secondaryRaysRender.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
		shaders->secondaryRaysRender.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
		shaders->secondaryRaysRender.SetInt(wx, "wx");
		shaders->secondaryRaysRender.SetInt(wy, "wy");
		shaders->secondaryRaysRender.SetInt(wz, "wz");

		shaders->secondaryRaysRender.SetFloat(timeSpeed, "timeSpeed");
		shaders->secondaryRaysRender.SetInt(resolution.width * renderPercentage, "renderWidth");
		shaders->secondaryRaysRender.SetInt(resolution.height * renderPercentage, "renderHeight");
		shaders->secondaryRaysRender.SetInt(sampleCount, "sampleCount");
		shaders->secondaryRaysRender.SetInt(frame, "frameIndex");
		shaders->secondaryRaysRender.Use((resolution.width / 32) * renderPercentage, (resolution.height / 32) * renderPercentage, 1);

		shaders->secondaryRaysRender.SetUIntArray(palette.data(), palette.size(), "packedColors");
	}
	inline void RasterizeMeshes(Matrix matView, Vector3 pos, Vector3 forward, const std::vector<uint32_t> palette, int frame, int triangleCount, Math::IVector2 resolution) {
		if (raytracePrimaryRays) {

			Anubis::SwitchShaderStage(shaders->prepassPrimaryRays.programID);
			shaders->prepassPrimaryRays.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shaders->prepassPrimaryRays.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shaders->prepassPrimaryRays.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shaders->prepassPrimaryRays.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
			shaders->prepassPrimaryRays.SetInt(wx, "wx");
			shaders->prepassPrimaryRays.SetInt(wy, "wy");
			shaders->prepassPrimaryRays.SetInt(wz, "wz");
			shaders->prepassPrimaryRays.SetInt(resolution.width / 4 * renderPercentage, "renderWidth");
			shaders->prepassPrimaryRays.SetInt(resolution.height / 4 * renderPercentage, "renderHeight");
			shaders->prepassPrimaryRays.SetInt(frame, "frameIndex");
			shaders->prepassPrimaryRays.SetInt(0, "offset");

			shaders->prepassPrimaryRays.Use((resolution.width / 32 / 4) * renderPercentage, (resolution.height / 4 / 16) * renderPercentage, 1);

			Anubis::SwitchShaderStage(shaders->primaryRays.programID);
			shaders->primaryRays.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
			shaders->primaryRays.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
			shaders->primaryRays.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
			shaders->primaryRays.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
			shaders->primaryRays.SetInt(wx, "wx");
			shaders->primaryRays.SetInt(wy, "wy");
			shaders->primaryRays.SetInt(wz, "wz");
			shaders->primaryRays.SetUIntArray(palette.data(), palette.size(), "packedColors");
			shaders->primaryRays.SetInt(resolution.width * renderPercentage, "renderWidth");
			shaders->primaryRays.SetInt(resolution.height * renderPercentage, "renderHeight");
			shaders->primaryRays.SetInt(frame, "frameIndex");
			shaders->primaryRays.SetInt(0, "offset");

			shaders->primaryRays.Use((resolution.width / 32) * renderPercentage, (resolution.height / 16) * renderPercentage, 1);

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
			shaders->rasterizer.SetInt(resolution.width * renderPercentage, "renderWidth");
			shaders->rasterizer.SetInt(resolution.height * renderPercentage, "renderHeight");
			shaders->rasterizer.SetInt(frame, "frameIndex");
			shaders->rasterizer.SetInt(0, "offset");

			shaders->rasterizer.Use(floor(triangleCount), 1, 1);
		}

	}
	inline void RenderGrass(Matrix matView, Vector3 pos, Vector3 forward, int frame, Math::IVector2 resolution, int grass) {

		Anubis::SwitchShaderStage(shaders->grassRenderer.programID);
		shaders->grassRenderer.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
		shaders->grassRenderer.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
		shaders->grassRenderer.SetInt(resolution.width * renderPercentage, "renderWidth");
		shaders->grassRenderer.SetInt(resolution.height * renderPercentage, "renderHeight");
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
	const inline void RenderAnimatedObject(Matrix matView, AnimatedObject* object, Vector3 pos, Vector3 forward, int frame, const std::vector<uint32_t> palette, Math::IVector2 resolution, StructureManager* structureManager) {
		Anubis::SwitchShaderStage(shaders->rasterizeAnimatedObjects.programID);
		shaders->rasterizeAnimatedObjects.SetVector3({ pos.x,pos.y,pos.z }, "camPos");
		shaders->rasterizeAnimatedObjects.SetVector3({ forward.x,forward.y,forward.z }, "camForward");
		shaders->rasterizeAnimatedObjects.SetMatrix({ matView.m0,matView.m1,matView.m2,matView.m3,matView.m4,matView.m5,matView.m6,matView.m7,matView.m8,matView.m9,matView.m10,matView.m11,matView.m12,matView.m13,matView.m14,matView.m15 }, "matView");
		shaders->rasterizeAnimatedObjects.SetMatrix({ matProj.m0,matProj.m1,matProj.m2,matProj.m3,matProj.m4,matProj.m5,matProj.m6,matProj.m7,matProj.m8,matProj.m9,matProj.m10,matProj.m11,matProj.m12,matProj.m13,matProj.m14,matProj.m15 }, "matProj");
		shaders->rasterizeAnimatedObjects.SetInt(wx, "wx");
		shaders->rasterizeAnimatedObjects.SetInt(wy, "wy");
		shaders->rasterizeAnimatedObjects.SetInt(wz, "wz");
		shaders->rasterizeAnimatedObjects.SetUIntArray(palette.data(), palette.size(), "packedColors");
		shaders->rasterizeAnimatedObjects.SetInt(resolution.width * renderPercentage, "renderWidth");
		shaders->rasterizeAnimatedObjects.SetInt(resolution.height * renderPercentage, "renderHeight");
		shaders->rasterizeAnimatedObjects.SetInt(frame, "frameIndex");
		shaders->rasterizeAnimatedObjects.SetInt(0, "offset");
		StructureDataHolder* data = structureManager->structureList[object->objectType].GetSpecificVariant(object->currentAnimationTick);
		shaders->rasterizeAnimatedObjects.SetUVec2(data->map.handle, "handle");
		shaders->rasterizeAnimatedObjects.SetVector3({ object->position.x - data->map.size.x / 2,object->position.y - data->map.size.y / 2,object->position.z - data->map.size.z / 2 }, "renderPosition");
		shaders->rasterizeAnimatedObjects.Use(data->map.size.x / 4, data->map.size.y / 4, data->map.size.z / 4);
	}
	const inline void ClearAnimatedObjectData(AnimatedObject* object, StructureManager* structureManager) {
		Anubis::SwitchShaderStage(shaders->clearAnimatedObjects.programID);
		shaders->clearAnimatedObjects.SetInt(wx, "wx");
		shaders->clearAnimatedObjects.SetInt(wy, "wy");
		shaders->clearAnimatedObjects.SetInt(wz, "wz");
		StructureDataHolder* data = structureManager->structureList[object->objectType].GetSpecificVariant(object->currentAnimationTick);
		shaders->clearAnimatedObjects.SetUVec2(data->map.handle, "handle");
		shaders->clearAnimatedObjects.SetVector3({ object->position.x - data->map.size.x / 2,object->position.y - data->map.size.y / 2,object->position.z - data->map.size.z / 2 }, "renderPosition");
		shaders->clearAnimatedObjects.Use(data->map.size.x / 4, data->map.size.y / 4, data->map.size.z / 4);
	}
	const inline void SetAnimatedObjectData(AnimatedObject* object, StructureManager* structureManager) {
		Anubis::SwitchShaderStage(shaders->setAnimatedObjects.programID);
		shaders->setAnimatedObjects.SetInt(wx, "wx");
		shaders->setAnimatedObjects.SetInt(wy, "wy");
		shaders->setAnimatedObjects.SetInt(wz, "wz");
		StructureDataHolder* data = structureManager->structureList[object->objectType].GetSpecificVariant(object->currentAnimationTick);
		shaders->setAnimatedObjects.SetUVec2(data->map.handle, "handle");
		shaders->setAnimatedObjects.SetVector3({ object->position.x - data->map.size.x / 2,object->position.y - data->map.size.y / 2,object->position.z - data->map.size.z / 2 }, "renderPosition");
		shaders->setAnimatedObjects.Use(data->map.size.x / 4, data->map.size.y / 4, data->map.size.z / 4);
	}
};