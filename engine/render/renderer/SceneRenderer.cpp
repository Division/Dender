#include "SceneRenderer.h"
#include "ecs/ECS.h"
#include "ecs/components/MultiMeshRenderer.h"
#include "ecs/components/Transform.h"
#include "ecs/components/Light.h"
#include "ecs/systems/RendererSystem.h"
#include "ecs/systems/UpdateDrawCallsSystem.h"
#include "scene/Scene.h"
#include "render/device/VulkanUtils.h"
#include "CommonIncludes.h"
#include "Engine.h"
#include "render/device/VulkanContext.h"
#include "render/buffer/VulkanBuffer.h"
#include "render/device/VulkanUploader.h"
#include "render/device/VulkanUtils.h"
#include "render/device/VulkanRenderPass.h"
#include "render/device/VulkanSwapchain.h"
#include "render/device/VulkanPipeline.h"
#include "render/device/VulkanRenderState.h"
#include "render/device/VulkanRenderTarget.h"
#include "loader/TextureLoader.h"
#include "render/shading/LightGrid.h"
#include "render/texture/Texture.h"
#include "render/shading/ShadowMap.h"
#include "render/mesh/Mesh.h"
#include "render/shading/IShadowCaster.h"
#include "render/shader/ShaderCache.h"
#include "render/shader/ShaderResource.h"
#include "render/shader/ShaderBindings.h"
#include "render/shader/ShaderDefines.h"
#include "render/renderer/RenderGraph.h"
#include "render/material/Material.h"
#include "render/buffer/DynamicBuffer.h"
#include "render/debug/DebugDraw.h"
#include "render/debug/DebugUI.h"
#include "EnvironmentSettings.h"
#include "SceneBuffers.h"
#include "objects/Camera.h"
#include "render/effects/Skybox.h"
#include "render/effects/PostProcess.h"
#include "render/effects/Bloom.h"
#include "render/effects/Blur.h"
#include "render/effects/GPUParticles.h"
#include "render/effects/BitonicSort.h"

#include "resources/TextureResource.h"
#include "render/debug/DebugSettings.h"
#include "RenderModeUtils.h"
#include "utils/MeshGeneration.h"

#include <functional>

#include "rps/runtime/vk/rps_vk_runtime.h"

using namespace Device;
using namespace Resources;

RPS_DECLARE_RPSL_ENTRY(test_triangle, main);

namespace render {

	using namespace ECS;
	using namespace profiler;

	static const int32_t shadow_atlas_size = 4096;

	

	RendererResources::RendererResources()
	{
		brdf_lut = TextureResource::Linear(L"assets/Textures/LUT/ibl_brdf_lut.dds");

		uint32_t colors[16];
		for (auto& color : colors)
			color = 0xFFFF00FF;
		blank_texture = Device::Texture::Create(Device::TextureInitializer(4, 4, 4, colors, false));

		for (auto& color : colors)
			color = 0x000000;

		TextureInitializer cube_initializer(4, 4, 1, 6, Device::Format::R8G8B8A8_unorm, 1);
		cube_initializer.SetData(colors, sizeof(colors))
			.SetArray(false)
			.SetCube(true)
			.SetNumDimensions(2);

		for (int i = 0; i < 6; i++)
		{
			cube_initializer.AddCopy(
				vk::BufferImageCopy(0, 0, 0,
					vk::ImageSubresourceLayers(vk::ImageAspectFlagBits::eColor, 0, i, 1),
					vk::Offset3D(0, 0, 0),
					vk::Extent3D(4, 4, 1)
				));
		}

		blank_cube_texture = Device::Texture::Create(cube_initializer);

		full_screen_quad_mesh = std::make_unique<Mesh>(false, 3, true, "full screen quad");
		MeshGeneration::generateFullScreenQuad(full_screen_quad_mesh.get());
		full_screen_quad_mesh->createBuffer();

		particle_quad_mesh = Common::Handle<Mesh>(std::make_unique<Mesh>());
		MeshGeneration::generateParticleQuad(particle_quad_mesh.get());
		particle_quad_mesh->createBuffer();
	}

	int32_t SceneRenderer::ShadowAtlasSize()
	{
		return shadow_atlas_size;
	}

	void SceneRenderer::SetRadianceCubemap(Resources::Handle<Resources::TextureResource> cubemap)
	{
		renderer_resources->radiance_cubemap = cubemap;
	}

	void SceneRenderer::SetIrradianceCubemap(Resources::Handle<Resources::TextureResource> cubemap)
	{
		renderer_resources->irradiance_cubemap = cubemap;
	}

