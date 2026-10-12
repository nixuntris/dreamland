This is my voxel engine I call "dreamland".
It's meant to be a sandbox game, somewhat akin to what John Lin aimed for. The most up to date explanation of the technical details are up on: https://jankysoftware.org/ under voxel rasterizer. 
The general idea is that it's an attempt at implementing virtualized geometry for voxels, it's a very messy as of now, and incomplete.

# Major:
It will not launch on nvidia cards, bindless textures have an odd behavior of having different syntax between AMD and Nvidia. This will be fixed in the future but it requires a pre-processor, which I haven't written yet.
It hasn't been tested on intel cards, I develop on AMD hardware.

# Requirments
An x86 CPU with AVX2 instructions
A modern GPU, must support opengl 4.6 with extensions (For now, an AMD GPU) with at least 4 gb of vram
8GB of ram

# Dependencies:

I use vcpkg for dependencies.
Raylib
Opengl
GLFW
ImGui
OpenMP

It's currently being developed by only me using opengl 4.6 with extensions.

Controls:
WASD - for movement
Mouse for controlling camera rotation
Left and Right button for building and destruction
K to enable the GUI
Scroll wheel spawns physics entities
# Worth noting:
I didn't hand write the abstraction, it was primarily generated using an LLM. It's holding me back quite a bit, it needs a fundamental rewrite and changes.
