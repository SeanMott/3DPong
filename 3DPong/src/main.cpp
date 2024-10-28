/*
Test rendering project Pong remade in 3D with the new Bytes The Dust Standard Library (3rd edition) and new Smok renderer.

- custom mesh loading
- GUI rendering
- mesh rendering
- local player input from multable controllers
*/

#include <3DPong/Engine.hpp>
#include <3DPong/Renderer/FrameManager.hpp>
#include <3DPong/Renderer/MeshRenderer.hpp>
#include <3DPong/ECS/Scene.hpp>

#include <BTDSTD/Time.hpp>
#include <BTDSTD/Input/KeyInput.hpp>

#include <BTDSTD/Wireframe/Core/GPU.hpp>
#include <BTDSTD/Wireframe/Core/DesktopSwapchain.hpp>
#include <BTDSTD/Wireframe/Core/DesktopWindow.hpp>
		 
#include <BTDSTD/Wireframe/Renderpass.hpp>
		 
#include <BTDSTD/Wireframe/CommandBuffer.hpp>
#include <BTDSTD/Wireframe/FrameBuffer.hpp>
#include <BTDSTD/Wireframe/SyncObjects.hpp>

#include <TyGUI/WidgetRenderer.hpp>

#include <Smok/Assets/Mesh.hpp>

#include <Smok/Components/Transform.hpp>
#include <Smok/Components/Camera.hpp>
#include <Smok/Components/MeshComponent.hpp>

#include <Smok/Memory/LifetimeDeleteQueue.hpp>

#include <SmokACT/ImportMesh.hpp>

#include <SDL_events.h>

#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

#include <chrono>
#include <thread>
#include <vector>


void init_pipelines(Wireframe::Pipeline::PipelineLayout& meshPipelineLayout, Wireframe::Pipeline::GraphicsPipeline& meshPipeline,
	Wireframe::Renderpass::Renderpass& renderpass,
	Pong3D::Core::Engine* engine)
{
	Wireframe::Device::GPU* GPU = &engine->GPU;

	Wireframe::Pipeline::PipelineSettings pipelineSettings;
	pipelineSettings.SetPipelineSettingToDefault_InputAssemblyState();
	pipelineSettings.SetPipelineSettingToDefault_RasterizerState();
	pipelineSettings.SetPipelineSettingToDefault_MultisampleState();
	pipelineSettings.SetPipelineSettingToDefault_ColorBlendAttachmentState();
	pipelineSettings.SetPipelineSettingToDefault_DepthStencilState();

	//pipelineSettings.SetPipelineSettingToDefault_ViewportAndScissorState();


	pipelineSettings.SetPipelineSettingToDefault_VertexInputState();
	Wireframe::Pipeline::VertexInputDescription vertexDescription = Smok::Asset::Mesh::Vertex::GenerateVertexInputDescription();
	pipelineSettings._vertexInputInfo.pVertexAttributeDescriptions = vertexDescription.attributes.data();
	pipelineSettings._vertexInputInfo.vertexAttributeDescriptionCount = vertexDescription.attributes.size();
	pipelineSettings._vertexInputInfo.pVertexBindingDescriptions = vertexDescription.bindings.data();
	pipelineSettings._vertexInputInfo.vertexBindingDescriptionCount = vertexDescription.bindings.size();

	pipelineSettings.SetPipelineSettingToDefault_ShaderStages();
	Wireframe::Shader::ShaderModule meshVertShader;
	if (!meshVertShader.Create("shaders/Compiled/mesh.vert.spv", GPU)) {}
	Wireframe::Shader::ShaderModule meshFragShader;
	if (!meshFragShader.Create("shaders/Compiled/mesh.frag.spv", GPU)) {}
	pipelineSettings._shaderStages = { Wireframe::Shader::GenerateVertexShaderStage(meshVertShader), Wireframe::Shader::GenerateFragmentShaderStage(meshFragShader) };

	Wireframe::Pipeline::PipelineLayout_CreateInfo pipelineLayoutInfo;
	VkPushConstantRange push_constant;
	push_constant.offset = 0;
	push_constant.size = sizeof(Pong3D::Renderer::MeshPushConstants);
	push_constant.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineLayoutInfo.pushConstants.resize(1); pipelineLayoutInfo.pushConstants[0] = push_constant;
	pipelineLayoutInfo.pushConstantNames.resize(1); pipelineLayoutInfo.pushConstantNames[0] = "Mesh Data";

	meshPipelineLayout.Create(pipelineLayoutInfo, GPU);
	meshPipeline.Create(pipelineSettings, meshPipelineLayout, renderpass._renderPass, GPU);

	meshFragShader.Destroy(GPU);
	meshVertShader.Destroy(GPU);
}

