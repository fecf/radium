# Radium
Radium is an minimal image viewer for Windows.  

<img src="https://github.com/fecf/radium/assets/6128431/cf59e2f5-46a4-47e2-9a55-8b9de0b8d5ef" height="150">
<img src="https://github.com/fecf/radium/assets/6128431/f95d720c-dc56-4156-8158-fe29eb930f36" height="150">
<img src="https://github.com/fecf/radium/assets/6128431/16f9c445-d7e0-4cfa-a646-2f67ce51ecc5" height="150">

### Download
- [radium.exe](https://github.com/fecf/radium/releases/download/0.3.3/radium.exe) v0.3.3 (2024/01/16)

### Features
- Zero configuration
- Distraction-free user interface
- No install required, single binary file
- Supports many formats (avif, jpeg, png, bmp, gif, tiff, psd, jxr, ico, pnm, pgm, ppm, pic, tga)
- Supports HDR images (avif, png)
- Supports over 32K resolution losslessly

### Plans
- [ ] Basic post-processing (blightness, contrast, gamma)
- [ ] Basic editing (resize, crop, save image)
- [ ] Supports layered formats

### Usage
- Menu: RightClick
- Open: RightClick, Ctrl+O
- Thumbnail: Space
- Rotate: Shift+W, Shift+R
- Zoom: Ctrl+WheelUp/Down, Ctrl+1~4, Ctrl+0
- Pan: MiddleClick+Drag

### Build
- git clone https://github.com/fecf/radium && cd radium
- vcpkg install --triplet x64-windows-static
- premake5 vs2022
- open build/radium.sln
