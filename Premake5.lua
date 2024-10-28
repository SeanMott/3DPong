workspace "TestRendering"
architecture "x64"
startproject "3DPong"

configurations
{
    "Debug",
    "Release",
    "Dist"
}

include "BTDSTD3"
include "TyGUI"
include "SmokACT"
include "Smok"

---The game
project "3DPong"
location "3DPong"
kind "ConsoleApp"
language "C++"
targetdir ("bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Game")
objdir ("bin/%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Game")


files 
{
---base code
"3DPong/includes/**.h",
"3DPong/src/**.c",
"3DPong/includes/**.hpp",
"3DPong/src/**.cpp",

}

includedirs 
{
---base code
"3DPong/includes",

BTD_INCLUDE,
"BTDSTD3/" .. GLM_INCLUDE,
"BTDSTD3/" .. FMT_INCLUDE,
"BTDSTD3/" .. SDL_INCLUDE,

"BTDSTD3/" .. VK_BOOTSTRAP_INCLUDE,
"BTDSTD3/" .. STB_INCLUDE,
"BTDSTD3/" .. VOLK_INCLUDE,
"BTDSTD3/" .. VMA_INCLUDE,
VULKAN_SDK_MANUAL_OVERRIDE,

"TyGUI/includes",
"Venders/ImGUI",

--"C:/GameDev/SurvivalBasic/Smok/includes",
"Smok/includes",

--"C:/GameDev/SurvivalBasic/SmokACT/includes",
"SmokACT/includes",
}

links
{
"TyGUI",
"SmokACT"
}


defines
{
"GLM_FORCE_DEPTH_ZERO_TO_ONE",
"GLM_FORCE_RADIANS",
"GLM_ENABLE_EXPERIMENTAL",
}


flags
{
"MultiProcessorCompile",
"NoRuntimeChecks",
}


buildoptions
{
"/utf-8",
}


--platforms
filter "system:windows"
    cppdialect "C++20"
    staticruntime "On"
    systemversion "latest"


defines
{
"Window_Build",
"VK_USE_PLATFORM_WIN32_KHR",
"Desktop_Build",
}

filter "system:linux"
    cppdialect "C++20"
    staticruntime "On"
    systemversion "latest"


defines
{
"Linux_Build",
"VK_USE_PLATFORM_XLIB_KHR",
"Desktop_Build",
}


    filter "system:mac"
    cppdialect "C++20"
    staticruntime "On"
    systemversion "latest"


defines
{
"MacOS_Build",
"VK_USE_PLATFORM_MACOS_MVK",
"Desktop_Build",
}

--configs
filter "configurations:Debug"
    defines "BTD_DEBUG"
    symbols "On"

filter "configurations:Release"
    defines "BTD_RELEASE"
    optimize "On"

filter "configurations:Dist"
    defines "BTD_DIST"
    optimize "On"


defines
{
"NDEBUG",
}


flags
{
"LinkTimeOptimization",
}