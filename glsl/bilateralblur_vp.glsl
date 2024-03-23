/*
generic_vp.glsl - generic vertex program
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

uniform vec2	u_ScreenSizeInv;

varying vec4	var_Vertex;
varying vec3	var_Position;
varying vec3 	var_Normal;
varying vec2	var_TexCoord;
varying vec4	var_TexCoords[4];

void main( void )
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;  

	var_Position = vec3( gl_ModelViewMatrix * gl_Vertex );	 // transformed point to world space

	var_Vertex = gl_Vertex;

	var_Normal = gl_Normal;

	var_TexCoord = gl_MultiTexCoord0.xy;

	// Compute the offset texture coords
	var_TexCoords[0].st = gl_MultiTexCoord0.xy - u_ScreenSizeInv * 2.0;
	var_TexCoords[0].pq = gl_MultiTexCoord0.xy + u_ScreenSizeInv * 2.0;

	var_TexCoords[1].st = gl_MultiTexCoord0.xy - u_ScreenSizeInv * 4.0;
	var_TexCoords[1].pq = gl_MultiTexCoord0.xy + u_ScreenSizeInv * 4.0;

	var_TexCoords[2].st = gl_MultiTexCoord0.xy - u_ScreenSizeInv * 6.0;
	var_TexCoords[2].pq = gl_MultiTexCoord0.xy + u_ScreenSizeInv * 6.0;

	var_TexCoords[3].st = gl_MultiTexCoord0.xy - u_ScreenSizeInv * 8.0;
	var_TexCoords[3].pq = gl_MultiTexCoord0.xy + u_ScreenSizeInv * 8.0;  
}