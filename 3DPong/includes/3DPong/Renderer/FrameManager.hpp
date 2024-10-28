#pragma once

//defines a frame manager for rendering to the screen

#include <3DPong/Engine.hpp>

#include <TyGUI/WidgetRenderer.hpp>

#include <BTDSTD/Wireframe/FrameBuffer.hpp>
#include <BTDSTD/Wireframe/SyncObjects.hpp>
#include <BTDSTD/Wireframe/CommandBuffer.hpp>

namespace Pong3D::Renderer
{
	//defines a frame
	struct Frame
	{
		bool isValid = true;

		uint32_t swapchainImageIndex;
		Wireframe::CommandBuffer::CommandBuffer cmd;
	};

	//defines a renderer frame manager
	struct FrameRenderManager
	{
		Pong3D::Core::Engine* engine;
		Wireframe::Device::GPU* GPU;

		int _frameNumber = 0;

		Smok::Memory::DeletionQueue renderObjectsDeleteQueue;
		Wireframe::Renderpass::Renderpass renderpass;

		VkSemaphore _presentSemaphore, _renderSemaphore;
		VkFence _renderFence;

		Wireframe::CommandBuffer::CommandPool commandPool;

		std::vector<VkFramebuffer> _framebuffers;

		//defines data for render operations
		Wireframe::Renderpass::RenderOperation::RenderPassData renderpassData;

		//handles ImGUI widgets
		bool TyGUIIsInitalized = false;
		bool TyGUIWidgetsShouldRender = false;
		TyGUI::WidgetRenderer widgetRenderer; //manages ImGUI

		//generates the render pass
		inline bool GenerateRenderPass()
		{
			//we define an attachment description for our main color image
			//the attachment is loaded as "clear" when renderpass start
			//the attachment is stored when renderpass ends
			//the attachment layout starts as "undefined", and transitions to "Present" so its possible to display it
			//we dont care about stencil, and dont use multisampling
			const std::vector<Wireframe::Util::Attachment::Attachment> attachments = { Wireframe::Util::Attachment::GenerateDefaultAttachment_Color(engine->swapchain._swachainImageFormat),
			Wireframe::Util::Attachment::GenerateDefaultAttachment_DepthStencil(engine->swapchain._depthFormat) };
			/*
			0 = color attachment
			1 = depth attachment
			*/

			//we are going to create 1 subpass, which is the minimum you can do
			Wireframe::Util::Subpass::Subpass subpass;
			subpass.colorAttachments.emplace_back(attachments[0]);
			subpass.depthStencilAttachments.emplace_back(attachments[1]);
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass.isDirty = true;

			std::vector<Wireframe::Util::Subpass::SubpassDependency> dependencies = {
				//0 dependency, which is from "outside" into the subpass. And we can read or write color
				Wireframe::Util::Subpass::GenerateDefault_SubpassDependency_ColorReadAndWrite(),

				//1 dependency from outside to the subpass, making this subpass dependent on the previous renderpasses
				Wireframe::Util::Subpass::GenerateDefault_SubpassDependency_DepthWriteData()
			};

			Wireframe::Renderpass::Renderpass_CreateInfo renderpassInfo;
			renderpassInfo.dependencies = dependencies;
			renderpassInfo.subpass = subpass;
			renderpassInfo.attachments = attachments;

			renderpass.Create(renderpassInfo, &engine->GPU);

			return true;
		}

		//initalizes TyGUI
		inline bool TyGUI_Init(bool TyGUIShouldRender = true)
		{
			TyGUI::WidgetRenderer_CreateInfo info;
			info.isDynamicRender = false; //ignored for now but only supported when Wireframe is in 1.3 mode
			if (!widgetRenderer.Init(&engine->GPU, &engine->window, renderpass._renderPass, info))
				return false;

			TyGUIIsInitalized = true;
			TyGUIWidgetsShouldRender = TyGUIShouldRender;
			return true;
		}

		//deinitalizes TyGUI
		inline void TyGUI_Shutdown()
		{
			TyGUIWidgetsShouldRender = false;
			if (!TyGUIIsInitalized)
				return;

			widgetRenderer.Shutdown(&engine->GPU);
			TyGUIIsInitalized = false;
		}

		//inits the renderer
		inline bool Init(Pong3D::Core::Engine* _engine)
		{
			engine = _engine;
			GPU = &engine->GPU;

			//create render pass
			bool state = GenerateRenderPass();
			if (!state)
				return false;
			renderObjectsDeleteQueue.push_function([&]() {
				renderpass.Destroy(GPU);
				});

			//framebuffer
			Wireframe::FrameBuffer::FrameBufferCreate(_framebuffers, engine->swapchain._swapchainImages.size(), engine->swapchain._swapchainImageViews.data(), engine->swapchain._depthImageView,
				renderpass._renderPass, engine->window._windowExtent, GPU);
			renderObjectsDeleteQueue.push_function([&]() {
				Wireframe::FrameBuffer::DestroyFrameBuffers(_framebuffers, GPU);
				});

			//creates the pool
			Wireframe::CommandBuffer::CommandPool_CreateInfo info;
			info.canBeReset = true; info.queueFamilyIndex = engine->GPU.graphicsQueueFamily;
			commandPool.Create(info, GPU);

			//allocate our command buffer
			commandPool.AllocateCommandBuffers(1, VkCommandBufferLevel::VK_COMMAND_BUFFER_LEVEL_PRIMARY, GPU);
			renderObjectsDeleteQueue.push_function([&]() {
				commandPool.Destroy(GPU);
				});

			Wireframe::SyncObjects::Fence_Create(_renderFence, GPU);
			Wireframe::SyncObjects::Semaphore_Create(_presentSemaphore, GPU);
			Wireframe::SyncObjects::Semaphore_Create(_renderSemaphore, GPU);
			renderObjectsDeleteQueue.push_function([&]() {
				Wireframe::SyncObjects::Semaphore_Destroy(_renderSemaphore, GPU);
				Wireframe::SyncObjects::Semaphore_Destroy(_presentSemaphore, GPU);
				Wireframe::SyncObjects::Fence_Destroy(_renderFence, GPU);
				});

			return state;
		}

