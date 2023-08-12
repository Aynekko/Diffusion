/*
monochrome_fp.glsl - monochrome effect
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

uniform sampler2D	u_ColorMap;
uniform float	u_Accum;

varying vec2	var_TexCoord;

void main( void )
{
	vec3 diffuse = texture2D( u_ColorMap, var_TexCoord ).rgb;

	diffuse = mix( diffuse, vec3( GetLuminance( diffuse )), saturate( u_Accum ));

	gl_FragColor = vec4( diffuse, 1.0 );
}