#include "SceneBuffers.h"

namespace render {

	SceneBuffers::SceneBuffers()
	{
		constant_buffer = std::make_unique<Device::ConstantBuffer>("Main CB", 4 * 1024 * 1024); // 4mb
		skinning_matrices = std::make_unique<Device::ConstantBuffer>("SkinningMatrices", 4 * 1024 * 1024, Device::BufferType::Storage); // 4mb
	}

}