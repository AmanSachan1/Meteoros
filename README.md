# Meteoros

## Demo


*polished version with more feaures to be released over winter break (before 2nd week of Jan)*

## Overview

This project is a real-time cloudscape renderer in Vulkan that was made as the final project for the University of Pennsylvania course, CIS 565: GPU Programming and Architecture. It is based on the cloud system NUBIS that was implemented for the Decima Engine by Guerrilla Games. The clouds were made for the game 'Horizon Zero Dawn' and were described in the following SIGGRAPH 2015 and 2017 presentations: 

* [2015](https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn) The Real-time Volumetric Cloudscapes of Horizon Zero Dawn

* [2017](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine) Nubis: Authoring Realtime Volumetric Cloudscapes with the Decima Engine 

Contributors:
1. Aman Sachan - M.S.E. Computer Graphics and Game Technology, UPenn
2. Meghana Seshadri - M.S.E. Computer Graphics and Game Technology, UPenn

Skip Forward to:
[Instructions](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
[Features](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
 - [Current](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
 - [Upcoming](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
[Implementation Overview](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
 - [Ray-Marching](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
 - [Modelling](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
 - [Lighting](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
 - [Rendering](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
[Pipeline Overview](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
[Optimizations](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
[Notes](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)
[Bloopers](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)

## Instructions

If you wish to run or develop on top of this program, please refer to the [INSTRUCTION.md](https://github.com/Aman-Sachan-asach/Meteoros/blob/master/INSTRUCTION.md) file.

## Features
### Current
### Upcoming 

## Implementation Overview 

### Modeling

Read in 3D textures for low and high frequency noise. The low frequency noise forms the base shape of the clouds, and the high frequency noise erodes the edges of the clouds to form them into finer shapes.

Low frequency texture = perlin-worley + 3 worley's

Perlin-Worley
![](/images/perlinworleyNoise.png)

Worley 1
![](/images/worleyNoiseLayer1.png)

Worley 2
![](/images/worleyNoiseLayer2.png)

Worley 3
![](/images/worleyNoiseLayer3.png)


High frequency = 3 worley's at higher frequencies
![](/images/highFrequencyDetail.png)


Curl Noise is used to simulate wind and other motion effects on the clouds. Sample along with the high frequency clouds.

![](/images/curlNoise.png)


![](/images/cloudmodelling.png)
![](/images/erodeclouds.png)
![](/images/modellingClouds.png)
![](/images/modellingClouds1.png)



### Lighting

The lighting model consists of 3 different probabilities:

* Beer-Lambert
* Henyey-Greenstein
* In-scattering

![](/images/FinalLightingModel.png)


1. Beers Law

![](/images/beerslaw.png)


2. Henyey-Greenstein

![](/images/beerspowderlaw.png)


3. In-Scattering



### Rendering 


### Optimizations and Post Processing 

![](/images/sampleoptimisation.png)


### Vulkan

Describe the Vulkan graphics and compute pipeline set up here.

#### Graphics Pipeline
![](/images/SimplifiedPipeline.png)


## Performance Analysis 

Performance analysis conducted on: Windows 10, i7-7700HQ @ 2.8GHz 32GB, GTX 1070(laptop GPU) 8074MB (Personal Machine: Customized MSI GT62VR 7RE)


## Resources, Upcoming Features, Other Notes

### Resources
- [Curl Noise Textures](http://bitsquid.blogspot.com/2016/07/volumetric-clouds.html)
- [Image Loading Library](https://github.com/nothings/stb)
- [Obj Loading Library](https://github.com/syoyo/tinyobjloader)
- [Setting Up Compute Shader that writes to a texture that is sampled by the fragment shader]( https://github.com/SaschaWillems/Vulkan/tree/master/examples/raytracing)
- [Why to include stb in .cpp file](https://stackoverflow.com/questions/43348798/double-inclusion-and-headers-only-library-stbi-image)
- FBM Procedural Noise Joe Klinger 
- Preetham Sun/Sky model from and Project Marshmallow 

### Upcoming Featurse
- Fully functional reprojection optimization 

### Notes
- We did not add checks to make sure some features are supported by the GPU before using them, such as anisotropic filtering.


## Bloopers

* Tone Mapping Madness

![](/images/READMEImages/meg01.gif)


* Sobel's "edgy" clouds

![](/images/READMEImages/meg02.gif)