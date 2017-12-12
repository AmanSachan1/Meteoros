# Meteoros
This project is a real-time cloudscape renderer in Vulkan that was made as the final project for the University of Pennsylvania course, CIS 565: GPU Programming and Architecture. It is based on the theory and implementation as described in the following SIGGRAPH 2015 and 2017 presentations: 

* [2015](https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn) The Real-time Volumetric Cloudscapes of Horizon Zero Dawn

* [2017](https://www.guerrilla-games.com/read/nubis-authoring-real-time-volumetric-cloudscapes-with-the-decima-engine) Nubis: Authoring Realtime Volumetric Cloudscapes with the Decima Engine 

Contributors:
1. Meghana Seshadri - M.S.E. Computer Graphics and Game Technology, UPenn
2. Aman Sachan - M.S.E. Computer Graphics and Game Technology, UPenn
 

Please see a more detailed description of the algorithms and set up below in the [Detailed Description](https://github.com/Aman-Sachan-asach/Meteoros#detailed-description)


## Instructions

If you wish to run or develop on top of this program, please refer to the [INSTRUCTION.md](https://github.com/Aman-Sachan-asach/Meteoros/blob/master/INSTRUCTION.md) file.


## Detailed Description


### Implementation Overview 

#### Modeling

Read in 3D textures for low and high frequency noise. The low frequency noise forms the base shape of the clouds, and the high frequency noise erodes the edges of the clouds to form them into finer shapes.

Low frequency texture = perlin-worley + 3 worley's

<img src="/images/perlinworleyNoise.png" width="243.25" height="231.25">
<img src="/images/worleyNoiseLayer1.png" width="243.25" height="231.25">
<img src="/images/worleyNoiseLayer2.png" width="243.25" height="231.25">
<img src="/images/worleyNoiseLayer3.png" width="243.25" height="231.25">


High frequency = 3 worley's at higher frequencies
![](/images/highFrequencyDetail.png)


Curl Noise is used to simulate wind and other motion effects on the clouds. Sample along with the high frequency clouds.

![](/images/curlNoise.png)


![](/images/cloudmodelling.png)
![](/images/erodeclouds.png)
![](/images/modellingClouds.png)
![](/images/modellingClouds1.png)



#### Lighting

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



#### Rendering 


#### Optimizations and Post Processing 

![](/images/sampleoptimisation.png)


#### Vulkan

Describe the Vulkan graphics and compute pipeline set up here.

##### Graphics Pipeline
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