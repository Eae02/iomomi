This is a 3D puzzle game about gravity manipulation.

### Play / Download

[Browser Version](https://eae02.github.io/iomomi/) - Requires WebGL2, and doesn't include levels with water (may also be somwhat buggy).

[Linux Binary](https://www.dropbox.com/s/7kx4t9a080srckg/iomomi_linux.tar.gz?dl=1) - Requires OpenGL 4.3 or Vulkan 1.0. Some libraries are included but you also need to have SDL2 and (optionally for audio) OpenAL installed.

[Windows Binary](https://www.dropbox.com/s/x43rrmr52xjr7lj/iomomi_windows.zip?dl=1) - Requires OpenGL 4.3 or Vulkan 1.0.

All versions need S3 Texture Compression, and potentially some other GPU features.

![Ingame Screenshot](https://raw.githubusercontent.com/Eae02/iomomi/main/screenshot.jpg)

### Compiling

To compile on linux you need to have the following libraries installed: `sdl2`, `zlib`, `freetype`, `openal`, `yaml-cpp`, `glslang`, `nlohmann-json`, `libogg`, `libvorbis`, `libvorbisfile`, `protobuf`. You also need `gcc`, `make`, `cmake`, `xxd`, `protoc` and `glslangValidator`.

To get the correct fonts, you need to have the fonts *Orbitron* and *DejaVu Sans* installed when running the game for the first time.

Make sure you clone the repository with git submodules 
(`--recurse-submodules`), then, from the repository root:

```bash
./PrepareBuild.sh
mkdir .build
cd .build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

You can then run the game by running `./Bin/Release-Linux/iomomi` from the repository root.
