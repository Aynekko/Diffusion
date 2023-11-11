/*
BmodelDlight_vp.glsl - vertex uber shader for all dlight types for bmodel surfaces
Copyright (C) 2015 Uncle Mike

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
#include "matrix.h"
#include "tnbasis.h"

#define u_RealTime	u_TexOffset.z

attribute vec3		attr_Position;
attribute vec4		attr_TexCoord0;

// model specific
uniform mat4		u_ModelMatrix;
uniform vec3		u_TexOffset;

// light specific
uniform mat4		u_LightViewProjectionMatrix;
uniform vec4			u_LightParams[5];
#define u_LightOrigin	u_LightParams[3]
#define u_ViewOrigin	u_LightParams[4].xyz
#define u_WaveHeight	u_LightParams[4].w

// shared variables
varying vec2		var_TexDiffuse;
varying vec2		var_TexGlobal;
varying vec3		var_LightVec;
varying vec3		var_ViewVec;
varying vec3		var_Normal;
varying mat3		var_MatrixTBN;
varying vec3		var_Position;

#if defined( BMODEL_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
varying vec4		var_ShadowCoord;
#endif

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 ); // in object space

	if( u_WaveHeight > 0.0f )
	{
		float nv = r_turbsin( u_RealTime * 2.6 + attr_Position.y + attr_Position.x ) + 8.0;
		nv = ( r_turbsin( attr_Position.x * 5.0 + u_RealTime * 2.71 - attr_Position.y ) + 8.0 ) * 0.8 + nv;
		position.z = nv * u_WaveHeight + attr_Position.z;
	}

	vec4 worldpos = u_ModelMatrix * position;

	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	mat3 tbn = ComputeTBN( u_ModelMatrix );
	vec3 srcN = tbn[2];

	var_TexDiffuse = ( attr_TexCoord0.xy + u_TexOffset.xy );

#if defined( BMODEL_WATER )
	float os = var_TexDiffuse.x;
	float ot = var_TexDiffuse.y;
	// during the war sine values can reach 3.0!
	var_TexDiffuse.x = ( os + sin( ot * 0.125 + u_TexOffset.z ) * 4.0 ) * (1.0 / 64.0);
	var_TexDiffuse.y = ( ot + sin( os * 0.125 + u_TexOffset.z ) * 4.0 ) * (1.0 / 64.0);
#endif

#if defined( BMODEL_LIGHT_PROJECTION )
	var_ProjCoord = ( Mat4Texture( -0.5 ) * u_LightViewProjectionMatrix ) * worldpos;
#if defined( BMODEL_HAS_SHADOWS )
	var_ShadowCoord = ( Mat4Texture( 0.5 ) * u_LightViewProjectionMatrix ) * worldpos;
#endif
#endif
	// these things are in worldspace and not a normalized
	var_LightVec = ( u_LightOrigin.xyz - worldpos.xyz );
	var_ViewVec = ( u_ViewOrigin.xyz - worldpos.xyz );
	var_Normal = srcN;
	var_MatrixTBN = tbn;

#if defined( BMODEL_INTERIOR )
	var_Position = worldpos.xyz;
#endif
	var_TexGlobal = attr_TexCoord0.zw;
}