
#pragma once

#include "render/device/VulkanContext.h"
#include "ReflectionInfo.h"
#include "utils/DataStructures.h"

namespace Device {
	
	class ReflectionInfo;
	class ShaderBindings;
	class Texture;

	class ShaderModule
	{
	public:
		ShaderModule(uint32_t hash = 0) : hash(hash) {};
		ShaderModule(void* data, size_t size, uint32_t hash = 0);
		ShaderModule(ShaderModule&&) = default;
		ShaderModule& operator= (ShaderModule&&) = default;

		vk::ShaderModule GetModule() const { return shader_module.get(); }
		const ReflectionInfo* GetReflectionInfo() const { return reflection_info.get(); }
		uint32_t GetHash() const { return hash; }
	private:
		uint32_t hash;
		vk::UniqueShaderModule shader_module;
		std::unique_ptr<ReflectionInfo> reflection_info;
	};


	class ShaderProgram
	{
	public:
		enum class Stage : unsigned
		{
			Vertex   = 1 << 0,
			Fragment = 1 << 1,
			Compute  = 1 << 2
		};

		enum class BindingType : unsigned
		{
			CombinedImageSampler = 0,
			SampledImage,
			Sampler,
			UniformBuffer,
			UniformBufferDynamic,
			StorageBuffer,
			Count
		};

		struct BindingAddress
		{
			unsigned set;
			unsigned binding;
		};

		struct BindingData
		{
			BindingType type;
			uint32_t id; // Engine internal identifier for texture/buffer
			unsigned stage_flags;
			std::string name;
			BindingAddress address;
		};

		struct DescriptorSet
		{
			unsigned set_index = -1;
			std::vector<vk::DescriptorSetLayoutBinding> layout_bindings;
			vk::UniqueDescriptorSetLayout layout;
			std::vector<BindingData> bindings;
			uint32_t layout_hash = 0;
			bool Empty() const { return bindings.empty(); }
			uint32_t GetBindingIndexByName(std::string name);
		};

		struct PushConstants
		{
			uint32_t id;
			uint32_t offset;
			uint32_t range;
			std::string name;
			uint32_t stage_flags;
		};

		static const unsigned max_descriptor_sets = 4;

		static uint32_t CalculateHash(uint32_t fragment_hash, uint32_t vertex_hash, uint32_t compute_hash = 0);
		BindingAddress GetBindingAddress(const std::string& name);

		ShaderProgram();
		void AddModule(ShaderModule* shader_module, Stage stage);
		void Prepare();

		const ShaderModule* VertexModule() const { return vertex_module; }
		const ShaderModule* FragmentModule() const { return fragment_module; }
		const ShaderModule* ComputeModule() const { return compute_module; }
		
		const auto& GetDescriptorSets() const { return descriptor_sets; }
		const DescriptorSet* GetDescriptorSet(unsigned set) const { return &descriptor_sets[set]; }
		const BindingData* GetBinding(unsigned set, unsigned binding) const;
		const BindingData* GetBindingByName(const std::string& name) const;
		const utils::SmallVectorBase<PushConstants>* GetPushConstants() const { return &push_constants; }
		const char* GetEntryPoint(Stage stage) const;

		uint32_t GetHash() const;

	private:
		void AppendBindings(const ShaderModule& module, ShaderProgram::Stage stage, std::map<std::pair<unsigned, unsigned>, int>& existing_bindings);
		void AppendPushConstants(const ShaderModule& module, ShaderProgram::Stage stage);

	private:
		ShaderModule* vertex_module = nullptr;
		ShaderModule* fragment_module = nullptr;
		ShaderModule* compute_module = nullptr;
		
		uint32_t hash;

		std::unordered_map<std::string, BindingAddress> name_binding_map;
		std::array<DescriptorSet, max_descriptor_sets> descriptor_sets;
		utils::SmallVector<PushConstants, 1> push_constants;
	};

}