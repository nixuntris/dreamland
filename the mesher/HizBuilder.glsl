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
                
                    frame[srcZ * 3840 + srcX] = packRGBA8(vec4(0.529, 0.808, 0.922, 1.0));;
                }
                depth[srcZ * 3840 + srcX] = 1000000;
                voxelId[srcZ * 3840 + srcX] = -1;
                pos[srcZ * 3840 + srcX] = vec3(-1000000.0);
            }
        }

}