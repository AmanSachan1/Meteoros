#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform sampler2D prevFrameImage;
layout (set = 0, binding = 1, rgba8) uniform image2D currentFrameResultImage;

layout(location = 0) in vec2 in_uv;
layout(location = 0) out vec4 outColor;

layout (set = 1, binding = 0) uniform CameraUBO
{
    mat4 view;
    mat4 proj;
    vec4 eye;
    vec2 tanFovBy2;
} camera;

layout (set = 2, binding = 0) uniform CameraOldUBO
{
    mat4 view;
    mat4 proj;
    vec4 eye;
    vec2 tanFovBy2;
} cameraOld;

layout (set = 3, binding = 0) uniform TimeUBO
{
    vec4 haltonSeq1;
    vec4 haltonSeq2;
    vec4 haltonSeq3;
    vec4 haltonSeq4;
    vec2 time; //stores delat time and total time
    int frameCountMod16;
};

struct Ray {
    vec3 origin;
    vec3 direction;
};

struct Intersection {
    vec3 normal;
    vec3 point;
    bool valid;
    float t;
};

//Global Defines for math constants
#define EPSILON 0.0000000001

#define TEXELSIZE vec2(1.0, 1.0)
#define FEED_BACK_MIN 0.0
#define FEED_BACK_MAX 0.5

//Global Defines for Earth and Cloud Layers 
#define EARTH_RADIUS 6371000.0 // earth's actual radius in km = 6371
#define ATMOSPHERE_RADIUS_INNER (EARTH_RADIUS + 7500.0) //paper suggests values of 15000-35000m above

//--------------------------------------------------------
//					TOOL BOX FUNCTIONS
//--------------------------------------------------------

vec2 getJitterOffset (in int index, ivec2 dim) 
{
    //index is a value from 0-15
    //Use pre generated halton sequence to jitter point --> halton sequence is a low discrepancy sampling pattern
    vec2 jitter = vec2(0.0);
    index = index/2;
    if(index < 4)
    {
        jitter.x = haltonSeq1[index];
        jitter.y = haltonSeq2[index];
    }
    else
    {
        index -= 4;
        jitter.x = haltonSeq3[index];
        jitter.y = haltonSeq4[index];
    }
    return jitter/dim;
}

// //Compute Ray for ray marching based on NDC point
Ray castRay( in vec2 screenPoint, in vec3 eye, in mat4 view, in vec2 tanFovBy2, int pixelID, ivec2 dim )
{
	Ray r;

    // Extract camera information from uniform
	vec3 camRight = normalize(vec3( view[0][0], 
				    				view[1][0], 
				    				view[2][0] ));
	vec3 camUp =    normalize(vec3( view[0][1], 
				    				view[1][1], 
				    				view[2][1] ));
	vec3 camLook =  -normalize(vec3(view[0][2], 
				    				view[1][2], 
				    				view[2][2] ));

	// Compute ndc space point from screenspace point //[-1,1] to [0,1] range
    vec2 NDC_Space_Point = screenPoint * 2.0 - 1.0; 

    //Jitter point with halton sequence
    NDC_Space_Point += getJitterOffset(pixelID, dim);

    //convert to camera space
    vec3 cam_x = NDC_Space_Point.x * tanFovBy2.x * camRight;
    vec3 cam_y = NDC_Space_Point.y * tanFovBy2.y * camUp;
    //convert to world space
    vec3 ref = eye + camLook;
    vec3 p = ref + cam_x + cam_y; //facing the screen

    r.origin = eye;
    r.direction = normalize(p - eye);

    return r;
}

//Sphere Intersection Testing
Intersection raySphereIntersection(in vec3 rO, in vec3 rD, in vec3 sphereCenter, in float sphereRadius)
{
    Intersection isect;
    isect.valid = false;
    isect.point = vec3(0.0);
    isect.normal = vec3(0.0, 1.0, 0.0);

    // Transform Ray such that the spheres move down, such that the camera is close to the sky dome
    // Only change sphere origin because you can't translate a direction
    rO -= sphereCenter;
    rO /= sphereRadius;

    float A = dot(rD, rD);
    float B = 2.0*dot(rD, rO);
    float C = dot(rO, rO) - 1.0; //uniform sphere
    float discriminant = B*B - 4.0*A*C;

    //If the discriminant is negative, then there is no real root
    if(discriminant < 0.0)
    {
        return isect;
    }

    float t = (-B - sqrt(discriminant))/(2.0*A);
    
    if(t < 0.0) 
    {
        t = (-B + sqrt(discriminant))/(2.0*A);
    }

    if(t >= 0.0)
    {
        vec3 p = vec3(rO + t*rD);
        isect.valid = true;
        isect.normal = normalize(p);

        p *= sphereRadius;
        p += sphereCenter;

        isect.point = p;
        isect.t = length(p-rO);
    }

    return isect;
}

float Luminance(vec3 color_rgb)
{
    const vec3 weight = vec3(0.2125, 0.7154, 0.0721); //Weighting according to human eye perception
    return dot(color_rgb, weight);
}

vec4 clip_aabb(vec3 aabb_min, vec3 aabb_max, vec4 p, vec4 q)
{
	// https://www.gdcvault.com/play/1022970/Temporal-Reprojection-Anti-Aliasing-in
	// note: only clips towards aabb center (but fast!) --> prper line box clipping is slow
	vec3 p_clip = 0.5 * (aabb_max + aabb_min);
	vec3 e_clip = 0.5 * (aabb_max - aabb_min) + EPSILON;

	vec4 v_clip = q - vec4(p_clip, p.w);
	vec3 v_unit = v_clip.xyz / e_clip;
	vec3 a_unit = abs(v_unit);
	float ma_unit = max(a_unit.x, max(a_unit.y, a_unit.z));

	if (ma_unit > 1.0)
	{
		return vec4(p_clip, p.w) + v_clip / ma_unit;
	}
	else
	{
		return q;// point inside aabb
	}
}