void load_meshes(Smok::Asset::Mesh::StaticMesh& mesh, VmaAllocator& allocator)
{
	//load the Kirby model
	Smok::AssetConvertionTool::Mesh::ConvertStaticMesh("assets/acoustic_guitar.obj", mesh);
	for (size_t i = 0; i < mesh.meshes.size(); ++i)
		mesh.meshes[i].CreateVertexAndIndexBuffers(allocator);
}

//entry point
int main()
{
	//--init

	//initalize the engine and create a window
	Pong3D::Core::Engine engine;
	if (!engine.Init())
	{
		engine.Shutdown();
		getchar();
		return -1;
	}
	Wireframe::Window::DesktopWindow* window = &engine.window;

	//initalize render stuff
	Pong3D::Renderer::FrameRenderManager renderManager;
	renderManager.Init(&engine);
	renderManager.TyGUI_Init();

	//stores the assets
	BTD::Map::IDStringRegistery assetNameRegistery;
	uint64_t meshPipelineAssetID = assetNameRegistery.GenerateID("meshPipeline_Default"),
		meshPipelineLayoutAssetID = assetNameRegistery.GenerateID("meshPipelineLayout_Default"),
		staticMeshAssetID = assetNameRegistery.GenerateID("staticMesh_Default");

	std::unordered_map<uint64_t, Wireframe::Pipeline::PipelineLayout> pipelineLayouts;
	std::unordered_map<uint64_t, Wireframe::Pipeline::GraphicsPipeline> pipelines;
	std::unordered_map<uint64_t, Smok::Asset::Mesh::StaticMesh> staticMeshes;

	//loads assets
	pipelineLayouts[meshPipelineLayoutAssetID] = Wireframe::Pipeline::PipelineLayout();
	pipelines[meshPipelineAssetID] = Wireframe::Pipeline::GraphicsPipeline();
	staticMeshes[staticMeshAssetID] = Smok::Asset::Mesh::StaticMesh();
	init_pipelines(pipelineLayouts[meshPipelineLayoutAssetID], pipelines[meshPipelineAssetID], renderManager.renderpass, &engine);
	load_meshes(staticMeshes[staticMeshAssetID], engine._allocator);

	//generates the mega mesh buffer of all index data

	//----scene
	Pong3D::Scene::Scene scene;

	//camera
	Smok::ECS::Comp::Transform transform; Smok::ECS::Comp::Camera cameraSettings;
	transform.position = { 0.f, 0.f, -10.f };
	Pong3D::Scene::Entity camera = scene.Camera_Create("Main Camera", transform, cameraSettings, &engine);

	//entity
	Smok::ECS::Comp::MeshRender entity_meshRenderComp;
	entity_meshRenderComp.pipelineID = meshPipelineAssetID;
	entity_meshRenderComp.pipelineLayoutID = meshPipelineLayoutAssetID;
	entity_meshRenderComp.staticMeshID = staticMeshAssetID;

	transform.position = { 0.0f, 0.0f, 7.0f };

	Pong3D::Scene::Entity kirby = scene.Rider_Create("Kirby", transform, entity_meshRenderComp, &engine);

	//goes through the scene and generate the render operations
	std::deque<Pong3D::Renderer::RenderOperationBatch> renderOperationBatchs;
	Pong3D::Renderer::RenderOperationBatch* batch = &renderOperationBatchs.emplace_back(Pong3D::Renderer::RenderOperationBatch());

	auto comps = BTD::ECS::queryEntities<Smok::ECS::Comp::Transform, Smok::ECS::Comp::MeshRender>();
	for (size_t i = 0; i < comps.size(); ++i)
	{
		transform = *BTD::ECS::getComponent<Smok::ECS::Comp::Transform>(comps[i]);
		Smok::ECS::Comp::MeshRender mr = *BTD::ECS::getComponent<Smok::ECS::Comp::MeshRender>(comps[i]);
		batch->AddStaticMesh(&pipelineLayouts[mr.pipelineLayoutID], &pipelines[mr.pipelineID], &staticMeshes[mr.staticMeshID], transform);
	}

	//gets the main camera
	Smok::ECS::Comp::Transform* camTrans = BTD::ECS::getComponent<Smok::ECS::Comp::Transform>(scene.cameras[0].ID);
	Smok::ECS::Comp::Camera* cam = BTD::ECS::getComponent<Smok::ECS::Comp::Camera>(scene.cameras[0].ID);

	//--game loop
	BTD::Time::Time time(60.0f);
	SDL_Event e;
	bool bQuit = false;
	bool stop_rendering = false;
	BTD::Input::KeyInput keyInputData;
	while (window->isRunning)
	{
		//--cal deltas
		//BTD::Time::Time::CalDeltaTime();
		//BTD::Time::Time::CalFixedDeltaTime();
		time.Update();

		//--input

		//Handle events on queue
		while (SDL_PollEvent(&e) != 0)
		{
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) bQuit = true;

			//window minimize or unminimize
			if (e.type == SDL_WINDOWEVENT) {

				if (e.window.event == SDL_WINDOWEVENT_MINIMIZED)
				{
					window->isMinimized = true;
					stop_rendering = true;
				}

				if (e.window.event == SDL_WINDOWEVENT_RESTORED)
				{
					window->isMinimized = false;
					stop_rendering = false;
				}
			}

			//sends input to the widgets
			if (renderManager.TyGUIIsInitalized && renderManager.TyGUIWidgetsShouldRender)
				renderManager.widgetRenderer.SendInputData(&e);
		}

		if (bQuit) //quits app
		{
			window->isRunning = false;
			break;
		}

		keyInputData.UpdateInputData();

		//input for moving the camera
		if (keyInputData.IsKeyHeld(SDL_SCANCODE_W))
		{
			camTrans->isDirty = true;
			camTrans->position += camTrans->Forward() * (-15.0f * time.GetFixedDeltaTime());
		}

		else if (keyInputData.IsKeyHeld(SDL_SCANCODE_S))
		{
			camTrans->isDirty = true;
			camTrans->position += camTrans->Forward() * (15.0f * time.GetFixedDeltaTime());
		}

		else if (keyInputData.IsKeyHeld(SDL_SCANCODE_A))
		{
			camTrans->isDirty = true;
			camTrans->position += camTrans->Left() * (15.0f * time.GetFixedDeltaTime());
		}

		else if (keyInputData.IsKeyHeld(SDL_SCANCODE_D))
		{
			camTrans->isDirty = true;
			camTrans->position += camTrans->Left() * (-15.0f * time.GetFixedDeltaTime());
		}

		//--update

		while (time.ShouldUpdate()) {
			// Perform fixed-timestep updates here (e.g., physics)
			time.ConsumeAccumulator();
		}

		//--render

		//do not draw if we are minimized
		if (stop_rendering) {
			//throttle the speed to avoid the endless spinning
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		//handles TyGUI widgets
		if (renderManager.TyGUIIsInitalized && renderManager.TyGUIWidgetsShouldRender)
		{
			renderManager.widgetRenderer.StartFrame();

			ImGui::Begin("OWO");

			ImGui::End();
		}

		//updates the cameras if they're bound to the screen
		cam->renderSize = { (float)engine.window._windowExtent.width, (float)engine.window._windowExtent.height };

		//starts the frame
		Pong3D::Renderer::Frame frame = renderManager.StartFrame();

		//performs renders
		for (size_t i = 0; i < renderOperationBatchs.size(); ++i)
			renderOperationBatchs[i].PerformRender(frame.cmd.handle, camTrans, cam);

		//submits the frame
		renderManager.SubmitFrame(frame);
	}
	window = nullptr;
	vkDeviceWaitIdle(engine.GPU.device); //make sure the gpu has stopped doing its things

	//--clean up

	for (auto& m : staticMeshes)
	{
		for (size_t i = 0; i < m.second.meshes.size(); ++i)
			m.second.meshes[i].DestroyVertexAndIndexBuffers(engine._allocator);
	}

	for (auto& p : pipelines)
		p.second.Destroy(&engine.GPU);

	for (auto& l : pipelineLayouts)
		l.second.Destroy(&engine.GPU);

	renderManager.Shutdown();
	engine.Shutdown();

	getchar();
	return 0;
}