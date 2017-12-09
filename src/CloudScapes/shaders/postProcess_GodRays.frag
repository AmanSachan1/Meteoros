#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (set = 0, binding = 0, rgba8) uniform image2D currentFrameResultImage;
layout (set = 0, binding = 1) uniform sampler2D godRayCreationDataSampler;

layout (set = 1, binding = 0) uniform CameraUBO
{
	mat4 view;
	mat4 proj;
	vec4 eye;
	vec2 tanFovBy2;
} camera;

layout (set = 2, binding = 0) uniform SunAndSkyUBO
{
	vec4 sunLocation;
	vec4 sunDirection;
	vec4 lightColor;
	float sunIntensity;
};

layout(location = 0) in vec2 in_uv;

#define MAX_SAMPLES 100
#define ATMOSPHERE_DENSITY 1.0f
#define WEIGHT_FOR_SAMPLE 0.01 //0.01
#define DECAY 1.0
#define EXPOSURE 1.1

#define PI 3.14159265

//References: - https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch13.html
//			  -	https://github.com/Erkaman/glsl-godrays
void main()
{
	ivec2 dim = imageSize(currentFrameResultImage); //texture(currentFrameResultImage, fragTexCoord).rgb;
	ivec2 pixelPos = ivec2(floor(dim.x * in_uv.x), floor(dim.y * in_uv.y));
	vec2 uv = in_uv;

	vec3 cam_to_sun = normalize(sunLocation.xyz - camera.eye.xyz);
	vec3 camForward =  -normalize(vec3(camera.view[0][2], camera.view[1][2], camera.view[2][2] ));

	float sunAngleY = abs(tan( (cam_to_sun.y-camForward.y) )); //would divide by z but z = 1
	float sunAngleX = abs(tan( (cam_to_sun.x-camForward.x) )); //would divide by z but z = 1

	int numSamples = MAX_SAMPLES;
	float sunAngle_totalexcess = 0.0f;
	bool flag_sunOutsideFrustum = false;
	// If Sun lies outside the camera frustum reduce num samples
	if(sunAngleY > camera.tanFovBy2.y)
	{
		sunAngle_totalexcess += sunAngleY-camera.tanFovBy2.y;
	}
	if(sunAngleX > camera.tanFovBy2.x)
	{
		sunAngle_totalexcess += sunAngleX-camera.tanFovBy2.x;
	}

	//reduce numSamples if sun outside frustum
	if(flag_sunOutsideFrustum)
	{
		numSamples = int(float(numSamples)*(sunAngle_totalexcess/PI));
	}	

	////////////////?????????????????????????
	//TODO: Use gradient when sun off screen

	vec4 sunPos_ndc = camera.proj * camera.view * sunLocation; 	// Sun Pos in NDC space

	//Clamping ensures that the screen space position is always some pixel that can be accessed in the image being sampled
	//This helps avoid weirdness due to the sun not lying inside the frustum 
	vec2 sunPos_ss = clamp((sunPos_ndc.xy+1.0f)/2.0f, 0.0, 1.0); //Sun Pos in Screen Space

	// Fix For Angling the god rays correctly --> screen space stuff doesnt account for z so the sun/light source can be 
	//behind you but the shader will assum its still in front of you
	if( dot(cam_to_sun, camForward) < 0.0f )
	{
		sunPos_ss = -sunPos_ss;
	}

	vec3 accumColor = vec3(0.0f, 0.0f, 0.0f);
	float illuminationDecay = 1.0f;
	// Calculate the radial change in uv coords in a direction towards the light source, i.e the sun
	vec2 delta_uv = ((uv - sunPos_ss)/float(numSamples)) * ATMOSPHERE_DENSITY;

	// Take samples along the vector from the pixel to the light source;
	//Use these samples to calculate an accumulated color
	for (int i = 0; i < numSamples; i++)
	{
		vec4 sampleValue = texture(godRayCreationDataSampler, uv);
		vec3 sampleColor = sampleValue.rgb * sampleValue.a; // Retrieve sample at new location.		
		sampleColor *= illuminationDecay * WEIGHT_FOR_SAMPLE; // Apply sample attenuation scale/decay factors.
		accumColor += sampleColor;
		
		illuminationDecay *= DECAY; // Update exponential decay factor.		
		uv -= delta_uv; // Step sample location along ray.
	}

	vec4 god_ray_color = vec4( accumColor*EXPOSURE, 1.0 );
	vec4 originalpixelColor = imageLoad( currentFrameResultImage, pixelPos );
	vec4 final_color = originalpixelColor + god_ray_color;

	imageStore( currentFrameResultImage, pixelPos, originalpixelColor );
}