#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D inputImageSampler;
layout (set = 0, binding = 1, rgba8) uniform writeonly image2D currentFrameResultImage;

layout (set = 1, binding = 0) uniform TimeUBO
{
    vec4 haltonSeq1;
    vec4 haltonSeq2;
    vec4 haltonSeq3;
    vec4 haltonSeq4;
    vec2 time; //stores delta time and total time
    int frameCount;
};

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 outColor;

//Reference: http://filmicworlds.com/blog/filmic-tonemapping-operators/
//Also https://www.shadertoy.com/view/lslGzl
//Originally from the game Uncharted 2 by Naughty Dog
#define A 0.15
#define B 0.50
#define C 0.10
#define D 0.20
#define E 0.02
#define F 0.30
#define INVGAMMA (1.0 / 2.2)
#define EXPOSURE 2.5

vec3 Uncharted2Tonemap(vec3 x)
{
   return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

vec3 tonemap(vec3 x, float whiteBalance) 
{
   vec3 color = Uncharted2Tonemap(EXPOSURE * x);

   vec3 white = vec3(whiteBalance);
   vec3 whitemap = 1.0 / Uncharted2Tonemap(white);

   color *= whitemap;
   return pow(color, vec3(INVGAMMA));
}

//Replacement for dithering noise function
//Reference: https://youtu.be/4D5uX8wL1V8?t=11m22s
//Super fast because of all the binary operations
//Quality noise without repeating patterns
float WangHashNoise(uint u, uint v, uint s)
{
	//u after a number ensures it is an unsigned int
	uint seed = (u * 1664525u + v) + s;

	seed = (seed ^ 61u) ^ (seed >> 16u);
	seed *= 9u;
	seed = seed ^ (seed >> 4u);
	seed *= 0x27d4eb2d;
	seed = seed ^ (seed >> 15u);

	float value = float(seed);
	value *= (1.0 / 4294967296.0);
	return value;
}

void main() 
{
	ivec2 dim = imageSize(currentFrameResultImage);
	ivec2 pixelPos = clamp(ivec2(round(float(dim.x) * in_uv.x), round(float(dim.y) * in_uv.y)), ivec2(0.0), ivec2(dim.x - 1, dim.y - 1));

	vec3 in_color = texture(inputImageSampler, in_uv).rgb;

	float whitepoint = 100.0f; //changes the point at which something becomes pure white
	//The white point is the value that is mapped to 1.0 in the regular RGB space.
	vec3 toneMapped_color = tonemap(in_color, whitepoint);

	//Dithering to prevent banding
	float noise = WangHashNoise(pixelPos.x, pixelPos.y, uint(time.y))*0.01;
	toneMapped_color += vec3(noise);

	imageStore( currentFrameResultImage, pixelPos, vec4(toneMapped_color, 1.0) );
}