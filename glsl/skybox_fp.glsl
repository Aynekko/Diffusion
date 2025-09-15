/*
skybox_fp.glsl - draw sun & skycolor
Copyright (C) 2014 Uncle Mike

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
*/

#include "const.h"
#include "mathlib.h"

uniform sampler2D		u_ColorMap;
uniform vec4		u_FogParams;
uniform float		u_GenericCondition;
#define MagicGamma	u_GenericCondition

varying vec2		var_TexCoord;

void main( void )
{
	vec4 sky_color = texture2D( u_ColorMap, var_TexCoord );
	vec4 diffuse = sky_color * MagicGamma;

	if( u_FogParams.w > 0.0 )
	{
		float fogFactor = exp( -32000.0 * u_FogParams.w );
		fogFactor = clamp( fogFactor, 0.0, 1.0 );
		diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
	}

	gl_FragColor = diffuse;
}