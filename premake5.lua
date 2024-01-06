name = "radium"

workspace (name)
    configurations {"Debug", "Release"}
    platforms {"x64"}
    location "build"

project (name)
    kind "WindowedApp"
    toolset "v143"
    language "C++"
    cppdialect "C++20"
    files { 
        "src/**.c*",
        "src/**.h*",
        "src/**.rc*",
        "src/**.hlsl",
        "src/radium/embed/**.*",
        "src/radium/resource/**.*",
        "premake5.lua"
    }
    includedirs {
        "src",
        "src/third_party",
        "src/third_party/d3d12memoryallocator",
        "src/third_party/directx-headers/include",
        "src/third_party/imgui",
        "src/third_party/imgui/backends",
        "src/third_party/linalg",
        "src/third_party/nlohmann",
        "src/third_party/pnm",
        "src/third_party/stb",
        "src/third_party/wil/include",
        "src/third_party/wuffs",
        "src/third_party/entt",
        "build/generated",
        "vcpkg_installed/x64-windows-static/include",
    }
    removefiles {
        "temp/**",
        "src/third_party/directx-headers/include/**.h",
        "src/third_party/wuffs/**.c"
    }
    libdirs {
        "vcpkg_installed/x64-windows-static/lib",
    }
    links {
        "d3d12.lib",
        "dxgi.lib",
        "dxguid.lib",
        "avif.lib",
        "jpeg.lib",
        "turbojpeg.lib",
        "yuv.lib",
    }
    flags { 
         "MultiProcessorCompile",
    }
    objdir "build/obj/%{cfg.platform}/%{cfg.buildcfg}"
    targetdir "build/bin/%{cfg.platform}/%{cfg.buildcfg}"

    nuget { "Microsoft.Direct3D.D3D12:1.711.3-preview" }
    vpaths { ["*"] = "./src" }

    defines {
        "_SILENCE_ALL_CXX23_DEPRECATION_WARNINGS",
        "NOMINMAX",
        "WIN32_LEAN_AND_MEAN",
    }
    buildoptions { 
        "/utf-8",
    }
    editandcontinue "On"
    vectorextensions "avx"

    filter {"platforms:x64"}
        system "Windows"
        architecture "x86_64"
        linkoptions {"/subsystem:windows /entry:mainCRTStartup"}

    filter {"configurations:Debug"}
        defines { "_DEBUG" }
        optimize "Debug"
        symbols "On"
        pchsource "src/pch.cc"
        pchheader "pch.h"
        forceincludes {"pch.h"}

    filter {"configurations:Release"}
        defines { "NDEBUG" }
        optimize "Speed"
        pchsource "src/pch.cc"
        pchheader "pch.h"
        forceincludes {"pch.h"}

    filter "files:**.c"
        flags {"NoPCH"}
        forceincludes {"pch.h"}

    -- /Zpr: Packs matrices in row-major order by default
    filter { "files:src/**.hlsl" }
        local shader_name = "%{file.basename:gsub('%.', '_')}_vs"
        local shader_out = "%{prj.location}/generated/shader/%{file.basename}.vs.h"
        buildaction "none"
        buildmessage "compiling vs hlsl with dxc (%{file.relpath}) ..."
        buildcommands { "tools\\dxc\\bin\\x64\\dxc.exe -T vs_6_0 %{file.relpath} -E VS -Zpr -Fh " .. shader_out .. " -Vn " .. shader_name }
        buildoutputs { shader_out }

    filter { "files:src/**.hlsl" }
        local shader_name = "%{file.basename:gsub('%.', '_')}_ps"
        local shader_out = "%{prj.location}/generated/shader/%{file.basename}.ps.h"
        buildaction "none"
        buildmessage "compiling ps hlsl with dxc (%{file.relpath}) ..."
        buildcommands { "tools\\dxc\\bin\\x64\\dxc.exe -T ps_6_0 %{file.relpath} -E PS -Zpr -Fh " .. shader_out .. " -Vn " .. shader_name }
        buildoutputs { shader_out }

    filter { "files:src/radium/embed/**.*" }
        local embed_out = "%{prj.location}/generated/embed/%{file.basename}.h"
        buildcommands { "tools\\bin2cpp -l 80 --always-escape %{file.relpath} " .. embed_out }
        buildoutputs { embed_out }

    postbuildcommands {"mt.exe -manifest %{wks.name}.exe.manifest -outputresource:$(TargetPath)"}