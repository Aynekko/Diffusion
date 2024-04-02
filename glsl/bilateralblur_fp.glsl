/*
 * Copyright (c) 2014-2021, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
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
 *
 * SPDX-FileCopyrightText: Copyright (c) 2014-2021 NVIDIA CORPORATION
 * SPDX-License-Identifier: Apache-2.0
 */

// taken from https://github.com/nvpro-samples/gl_ssao

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_DepthMap;
uniform float		u_zFar;
uniform vec2		u_ScreenSizeInv;

varying vec2		var_TexCoord;

const float KERNEL_RADIUS = 3;
const float g_Sharpness = 2.0;

vec4 BlurFunction(vec2 uv, float r, vec4 center_c, float center_d, inout float w_total)
{
	vec4  c = texture2D( u_ScreenMap, uv );
	float d = texture2D( u_DepthMap, uv ).x;

	const float BlurSigma = float(KERNEL_RADIUS) * 0.5;
	const float BlurFalloff = 1.0 / (2.0 * BlurSigma * BlurSigma);

	float ddiff = (d - center_d) * g_Sharpness;
	float w = exp2(-r * r * BlurFalloff - ddiff * ddiff);
	w_total += w;

	return c * w;
}

void main( void )
{
	vec4  center_c = texture2D( u_ScreenMap, var_TexCoord );
	float center_d = texture2D( u_DepthMap, var_TexCoord).x;

	vec4  c_total = center_c;
	float w_total = 1.0;

	vec2 g_InvResolutionDirection = u_ScreenSizeInv;

	for (float r = 1; r <= KERNEL_RADIUS; ++r)
	{
		vec2 uv = var_TexCoord + g_InvResolutionDirection * r;
		c_total += BlurFunction(uv, r, center_c, center_d, w_total);  
	}

	for (float r = 1; r <= KERNEL_RADIUS; ++r)
	{
		vec2 uv = var_TexCoord - g_InvResolutionDirection * r;
		c_total += BlurFunction(uv, r, center_c, center_d, w_total);  
	}

	gl_FragColor = c_total / w_total;
}