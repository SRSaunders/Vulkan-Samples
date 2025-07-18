/* Copyright (c) 2019-2025, Arm Limited and Contributors
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

#include "afbc.h"

#include "common/vk_common.h"
#include "filesystem/legacy.h"
#include "gltf_loader.h"
#include "gui.h"

#include "rendering/subpasses/forward_subpass.h"
#include "stats/stats.h"

#if defined(VK_USE_PLATFORM_ANDROID_KHR)
#	include "platform/android/android_platform.h"
#endif

#include "scene_graph/components/camera.h"
#include "scene_graph/components/transform.h"
#include "scene_graph/node.h"

AFBCSample::AFBCSample()
{
	// Extension that may be used to query if AFBC is enabled
	add_device_extension(VK_EXT_IMAGE_COMPRESSION_CONTROL_EXTENSION_NAME, true);

	auto &config = get_configuration();

	config.insert<vkb::BoolSetting>(0, afbc_enabled, false);
	config.insert<vkb::BoolSetting>(1, afbc_enabled, true);
}

bool AFBCSample::prepare(const vkb::ApplicationOptions &options)
{
	if (!VulkanSample::prepare(options))
	{
		return false;
	}

	// We want AFBC disabled at start-up
	afbc_enabled = false;
	recreate_swapchain();

	load_scene("scenes/sponza/Sponza01.gltf");

	auto &camera_node = vkb::add_free_camera(get_scene(), "main_camera", get_render_context().get_surface_extent());
	camera            = &camera_node.get_component<vkb::sg::Camera>();

	vkb::ShaderSource vert_shader("base.vert.spv");
	vkb::ShaderSource frag_shader("base.frag.spv");
	auto              scene_subpass = std::make_unique<vkb::ForwardSubpass>(get_render_context(), std::move(vert_shader), std::move(frag_shader), get_scene(), *camera);

	auto render_pipeline = std::make_unique<vkb::RenderPipeline>();
	render_pipeline->add_subpass(std::move(scene_subpass));

	set_render_pipeline(std::move(render_pipeline));

	get_stats().request_stats({vkb::StatIndex::gpu_ext_write_bytes});

	create_gui(*window, &get_stats());

	// Store the start time to calculate rotation
	start_time = std::chrono::system_clock::now();

	return true;
}

void AFBCSample::update(float delta_time)
{
	if (afbc_enabled != afbc_enabled_last_value)
	{
		recreate_swapchain();

		afbc_enabled_last_value = afbc_enabled;
	}

	/* Pan the camera back and forth. */
	auto &camera_transform = camera->get_node()->get_component<vkb::sg::Transform>();

	float rotation_factor = std::chrono::duration<float>(std::chrono::system_clock::now() - start_time).count();

	glm::quat qy          = glm::angleAxis(0.003f * sin(rotation_factor * 0.7f), glm::vec3(0.0f, 1.0f, 0.0f));
	glm::quat orientation = glm::normalize(qy * camera_transform.get_rotation() * glm::angleAxis(0.0f, glm::vec3(1.0f, 0.0f, 0.0f)));
	camera_transform.set_rotation(orientation);

	VulkanSample::update(delta_time);
}

void AFBCSample::recreate_swapchain()
{
	std::set<VkImageUsageFlagBits> image_usage_flags = {VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};

	if (!afbc_enabled)
	{
		// To force-disable AFBC, set an invalid image usage flag
		image_usage_flags.insert(VK_IMAGE_USAGE_STORAGE_BIT);
	}

	get_device().wait_idle();

	get_render_context().update_swapchain(image_usage_flags);
}

void AFBCSample::draw_gui()
{
	get_gui().show_options_window(
	    /* body = */ [this]() {
		    ImGui::Checkbox("Enable AFBC", &afbc_enabled);

		    if (get_device().is_extension_enabled(VK_EXT_IMAGE_COMPRESSION_CONTROL_EXTENSION_NAME))
		    {
			    ImGui::SameLine();
			    ImGui::Text("(%s)", vkb::image_compression_flags_to_string(get_render_context().get_swapchain().get_applied_compression()).c_str());
		    }
	    },
	    /* lines = */ 1);
}

std::unique_ptr<vkb::VulkanSampleC> create_afbc()
{
	return std::make_unique<AFBCSample>();
}
