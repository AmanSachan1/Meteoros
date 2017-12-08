#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, rgba8) uniform writeonly image2D currentFrameResultImage;
layout(set = 0, binding = 1) uniform sampler2D godRayCreationDataSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

void main() 
{
	outColor = vec4(texture(godRayCreationDataSampler, fragTexCoord).rgb, 1.0);
}