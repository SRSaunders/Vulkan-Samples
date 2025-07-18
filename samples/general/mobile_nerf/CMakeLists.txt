# Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 the "License";
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

get_filename_component(FOLDER_NAME ${CMAKE_CURRENT_LIST_DIR} NAME)
get_filename_component(PARENT_DIR ${CMAKE_CURRENT_LIST_DIR} PATH)
get_filename_component(CATEGORY_NAME ${PARENT_DIR} NAME)

add_sample_with_tags(
    ID ${FOLDER_NAME}
    CATEGORY ${CATEGORY_NAME}
    AUTHOR "Qualcomm"
    NAME "Mobile NeRF"
    DESCRIPTION "A Mobile Neural Radiance Field synthesizer sample, based on textured polygons"
    SHADER_FILES_GLSL
        "mobile_nerf/merged.frag"
        "mobile_nerf/merged_morpheus.frag"
        "mobile_nerf/mlp.frag"
        "mobile_nerf/mlp_combo.frag"
        "mobile_nerf/mlp_morpheus.frag"
        "mobile_nerf/mlp_morpheus_combo.frag"
        "mobile_nerf/quad.vert"
        "mobile_nerf/raster.frag"
        "mobile_nerf/raster.vert"
        "mobile_nerf/raster_combo.frag"
        "mobile_nerf/raster_morpheus.frag"
        "mobile_nerf/raster_morpheus_combo.frag")