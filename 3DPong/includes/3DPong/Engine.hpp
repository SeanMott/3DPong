#pragma once

//defines a core engine

#include <BTDSTD/Wireframe/Core/DesktopSwapchain.hpp>
#include <BTDSTD/ECS/ECSManager.hpp>

#include <Smok/Memory/LifetimeDeleteQueue.hpp>

namespace Pong3D::Core
{
	//defines core engine
	struct Engine
	{
		Smok::Memory::DeletionQueue engineObjectDeleteQueue;

		Wireframe::Window::DesktopWindow window;
		Wireframe::Device::GPU GPU;
		VmaAllocator _allocator;
		Wireframe::Swapchain::DesktopSwapchain swapchain;

		//---ECS subsystem data for all components

		//creates engine
		inline bool Init()
		{
			//creates Window
			Wireframe::Window::DesktopWindow_CreateInfo windowInfo;
			windowInfo.size = { 1700 , 900 }; windowInfo.title = "Survial Game OwO";
			if (!window.Create(windowInfo))
				return false;
			engineObjectDeleteQueue.push_function([&]() {
				window.Destroy();
				});

			//creates GPU
			Wireframe::Device::GPU_CreateInfo GPUInfo;

			GPUInfo.isDebug = true;

			GPUInfo.vulkanMajorVersion = 1;
			GPUInfo.vulkanMinorVersion = 3;

			//vulkan 1.3 features
			//GPUInfo.specific13FeaturesNeeded = true;
			//GPUInfo.features13.dynamicRendering = true;
			//GPUInfo.features13.synchronization2 = true;

			//vulkan 1.2 features
			GPUInfo.specific12FeaturesNeeded = true;
			GPUInfo.features12.bufferDeviceAddress = true;
			GPUInfo.features12.descriptorIndexing = true;

			if (!GPU.Create(GPUInfo, &window))
				return false;
			engineObjectDeleteQueue.push_function([&]() {
				window.DestroySurface(GPU.instance);
				GPU.Destroy();
				});

			//initialize the memory allocator
			VmaAllocatorCreateInfo allocatorInfo = {};
			allocatorInfo.physicalDevice = GPU.chosenGPU;
			allocatorInfo.device = GPU.device;
			allocatorInfo.instance = GPU.instance;
			VmaVulkanFunctions VMFuncs;
			VMFuncs.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
			VMFuncs.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
			VMFuncs.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
				VMFuncs.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
				VMFuncs.vkAllocateMemory = vkAllocateMemory,
				VMFuncs.vkFreeMemory = vkFreeMemory,
				VMFuncs.vkMapMemory = vkMapMemory,
				VMFuncs.vkUnmapMemory = vkUnmapMemory,
				VMFuncs.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
				VMFuncs.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
				VMFuncs.vkBindBufferMemory = vkBindBufferMemory,
				VMFuncs.vkBindImageMemory = vkBindImageMemory,
				VMFuncs.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
				VMFuncs.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
				VMFuncs.vkCreateBuffer = vkCreateBuffer,
				VMFuncs.vkDestroyBuffer = vkDestroyBuffer,
				VMFuncs.vkCreateImage = vkCreateImage,
				VMFuncs.vkDestroyImage = vkDestroyImage,
				VMFuncs.vkCmdCopyBuffer = vkCmdCopyBuffer,
				VMFuncs.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
				VMFuncs.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2,
				VMFuncs.vkBindBufferMemory2KHR = vkBindBufferMemory2,
				VMFuncs.vkBindImageMemory2KHR = vkBindImageMemory2,
				VMFuncs.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2,
				allocatorInfo.pVulkanFunctions = &VMFuncs;

			vmaCreateAllocator(&allocatorInfo, &_allocator);
			engineObjectDeleteQueue.push_function([&]() {
				vmaDestroyAllocator(_allocator);
				});


			//creates swapchain
			Wireframe::Swapchain::DesktopSwapchain_CreateInfo info;
			if (!swapchain.Create(info, &GPU, &window, _allocator))
				return false;
			engineObjectDeleteQueue.push_function([&]() {
				swapchain.Destroy(&GPU, _allocator);
				});

			return true;
		}

		//shutdown
		inline void Shutdown()
		{
			engineObjectDeleteQueue.flush();
		}
	};
}