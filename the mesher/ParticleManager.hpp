#pragma once
#include "raymath.h"
#include "Chunk.hpp"

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
	Particle* particles;
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

	void Render(ShaderManager* shaders, Matrix matView, Matrix matProj, Vector3 forward, Vector3 pos, int wWidth, int wHeight, int frameIndex) {
		Math::Vector3 billboardPoses[1024];
		int renderPassAmount = 0;
		for (int i = 0; i < renderQueue; i++) {
			billboardPoses[renderPassAmount] = particles[i].position;
			renderPassAmount++;
			if (renderPassAmount == 1024 - 1) {

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

			shaders->bilboards.Use(renderQueue / 64, 1, 1);
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