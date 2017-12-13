# Meteoros Getting Started: Instructions

**The following will be required for the operation and/or development of the program:**

- An NVIDIA graphics card. Any card with Compute Capability 2.0 (sm_20) or greater will work. Check your GPU in this [compatibility table](https://developer.nvidia.com/cuda-gpus). 
- Vulkan 1.0.61
- Visual Studio 2015
- CMake
- Git (optional)


## Step 1: Setting up your development environment

### Windows

1. Make sure you are running Windows 7/8/10 and that your NVIDIA drivers are up-to-date
2. Install Visual Studio 2015
	* 2012/2013 will also work, if you already have one installed. 
	* 2010 doesn't work because glfw only supports 32-bit binaries for vc2010. **We don't provide libraries for Win32**
	* You need C++ support. None of the optional components are necessary.
3. Install Vulkan 1.0.61 or higher
	* Download Vulkan [here](https://www.khronos.org/vulkan/). Please note that Vulkan runs on Windows and Linux machines **only**. 
	* Make sure to run the downloaded installed as administrator so that the installer can set the appropriate environment variables for you.
	* To check that Vulkan is ready for use, go to your Vulkan SDK directory (C:/VulkanSDK/ unless otherwise specified) and run the cube.exe example within the Bin directory. IF you see a rotating gray cube with the LunarG logo, then you are all set!
	* If not, you may need to make sure your GPU driver supports Vulkan. If need be, download and install a [Vulkan driver](https://developer.nvidia.com/vulkan-driver) from NVIDIA's website.
4. Install [CMake]()
	* Windows binaries are under "Binary distributions"
5. Install [Git]()
	* Only necessary if you are forking this repository, **not** if you're downloading the project


### Linux

1. Install Vulkan 1.0.61
	* Download Vulkan [here](https://www.khronos.org/vulkan/). Please note that Vulkan runs on Windows and Linux machines **only**. 
	* Make sure to run the downloaded installed as administrator so that the installer can set the appropriate environment variables for you.
	* To check that Vulkan is ready for use, go to your Vulkan SDK directory (C:/VulkanSDK/ unless otherwise specified) and run the cube.exe example within the Bin directory. IF you see a rotating gray cube with the LunarG logo, then you are all set!
	* If not, you may need to make sure your GPU driver supports Vulkan. If need be, download and install a [Vulkan driver](https://developer.nvidia.com/vulkan-driver) from NVIDIA's website.
2. Install Git (`apt-get install git` on Debian/Ubuntu)
3. Install CMake (`apt-get install cmake` on Debian/Ubuntu)



## Step 2: Fork or download the code

1. Use GitHub to fork this repository into your own GitHub account.
2. If you haven't used Git, you'll need to set up a few things.
   * On Windows: In order to use Git commands, you can use Git Bash. You can right-click in a folder and open Git Bash there.
   * On Linux: Open a terminal.
   * Configure git with some basic options by running these commands:
     * `git config --global push.default simple`
     * `git config --global user.name "YOUR NAME"`
     * `git config --global user.email "GITHUB_USER@users.noreply.github.com"`
     * (Or, you can use your own address, but remember that it will be public!)
3. Clone from GitHub onto your machine:
   * Navigate to the directory where you want to keep your files, then clone your fork.
     * `git clone` the clone URL from your GitHub fork homepage.

* [How to use GitHub](https://guides.github.com/activities/hello-world/)
* [How to use Git](http://git-scm.com/docs/gittutorial)


## Step 3: Build and run

* `src/` contains the source code.
* `external/` contains the binaries and headers for GLFW, glm, tiny_obj, and stb_image.

**CMake note:** Do not change any build settings or add any files to your project directly in Visual Studio. Instead, edit the `src/CMakeLists.txt` file. Any files you add that is outside the `src/Cloudscapes` directory must be added here. If you edit it, just rebuild your VS project to make it update itself.


### Windows [EDIT]

1. In Git Bash, navigate to your cloned project directory.
2. Create a `build` directory: `mkdir build`
   	* (This "out-of-source" build makes it easy to delete the `build` directory and try again if something goes wrong with the configuration.)
3. Navigate into that directory: `cd build`
4. Open the CMake GUI to configure the project:
   	* `cmake-gui ..` or `"C:\Program Files (x86)\cmake\bin\cmake-gui.exe" ..`
   		* Don't forget the `..` part!
   	* Make sure that the "Source" directory is like `.../Meteoros`.
   	* Click *Configure*.  Select your version of Visual Studio, Win64. (**NOTE:** you must use Win64, as we don't provide libraries for Win32.)
   	* If you see an error like .................., 
   * Click *Generate*.
5. If generation was successful, there should now be a Visual Studio solution (`.sln`) file in the `build` directory that you just created. Open this (from the command line: `explorer *.sln`)
6. Build. (Note that there are Debug and Release configuration options.)
7. Run. Make sure you run the `Cloudscapes` target (not `ALL_BUILD`) by right-clicking it and selecting "Set as StartUp Project".
	* If you have switchable graphics (NVIDIA Optimus), you may need to force your program to run with only the NVIDIA card. In NVIDIA Control Panel, under "Manage 3D Settings," set "Multi-display/Mixed GPU acceleration" to "Single display performance mode".

### Linux [EDIT]

1. *File->Import...->General->Existing Projects Into Workspace*.
   * Select the Project 0 repository as the *root directory*.
3. Select the *Meteoros* project in the Project Explorer. Right click the project. Select *Build Project*.
   * For later use, note that you can select various Debug and Release build
     configurations under *Project->Build Configurations->Set Active...*.
4. If you see an error like ........................,
   * Click *Configure*, then *Generate*.
5. Right click and *Refresh* the project.
6. From the *Run* menu, *Run*. Select "Local C/C++ Application" and the `Meteoros` binary.



**Note:** 

While developing, you will want to keep validation layers enabled so that error checking is turned on. The project is set up such that when you are in debug mode, validation layers are enabled, and when you are in release mode, validation layers are disabled. After building the code, you should be able to run the project without any errors. Two windows will open -  one will either be blank or show errors if any (if you're running in debug mode), the other (pending no errors) will show the clouds with a sun and sky background above an ocean-like gradient.




## Other Notes

* Compile GLSL shaders into SPIR-V bytecode:
* **Windows ONLY** Create a compile.bat file with the following contents:

```
C:/VulkanSDK/1.0.17.0/Bin32/glslangValidator.exe -V shader.vert
C:/VulkanSDK/1.0.17.0/Bin32/glslangValidator.exe -V shader.frag
pause
```
Note: Replace the path to glslangValidator.exe with the path to where you installed the Vulkan SDK. Double click the file to run it.


