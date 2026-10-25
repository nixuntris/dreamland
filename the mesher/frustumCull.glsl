#version 460

layout (local_size_x = 32, local_size_y = 1, local_size_z = 1) in;

uniform vec3 camPos;
uniform vec3 camForward;



layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
};

layout(std430, binding = 2) buffer VerticesBuffer {
    float vertices[];
};

layout(std430, binding = 5) buffer BoolBuffer {
    int triangleAtomic;
    int triangleActive[];
};
layout(std430, binding = 7) buffer AABBBuffer {
    float AABBCorners[];
};
layout(std430, binding = 9) buffer MeshletCounter {
    int meshletCounter;
    int chunkBeginning[1024*1024];
    int chunkEnd[1024*1024];
};

uniform vec3 planeNormals[6];
uniform float planeDs[6];

const float halfW = 3840.0 / 2.0;
const float halfH = 2160.0 / 2.0;
const float reprocOfhalfW = 1.0 / halfW;
const float reprocOfhalfH = 1.0 / halfH;
uniform mat4 matView;
uniform mat4 matProj;
bool TriangleInFrustum(vec3 v1, vec3 v2, vec3 v3) {
	for (int i = 0; i < 6; i++)
	{
		if ((planeNormals[i].x * v2.x + planeNormals[i].y * v2.y + planeNormals[i].z * v2.z) + planeDs[i] < 0 &&
			(planeNormals[i].x * v1.x + planeNormals[i].y * v1.y + planeNormals[i].z * v1.z) + planeDs[i] < 0 &&
			(planeNormals[i].x * v3.x + planeNormals[i].y * v3.y + planeNormals[i].z * v3.z) + planeDs[i] < 0) {
			return false;
		}
	}
	return true;
}
bool IsCubeInFrustum(vec3 bMin, vec3 bMax) {
    for (int i = 0; i < 6; i++) {
        vec3 p = bMin;

        if (planeNormals[i].x >= 0.0) p.x = bMax.x;
        if (planeNormals[i].y >= 0.0) p.y = bMax.y;
        if (planeNormals[i].z >= 0.0) p.z = bMax.z;

        if (dot(planeNormals[i], p) + planeDs[i] < 0.0) {
            return false;
        }
    }
    return true;
}
float edge(vec2 a, vec2 b, vec2 p) {
    return (p.x - a.x) * (b.y - a.y) -
    (p.y - a.y) * (b.x - a.x);
}
bool isTopLeft(vec2 a, vec2 b) {
    vec2 e = b - a;
    return (e.y > 0.0) || (e.y == 0.0 && e.x < 0.0);
}

void main() {
    
    int idx = int(gl_GlobalInvocationID.x);
    int baseAABB = int(gl_GlobalInvocationID.x)*6;
    vec3 near = vec3(AABBCorners[baseAABB+0],AABBCorners[baseAABB+1],AABBCorners[baseAABB+2]);
    vec3 far = vec3(AABBCorners[baseAABB+3],AABBCorners[baseAABB+4],AABBCorners[baseAABB+5]);
    
    vec3 centre = (near+far)/2.0f;
    vec3 relativePos = centre - camPos; 
       
    //float projection = dot(relativePos, camForward); //also distance to camera
    
    //if (projection < 0.0f) return;
    
    if (IsCubeInFrustum(near,far)) {
            int triangleFrustumedId = atomicAdd(triangleAtomic,1);
            triangleActive[triangleFrustumedId] = idx;
        
    }
}