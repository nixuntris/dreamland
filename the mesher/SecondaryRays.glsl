#version 460
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_NV_gpu_shader5 : enable
#extension GL_ARB_bindless_texture : require

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;



layout(std430, binding = 0) buffer GBuffer {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
};

uint packRGBA8(vec4 color)
{
    vec4  c     = clamp(color, 0.0, 1.0);
    uvec4 bytes = uvec4(c * 255.0 + 0.5);
    return (bytes.r) | (bytes.g << 8) | (bytes.b << 16) | (bytes.a << 24);
}

vec4 unpackRGBA8(uint packedr)
{
    return vec4(
        float( packedr        & 0xFFu),
        float((packedr >>  8) & 0xFFu),
        float((packedr >> 16) & 0xFFu),
        float((packedr >> 24) & 0xFFu)
    ) * (1.0 / 255.0);
}
uniform vec3  camPos;
uniform vec3  camForward;
uniform mat4  matView;
uniform mat4  matProj;
uniform int   wx, wy, wz;
uniform int   frameIndex;

vec3 sunDir = normalize(vec3(0.0, 1.0, 0.0));
struct Region {
    uvec2 handle;
    int lod;
};
layout(std430, binding = 3) buffer BitHandles {
    Region handles[];
};
const int WIDTH = 3840;
const int HEIGHT = 2160;

const vec3 offsets[9] = vec3[](
        vec3(0.0, 0.0, 0.0),
        vec3(0.2, 0.1, 0.0),
        vec3(-0.2, 0.1, 0.0),
        vec3(0.1, 0.2, 0.0),
        vec3(-0.1, 0.2, 0.0),
        vec3(0.0, 0.1, 0.2),
        vec3(0.0, 0.1,-0.2),
        vec3(0.2, 0.2, 0.1),
        vec3(-0.2, 0.2,-0.1)
    );

    const vec3 bounceDirs[4] = vec3[](
        vec3( 0.3, 0.8, 0.1),
        vec3(-0.4, 0.7, 0.2),
        vec3( 0.2, 0.6,-0.3),
        vec3(-0.2, 0.9,-0.1)
    );

uint readVoxelData(ivec3 pos) {
    ivec3 chunk = pos/64;
    
    uvec2 handle = handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].handle;

    if (handle != uvec2(0))
    {
        uimage3D fluidMap = uimage3D(handle);
        ivec3 lp = ivec3(mod(pos, 64.0));
                    
        lp/=handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod;
        return imageLoad(fluidMap,lp).x;
    }
    return 0;

}
bool inBounds(ivec3 p) {
    
    
    return (p.x < 0 || p.x >= wx *64||
        p.y < 0 || p.y >= wy*64 ||
        p.z < 0 || p.z >= wz*64);

}

const float CLOUD_BOTTOM  = 990.0 * 2.0;
const float CLOUD_TOP     = 1100.0 * 2.0;
const float DENSITY_SCALE = 0.35;
const float MAX_DIST      = 16000.0;
const int   SHADOW_STEPS  = 12;
const float SHADOW_EXTINCTION = 3.0;   // same as the view march; lower = softer/lighter shadows

float hash3(vec3 p) {
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise3d(vec3 p) {
    vec3 i = floor(p), f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix(hash3(i + vec3(0,0,0)), hash3(i + vec3(1,0,0)), u.x),
                   mix(hash3(i + vec3(0,1,0)), hash3(i + vec3(1,1,0)), u.x), u.y),
               mix(mix(hash3(i + vec3(0,0,1)), hash3(i + vec3(1,0,1)), u.x),
                   mix(hash3(i + vec3(0,1,1)), hash3(i + vec3(1,1,1)), u.x), u.y), u.z);
}

float fbm3(vec3 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) {
        v += a * noise3d(p);
        p *= 2.0;
        a *= 0.5;
    }
    return v;
}

float cloudDensity(vec3 p, float time) {
    float hf = (p.y - CLOUD_BOTTOM) / (CLOUD_TOP - CLOUD_BOTTOM);
    if (hf <= 0.0 || hf >= 1.0) return 0.0;
    float heightMask = smoothstep(0.0, 0.15, hf) * smoothstep(1.0, 0.55, hf);
    vec3 wind = vec3(time * 1.5, 0.0, time * 0.6);
    float n      = fbm3(p * 0.008 + wind);
    float detail = noise3d(p * 0.06 + wind * 2.0); 
    n -= detail * 0.25 * (1.0 - hf);
    return smoothstep(0.45, 0.75, n) * heightMask * DENSITY_SCALE;
}

