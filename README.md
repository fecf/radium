# Radium
Radium is an minimal image viewer for Windows.  

<img src="https://github.com/fecf/radium/assets/6128431/8b8c7aea-0206-470a-8f11-e6572f68a16a" height="200">
<img src="https://github.com/fecf/radium/assets/6128431/7f3d4e89-c1b8-444b-a85b-4f92fa6598da" height="200">

### Download
- [radium.exe](https://github.com/fecf/radium/releases/download/0.3.0/radium.exe) v0.3.0 (2024/01/07)

### Features
- Zero configuration
- Distraction-free user interface
- No install required, single binary file
- Supports many formats (avif, jpeg, png, bmp, gif, tiff, psd, jxr, ico, pnm, pgm, ppm, pic, tga)
- Supports over 32K resolution losslessly

### Plans
- [ ] Supports over 10-bit & HDR format
- [ ] Supports layered format

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