	SceneRenderer::~SceneRenderer() = default;

    void SceneRenderer::DrawTriangleWithRPSCb(const RpsCmdCallbackContext* pContext)
    {
        //if (m_psoWithRps == VK_NULL_HANDLE)
        //{
        //    VkRenderPass renderPassFromRps = {};
        //    REQUIRE_RPS_OK(rpsVKGetCmdRenderPass(pContext, &renderPassFromRps));

        //    m_psoWithRps = CreateVkPipeline(renderPassFromRps);
        //}

        //DrawTriangle(rpsVKCommandBufferFromHandle(pContext->hCommandBuffer), m_psoWithRps);
		auto info = ShaderProgramInfo()
			.AddShader(ShaderProgram::Stage::Vertex, L"shaders/postprocess/postprocess.hlsl", "vs_main")
			.AddShader(ShaderProgram::Stage::Fragment, L"shaders/postprocess/postprocess.hlsl", "ps_main");
		auto shader = shader_cache->GetShaderProgram(info);


		RenderMode mode;
		mode.SetDepthWriteEnabled(false);
		mode.SetDepthTestEnabled(false);
		mode.SetPolygonMode(PolygonMode::Fill);
		mode.SetPrimitiveTopology(PrimitiveTopology::TriangleList);

		ResourceBindings resource_bindings;

		const auto* descriptor_set_layout = shader->GetDescriptorSetLayout(0);
		const DescriptorSetBindings bindings(resource_bindings, *descriptor_set_layout);
		
		auto& state = *static_cast<VulkanRenderState*>(pContext->pUserRecordContext);

		VkRenderPass renderPassFromRps = {};
		rpsVKGetCmdRenderPass(pContext, &renderPassFromRps);
		auto renderPass = Device::VulkanRenderPass(Device::VulkanRenderPassInitializer(renderPassFromRps));

		uint32_t num = 0;
		ConstantBindings constants;
		constants.AddUIntBinding(&num, "exposure");

		auto full_screen_quad_mesh = GetRendererResources().full_screen_quad_mesh.get();

		static VulkanPipeline pipeline(VulkanPipelineInitializer(shader, &renderPass, &full_screen_quad_mesh->GetVertexLayout(), &mode));
		state.BindPipeline(pipeline);
		state.SetDescriptorSetBindings(bindings, constants);
		state.Draw(*full_screen_quad_mesh->vertexBuffer(), full_screen_quad_mesh->indexCount(), 0);

		scene_buffers->GetConstantBuffer()->Upload();
		scene_buffers->GetSkinningMatricesBuffer()->Upload();
    }


	SceneRenderer::SceneRenderer(Scene& scene, ShaderCache* shader_cache, DebugSettings* settings)
		: scene(scene)
		, shader_cache(shader_cache)
		, debug_settings(settings)
	{
		scene_buffers = std::make_unique<SceneBuffers>();
		directional_light = std::make_unique<components::DirectionalLight>();
		scene.GetEntityManager()->AddStaticComponent(directional_light.get());
		
		renderer_resources = std::unique_ptr<RendererResources>(new RendererResources());

		environment_settings = std::make_unique<EnvironmentSettings>();
		environment_settings->directional_light = directional_light.get();

		draw_call_manager = std::make_unique<DrawCallManager>(*this);
		create_draw_calls_system = std::make_unique<systems::CreateDrawCallsSystem>(*scene.GetEntityManager(), *draw_call_manager);
		upload_draw_calls_system = std::make_unique<systems::UploadDrawCallsSystem>(*draw_call_manager, *scene_buffers);
		upload_skinning_system = std::make_unique<systems::UploadSkinningSystem>(*draw_call_manager, *scene_buffers);

		light_grid = std::make_unique<LightGrid>();
		shadow_map = std::make_unique<ShadowMap>(ShadowAtlasSize(), ShadowAtlasSize());
		render_graph = std::make_unique<graph::RenderGraph>();
		auto* context = Engine::GetVulkanContext();
		context->AddRecreateSwapchainCallback(std::bind(&SceneRenderer::OnRecreateSwapchain, this, std::placeholders::_1, std::placeholders::_2));
		shadowmap_atlas_attachment = std::make_unique<VulkanRenderTargetAttachment>("Shadowmap Atlas", VulkanRenderTargetAttachment::Type::Depth, ShadowAtlasSize(), ShadowAtlasSize(), Format::D24_unorm_S8_uint);
		shadowmap_attachment = std::make_unique<VulkanRenderTargetAttachment>("Shadowmap", VulkanRenderTargetAttachment::Type::Depth, ShadowAtlasSize(), ShadowAtlasSize(), Format::D24_unorm_S8_uint);

		global_resource_bindings = std::make_unique<Device::ResourceBindings>();
		global_constant_bindings = std::make_unique<Device::ConstantBindings>();
		
		skybox = std::make_unique<effects::Skybox>(*shader_cache);

		post_process = std::make_unique<effects::PostProcess>(*shader_cache, *environment_settings, *renderer_resources);
		bitonic_sort = std::make_unique<BitonicSort>(*shader_cache);
		gpu_particles = std::make_unique<GPUParticles::GPUParticles>(*this, *bitonic_sort, *scene.GetEntityManager());
		blur = std::make_unique<Blur>(*shader_cache, *renderer_resources);
		bloom = std::make_unique<Bloom>(*shader_cache, *blur, *environment_settings, *renderer_resources);


        RpsRenderGraphCreateInfo renderGraphInfo            = {};
        renderGraphInfo.mainEntryCreateInfo.hRpslEntryPoint = RPS_ENTRY_REF(test_triangle, main);
        rpsRenderGraphCreate(Engine::GetVulkanContext()->GetRpsDevice(), &renderGraphInfo, &m_rpsRenderGraph);

        rpsProgramBindNode(
            rpsRenderGraphGetMainEntry(m_rpsRenderGraph), "Triangle", &SceneRenderer::DrawTriangleWithRPSCb, this);
	}