void neighbourHoodClamping(vec2 pixelPos, out vec4 cmin, out vec4 cmax, out vec4 cavg)
{
	//https://github.com/playdeadgames/temporal/blob/master/Assets/Shaders/TemporalReprojection.shader
	//3x3 minmax rounded neighbourhood sampling

	vec2 du = vec2(TEXELSIZE.x, 0.0);
	vec2 dv = vec2(0.0, TEXELSIZE.y);

	vec4 ctl = imageLoad( currentFrameResultImage, ivec2(pixelPos - dv - du)); //top left
	vec4 ctc = imageLoad( currentFrameResultImage, ivec2(pixelPos - dv));		 //top center
	vec4 ctr = imageLoad( currentFrameResultImage, ivec2(pixelPos - dv + du)); //top right
	vec4 cml = imageLoad( currentFrameResultImage, ivec2(pixelPos - du));		 //middle left
	vec4 cmc = imageLoad( currentFrameResultImage, ivec2(pixelPos));			 //middle center
	vec4 cmr = imageLoad( currentFrameResultImage, ivec2(pixelPos + du));		 //middle right
	vec4 cbl = imageLoad( currentFrameResultImage, ivec2(pixelPos + dv - du)); //bottom left
	vec4 cbc = imageLoad( currentFrameResultImage, ivec2(pixelPos + dv));		 //bottom center
	vec4 cbr = imageLoad( currentFrameResultImage, ivec2(pixelPos + dv + du)); //bottom right

	cmin = min(ctl, min(ctc, min(ctr, min(cml, min(cmc, min(cmr, min(cbl, min(cbc, cbr))))))));
	cmax = max(ctl, max(ctc, max(ctr, max(cml, max(cmc, max(cmr, max(cbl, max(cbc, cbr))))))));
	cavg = (ctl + ctc + ctr + cml + cmc + cmr + cbl + cbc + cbr) / 9.0;

	vec4 cmin5 = min(ctc, min(cml, min(cmc, min(cmr, cbc))));
	vec4 cmax5 = max(ctc, max(cml, max(cmc, max(cmr, cbc))));
	vec4 cavg5 = (ctc + cml + cmc + cmr + cbc) / 5.0;
	cmin = 0.5 * (cmin + cmin5);
	cmax = 0.5 * (cmax + cmax5);
	cavg = 0.5 * (cavg + cavg5);
}

void main()
{
	//A lot of this has simply been ported from the reprojection compute shader
	ivec2 dim = imageSize(currentFrameResultImage);
	ivec2 pixelPos = clamp(ivec2(round(float(dim.x) * in_uv.x), round(float(dim.y) * in_uv.y)), ivec2(0.0), ivec2(dim.x - 1, dim.y - 1));
    int pixelID = frameCountMod16;

	vec3 eyePos = -camera.eye.xyz;
	Ray ray = castRay(in_uv, eyePos, camera.view, camera.tanFovBy2, pixelID, dim);

	// Hit the inner sphere with the ray you just found to get some basis world position along your current ray
	vec3 earthCenter = eyePos;
	earthCenter.y = -EARTH_RADIUS; //move earth below camera
	Intersection atmosphereInnerIsect = raySphereIntersection(ray.origin, ray.direction, earthCenter, ATMOSPHERE_RADIUS_INNER);

    vec3 oldCameraRayDir = normalize((cameraOld.view * vec4(atmosphereInnerIsect.point, 1.0f)).xyz);

    // We have a normalized ray that we need to convert into uv space
    // We can achieve this by multiplying the xy componentes of the ray by the camera's Right and Up basis vectors  and scaling it down to a 0 to 1 range
    // In other words the ray goes from camera space <x,y,z> to uv space <u,v,1> using the R U F basis that defines the camera
    oldCameraRayDir /= -oldCameraRayDir.z; //-z because in camera space we look down negative z and so we don't want 
                                           //to divide by a negative number --> that would make our uv's negative
    float old_u = (oldCameraRayDir.x / (camera.tanFovBy2.x)) * 0.5 + 0.5;
    float old_v = (oldCameraRayDir.y / (camera.tanFovBy2.y)) * 0.5 + 0.5;
    vec2 old_uv = vec2(old_u, old_v); //if old_uv is out of range -> the texture sampler simply returns black --> keep in mind when porting

    //Current Pixel Neighborhood color space bounds
    vec4 cmin, cmax, cavg;
    neighbourHoodClamping(pixelPos, cmin, cmax, cavg);

    vec4 currColor = imageLoad( currentFrameResultImage, pixelPos );
	vec4 prevColor = texture( prevFrameImage, old_uv );
	
	prevColor = clip_aabb(cmin.xyz, cmax.xyz, clamp(cavg, cmin, cmax), prevColor);

	float lum0 = Luminance(currColor.xyz);
	float lum1 = Luminance(prevColor.xyz);

	float unbiased_diff = abs(lum0 - lum1) / max(lum0, max(lum1, 0.2));
	float unbiased_weight = 1.0 - unbiased_diff;
	float unbiased_weight_sqr = unbiased_weight * unbiased_weight;
	float k_feedback = mix(FEED_BACK_MIN, FEED_BACK_MAX, unbiased_weight_sqr);

	vec4 color_TXAA = mix(prevColor, currColor, k_feedback);

	imageStore( currentFrameResultImage, pixelPos, color_TXAA );
	outColor = color_TXAA;
}