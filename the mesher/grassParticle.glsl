#version 460

#extension GL_ARB_gpu_shader_int64 : require
#extension GL_NV_gpu_shader5 : enable
#extension GL_ARB_bindless_texture : require
layout (local_size_x = 64, local_size_y = 1, local_size_z = 1) in;


layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
};

layout(std430, binding = 20) buffer PointMap {
    float particlePoint[];
};
layout(std430, binding = 11) buffer Colors {
    uint colors[];
};

uniform int renderWidth;
uniform int renderHeight;
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

float halfW = float(renderWidth) / 2.0;
float halfH = float(renderHeight) / 2.0;
float reprocOfhalfW = 1.0 / halfW;
float reprocOfhalfH = 1.0 / halfH;

uniform int wx,wy,wz;
uniform vec3 planeNormals[6];
uniform float planeDs[6];
uniform vec3 camPos;
uniform vec3 camForward;
uniform mat4 matView;
uniform mat4 matProj;
uniform int frameIndex;

vec4 unpackRGBA8(uint packedr)
{
    return vec4(
        float( packedr        & 0xFFu),
        float((packedr >>  8) & 0xFFu),
        float((packedr >> 16) & 0xFFu),
        float((packedr >> 24) & 0xFFu)
    ) * (1.0 / 255.0);
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
float edge(vec2 a, vec2 b, vec2 p) {
    return (p.x - a.x) * (b.y - a.y) -
    (p.y - a.y) * (b.x - a.x);
}
bool isTopLeft(vec2 a, vec2 b) {
    vec2 e = b - a;
    return (e.y > 0.0) || (e.y == 0.0 && e.x < 0.0);
}
void DrawQuadSoftwareCS(
    vec2 v0,
    vec2 v1,
    vec2 v2,
    vec2 v3,
    uint color,
    int quadDepth,
    vec3 quadPos
) {
    float minXf = floor(min(min(v0.x, v1.x), min(v2.x, v3.x)));
    float maxXf = ceil (max(max(v0.x, v1.x), max(v2.x, v3.x)));
    float minYf = floor(min(min(v0.y, v1.y), min(v2.y, v3.y)));
    float maxYf = ceil (max(max(v0.y, v1.y), max(v2.y, v3.y)));

    int minX = clamp(int(minXf), 0, renderWidth  - 1);
    int maxX = clamp(int(maxXf), 0, renderWidth  - 1);
    int minY = clamp(int(minYf), 0, renderHeight - 1);
    int maxY = clamp(int(maxYf), 0, renderHeight - 1);
    float area = edge(v0, v1, v2) + edge(v0, v2, v3);
    bool ccw = area > 0.0;

    bool tl0 = isTopLeft(v1, v0);
    bool tl1 = isTopLeft(v2, v1);
    bool tl2 = isTopLeft(v3, v2);
    bool tl3 = isTopLeft(v0, v3);

    for (int y = minY; y <= maxY; y++) {
        if ((y + frameIndex) % 2 == 0) continue;

        for (int x = minX; x <= maxX; x++) {

            vec2 p = vec2(float(x) + 0.5, float(y) + 0.5);

            float w0 = edge(v0, v1, p);
            float w1 = edge(v1, v2, p);
            float w2 = edge(v2, v3, p);
            float w3 = edge(v3, v0, p);

            if (ccw) {
                if (w0 < 0.0 || (w0 == 0.0 && !tl0)) continue;
                if (w1 < 0.0 || (w1 == 0.0 && !tl1)) continue;
                if (w2 < 0.0 || (w2 == 0.0 && !tl2)) continue;
                if (w3 < 0.0 || (w3 == 0.0 && !tl3)) continue;
            } else {
                if (w0 > 0.0 || (w0 == 0.0 && !tl0)) continue;
                if (w1 > 0.0 || (w1 == 0.0 && !tl1)) continue;
                if (w2 > 0.0 || (w2 == 0.0 && !tl2)) continue;
                if (w3 > 0.0 || (w3 == 0.0 && !tl3)) continue;
            }

            int idx = y * 3840 + x;

            if (depth[idx] > quadDepth) {
                depth[idx] = quadDepth;
                frame[idx] = color;
                pos[idx] = quadPos;
            }
        }
    }
}

const float particleSize = 0.5;

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

struct Region {
    uvec2 handle;
    int lod;
};
layout(std430, binding = 3) buffer BitHandles {
    Region handles[];
};

uint hashVec3(vec3 v) {
    uint x = uint(v.x);
    uint y = uint(v.y);
    uint z = uint(v.z);
    uint h = x * 374761393u + y * 668265263u + z * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}
void main()
{

    float m15reproc = 1.0 / matView[3][3];

    int triangleId = int(gl_GlobalInvocationID.x);
    int height = 8;
    vec3 camRight = vec3(matView[0][0], matView[1][0], matView[2][0]);
    vec3 camUp    = vec3(matView[0][1], matView[1][1], matView[2][1]);
    vec4 cdd = vec4(0.0, 0.459*1.2, 0.173*1.2, 1);
    cdd.rgb*=0.8;
    vec3 position = vec3(
        particlePoint[triangleId*3] ,
        particlePoint[triangleId*3+1],
        particlePoint[triangleId*3+2]
    );
    const float halfSize = particleSize * 0.5;
    vec3 scaledRight = camRight * halfSize;
    vec3 scaledUp    = camUp * halfSize;
    
    vec3 v1 = position - scaledRight - scaledUp;
    vec3 v2 = position + scaledRight - scaledUp;
    vec3 v3 = position - scaledRight + scaledUp;
    if (!TriangleInFrustum(v1,v2,v3)) {
        return;
    }
    vec3 toParticle = position - camPos;

    if (dot(camForward, toParticle) <= 0.0) {
        return;
    }
    vec3 toV1 = v1 - camPos;
    vec3 toV2 = v2 - camPos;
    vec3 toV3 = v3 - camPos;

    float z1 = dot(toV1, camForward);
    float z2 = dot(toV2, camForward);
    float z3 = dot(toV3, camForward);

    if (z1 <= 0 && z2 <= 0 && z3 <= 0)
        return;
    vec3 base = vec3(
        particlePoint[triangleId*3],
        particlePoint[triangleId*3+1],
        particlePoint[triangleId*3+2]
    );
    vec3 tobase = base - camPos;
    float distancer = dot(tobase, camForward);
    float phase = float(triangleId) * 1.618;
    float sway = 
                sin(float(frameIndex) * 0.03 + phase) * 0.6          // slow primary sway
              + sin(float(frameIndex) * 0.11 + phase * 0.7) * 0.25   // faster gust flutter
              + cos(float(frameIndex) * 0.07 + phase * 1.3) * 0.1;   // subtle fore-aft lean
    
    uint hash = hashVec3(position);
    if (hash%2==0) {
                cdd.rgb *= 0.95;
            }
            else if (hash%3==0) {
                cdd.rgb *= 1.05;
            }
            
    if (distancer>340) {
        for (int y = 0; y < height; y++) {
            float normalizedHeight = float(y) / float(height);
        
            
            float swayAmount = sway * normalizedHeight * normalizedHeight;
            vec3 position = base+vec3(swayAmount,float(y)/2.0,0);
            ivec3 chunk = ivec3(position)/64;
            if (handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod!=0) {

            //uimage3D fluidMap = uimage3D(handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].handle);
            
            //imageStore(fluidMap,ivec3(mod(position,64))/handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod,uvec4(255));
            }
            vec3 toC = position  - camPos;
            float distance = dot(toC, camForward);
            
            if (distance <= 0.0) continue;

            vec2 p1;
            if (!GetWorldToScreenClipped(position , p1)) 
                return;
            
            vec2 SCREEN_MIN = vec2(0.0, 0.0);
            vec2 SCREEN_MAX = vec2(float(renderWidth), float(renderHeight));
    
            if ((p1.x < SCREEN_MIN.x) || 
                (p1.x >= SCREEN_MAX.x) || 
                (p1.y < SCREEN_MIN.y) ||
                (p1.y >= SCREEN_MAX.y)) continue;
            cdd.rgb *= 1.01;
            
            uint val = packRGBA8(cdd);
            int idx = int(p1.y) * 3840 + int(p1.x);
            if ((int(p1.y) + frameIndex) % 2 == 0) continue;

            if (depth[idx] > int(distancer)) {
                depth[idx] = int(distancer);
                frame[idx] = val;
                pos[idx] = position ;
            }
        }    
        return;
    }
    
    for (int y = 0; y < height; y++) {
        float normalizedHeight = float(y) / float(height);
            float swayAmount = sway * normalizedHeight * normalizedHeight;
        
        vec3 position = base+vec3(swayAmount,float(y)/2.0,0);
                    ivec3 chunk = ivec3(position)/64;
        if (handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod!=0) {
            //uimage3D fluidMap = uimage3D(handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].handle);
            
            //imageStore(fluidMap,ivec3(mod(position,64))/handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod,uvec4(255));

        }
        
        vec3 v1 = position - scaledRight - scaledUp;
        vec3 v2 = position + scaledRight - scaledUp;
        vec3 v3 = position - scaledRight + scaledUp;
        vec3 v4 = position + scaledRight + scaledUp;
    
        vec3 c = vec3(
            (v1+v2+v3+v4)/4.0f
        );
            
        vec3 toC = c - camPos;
        float distance = dot(toC, camForward);
        vec2 p1, p2, p3, p4;
        if (!GetWorldToScreenClipped(v1, p1) ||
            !GetWorldToScreenClipped(v2, p2) ||
            !GetWorldToScreenClipped(v4, p4) ||
            !GetWorldToScreenClipped(v3, p3)) 
            continue;

        vec2 SCREEN_MIN = vec2(0.0, 0.0);
        vec2 SCREEN_MAX = vec2(float(renderWidth), float(renderHeight));
    
        if ((p1.x < SCREEN_MIN.x && p2.x < SCREEN_MIN.x && p3.x < SCREEN_MIN.x) || 
            (p1.x >= SCREEN_MAX.x && p2.x >= SCREEN_MAX.x && p3.x >= SCREEN_MAX.x) || 
            (p1.y < SCREEN_MIN.y && p2.y < SCREEN_MIN.y && p3.y < SCREEN_MIN.y) ||
            (p1.y >= SCREEN_MAX.y && p2.y >= SCREEN_MAX.y && p3.y >= SCREEN_MAX.y)) continue;
        cdd.rgb *= 1.01;
        uint val = packRGBA8(cdd);
        DrawQuadSoftwareCS(p1, p2, p4, p3, val, int(distance), c);
    }
    
}