	void SceneRenderer::OnRecreateSwapchain(int32_t width, int32_t height)
	{
		render_graph->ClearCache();

		main_depth_attachment = std::make_unique<VulkanRenderTargetAttachment>("Main Depth", VulkanRenderTargetAttachment::Type::Depth, width, height, Format::D24_unorm_S8_uint);
		main_color_attachment[0] = std::make_unique<VulkanRenderTargetAttachment>("Main Color 0", VulkanRenderTargetAttachment::Type::Color, width, height, Format::R16G16B16A16_float);
		main_color_attachment[1] = std::make_unique<VulkanRenderTargetAttachment>("Main Color 1", VulkanRenderTargetAttachment::Type::Color, width, height, Format::R16G16B16A16_float);
		post_process->OnRecreateSwapchain(width, height);
		bloom->OnRecreateSwapchain(width, height);

		auto swapchain = Engine::GetVulkanContext()->GetSwapchain();

		for (uint32_t i = 0; i < caps::MAX_FRAMES_IN_FLIGHT; i++)
		{
			vulkanRT[i] = std::make_unique<Device::VulkanRenderTarget>(Device::VulkanRenderTargetInitializer(swapchain)
				.AddAttachment(*swapchain->GetColorAttachment(i))
				.AddAttachment(*main_depth_attachment));
		}
	}

	void SceneRenderer::CreateDrawCalls()
	{
		auto* entity_manager = scene.GetEntityManager();
		auto list = entity_manager->GetChunkLists([=](ChunkList* chunk_list) {
			return chunk_list->HasComponent(GetComponentHash<components::MultiMeshRenderer>());
		});
		create_draw_calls_system->ProcessChunks(list);
	}

	void SceneRenderer::UploadDrawCalls()
	{
		upload_draw_calls_system->ProcessChunks(draw_call_manager->GetDrawCallChunks());
		upload_skinning_system->ProcessChunks(draw_call_manager->GetSkinningChunks());
	}

	static ShaderBufferStruct::Camera GetCameraData(ICameraParamsProvider* camera, uvec2 screen_size)
	{
		ShaderBufferStruct::Camera result;
		result.projectionMatrix = camera->cameraProjectionMatrix();
		result.projectionMatrix[1][1] *= -1;
		result.position = camera->cameraPosition();
		result.viewMatrix = camera->cameraViewMatrix();
		result.screenSize = screen_size;
		result.zMin = camera->cameraZMinMax().x;
		result.zMax = camera->cameraZMinMax().y;
		return result;
	}

	static ShaderBufferStruct::EnvironmentSettings GetEnvSettings(const EnvironmentSettings& settings)
	{
		ShaderBufferStruct::EnvironmentSettings result;

		result.direction_light_enabled = settings.directional_light && settings.directional_light->enabled;
		if (result.direction_light_enabled)
		{
			result.direction_light_color = settings.directional_light->color;
			result.direction_light_direction = settings.directional_light->transform.Forward();
			result.direction_light_projection_matrix = settings.directional_light->cameraViewProjectionMatrix();
			result.direction_light_cast_shadows = settings.directional_light->cast_shadows;
		}

		result.environment_brightness = settings.environment_brightness;
		result.exposure = settings.exposure;
		return result;
	}

