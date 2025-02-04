#pragma once

#include "CommonIncludes.h"
#include "render/renderer/IRenderer.h"
#include "render/shader/ShaderBufferStruct.h"
#include "render/shader/ShaderBindings.h"
#include "render/material/Material.h"
#include "utils/Math.h"
#include "utils/DataStructures.h"

namespace Device
{
	class ShaderProgram;
	class DescriptorSet;
	class VulkanBuffer;
}

namespace ECS::components {

	class Transform;

	struct DrawCall
	{
		// TODO: remove mesh from draw call
		const Mesh* mesh = nullptr;
		OBB obb;
		mat4 transform;
		mat4 normal_transform;
		Device::ShaderProgram* shader = nullptr;
		Device::DescriptorSet* descriptor_set = nullptr;
		Device::ShaderProgram* depth_only_shader = nullptr;
		Device::DescriptorSet* depth_only_descriptor_set = nullptr;
		Device::VulkanBuffer* indirect_buffer = nullptr;
		Device::ConstantBindings constants;
		const Material* material = nullptr;
		uint32_t instance_count = 1;
		uint32_t indirect_buffer_offset = 0;
		uint32_t visible = 0;
		RenderQueue queue = RenderQueue::Opaque;
	};

	struct SkinningData
	{
		utils::SmallVector<mat4, 40> bone_matrices;
		uint32_t matrices_offset = 0;
	};

}