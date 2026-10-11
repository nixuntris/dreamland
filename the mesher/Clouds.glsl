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

uniform int renderWidth;
uniform int renderHeight;
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

vec3 getRayDirection(vec2 uv) {
    vec2 ndc = vec2(
        uv.x * 2.0 - 1.0,
        1.0 - uv.y * 2.0
    );
    vec4 viewPos = inverse(matProj) * vec4(ndc, -1.0, 1.0);
    viewPos /= viewPos.w;
    vec3 viewDir = normalize(viewPos.xyz);
    return normalize(
        (inverse(matView) * vec4(viewDir, 0.0)).xyz
    );
}

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

const float MAX_STEPS = 96.0;
const float CLOUD_BOTTOM  = 990.0 * 2.0;
const float CLOUD_TOP     = 1100.0 * 2.0;
const float DENSITY_SCALE = 0.35;
const float MAX_DIST      = 16000.0;

float sampleCloud(vec3 p, float time, float lod) {
    float hf = (p.y - CLOUD_BOTTOM) / (CLOUD_TOP - CLOUD_BOTTOM);
    if (hf < 0.0 || hf > 1.0) return 0.0;
    float heightMask = smoothstep(0.0, 0.15, hf) * smoothstep(1.0, 0.55, hf);
    vec3 wind = vec3(time * 1.5, 0.0, time * 0.6);
    float n = fbm3(p * 0.008 + wind);
    float detail = fbm3(p * 0.06 + wind * 2.0);
    n -= detail * 0.25 * (1.0 - hf);
    float density = smoothstep(0.45, 0.75, n) * heightMask;
    return density * DENSITY_SCALE;
}
const float VOXEL_SIZE      = 16.0;
const int   MAX_VOXEL_STEPS = 256;
const float EXTINCTION      = 0.3;

float voxelDensity(ivec3 cell, float time) {
    vec3 c = (vec3(cell) + 0.5) * VOXEL_SIZE;

    float hf = (c.y - CLOUD_BOTTOM) / (CLOUD_TOP - CLOUD_BOTTOM);
    if (hf <= 0.0 || hf >= 1.0) return 0.0;

    float heightMask = smoothstep(0.0, 0.15, hf) * smoothstep(1.0, 0.55, hf);

    vec3 wind = vec3(time * 1.5, 0.0, time * 0.6);
    float n      = fbm3(c * 0.008 + wind);
    float detail = noise3d(c * 0.06 + wind * 2.0);
    n -= detail * 0.25 * (1.0 - hf);

    return smoothstep(0.45, 0.75, n) * heightMask * DENSITY_SCALE;
}

float voxelDensityAt(vec3 p, float time) {
    return voxelDensity(ivec3(floor(p / VOXEL_SIZE)), time);
}

float lightMarch(vec3 p, vec3 sunDir, float time) {
    float tau = 0.0;
    for (int i = 1; i <= 6; ++i) {
        vec3 sp = p + sunDir * float(i) * VOXEL_SIZE;
        tau += voxelDensityAt(sp, time) * VOXEL_SIZE;
    }
    return max(exp(-tau * 0.1), 0.5 * exp(-tau * 0.025));
}

float hgPhase(float cosTheta, float g) {
    float g2 = g * g;
    return (1.0 - g2) / (4.0 * 3.14159265 * pow(1.0 + g2 - 2.0*g*cosTheta, 1.5));
}

float sampleCloudCoarse(vec3 p, float time)
{
    float hf = (p.y - CLOUD_BOTTOM) / (CLOUD_TOP - CLOUD_BOTTOM);
    if (hf <= 0.0 || hf >= 1.0) return 0.0;

    float heightMask = smoothstep(0.0, 0.15, hf)
                     * smoothstep(1.0, 0.55, hf);

    vec3 wind = vec3(time * 1.5, 0.0, time * 0.6);
    float n = noise3d(p * 0.008 + wind);

    return smoothstep(0.45, 0.75, n) * heightMask;
}

