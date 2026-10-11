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
uniform vec3 planeNormals[6];
uniform float planeDs[6];

const float halfW = 1920.0 / 2.0;
const float halfH = 1080.0 / 2.0;
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

bool GetWorldToScreenClipped(vec3 position, out vec2 outScreen)
{
    vec4 clipPos;

    clipPos.x = (matView[0][0] * position.x + matView[2][0] * position.z + matView[3][0]) * matProj[0][0];
    clipPos.y = (matView[0][1] * position.x + matView[1][1] * position.y + matView[2][1] * position.z + matView[3][1]) * matProj[1][1];
    clipPos.z = (matView[0][2] * position.x + matView[1][2] * position.y + matView[2][2] * position.z + matView[3][2]);
    clipPos.w = matProj[2][3] * clipPos.z;
    clipPos.z = matProj[2][2] * clipPos.z + matProj[3][2] * matView[3][3];

    if (clipPos.w <= 0.0)
        return false;

    vec3 ndc = vec3(
        clipPos.x / clipPos.w,
        -clipPos.y / clipPos.w,
        clipPos.z / clipPos.w
    );

    outScreen = vec2(
        (ndc.x + 1.0) * halfW,
        (ndc.y + 1.0) * halfH
    );

    return true;
}
void RenderAABBOutline(vec3 bMin, vec3 bMax, uint color)
{
    vec3 corners[8];

    corners[0] = vec3(bMin.x, bMin.y, bMin.z);
    corners[1] = vec3(bMax.x, bMin.y, bMin.z);
    corners[2] = vec3(bMax.x, bMax.y, bMin.z);
    corners[3] = vec3(bMin.x, bMax.y, bMin.z);

    corners[4] = vec3(bMin.x, bMin.y, bMax.z);
    corners[5] = vec3(bMax.x, bMin.y, bMax.z);
    corners[6] = vec3(bMax.x, bMax.y, bMax.z);
    corners[7] = vec3(bMin.x, bMax.y, bMax.z);

    vec2 screen[8];
    bool visible[8];

    for (int i = 0; i < 8; i++)
        visible[i] = GetWorldToScreenClipped(corners[i], screen[i]);

    // 12 edges
    int edges[24] = int[24](
        0,1, 1,2, 2,3, 3,0,
        4,5, 5,6, 6,7, 7,4,
        0,4, 1,5, 2,6, 3,7
    );

    for (int i = 0; i < 24; i += 2)
    {
        int a = edges[i];
        int b = edges[i+1];
        if (visible[a]) {
            for (int x = 0; x < 2; x++) {
                for (int y = 0; y < 2; y++) {
                
                    int pixelIdx = int(screen[a].y+y) * 1920 + int(screen[a].x+x);
                    if (int(screen[a].y+y)>0 && int(screen[a].y+y)<1080 && int(screen[a].x+x)>0 && int(screen[a].x+x)<1920) {
                
                        frame[pixelIdx] = color;
                    }
                }
            }
        }
        if (visible[b]) {
            for (int x = 0; x < 2; x++) {
                for (int y = 0; y < 2; y++) {
                
                    int pixelIdx = int(screen[b].y+y) * 1920 + int(screen[b].x+x);
                    if (int(screen[a].y+y)>0 && int(screen[a].y+y)<1080 && int(screen[a].x+x)>0 && int(screen[a].x+x)<1920) {
                        frame[pixelIdx] = color;
                    }
                }
            }
        }
    }
}

uint packRGBA8(vec4 color)
{
   vec4 c = clamp(color, 0.0, 1.0);
   uvec4 bytes = uvec4(c * 255.0 + 0.5);

   return
        ( bytes.r        ) |
        ( bytes.g <<  8 ) |
        ( bytes.b << 16 ) |
        ( bytes.a << 24 );
}

vec4 debugColors[12] = vec4[](
    vec4(1.0, 0.0, 0.0, 1.0), // Red
    vec4(0.0, 1.0, 0.0, 1.0), // Green
    vec4(0.0, 0.0, 1.0, 1.0), // Blue
    vec4(1.0, 1.0, 0.0, 1.0), // Yellow
    vec4(1.0, 0.0, 1.0, 1.0), // Magenta
    vec4(0.0, 1.0, 1.0, 1.0), // Cyan
    vec4(1.0, 0.5, 0.0, 1.0), // Orange
    vec4(0.5, 0.0, 1.0, 1.0), // Purple
    vec4(0.5, 0.5, 0.5, 1.0), // Grey
    vec4(0.0, 0.5, 0.0, 1.0), // Dark Green
    vec4(0.5, 0.0, 0.0, 1.0), // Dark Red
    vec4(0.0, 0.0, 0.5, 1.0)  // Dark Blue
);

void main() {
    
    int idx = int(gl_GlobalInvocationID.x);
    int baseAABB = int(gl_GlobalInvocationID.x)*6;
    vec3 near = vec3(AABBCorners[baseAABB+0],AABBCorners[baseAABB+1],AABBCorners[baseAABB+2]);
    vec3 far = vec3(AABBCorners[baseAABB+3],AABBCorners[baseAABB+4],AABBCorners[baseAABB+5]);
    
    vec2 p1, p2, p3,p4;
    
    vec3 centre = (near+far)/2.0f;
    vec3 relativePos = centre - camPos; 

    float projection = dot(relativePos, camForward);
    if (projection < 0.0f) return;
    if (distance(camPos,centre)>1000) {
        RenderAABBOutline(near, far, packRGBA8(debugColors[0]));

    }
    else {
        RenderAABBOutline(near, far, packRGBA8(debugColors[1]));

    }
}