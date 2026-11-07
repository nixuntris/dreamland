#version 460
#extension GL_ARB_bindless_texture : require
#extension GL_ARB_shader_ballot : require
#extension GL_ARB_shader_group_vote : require
layout (local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

const int chunkSize = 32;

layout(std430, binding = 0) buffer GBufferEx {
    uint frame[3840 * 2160];
    vec3 pos[3840*2160];
    int depth[3840* 2160];
    int voxelId[3840*2160];
    float lightVal[3840*2160];
};

struct Region {
    uvec2 handle;
    uvec2 probeHandle;
    int lod;
    int filled;
};

layout(std430, binding = 3) buffer BitHandles {
    Region handles[];
};
uniform vec3 camForward;
uniform int wx;
uniform int wy;
uniform int wz;
uniform int renderWidth;
uniform int renderHeight;
uniform vec3 camPos;
uniform mat4 matView;
uniform mat4 matProj;
uniform int frameIndex;
uint readVoxelData(ivec3 pos, inout int lod) {
    if (any(lessThan(pos, ivec3(0))) ||
        pos.x >= wx * chunkSize ||
        pos.y >= wy * chunkSize ||
        pos.z >= wz * chunkSize)
        return 0u;   
    ivec3 chunk = pos / chunkSize;
    int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;
    int regionLod = handles[index].lod;
    lod = regionLod;
    if (regionLod==32) return handles[index].filled;
    if (regionLod == 3000) return 1u;
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

uniform uint packedColors[1024];
uint hashVec3(vec3 v) {
    uint x = uint(v.x);
    uint y = uint(v.y);
    uint z = uint(v.z);
    uint h = x * 374761393u + y * 668265263u + z * 1274126177u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
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

void skipChunk(inout vec3 cell, inout vec3 tMax, ivec3 stepDir, vec3 tDelta)
{
    ivec3 chunkMin = (ivec3(cell) / chunkSize) * chunkSize;
    ivec3 chunkMax = chunkMin + chunkSize;
    ivec3 nI = mix(ivec3(cell) - chunkMin + 1,
                   chunkMax - ivec3(cell),
                   greaterThan(stepDir, ivec3(0)));
    vec3 n = vec3(nI);
    vec3 tAxisExit = tMax + (n - 1.0) * tDelta;
    float tExit = min(tAxisExit.x, min(tAxisExit.y, tAxisExit.z));
    vec3 k = floor((tExit - tMax) / tDelta) + 1.0;
    k = clamp(k, vec3(0.0), n);
    k = mix(k, n, equal(tAxisExit, vec3(tExit)));

    ivec3 ki = ivec3(k);
    cell += stepDir * ki;
    tMax += tDelta * vec3(ki);
}
void main() {
    ivec2 px = ivec2(gl_GlobalInvocationID.xy);
    px.y*=2;
    px.x*=2;
    px.y += (frameIndex+1)%2;
    if (px.x >= renderWidth || px.y >= renderHeight) return;

    vec2 ndc = vec2((px.x + 0.5) / renderWidth * 2.0 - 1.0,
                    1.0 - (px.y + 0.5) / renderHeight * 2.0);
    vec4 far = inverse(matProj * matView) * vec4(ndc, 1.0, 1.0);
    vec3 rd = normalize(far.xyz / far.w - camPos);
    rd = mix(rd, vec3(1e-6), lessThan(abs(rd), vec3(1e-6)));

    vec3 cell = ivec3(floor(camPos));
    ivec3 stepDir = ivec3(sign(rd));
    vec3 tDelta = abs(1.0 / rd);
    vec3 tMax = (vec3(cell) + vec3(max(stepDir, ivec3(0))) - camPos) / rd;

    uint color = 0xFFF2BF87u; 
    int axis = 0;
    bool hitAnything = false;
    for (int i = 0; i < 960; i++) {
        int lod;
        uint v = readVoxelData(ivec3(cell),lod);

        float distanceTravelled = distance(floor(camPos),cell);
        if (any(lessThan(cell, ivec3(0))) ||
            cell.x >= wx * chunkSize ||
            cell.y >= wy * chunkSize ||
            cell.z >= wz * chunkSize || 
            distanceTravelled>960)
            break;   
            

        ivec3 chunk = ivec3(cell) / chunkSize;
        int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;

        if (handles[index].lod == 0) { 
            skipChunk(cell, tMax, stepDir, tDelta);
            continue;
        }
        
        if (v != 0u) {
            color = packedColors[v-1];
            hitAnything = true;
            break;
        }
        bvec3 mask = lessThanEqual(tMax.xyz, min(tMax.yzx, tMax.zxy));

        ivec3 stepMask = ivec3(mask);
        cell += stepDir * stepMask;
        tMax += tDelta * vec3(stepMask);

    }
    if (!hitAnything) {
        
        for (int i = 0; i < 4096-960; i++) {
            int lod;
            uint v = readVoxelData(ivec3(cell),lod);

            float distanceTravelled = distance(floor(camPos),cell);
            if (any(lessThan(cell, ivec3(0))) ||
                cell.x >= wx * chunkSize ||
                cell.y >= wy * chunkSize ||
                cell.z >= wz * chunkSize || 
                distanceTravelled>4096)
                break;   
            

            ivec3 chunk = ivec3(cell) / chunkSize;
            int index = chunk.y * (wx * wz) + chunk.z * wx + chunk.x;

            if (handles[index].lod == 0) { 
                skipChunk(cell, tMax, stepDir, tDelta);
                continue;
            }
        
            if (v != 0u) {
                color = packedColors[v-1];
                hitAnything = true;
                break;
            }

            cell += rd*float(lod);

        }
    }
    int distance = int(dot(cell-camPos, camForward));
    if (distance<depth[px.y * 3840 + px.x] && hitAnything) {
    
        uint hash = hashVec3(vec3(cell));
        if (hash%2==0) {
            vec4 hr = unpackRGBA8(color);
            hr.rgb *= 0.95;
            color = packRGBA8(hr);
        }
        else if (hash%3==0) {
            vec4 hr = unpackRGBA8(color);
            hr.rgb *= 1.05;
            color = packRGBA8(hr);
        }
        frame[px.y * 3840 + px.x] = color;
        depth[px.y * 3840 + px.x] = distance;
        pos[px.y * 3840 + px.x] = vec3(cell);
        int lod;
        voxelId[px.y * 3840 + px.x] = int(readVoxelData(ivec3(cell),lod));
    }
}
