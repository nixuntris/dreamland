#version 460

layout (local_size_x = 16, local_size_y = 1, local_size_z = 16) in;
uniform int buildLevel;
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
uniform int frameIndex;
uniform vec3 planeNormals[6];
uniform float planeDs[6];

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

vec3 viewRay(vec3 forward, vec2 fragCoord, vec2 resolution, float fovY)
{
    vec2 uv = (fragCoord * 2.0 - resolution) / resolution.y;
    vec3 f = normalize(forward);
    vec3 r = normalize(cross(f, vec3(0.0, 1.0, 0.0)));
    vec3 u = cross(r, f);
    float t = tan(fovY * 0.5);
    return normalize(f + r * (uv.x * t) + u * (uv.y * t));
}

vec4 getSkyColor(float totalTime, vec3 cPos, vec3 cForward, vec2 fragCoord)
{
    vec3 rd  = viewRay(cForward, fragCoord, RESOLUTION, FOV_Y);
    vec3 sun = sunDirection(totalTime);
    float sunY = sun.y;

    float daylight = smoothstep(-0.15, 0.15, sunY);
    float twilight = smoothstep(-0.25, 0.0, sunY) * (1.0 - smoothstep(0.0, 0.35, sunY));

    float density = exp(-max(cPos.y, 0.0) / ATMOS_SCALE);

    float h = clamp(rd.y, 0.0, 1.0);
    float horizonBand = pow(1.0 - h, 4.0) * density;

    vec3 zenith = DAY_ZENITH * mix(0.35, 1.0, density);
    vec3 sky = mix(zenith, DAY_HORIZON, horizonBand);

    float sunAmt  = max(dot(rd, sun), 0.0);
    float lowBand = pow(1.0 - h, 3.0);
    vec3 duskTone = mix(DEEP_ORANGE, ORANGE, pow(sunAmt, 2.0));
    sky = mix(sky, duskTone, twilight * lowBand * (0.35 + 0.65 * sunAmt));
    sky += ORANGE * pow(sunAmt, 8.0) * twilight * 0.4;

    sky = mix(NIGHT, sky, daylight + twilight * 0.6 * lowBand);

    float aboveHorizon = smoothstep(-0.05, 0.05, sunY);
    vec3 sunColor = mix(ORANGE, vec3(1.0, 0.95, 0.85), smoothstep(0.0, 0.4, sunY));
    sky += sunColor * pow(sunAmt, 64.0) * 0.5 * aboveHorizon;
    sky += sunColor * smoothstep(0.9995, 0.9999, sunAmt) * 2.0 * aboveHorizon;

    if (rd.y < 0.0)
        sky = mix(sky, sky * 0.6, smoothstep(0.0, -0.3, rd.y));

    return vec4(sky, 1.0);
}

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
void main()
{
    int dx = int(gl_GlobalInvocationID.x);
    int dz = int(gl_GlobalInvocationID.z);

    float avg = 0.0;
    
    if (dx==0 && dz==0) {
    }
        for (int x = 0; x < 2; x++)
        {
            for (int z = 0; z < 2; z++)
            {
                int srcX = dx * 2 + x;
                int srcZ = dz * 2 + z;

                if ((dz * 2 + z+frameIndex)%2==0) {
                    vec2 fragCoord = vec2(float(srcX) + 0.5, RESOLUTION.y - float(srcZ) - 0.5);
                    frame[srcZ * 3840 + srcX] = packRGBA8(getSkyColor(float(frameIndex) / 60.0, camPos, camForward, fragCoord));
                }
                depth[srcZ * 3840 + srcX] = 1000000;
                voxelId[srcZ * 3840 + srcX] = -1;
                pos[srcZ * 3840 + srcX] = vec3(-1000000.0);
            }
        }

}