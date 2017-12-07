#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D preFinalImageSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() 
{
	outColor = vec4(texture(preFinalImageSampler, fragTexCoord).rgb, 1.0);
}