#pragma once 

#include "CommonIncludes.h"

namespace core { namespace Device {

	class VulkanRenderPass;
	class VulkanRenderTarget;

	class VulkanSwapchain : NonCopyable
	{
	public:
		VulkanSwapchain(vk::SurfaceKHR surface, uint32_t width, uint32_t height, VulkanSwapchain* old_swapchain = nullptr);

		vk::SwapchainKHR GetSwapchain() const { return swapchain.get(); }
		vk::Format GetImageFormat() const { return image_format; }
		vk::Extent2D GetExtent() const { return vk::Extent2D(width, height); }
		uint32_t GetWidth() const { return width; }
		uint32_t GetHeight() const { return height; }
		const std::vector<vk::Image>& GetImages() const { return images; }
		VulkanRenderPass* GetRenderPass() const { return render_pass.get(); }
		VulkanRenderTarget* GetRenderTarget() const { return render_target.get(); }

	private:
		vk::UniqueSwapchainKHR swapchain;
		vk::SurfaceKHR surface;
		uint32_t width;
		uint32_t height;
		std::vector<vk::Image> images;
		vk::Format image_format;
		std::unique_ptr<VulkanRenderPass> render_pass;
		std::unique_ptr<VulkanRenderTarget> render_target;
	};

} }