    static uint64_t CalcGuaranteedCompletedFrameIndexForRps() 
    {
		auto context = Engine::GetVulkanContext();
        // For VK we wait for swapchain before submitting, so max queued frame count is swapChainImages + 1.
        const uint32_t maxQueuedFrames = uint32_t(context->GetSwapchainImageCount() + 1);

        return (context->GetCurrentFrame() > maxQueuedFrames) ? context->GetCurrentFrame() - maxQueuedFrames
															  : RPS_GPU_COMPLETED_FRAME_INDEX_NONE;
    }

	void SceneRenderer::RenderScene(float dt)
	{
		OPTICK_EVENT();

		auto context = Engine::GetVulkanContext();
		auto swapchain = context->GetSwapchain();

        if (m_rpsRenderGraph != RPS_NULL_HANDLE)
        {
			RpsRuntimeResource backBufferResources[16] = {};

			for (uint32_t i = 0; i < swapchain->GetImages().size(); i++)
			{
				backBufferResources[i] = { swapchain->GetImages()[i] };
			}
			const RpsRuntimeResource* argResources[] = { backBufferResources };

			RpsResourceDesc backBufferDesc = {};
			backBufferDesc.type = RPS_RESOURCE_TYPE_IMAGE_2D;
			backBufferDesc.temporalLayers = uint32_t(swapchain->GetImages().size());
			backBufferDesc.image.arrayLayers = 1;
			backBufferDesc.image.mipLevels = 1;
			backBufferDesc.image.format = rpsFormatFromVK((VkFormat)swapchain->GetImageFormat());
			backBufferDesc.image.width = swapchain->GetWidth();
			backBufferDesc.image.height = swapchain->GetHeight();
			backBufferDesc.image.sampleCount = 1;

			RpsConstant argData[] = { &backBufferDesc };

			const uint64_t gpuCompletedFrameIndex = CalcGuaranteedCompletedFrameIndexForRps();

			RpsRenderGraphUpdateInfo updateInfo = {};
			updateInfo.frameIndex = context->GetCurrentFrame();
			updateInfo.gpuCompletedFrameIndex = gpuCompletedFrameIndex;
			updateInfo.numArgs = 1;
			updateInfo.ppArgs = argData;
			updateInfo.ppArgResources = argResources;

			updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_RUNTIME_DEBUG_NAMES;
			if (context->GetCurrentFrame() < context->GetSwapchainImageCount())
			{
				updateInfo.diagnosticFlags = RPS_DIAGNOSTIC_ENABLE_ALL;
			}

			rpsRenderGraphUpdate(m_rpsRenderGraph, &updateInfo);

			RpsRenderGraphBatchLayout batchLayout = {};
			RpsResult                 result = rpsRenderGraphGetBatchLayout(m_rpsRenderGraph, &batchLayout);
			assert(result == RPS_OK);

			context->ReserveSemaphores(batchLayout.numFenceSignals);

			for (uint32_t iBatch = 0; iBatch < batchLayout.numCmdBatches; iBatch++)
			{
				auto& batch = batchLayout.pCmdBatches[iBatch];

				auto* state = Engine::GetVulkanContext()->GetRenderState();

				state->BeginRecording(Device::PipelineBindPoint::Graphics);
				RpsRenderGraphRecordCommandInfo recordInfo = {};

				auto commandBuffer = state->GetCurrentCommandBuffer();
				recordInfo.pUserContext = state;
				recordInfo.cmdBeginIndex = batch.cmdBegin;
				recordInfo.numCmds = batch.numCmds;
				recordInfo.hCmdBuffer = rpsVKCommandBufferToHandle(commandBuffer);

				result = rpsRenderGraphRecordCommands(m_rpsRenderGraph, &recordInfo);
				assert(result == RPS_OK);

				state->EndRecording();

				context->AddFrameCommandBuffer(Device::FrameCommandBufferData(*state, batch, batchLayout.pWaitFenceIndices));
			}
        }
	}

	Device::Texture* SceneRenderer::GetBlankTexture() const
	{
		return renderer_resources->blank_texture.get();
	}

	void SceneRenderer::UpdateGlobalBindings()
	{
		global_bindings_dirty = true;
	}

}