		//destroys the renderer
		inline void Shutdown()
		{
			if (TyGUIIsInitalized)
				TyGUI_Shutdown();

			renderObjectsDeleteQueue.flush();
		}

		//starts frame
		inline Frame StartFrame()
		{
			Wireframe::Device::GPU* GPU = &engine->GPU;
			Wireframe::Swapchain::DesktopSwapchain* swapchain = &engine->swapchain;

			//wait until the gpu has finished rendering the last frame. Timeout of 1 second and reset it
			VK_CHECK(vkWaitForFences(GPU->device, 1, &_renderFence, true, 1000000000));
			VK_CHECK(vkResetFences(GPU->device, 1, &_renderFence));

			Frame frame;
			frame.cmd = commandPool.commandBuffers[0];

			//now that we are sure that the commands finished executing, we can safely reset the command buffer to begin recording again.
			frame.cmd.Reset();

			//request image from the swapchain
			VK_CHECK(vkAcquireNextImageKHR(GPU->device, swapchain->_swapchain, 1000000000, _presentSemaphore, nullptr, &frame.swapchainImageIndex));

			frame.cmd.StartRecording(); //starts recording

			//make a clear-color from frame number. This will flash with a 120 frame period.
			float flash = abs(sin(_frameNumber / 120.f));
			renderpassData.clearColor = { { 0.0f, 0.0f, flash, 1.0f } };

			renderpassData.renderSize = engine->window._windowExtent; //sets the size for rendering

			//starts the render pass
			Wireframe::Renderpass::RenderOperation::StartRenderPass_InlinedContent(frame.cmd.handle, renderpass._renderPass, _framebuffers[frame.swapchainImageIndex], renderpassData);
		
			return frame;
		}

		//submits frame
		inline void SubmitFrame(Frame& frame)
		{
			//renders ImGUI widget data
			if (TyGUIWidgetsShouldRender)
				widgetRenderer.Render(frame.cmd.handle);

			//finalize the render pass
			Wireframe::Renderpass::RenderOperation::EndRenderPass(frame.cmd.handle);

			frame.cmd.EndRecording(); //stops recording

			//prepare the submission to the queue. 
			//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
			//we will signal the _renderSemaphore, to signal that rendering has finished

			VkSubmitInfo submit = vkinit::submit_info(&frame.cmd.handle);
			VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

			submit.pWaitDstStageMask = &waitStage;

			submit.waitSemaphoreCount = 1;
			submit.pWaitSemaphores = &_presentSemaphore;

			submit.signalSemaphoreCount = 1;
			submit.pSignalSemaphores = &_renderSemaphore;

			//submit command buffer to the queue and execute it.
			// _renderFence will now block until the graphic commands finish execution
			VK_CHECK(vkQueueSubmit(GPU->graphicsQueue, 1, &submit, _renderFence));

			//prepare present
			// this will put the image we just rendered to into the visible window.
			// we want to wait on the _renderSemaphore for that, 
			// as its necessary that drawing commands have finished before the image is displayed to the user
			VkPresentInfoKHR presentInfo = vkinit::present_info();

			presentInfo.pSwapchains = &engine->swapchain._swapchain;
			presentInfo.swapchainCount = 1;

			presentInfo.pWaitSemaphores = &_renderSemaphore;
			presentInfo.waitSemaphoreCount = 1;

			presentInfo.pImageIndices = &frame.swapchainImageIndex;

			VK_CHECK(vkQueuePresentKHR(GPU->graphicsQueue, &presentInfo));

			//increase the number of frames drawn
			_frameNumber++;
		}

		//enables TyGUI widget rendering
		inline void EnableTyGUIWidgetRendering()
		{
			if (!TyGUIIsInitalized)
			{
				DisableTyGUIWidgetRendering(); //disable it just in case
				fmt::print(fmt::fg(fmt::color::red), "PS ENGINE RENDER MANAGER ERROR: TyGUI || EnableTyGUIWidgetRendering || TyGUI has NOT been initalized, we won't do it for you! Call \"TyGUI_Init\".\n");
				return;
			}

			TyGUIWidgetsShouldRender = true;
		}

		//disables TyGUI widget rendering
		inline void DisableTyGUIWidgetRendering() { TyGUIWidgetsShouldRender = false; }
	};
}