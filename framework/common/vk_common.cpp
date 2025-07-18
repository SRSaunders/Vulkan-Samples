/* Copyright (c) 2018-2025, Arm Limited and Contributors
 * Copyright (c) 2019-2025, Sascha Willems
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 the "License";
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "vk_common.h"

#include <fmt/format.h>

#include "filesystem/legacy.h"

std::ostream &operator<<(std::ostream &os, const VkResult result)
{
#define WRITE_VK_ENUM(r) \
	case VK_##r:         \
		os << #r;        \
		break;

	switch (result)
	{
		WRITE_VK_ENUM(NOT_READY);
		WRITE_VK_ENUM(TIMEOUT);
		WRITE_VK_ENUM(EVENT_SET);
		WRITE_VK_ENUM(EVENT_RESET);
		WRITE_VK_ENUM(INCOMPLETE);
		WRITE_VK_ENUM(ERROR_OUT_OF_HOST_MEMORY);
		WRITE_VK_ENUM(ERROR_OUT_OF_DEVICE_MEMORY);
		WRITE_VK_ENUM(ERROR_INITIALIZATION_FAILED);
		WRITE_VK_ENUM(ERROR_DEVICE_LOST);
		WRITE_VK_ENUM(ERROR_MEMORY_MAP_FAILED);
		WRITE_VK_ENUM(ERROR_LAYER_NOT_PRESENT);
		WRITE_VK_ENUM(ERROR_EXTENSION_NOT_PRESENT);
		WRITE_VK_ENUM(ERROR_FEATURE_NOT_PRESENT);
		WRITE_VK_ENUM(ERROR_INCOMPATIBLE_DRIVER);
		WRITE_VK_ENUM(ERROR_TOO_MANY_OBJECTS);
		WRITE_VK_ENUM(ERROR_FORMAT_NOT_SUPPORTED);
		WRITE_VK_ENUM(ERROR_SURFACE_LOST_KHR);
		WRITE_VK_ENUM(ERROR_NATIVE_WINDOW_IN_USE_KHR);
		WRITE_VK_ENUM(SUBOPTIMAL_KHR);
		WRITE_VK_ENUM(ERROR_OUT_OF_DATE_KHR);
		WRITE_VK_ENUM(ERROR_INCOMPATIBLE_DISPLAY_KHR);
		WRITE_VK_ENUM(ERROR_VALIDATION_FAILED_EXT);
		WRITE_VK_ENUM(ERROR_INVALID_SHADER_NV);
		default:
			os << "UNKNOWN_ERROR";
	}

#undef WRITE_VK_ENUM

	return os;
}

namespace vkb
{
bool is_depth_only_format(VkFormat format)
{
	return format == VK_FORMAT_D16_UNORM ||
	       format == VK_FORMAT_D32_SFLOAT;
}

bool is_depth_stencil_format(VkFormat format)
{
	return format == VK_FORMAT_D16_UNORM_S8_UINT ||
	       format == VK_FORMAT_D24_UNORM_S8_UINT ||
	       format == VK_FORMAT_D32_SFLOAT_S8_UINT;
}

bool is_depth_format(VkFormat format)
{
	return is_depth_only_format(format) || is_depth_stencil_format(format);
}

VkFormat get_suitable_depth_format(VkPhysicalDevice physical_device, bool depth_only, const std::vector<VkFormat> &depth_format_priority_list)
{
	VkFormat depth_format{VK_FORMAT_UNDEFINED};

	for (auto &format : depth_format_priority_list)
	{
		if (depth_only && !is_depth_only_format(format))
		{
			continue;
		}

		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

		// Format must support depth stencil attachment for optimal tiling
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depth_format = format;
			break;
		}
	}

	if (depth_format != VK_FORMAT_UNDEFINED)
	{
		LOGI("Depth format selected: {}", to_string(depth_format));
		return depth_format;
	}

	throw std::runtime_error("No suitable depth format could be determined");
}

VkFormat choose_blendable_format(VkPhysicalDevice physical_device, const std::vector<VkFormat> &format_priority_list)
{
	for (const auto &format : format_priority_list)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);
		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT)
		{
			return format;
		}
	}

	throw std::runtime_error("No suitable blendable format could be determined");
}

void make_filters_valid(VkPhysicalDevice physical_device, VkFormat format, VkFilter *filter, VkSamplerMipmapMode *mipmapMode)
{
	// Not all formats support linear filtering, so we need to adjust them if they don't
	if (*filter == VK_FILTER_NEAREST && (mipmapMode == nullptr || *mipmapMode == VK_SAMPLER_MIPMAP_MODE_NEAREST))
	{
		return;        // These must already be valid
	}

	VkFormatProperties properties;
	vkGetPhysicalDeviceFormatProperties(physical_device, format, &properties);

	if (!(properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
	{
		*filter = VK_FILTER_NEAREST;
		if (mipmapMode)
		{
			*mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
		}
	}
}

bool is_dynamic_buffer_descriptor_type(VkDescriptorType descriptor_type)
{
	return descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
	       descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
}

bool is_buffer_descriptor_type(VkDescriptorType descriptor_type)
{
	return descriptor_type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
	       descriptor_type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
	       is_dynamic_buffer_descriptor_type(descriptor_type);
}

int32_t get_bits_per_pixel(VkFormat format)
{
	switch (format)
	{
		case VK_FORMAT_R4G4_UNORM_PACK8:
			return 8;
		case VK_FORMAT_R4G4B4A4_UNORM_PACK16:
		case VK_FORMAT_B4G4R4A4_UNORM_PACK16:
		case VK_FORMAT_R5G6B5_UNORM_PACK16:
		case VK_FORMAT_B5G6R5_UNORM_PACK16:
		case VK_FORMAT_R5G5B5A1_UNORM_PACK16:
		case VK_FORMAT_B5G5R5A1_UNORM_PACK16:
		case VK_FORMAT_A1R5G5B5_UNORM_PACK16:
			return 16;
		case VK_FORMAT_R8_UNORM:
		case VK_FORMAT_R8_SNORM:
		case VK_FORMAT_R8_USCALED:
		case VK_FORMAT_R8_SSCALED:
		case VK_FORMAT_R8_UINT:
		case VK_FORMAT_R8_SINT:
		case VK_FORMAT_R8_SRGB:
			return 8;
		case VK_FORMAT_R8G8_UNORM:
		case VK_FORMAT_R8G8_SNORM:
		case VK_FORMAT_R8G8_USCALED:
		case VK_FORMAT_R8G8_SSCALED:
		case VK_FORMAT_R8G8_UINT:
		case VK_FORMAT_R8G8_SINT:
		case VK_FORMAT_R8G8_SRGB:
			return 16;
		case VK_FORMAT_R8G8B8_UNORM:
		case VK_FORMAT_R8G8B8_SNORM:
		case VK_FORMAT_R8G8B8_USCALED:
		case VK_FORMAT_R8G8B8_SSCALED:
		case VK_FORMAT_R8G8B8_UINT:
		case VK_FORMAT_R8G8B8_SINT:
		case VK_FORMAT_R8G8B8_SRGB:
		case VK_FORMAT_B8G8R8_UNORM:
		case VK_FORMAT_B8G8R8_SNORM:
		case VK_FORMAT_B8G8R8_USCALED:
		case VK_FORMAT_B8G8R8_SSCALED:
		case VK_FORMAT_B8G8R8_UINT:
		case VK_FORMAT_B8G8R8_SINT:
		case VK_FORMAT_B8G8R8_SRGB:
			return 24;
		case VK_FORMAT_R8G8B8A8_UNORM:
		case VK_FORMAT_R8G8B8A8_SNORM:
		case VK_FORMAT_R8G8B8A8_USCALED:
		case VK_FORMAT_R8G8B8A8_SSCALED:
		case VK_FORMAT_R8G8B8A8_UINT:
		case VK_FORMAT_R8G8B8A8_SINT:
		case VK_FORMAT_R8G8B8A8_SRGB:
		case VK_FORMAT_B8G8R8A8_UNORM:
		case VK_FORMAT_B8G8R8A8_SNORM:
		case VK_FORMAT_B8G8R8A8_USCALED:
		case VK_FORMAT_B8G8R8A8_SSCALED:
		case VK_FORMAT_B8G8R8A8_UINT:
		case VK_FORMAT_B8G8R8A8_SINT:
		case VK_FORMAT_B8G8R8A8_SRGB:
		case VK_FORMAT_A8B8G8R8_UNORM_PACK32:
		case VK_FORMAT_A8B8G8R8_SNORM_PACK32:
		case VK_FORMAT_A8B8G8R8_USCALED_PACK32:
		case VK_FORMAT_A8B8G8R8_SSCALED_PACK32:
		case VK_FORMAT_A8B8G8R8_UINT_PACK32:
		case VK_FORMAT_A8B8G8R8_SINT_PACK32:
		case VK_FORMAT_A8B8G8R8_SRGB_PACK32:
			return 32;
		case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
		case VK_FORMAT_A2R10G10B10_SNORM_PACK32:
		case VK_FORMAT_A2R10G10B10_USCALED_PACK32:
		case VK_FORMAT_A2R10G10B10_SSCALED_PACK32:
		case VK_FORMAT_A2R10G10B10_UINT_PACK32:
		case VK_FORMAT_A2R10G10B10_SINT_PACK32:
		case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
		case VK_FORMAT_A2B10G10R10_SNORM_PACK32:
		case VK_FORMAT_A2B10G10R10_USCALED_PACK32:
		case VK_FORMAT_A2B10G10R10_SSCALED_PACK32:
		case VK_FORMAT_A2B10G10R10_UINT_PACK32:
		case VK_FORMAT_A2B10G10R10_SINT_PACK32:
			return 32;
		case VK_FORMAT_R16_UNORM:
		case VK_FORMAT_R16_SNORM:
		case VK_FORMAT_R16_USCALED:
		case VK_FORMAT_R16_SSCALED:
		case VK_FORMAT_R16_UINT:
		case VK_FORMAT_R16_SINT:
		case VK_FORMAT_R16_SFLOAT:
			return 16;
		case VK_FORMAT_R16G16_UNORM:
		case VK_FORMAT_R16G16_SNORM:
		case VK_FORMAT_R16G16_USCALED:
		case VK_FORMAT_R16G16_SSCALED:
		case VK_FORMAT_R16G16_UINT:
		case VK_FORMAT_R16G16_SINT:
		case VK_FORMAT_R16G16_SFLOAT:
			return 32;
		case VK_FORMAT_R16G16B16_UNORM:
		case VK_FORMAT_R16G16B16_SNORM:
		case VK_FORMAT_R16G16B16_USCALED:
		case VK_FORMAT_R16G16B16_SSCALED:
		case VK_FORMAT_R16G16B16_UINT:
		case VK_FORMAT_R16G16B16_SINT:
		case VK_FORMAT_R16G16B16_SFLOAT:
			return 48;
		case VK_FORMAT_R16G16B16A16_UNORM:
		case VK_FORMAT_R16G16B16A16_SNORM:
		case VK_FORMAT_R16G16B16A16_USCALED:
		case VK_FORMAT_R16G16B16A16_SSCALED:
		case VK_FORMAT_R16G16B16A16_UINT:
		case VK_FORMAT_R16G16B16A16_SINT:
		case VK_FORMAT_R16G16B16A16_SFLOAT:
			return 64;
		case VK_FORMAT_R32_UINT:
		case VK_FORMAT_R32_SINT:
		case VK_FORMAT_R32_SFLOAT:
			return 32;
		case VK_FORMAT_R32G32_UINT:
		case VK_FORMAT_R32G32_SINT:
		case VK_FORMAT_R32G32_SFLOAT:
			return 64;
		case VK_FORMAT_R32G32B32_UINT:
		case VK_FORMAT_R32G32B32_SINT:
		case VK_FORMAT_R32G32B32_SFLOAT:
			return 96;
		case VK_FORMAT_R32G32B32A32_UINT:
		case VK_FORMAT_R32G32B32A32_SINT:
		case VK_FORMAT_R32G32B32A32_SFLOAT:
			return 128;
		case VK_FORMAT_R64_UINT:
		case VK_FORMAT_R64_SINT:
		case VK_FORMAT_R64_SFLOAT:
			return 64;
		case VK_FORMAT_R64G64_UINT:
		case VK_FORMAT_R64G64_SINT:
		case VK_FORMAT_R64G64_SFLOAT:
			return 128;
		case VK_FORMAT_R64G64B64_UINT:
		case VK_FORMAT_R64G64B64_SINT:
		case VK_FORMAT_R64G64B64_SFLOAT:
			return 192;
		case VK_FORMAT_R64G64B64A64_UINT:
		case VK_FORMAT_R64G64B64A64_SINT:
		case VK_FORMAT_R64G64B64A64_SFLOAT:
			return 256;
		case VK_FORMAT_B10G11R11_UFLOAT_PACK32:
			return 32;
		case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:
			return 32;
		case VK_FORMAT_D16_UNORM:
			return 16;
		case VK_FORMAT_X8_D24_UNORM_PACK32:
			return 32;
		case VK_FORMAT_D32_SFLOAT:
			return 32;
		case VK_FORMAT_S8_UINT:
			return 8;
		case VK_FORMAT_D16_UNORM_S8_UINT:
			return 24;
		case VK_FORMAT_D24_UNORM_S8_UINT:
			return 32;
		case VK_FORMAT_D32_SFLOAT_S8_UINT:
			return 40;
		case VK_FORMAT_UNDEFINED:
		default:
			return -1;
	}
}

VkShaderModule load_shader(const std::string &filename, VkDevice device, VkShaderStageFlagBits stage)
{
	auto spirv = vkb::fs::read_shader_binary_u32(filename);

	VkShaderModule           shader_module;
	VkShaderModuleCreateInfo module_create_info{};
	module_create_info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	module_create_info.codeSize = spirv.size() * sizeof(uint32_t);
	module_create_info.pCode    = spirv.data();

	VK_CHECK(vkCreateShaderModule(device, &module_create_info, NULL, &shader_module));

	return shader_module;
}

VkAccessFlags getAccessFlags(VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_UNDEFINED:
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			return 0;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_ACCESS_HOST_WRITE_BIT;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			return VK_ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT_KHR;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_GENERAL:
			assert(false && "Don't know how to get a meaningful VkAccessFlags for VK_IMAGE_LAYOUT_GENERAL! Don't use it!");
			return 0;
		default:
			assert(false);
			return 0;
	}
}

VkPipelineStageFlags getPipelineStageFlags(VkImageLayout layout)
{
	switch (layout)
	{
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_PIPELINE_STAGE_HOST_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_PIPELINE_STAGE_TRANSFER_BIT;
		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
			return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
		case VK_IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL_KHR:
			return VK_PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT_KHR;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		case VK_IMAGE_LAYOUT_GENERAL:
			assert(false && "Don't know how to get a meaningful VkPipelineStageFlags for VK_IMAGE_LAYOUT_GENERAL! Don't use it!");
			return 0;
		default:
			assert(false);
			return 0;
	}
}

// Create an image memory barrier for changing the layout of
// an image and put it into an active command buffer
// See chapter 12.4 "Image Layout" for details

void image_layout_transition(VkCommandBuffer                command_buffer,
                             VkImage                        image,
                             VkPipelineStageFlags           src_stage_mask,
                             VkPipelineStageFlags           dst_stage_mask,
                             VkAccessFlags                  src_access_mask,
                             VkAccessFlags                  dst_access_mask,
                             VkImageLayout                  old_layout,
                             VkImageLayout                  new_layout,
                             VkImageSubresourceRange const &subresource_range)
{
	// Create an image barrier object
	VkImageMemoryBarrier image_memory_barrier{};
	image_memory_barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_memory_barrier.srcAccessMask       = src_access_mask;
	image_memory_barrier.dstAccessMask       = dst_access_mask;
	image_memory_barrier.oldLayout           = old_layout;
	image_memory_barrier.newLayout           = new_layout;
	image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	image_memory_barrier.image               = image;
	image_memory_barrier.subresourceRange    = subresource_range;

	// Put barrier inside setup command buffer
	vkCmdPipelineBarrier(command_buffer, src_stage_mask, dst_stage_mask, 0, 0, nullptr, 0, nullptr, 1, &image_memory_barrier);
}

void image_layout_transition(VkCommandBuffer                command_buffer,
                             VkImage                        image,
                             VkImageLayout                  old_layout,
                             VkImageLayout                  new_layout,
                             VkImageSubresourceRange const &subresource_range)
{
	VkPipelineStageFlags src_stage_mask  = getPipelineStageFlags(old_layout);
	VkPipelineStageFlags dst_stage_mask  = getPipelineStageFlags(new_layout);
	VkAccessFlags        src_access_mask = getAccessFlags(old_layout);
	VkAccessFlags        dst_access_mask = getAccessFlags(new_layout);

	image_layout_transition(command_buffer, image, src_stage_mask, dst_stage_mask, src_access_mask, dst_access_mask, old_layout, new_layout, subresource_range);
}

// Fixed sub resource on first mip level and layer
void image_layout_transition(VkCommandBuffer command_buffer,
                             VkImage         image,
                             VkImageLayout   old_layout,
                             VkImageLayout   new_layout)
{
	VkImageSubresourceRange subresource_range = {};
	subresource_range.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
	subresource_range.baseMipLevel            = 0;
	subresource_range.levelCount              = 1;
	subresource_range.baseArrayLayer          = 0;
	subresource_range.layerCount              = 1;
	image_layout_transition(command_buffer, image, old_layout, new_layout, subresource_range);
}

void image_layout_transition(VkCommandBuffer                                                 command_buffer,
                             std::vector<std::pair<VkImage, VkImageSubresourceRange>> const &imagesAndRanges,
                             VkImageLayout                                                   old_layout,
                             VkImageLayout                                                   new_layout)
{
	VkPipelineStageFlags src_stage_mask  = getPipelineStageFlags(old_layout);
	VkPipelineStageFlags dst_stage_mask  = getPipelineStageFlags(new_layout);
	VkAccessFlags        src_access_mask = getAccessFlags(old_layout);
	VkAccessFlags        dst_access_mask = getAccessFlags(new_layout);

	// Create image barrier objects
	std::vector<VkImageMemoryBarrier> image_memory_barriers;
	image_memory_barriers.reserve(imagesAndRanges.size());
	for (size_t i = 0; i < imagesAndRanges.size(); i++)
	{
		image_memory_barriers.emplace_back(VkImageMemoryBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		                                                        nullptr,
		                                                        src_access_mask,
		                                                        dst_access_mask,
		                                                        old_layout,
		                                                        new_layout,
		                                                        VK_QUEUE_FAMILY_IGNORED,
		                                                        VK_QUEUE_FAMILY_IGNORED,
		                                                        imagesAndRanges[i].first,
		                                                        imagesAndRanges[i].second});
	}

	// Put barriers inside setup command buffer
	vkCmdPipelineBarrier(command_buffer,
	                     src_stage_mask,
	                     dst_stage_mask,
	                     0,
	                     0,
	                     nullptr,
	                     0,
	                     nullptr,
	                     static_cast<uint32_t>(image_memory_barriers.size()),
	                     image_memory_barriers.data());
}

std::vector<VkImageCompressionFixedRateFlagBitsEXT> fixed_rate_compression_flags_to_vector(VkImageCompressionFixedRateFlagsEXT flags)
{
	const std::vector<VkImageCompressionFixedRateFlagBitsEXT> all_flags = {VK_IMAGE_COMPRESSION_FIXED_RATE_1BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_2BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_3BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_4BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_5BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_6BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_7BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_8BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_9BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_10BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_11BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_12BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_13BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_14BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_15BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_16BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_17BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_18BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_19BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_20BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_21BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_22BPC_BIT_EXT,
	                                                                       VK_IMAGE_COMPRESSION_FIXED_RATE_23BPC_BIT_EXT, VK_IMAGE_COMPRESSION_FIXED_RATE_24BPC_BIT_EXT};

	std::vector<VkImageCompressionFixedRateFlagBitsEXT> flags_vector;

	for (size_t i = 0; i < all_flags.size(); i++)
	{
		if (all_flags[i] & flags)
		{
			flags_vector.push_back(all_flags[i]);
		}
	}

	return flags_vector;
}

VkImageCompressionPropertiesEXT query_supported_fixed_rate_compression(VkPhysicalDevice gpu, const VkImageCreateInfo &create_info)
{
	VkImageCompressionPropertiesEXT supported_compression_properties{VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT};

	VkImageCompressionControlEXT compression_control{VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_CONTROL_EXT};
	compression_control.flags = VK_IMAGE_COMPRESSION_FIXED_RATE_DEFAULT_EXT;

	VkPhysicalDeviceImageFormatInfo2 image_format_info{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_FORMAT_INFO_2};
	image_format_info.format = create_info.format;
	image_format_info.type   = create_info.imageType;
	image_format_info.tiling = create_info.tiling;
	image_format_info.usage  = create_info.usage;
	image_format_info.pNext  = &compression_control;

	VkImageFormatProperties2 image_format_properties{VK_STRUCTURE_TYPE_IMAGE_FORMAT_PROPERTIES_2};
	image_format_properties.pNext = &supported_compression_properties;

	vkGetPhysicalDeviceImageFormatProperties2KHR(gpu, &image_format_info, &image_format_properties);

	return supported_compression_properties;
}

VkImageCompressionPropertiesEXT query_applied_compression(VkDevice device, VkImage image)
{
	VkImageSubresource2EXT image_subresource{VK_STRUCTURE_TYPE_IMAGE_SUBRESOURCE_2_KHR};
	image_subresource.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_subresource.imageSubresource.mipLevel   = 0;
	image_subresource.imageSubresource.arrayLayer = 0;

	VkImageCompressionPropertiesEXT compression_properties{VK_STRUCTURE_TYPE_IMAGE_COMPRESSION_PROPERTIES_EXT};
	VkSubresourceLayout2EXT         subresource_layout{VK_STRUCTURE_TYPE_SUBRESOURCE_LAYOUT_2_KHR};
	subresource_layout.pNext = &compression_properties;

	vkGetImageSubresourceLayout2EXT(device, image, &image_subresource, &subresource_layout);

	return compression_properties;
}

VkSurfaceFormatKHR select_surface_format(VkPhysicalDevice gpu, VkSurfaceKHR surface, std::vector<VkFormat> const &preferred_formats)
{
	uint32_t surface_format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &surface_format_count, nullptr);
	assert(0 < surface_format_count);
	std::vector<VkSurfaceFormatKHR> supported_surface_formats(surface_format_count);
	vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &surface_format_count, supported_surface_formats.data());

	auto it = std::ranges::find_if(supported_surface_formats,
	                               [&preferred_formats](VkSurfaceFormatKHR surface_format) {
		                               return std::ranges::any_of(preferred_formats,
		                                                          [&surface_format](VkFormat format) { return format == surface_format.format; });
	                               });

	// We use the first supported format as a fallback in case none of the preferred formats is available
	return it != supported_surface_formats.end() ? *it : supported_surface_formats[0];
}

namespace gbuffer
{
std::vector<LoadStoreInfo> get_load_all_store_swapchain()
{
	// Load every attachment and store only swapchain
	std::vector<LoadStoreInfo> load_store{4};

	// Swapchain
	load_store[0].load_op  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	load_store[0].store_op = VK_ATTACHMENT_STORE_OP_STORE;

	// Depth
	load_store[1].load_op  = VK_ATTACHMENT_LOAD_OP_LOAD;
	load_store[1].store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// Albedo
	load_store[2].load_op  = VK_ATTACHMENT_LOAD_OP_LOAD;
	load_store[2].store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// Normal
	load_store[3].load_op  = VK_ATTACHMENT_LOAD_OP_LOAD;
	load_store[3].store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	return load_store;
}

std::vector<LoadStoreInfo> get_clear_all_store_swapchain()
{
	// Clear every attachment and store only swapchain
	std::vector<LoadStoreInfo> load_store{4};

	// Swapchain
	load_store[0].load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	load_store[0].store_op = VK_ATTACHMENT_STORE_OP_STORE;

	// Depth
	load_store[1].load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	load_store[1].store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// Albedo
	load_store[2].load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	load_store[2].store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	// Normal
	load_store[3].load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	load_store[3].store_op = VK_ATTACHMENT_STORE_OP_DONT_CARE;

	return load_store;
}

std::vector<LoadStoreInfo> get_clear_store_all()
{
	// Clear and store every attachment
	std::vector<LoadStoreInfo> load_store{4};

	// Swapchain
	load_store[0].load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	load_store[0].store_op = VK_ATTACHMENT_STORE_OP_STORE;

	// Depth
	load_store[1].load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	load_store[1].store_op = VK_ATTACHMENT_STORE_OP_STORE;

	// Albedo
	load_store[2].load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	load_store[2].store_op = VK_ATTACHMENT_STORE_OP_STORE;

	// Normal
	load_store[3].load_op  = VK_ATTACHMENT_LOAD_OP_CLEAR;
	load_store[3].store_op = VK_ATTACHMENT_STORE_OP_STORE;

	return load_store;
}

std::vector<VkClearValue> get_clear_value()
{
	// Clear values
	std::vector<VkClearValue> clear_value{4};
	clear_value[0].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
	clear_value[1].depthStencil = {0.0f, ~0U};
	clear_value[2].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};
	clear_value[3].color        = {{0.0f, 0.0f, 0.0f, 1.0f}};

	return clear_value;
}
}        // namespace gbuffer

uint32_t get_queue_family_index(std::vector<VkQueueFamilyProperties> const &queue_family_properties, VkQueueFlagBits queue_flag)
{
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if (queue_flag & VK_QUEUE_COMPUTE_BIT)
	{
		auto propertyIt = std::ranges::find_if(queue_family_properties,
		                                       [queue_flag](const VkQueueFamilyProperties &property) { return (property.queueFlags & queue_flag) && !(property.queueFlags & VK_QUEUE_GRAPHICS_BIT); });
		if (propertyIt != queue_family_properties.end())
		{
			return static_cast<uint32_t>(std::distance(queue_family_properties.begin(), propertyIt));
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (queue_flag & VK_QUEUE_TRANSFER_BIT)
	{
		auto propertyIt = std::ranges::find_if(queue_family_properties,
		                                       [queue_flag](const VkQueueFamilyProperties &property) {
			                                       return (property.queueFlags & queue_flag) && !(property.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
			                                              !(property.queueFlags & VK_QUEUE_COMPUTE_BIT);
		                                       });
		if (propertyIt != queue_family_properties.end())
		{
			return static_cast<uint32_t>(std::distance(queue_family_properties.begin(), propertyIt));
		}
	}

	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	auto propertyIt = std::ranges::find_if(
	    queue_family_properties, [queue_flag](const VkQueueFamilyProperties &property) { return (property.queueFlags & queue_flag) == queue_flag; });
	if (propertyIt != queue_family_properties.end())
	{
		return static_cast<uint32_t>(std::distance(queue_family_properties.begin(), propertyIt));
	}

	throw std::runtime_error("Could not find a matching queue family index");
}

}        // namespace vkb
