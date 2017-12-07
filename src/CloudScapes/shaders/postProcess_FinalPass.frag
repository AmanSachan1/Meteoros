#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D preFinalImageSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 0) out vec4 outColor;

#define gamma 2.2

//Reference: https://www.shadertoy.com/view/lslGzl
//Originally from the game Uncharted 2 by Naughty Dog
vec3 Uncharted2ToneMapping(vec3 color)
{
	float A = 0.15;
	float B = 0.50;
	float C = 0.10;
	float D = 0.20;
	float E = 0.02;
	float F = 0.30;
	float W = 11.2;
	float exposure = 2.;
	color *= exposure;
	color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
	float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
	color /= white;
	color = pow(color, vec3(1.0 / gamma));
	return color;
}

void main() 
{
	vec3 in_color = texture(preFinalImageSampler, fragTexCoord).rgb;
	vec3 toneMapped_color = Uncharted2ToneMapping(in_color);
	//outColor = vec4(toneMapped_color, 1.0);
	outColor = vec4(in_color, 1.0);
}