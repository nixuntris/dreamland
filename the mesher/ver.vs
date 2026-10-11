#version 460
layout (location=0) in vec3 aPos;
layout (location=1) in vec3 aColor;

uniform mat4 MVP;

out vec3 color;

uniform int offsetInt;
layout(std430, binding = 6) buffer Colors {
    uint colors[];
};
uniform float size;

uniform vec3 colorr;
uniform vec3 camPos;
uniform vec3 camForward;
out float depthr;
void main() {
	gl_Position = MVP * vec4(aPos* size,1.0);
	color = colorr;
    vec3 toC = (aPos) - camPos;
    depthr = dot(toC, camForward);


}