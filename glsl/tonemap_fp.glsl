/*
tonemap_fp.glsl - tone mapping for HDRL
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

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_HDRExposure;

varying vec2	        var_TexCoord;

void main( void )
{
    vec4 color = texture2DLod( u_ScreenMap, var_TexCoord, 0.0 );

    float exposure = texture2D( u_HDRExposure, vec2( 0.5 )).r;
    float gamma = 1.5 * exposure;

    gl_FragColor = color * gamma;
}