# Meteoros
This project is a real-time cloudscape renderer in Vulkan that was made as the final project for the University of Pennsylvania course, CIS 565: GPU Programming and Architecture. It is based on the theory and implementation as described in the following SIGGRAPH 2015 and 2017 presentations: 

* [2015](https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn) The Real-time Volumetric Cloudscapes of Horizon Zero Dawn

* [2017](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine) Nubis: Authoring Realtime Volumetric Cloudscapes with the Decima Engine 

Contributors:
1. Meghana Seshadri - M.S.E. Computer Graphics and Game Technology, UPenn
2. Aman Sachan - M.S.E. Computer Graphics and Game Technology, UPenn
 

![](/images/READMEImages/godrays.PNG)


Skip Forward to:
1. [Instructions](#Instructions)
2. [Features](#Features)
	- [Current](#Current)
	- [Upcoming](#Upcoming)
3. [Pipeline Overview](#Pipeline)
4. [Implementation Overview](#Implementation)
	- [Ray-Marching](#Raymarching)
	- [Modelling](#Modeling)
	- [Lighting](#Lighting)
	- [Rendering](#Rendering)
	- [Post-Processing](#Post)
5. [Optimizations](#Optimizations)
5. [Performance Analysis](#Performance)
6. [Notes](#Notes)
7. [Resources](#Resources)
8. [Bloopers](#Bloopers)


## Instructions

If you wish to run or develop on top of this program, please refer to the [INSTRUCTION.md](https://github.com/Aman-Sachan-asach/Meteoros/blob/master/INSTRUCTION.md) file.


## Features

### Current

### Upcoming
- Fully functional reprojection optimization 





## Pipeline Overview

### Vulkan

Describe the Vulkan graphics and compute pipeline set up here.

### Graphics Pipeline
![](/images/SimplifiedPipeline.png)




## Implementation Overview 

### Ray-Marching

### Modeling

Read in 3D textures for low and high frequency noise. The low frequency noise forms the base shape of the clouds, and the high frequency noise erodes the edges of the clouds to form them into finer shapes.

Low frequency texture = perlin-worley + 3 worley's

<img src="/images/perlinworleyNoise.png" width="243.25" height="231.25"> <img src="/images/worleyNoiseLayer1.png" width="243.25" height="231.25"> <img src="/images/worleyNoiseLayer2.png" width="243.25" height="231.25"> <img src="/images/worleyNoiseLayer3.png" width="243.25" height="231.25">


High frequency = 3 worley's at higher frequencies
![](/images/highFrequencyDetail.png)


Curl Noise is used to simulate wind and other motion effects on the clouds. Sample along with the high frequency clouds.

![](/images/curlNoise.png)


![](/images/cloudmodelling.png)
![](/images/erodeclouds.png)
![](/images/modellingClouds.png)
![](/images/modellingClouds1.png)



### Lighting

The lighting model as described in the 2017 presentation is an attenuation based lighting model. This means that you start with full intensity, and then reduce it as combination of the following 3 probabilities: 

1. Directional Scattering
2. Absorption / Out-scattering 
3. In-scattering


![](/images/READMEImages/lightingProbs.PNG)


#### Directional Scattering

This retains baseline forward scattering and produces silver lining effects. It is calculated using Henyey-Greenstein equation.

The eccentricity value that generally works well for mid-day sunlight doesn't provide enough bright highlights around the sun during sunset. 

![](/images/READMEImages/hg01.PNG)

Change the eccentricity to have more forward scattering, hence bringing the highlights around the sun. Clouds 90 degrees away from the sun, however, become too dark.

![](/images/READMEImages/hg02.PNG)

To retain baseline forward scattering behavior and get the silver lining highlights, combine 2 HG functions, and factors to control the intensity of this effect as well as its spread away from the sun.

![](/images/READMEImages/hg03.PNG)

![](/images/READMEImages/hg04.PNG)



#### Absorption / Out-scattering

This is the transmittance produced as a result of the Beer-Lambert equation. 

Beer's Law only accounts for attenuation of light and not the emission of light that has in-scattered to the sample point, hence making clouds too dark. 

![](/images/beerslaw.png)

![](/images/READMEImages/beer01.PNG)


By combining 2 Beer-Lambert equations, the attenuation for the second one is reduced to push light further into the cloud.

![](/images/READMEImages/beer02.PNG)



#### In-scattering
This produces the dark edges and bases to the clouds. 

In-scattering is when a light ray that has scattered in a cloud is combined with others on its way to the eye, essentially brightening the region of the cloud you are looking at. In order for this to occur, an area must have a lot of rays scattering into it, which only occurs where there is cloud material. This means that the deeper in the cloud, the more scattering contributors there are, and the amount of in-scattering on the edges of the clouds is lower, which makes them appear dark. Also, since there are no strong scattering sources below clouds, the bottoms of them will have less occurences of in-scattering as well. 

Only attenuation and HG phase: 

![](/images/READMEImages/in01.PNG)

Sampling cloud at low level of density, and accounting for attenuation along in-scatter path. This appears dark because there is little to no in-scattering on the edges.

![](/images/READMEImages/in02.PNG)

Relax the effect over altitude and apply a bias to compensate. 

![](/images/READMEImages/in03.PNG)

Second component accounts for decrease in-scattering over height. 

![](/images/READMEImages/in04.PNG)






### Rendering 


### Post Processing 


### Optimizations

![](/images/sampleoptimisation.png)




## Performance Analysis 

Performance analysis conducted on: Windows 10, i7-7700HQ @ 2.8GHz 32GB, GTX 1070(laptop GPU) 8074MB (Personal Machine: Customized MSI GT62VR 7RE)



## Notes
- We did not add checks to make sure some features are supported by the GPU before using them, such as anisotropic filtering.



## Resources

#### Texture Resources:
- [Low and High Frequency Noise Textures](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine) were made using the 'Nubis Noise Generator' houdini tool that was released along with the 2015 paper. 
- [Curl Noise Textures](http://bitsquid.blogspot.com/2016/07/volumetric-clouds.html)
- Weather Map Texture made by Dan Mccan

#### Libraries:
- [Image Loading Library](https://github.com/nothings/stb)
- [Obj Loading Library](https://github.com/syoyo/tinyobjloader)
- [Why to include stb in .cpp file](https://stackoverflow.com/questions/43348798/double-inclusion-and-headers-only-library-stbi-image)
- [Imgui](https://github.com/ocornut/imgui) for our partially wriiten gui

#### Vulkan
- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [RenderDoc](https://renderdoc.org/)
- [Setting Up Compute Shader that writes to a texture](https://github.com/SaschaWillems/Vulkan/tree/master/examples/raytracing)
- [3D Textures](https://github.com/SaschaWillems/Vulkan/tree/master/examples/texture3d)
- [Pipeline Caching](https://github.com/SaschaWillems/Vulkan/tree/master/examples/radialblur) was used for post-processing and so it made more sense to see how it is done for post processing
- [Radial Blur](https://github.com/SaschaWillems/Vulkan/tree/master/examples/radialblur)

#### Post-Processing:
- [Uncharted 2 Tone Mapping](http://filmicworlds.com/blog/filmic-tonemapping-operators/)
- [God Rays](https://developer.nvidia.com/gpugems/GPUGems3/gpugems3_ch13.html)

#### Upcoming Feature Set:
- [Off-screen Rendering](https://github.com/SaschaWillems/Vulkan/tree/master/examples/offscreen)
- [Off-screen Rendering](https://github.com/SaschaWillems/Vulkan/tree/master/examples/pushconstants)

#### Other Resources
- FBM Procedural Noise Joe Klinger
- Preetham Sun/Sky model from Project Marshmallow 


## Bloopers

* Tone Mapping Madness

![](/images/READMEImages/meg01.gif)


* Sobel's "edgy" clouds

![](/images/READMEImages/sobeltest.PNG)