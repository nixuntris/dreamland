#version 460
#extension GL_ARB_gpu_shader_int64 : require
#extension GL_NV_gpu_shader5 : enable
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_ballot : require
#extension GL_ARB_shader_group_vote : require

layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;


//This shader somewhere has a bug causing any chunk over wy 0 to have bugged lighting

layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
    float lightVal[3840*2160];
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
uniform int sampleCount;
vec3 sunDir = normalize(vec3(0.0, 1.0, 0.0));
struct Region {
    uvec2 handle;
    uvec2 probeHandle;

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
int chunkSize = 32;

uint readVoxelData(ivec3 pos, inout int lod) {
    if (any(lessThan(pos, ivec3(0))) ||
        pos.x >= wx * chunkSize ||
        pos.y >= wy * chunkSize ||
        pos.z >= wz * chunkSize)
        return 0u;   
    ivec3 chunk = pos / chunkSize;
    int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;
    int regionLod = handles[index].lod;
    if (regionLod == 3000) return 1u;
    lod = regionLod;
    uvec2 handle = handles[index].handle;
    if (handle != uvec2(0)) {
        uimage3D fluidMap = uimage3D(handle);
        ivec3 lp = (pos & (chunkSize - 1)) / regionLod;
        uint r = 0u;
        bool pending = true;
        while (anyInvocationARB(pending)) {
            if (pending) {
                uvec2 cur = uvec2(readFirstInvocationARB(handle.x), readFirstInvocationARB(handle.y));
                if (handle == cur) {
                    r = imageLoad(uimage3D(cur), lp).x;
                    pending = false;
                }
            }
        }
        return r;
    }
    return 0u;
}
bool inBounds(ivec3 p) {
    
    
    return (p.x < 0 || p.x >= wx *chunkSize||
        p.y < 0 || p.y >= wy*chunkSize ||
        p.z < 0 || p.z >= wz*chunkSize);

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
uniform float timeOfDay;
uniform vec3 lightPositions[16];
uniform vec3 lightValues[16];
uniform int lightRadiusList[16];
uniform int lightCount;


const vec3 DAY_ZENITH   = vec3(0.529, 0.808, 0.922);
const vec3 DAY_HORIZON  = vec3(0.780, 0.900, 0.960);
const vec3 ORANGE       = vec3(1.000, 0.550, 0.200);
const vec3 DEEP_ORANGE  = vec3(0.600, 0.180, 0.080);
const vec3 NIGHT        = vec3(0.0);

const float DAY_LENGTH  = 120.0;
const float ATMOS_SCALE = 500.0;
const vec2 RESOLUTION   = vec2(3840.0, 2160.0);
const float FOV_Y       = 1.0471976;

vec3 sunDirection(float totalTime)
{
    float p = fract(totalTime / DAY_LENGTH);
    float a = 6.2831853 * (p - 0.2);
    return normalize(vec3(sin(a), cos(a), 0.3));
}

float calculateLightStrength(int frameIndex)
{
    float totalTime = float(frameIndex) / 60.0;
    
    vec3 sun = sunDirection(totalTime);
    float sunY = sun.y;
    float daylight = smoothstep(-0.15, 0.15, sunY);
    float twilight = smoothstep(-0.25, 0.0, sunY) * (1.0 - smoothstep(0.0, 0.35, sunY));
    
    float lightStrength = clamp(daylight + twilight * 0.35, 0.0, 1.0);
    
    return lightStrength;
}
bool outOfBounds(vec3 p) {
    return any(lessThan(p, vec3(0))) ||
           any(greaterThanEqual(p, vec3(wx, wy, wz) * chunkSize));
}

bool skipChunkFloat(inout vec3 cell, vec3 rd, inout int steps, ivec3 chunkSize)
{
    ivec3 c = ivec3(floor(cell));
    ivec3 chunkMin = ivec3(floor(vec3(c) / vec3(chunkSize))) * chunkSize;
    ivec3 chunkMax = chunkMin + chunkSize; 
    vec3 tExit;
    for (int a = 0; a < 3; a++)
    {
        float r = rd[a];
        if (abs(r) < 1e-8) { tExit[a] = 1e30; continue; }
        float bound = (r > 0.0) ? float(chunkMax[a]) : float(chunkMin[a]);
        tExit[a] = (bound - cell[a]) / r;
    }
    float t = min(tExit.x, min(tExit.y, tExit.z));
    if (t <= 0.0 || t > 1e29) return false;

    int skipped = int(ceil(t / length(rd))); 
    if (skipped <= 0) skipped = 1;

    if (skipped >= steps) {
        cell += rd * (float(steps) + 1e-4);
        steps = 0;
        return false;
    }

    cell += rd * t + rd * 1e-4;
    steps += skipped;
    return true;
}
bool sunRayOccluded(vec3 ro, vec3 rd, int maxSteps)
{
    vec3 cell  = floor(ro);

    vec3 nextB  = vec3(cell) + vec3(greaterThan(rd, vec3(0.0)));

    for (int i = 0; i < maxSteps; i++)
    {
        cell += rd;
        if (outOfBounds(cell)) return false;
        
        int lod;        
        if (lod==0) {
            skipChunkFloat(cell,rd,i,ivec3(32));
        }
        if (readVoxelData(ivec3(cell),lod) != 0u) return true;
    }
    return false;
}
void main()
{
    ivec2 gid = ivec2(gl_GlobalInvocationID.xy);
    gid.y*=2;
    gid.x*=2;
    gid.y+=(frameIndex+1)%2;
    int idx = gid.y * 3840 + gid.x;
    if (depth[idx] == 1000000) return;
     float time = float(frameIndex) *timeSpeed;
   
    vec3 origin = pos[idx];
    ivec3 chunk = ivec3(origin/chunkSize);
    int lod = handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lod;
    uimage3D probeMap = uimage3D(handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].probeHandle);
    uint val = imageLoad(probeMap,ivec3(mod(origin,32)/lod)).r;
    if (val==0) {
        float radiance = 0.0;

        vec3 ro = origin;
        vec3 rd = normalize(sunDir);
        vec3  invD      = 1.0 / (rd + vec3(equal(rd, vec3(0.0))) * 1e-8);
    
        float T = 1.0;

        float t = 0;
        float jitter = hash3(vec3(gid, frameIndex));
        float shadow = cloudShadow(ro, rd, time, jitter);
        shadow = mix(1.0, shadow, 0.3);
        T *= shadow;   

        
        radiance += T;
    
        uint hitValue = 0;
        for (int i = 0; i < sampleCount; i++) {
            vec3 offsetDir = normalize(sunDir+offsets[i]);
            if (sunRayOccluded(origin+offsetDir*2, offsetDir, 100)) {
                hitValue+=1;
            }
        }
    
        val = hitValue;
        imageStore(probeMap,ivec3(mod(origin,32)/lod), uvec4(hitValue));

    }
    
    /*
    uvec2 handle = handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].handle;
    vec3 accumLightAdditional = vec3(0.0);
    for (int i = 0; i < 16; i++) {
        if (handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lightIds[i]==-1) break;
        int id = handles[chunk.y * (wx * wz) + chunk.z * wx + chunk.x].lightIds[i];

        float dist = distance(origin,lightPositions[id]);
        vec3 lightToDir = normalize(origin-lightPositions[id]);
        vec3 beginPos = lightPositions[id];
        float radius = float(lightRadiusList[id]);
        for (int d = 0; d < int(radius); d++) {
            beginPos += lightToDir;
            if (readVoxelData(ivec3(beginPos))!=0) break;
            if (distance(beginPos,origin)<1) {
                accumLightAdditional += vec3(1-(dist/radius))*lightValues[id];
                
            }   
        }
    } 
    
    */
    vec4 prev = unpackUnorm4x8(frame[idx]);
    float mean = 1.0-(((float(val)/2)) / float(sampleCount));
    vec3 col = prev.rgb * (mean*calculateLightStrength(frameIndex));
    // vec3 col = prev.rgb * (mean);
    
    uint pp = packUnorm4x8(vec4(col, 1.0));
    frame[ gid.y * 3840 + gid.x] = pp;
    frame[ gid.y * 3840 + gid.x+1] = pp;
}