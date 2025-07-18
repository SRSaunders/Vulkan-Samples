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

#pragma once

#include <string>

#include "debug_info.h"
#include "drawer.h"
#include "platform/configuration.h"
#include "platform/input_events.h"
#include "timer.h"

namespace vkb
{
class Window;

struct ApplicationOptions
{
	bool    benchmark_enabled{false};
	Window *window{nullptr};
};

class Application
{
  public:
	Application();

	virtual ~Application() = default;

	/**
	 * @brief Prepares the application for execution
	 */
	virtual bool prepare(const ApplicationOptions &options);

	/**
	 * @brief Updates the application
	 * @param delta_time The time since the last update
	 */
	virtual void update(float delta_time);

	/**
	 * @brief Main loop sample overlay events
	 * @param delta_time The time taken since the last frame
	 * @param additional_ui Function that implements an additional Gui
	 */
	virtual void update_overlay(
	    float delta_time, const std::function<void()> &additional_ui = []() {});

	/**
	 * @brief Handles cleaning up the application
	 */
	virtual void finish();

	/**
	 * @brief Handles resizing of the window
	 * @param width New width of the window
	 * @param height New height of the window
	 */
	virtual bool resize(const uint32_t width, const uint32_t height);

	/**
	 * @brief Handles input events of the window
	 * @param input_event The input event object
	 */
	virtual void input_event(const InputEvent &input_event);

	/**
	 * @brief Returns the drawer object for the sample
	 */
	virtual Drawer *get_drawer();

	const std::string &get_name() const;

	void set_name(const std::string &name);

	DebugInfo &get_debug_info();

	inline bool should_close() const
	{
		return requested_close;
	}

	// request the app to close
	// does not guarantee that the app will close immediately
	inline void close()
	{
		requested_close = true;
	}

	/**
	 * @brief Set the shading language to be used for this sample (glsl, hlsl)
	 * @param language The shading language that the sample will use
	 */
	static void set_shading_language(const vkb::ShadingLanguage language);

	/**
	 * @brief Returns the selected shading language to be used for this sample (glsl, hlsl)
	 */
	static vkb::ShadingLanguage get_shading_language();

	/**
	 * @brief Returns the default folder for the currently selected shading language
	 */
	static std::string get_shader_folder();

  protected:
	float fps{0.0f};

	float frame_time{0.0f};        // In ms

	uint32_t frame_count{0};

	uint32_t last_frame_count{0};

	bool lock_simulation_speed{false};

	Window *window{nullptr};

  private:
	std::string name{};

	// The debug info of the app
	DebugInfo debug_info{};

	bool requested_close{false};

	/** @brief Used to select between different shader languages, static so it can be changed from a plugin */
	inline static vkb::ShadingLanguage shading_language{vkb::ShadingLanguage::GLSL};
};

inline void Application::set_shading_language(const vkb::ShadingLanguage language)
{
	shading_language = language;
}

inline vkb::ShadingLanguage Application::get_shading_language()
{
	return shading_language;
}

inline std::string Application::get_shader_folder()
{
	switch (shading_language)
	{
		case ShadingLanguage::HLSL:
			return "hlsl";
		case ShadingLanguage::SLANG:
			return "slang";
		default:
			return "glsl";
	}
}

}        // namespace vkb