float cloudShadow(vec3 ro, vec3 rd, float time, float jitter) {
    if (rd.y <= 1e-4) return 1.0;              

    float t0 = max((CLOUD_BOTTOM - ro.y) / rd.y, 0.0);
    float t1 = min((CLOUD_TOP    - ro.y) / rd.y, MAX_DIST);
    if (t1 <= t0) return 1.0; 

    float dt  = (t1 - t0) / float(SHADOW_STEPS);
    float tau = 0.0;
    for (int i = 0; i < SHADOW_STEPS; i++) {
        float t = t0 + dt * (float(i) + jitter);
        tau += cloudDensity(ro + rd * t, time) * dt * SHADOW_EXTINCTION;
    }
    return exp(-tau);
}
uniform float timeSpeed;

uniform vec3 lightPositions[16];
uniform vec3 lightValues[16];
uniform int lightCount;

void main()
{
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    gid.y*=2;
    gid.x*=2;
    gid.y+=(frameIndex+1)%2;
    int idx = gid.y * 3840 + gid.x;

    if (depth[idx] == 1000000) return;

    vec3 origin = pos[idx];
    float time = float(frameIndex) *timeSpeed;
    if (readVoxelData(ivec3(origin))==7) frame[idx] = 0;
    float radiance = 0.0;

    for (int d = 0; d < 2; d++)
    {
        vec3 ro = origin + offsets[d];
        vec3 rd = normalize(sunDir);
        vec3  invD      = 1.0 / (rd + vec3(equal(rd, vec3(0.0))) * 1e-8);
    
        float T = 1.0;

        float t = 0;
        float jitter = hash3(vec3(gid, frameIndex * 2 + d));
        float shadow = cloudShadow(ro, rd, time, jitter);
        shadow = mix(1.0, shadow, 0.3);
        T *= shadow;   
        for (int i = 0; i < 64; i++)
        {
            t+=1;
            vec3 p = ro + rd * t;

            
            ivec3 chunk = ivec3(p/64);
    
            uvec2 handle = handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].handle;
            if (handle==uvec2(0)) {                        // empty chunk: jump to its exit
                vec3 bmin = vec3(chunk << 6), bmax = bmin + 64.0;
                vec3 tFar = max((bmin - ro) * invD, (bmax - ro) * invD);
                t = min(min(tFar.x, tFar.y), tFar.z) + 1e-3;
                continue;
            }
            if (inBounds(ivec3(p))) break;
            
             if (readVoxelData(ivec3(p)) != 0)
            {
                T *= 0.70;
                break;
                
            }
        }

        float bounceLight = 0.0;

        for (int b = 0; b < 2; b++)
        {
            vec3 br = normalize(bounceDirs[b]);

            vec3 ro2 = ro;
            float Tb = 1.0;

            for (int i = 0; i < 32; i++)
            {
                vec3 p = ro2 + br * float(i);

                
                if (inBounds(ivec3(p))) break;
                if (readVoxelData(ivec3(p)) != 0)
                {
                    Tb *= 0.15;
                    break;
                   
                }
            }

            bounceLight += Tb;
        }

        bounceLight /= 2.0;

        radiance += T + 0.35 * bounceLight;
    }

    float mean = radiance / 2.0;

    vec4 prev = unpackUnorm4x8(frame[idx]);
    float accumLightAdditional = 0;
    for (int i = 0; i < lightCount; i++) {
        float dist = distance(origin,lightPositions[i]);
        vec3 lightToDir = normalize(origin-lightPositions[i]);
        vec3 beginPos = lightPositions[i];
        for (int d = 0; d < 40; d++) {
            beginPos += lightToDir;
            if (readVoxelData(ivec3(beginPos))!=0) break;
            if (distance(beginPos,origin)<1) {
                accumLightAdditional += 1-(dist/40);
            }   
        }
    } 
    
    vec3 col = prev.rgb * (mean+accumLightAdditional);
    
    uint pp = packUnorm4x8(vec4(col, 1.0));
    frame[ gid.y * 3840 + gid.x] = pp;
    frame[ gid.y * 3840 + gid.x+1] = pp;
}