void main() {
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    int idx = gid.y * 3840 + gid.x;

    vec3 ro = camPos;
    if (pos[(gid.y*2+1) * 3840 + gid.x*2] != vec3(-1000000.0) ||
    pos[(gid.y*2+1) * 3840 + gid.x*2+1] != vec3(-1000000.0) ||
    pos[(gid.y*2) * 3840 + gid.x*2+1] != vec3(-1000000.0) ||
    pos[(gid.y*2) * 3840 + gid.x*2] != vec3(-1000000.0)) {
       return;
    }
    vec2 uv = (vec2(gid) + 0.5) / (vec2(renderWidth,renderHeight)/2);
    vec3 rd = getRayDirection(uv);
    float t0, t1;
    if (abs(rd.y) < 1e-4) {
         if (ro.y < CLOUD_BOTTOM || ro.y > CLOUD_TOP) return;
         t0 = 0.0; t1 = MAX_DIST;
    } else {
         float ta = (CLOUD_BOTTOM - ro.y) / rd.y;
         float tb = (CLOUD_TOP    - ro.y) / rd.y;
         t0 = min(ta, tb);
         t1 = max(ta, tb);
         t0 = max(t0, 0.0);
         t1 = min(t1, MAX_DIST);
    }
    if (t1 <= t0) return;

    float dt = (t1 - t0) / MAX_STEPS;

    float t = t0 + dt * hash3(vec3(gid, frameIndex));
    vec3  scatteredLight = vec3(0.0);
    float transmittance  = 1.0;

    const vec3 sunColor  = vec3(1.0) * 3.0;
    const vec3 skyColor  = vec3(0.55, 0.7, 1.0);
    float cosTheta = dot(rd, sunDir);
    float phase = mix(hgPhase(cosTheta, 0.3), hgPhase(cosTheta, -0.2), 0.3) * 4.0 * 3.14159265;
    phase = clamp(phase, 0.8, 2.0);
    float time = float(frameIndex) * 0.016;
    for (int i = 0; i < 96 && t < t1; ++i)
    {
        vec3 p = ro + rd * t;

        float coarse = sampleCloudCoarse(p, time);

        float stepLen = dt;

        if (coarse < 0.02)
            stepLen = dt * 4.0;
        else if (coarse < 0.15)
            stepLen = dt * 2.0;

        stepLen = min(stepLen, t1 - t);

        if (coarse >= 0.02)
        {
            float dens = sampleCloud(p, time, 0.0);

            if (dens > 0.001)
            {
                float lightT = lightMarch(p, sunDir, time);
                float powder = 1.0 - exp(-dens * 8.0);

                vec3 lum = sunColor * lightT * phase * powder
                         + skyColor * 0.25;

                float sampleTrans = exp(-dens * stepLen * 3.0);

                scatteredLight += transmittance
                                * (1.0 - sampleTrans) * lum;

                transmittance *= sampleTrans;

                if (transmittance < 0.01)
                    break;
            }
        }

        t += stepLen*4;
    }

    if (transmittance < 0.999) {
        
        if ((gid.y*2 + frameIndex) % 2 != 0) frame[(gid.y*2) * 3840 + gid.x*2] = packRGBA8(vec4((scatteredLight + unpackRGBA8(frame[(gid.y*2) * 3840 + gid.x*2]).rgb * transmittance), 1.0));
        if ((gid.y*2+1 + frameIndex) % 2 != 0) frame[(gid.y*2+1) * 3840 + gid.x*2] = packRGBA8(vec4((scatteredLight + unpackRGBA8(frame[(gid.y*2+1) * 3840 + gid.x*2]).rgb * transmittance), 1.0));
        if ((gid.y*2+1 + frameIndex) % 2 != 0) frame[(gid.y*2+1) * 3840 + gid.x*2+1] = packRGBA8(vec4((scatteredLight + unpackRGBA8(frame[(gid.y*2+1) * 3840 + gid.x*2+1]).rgb * transmittance), 1.0));
        if ((gid.y*2 + frameIndex) % 2 != 0) frame[(gid.y*2) * 3840 + gid.x*2+1] = packRGBA8(vec4((scatteredLight + unpackRGBA8(frame[(gid.y*2) * 3840 + gid.x*2+1]).rgb * transmittance), 1.0));
    }
}