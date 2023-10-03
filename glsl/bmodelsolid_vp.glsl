/*
BmodelSolid_vp.glsl - vertex uber shader for all solid bmodel surfaces
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
#if defined( BMODEL_WAVEHEIGHT )
uniform float u_WaveHeight;
#endif

attribute vec3		attr_Position;
attribute vec4		attr_TexCoord0;	// diffuse\terrain
attribute vec4		attr_TexCoord1;	// lightmap 0-1
attribute vec4		attr_TexCoord2;	// lightmap 2-3
attribute vec4		attr_LightStyles;

uniform float		u_LightStyleValues[MAX_LIGHTSTYLES];
uniform mat4		u_ModelMatrix;
uniform vec3		u_TexOffset;	// conveyor stuff
uniform vec3	    u_ViewOrigin;

// shared variables
varying vec2		var_TexDiffuse;
varying vec3		var_TexLight0;
varying vec3		var_TexLight1;
varying vec3		var_TexLight2;
varying vec3		var_TexLight3;
varying vec2		var_TexGlobal;
varying vec3		var_ViewVec;
varying vec3		var_Normal;
varying mat3		var_MatrixTBN;

#if defined( BMODEL_REFLECTION_PLANAR ) || defined( BMODEL_WATER_PLANAR ) || defined( BMODEL_PORTAL )
varying vec4		var_TexMirror;	// mirror coords
#endif

#if defined( REFLECTION_CUBEMAP ) || defined( BMODEL_INTERIOR )
varying vec3	        var_Position;
varying vec3	        var_WorldNormal;
#endif

void main( void )
{
	vec4 position = vec4( attr_Position, 1.0 ); // in object space

#if defined( BMODEL_WAVEHEIGHT )
	float nv = r_turbsin( u_RealTime * 2.6 + attr_Position.y + attr_Position.x ) + 8.0;
	nv = ( r_turbsin( attr_Position.x * 5.0 + u_RealTime * 2.71 - attr_Position.y ) + 8.0 ) * 0.8 + nv;
	position.z = nv * u_WaveHeight + attr_Position.z;
#endif
	vec4 worldpos = u_ModelMatrix * position;
	gl_Position = gl_ModelViewProjectionMatrix * worldpos;
	gl_ClipVertex = gl_ModelViewMatrix * worldpos;

	// compute TBN
	mat3 tbn = ComputeTBN( u_ModelMatrix );
	vec3 srcN = tbn[2];

	// used for diffuse, normalmap, specular and height map
	var_TexDiffuse = ( attr_TexCoord0.xy + u_TexOffset.xy );

#if defined( BMODEL_WATER )
	float os = var_TexDiffuse.x;
	float ot = var_TexDiffuse.y;

	var_TexDiffuse.x = ( os + r_turbsin( ot * 0.125 + u_RealTime )) * (1.0 / 64.0);
	var_TexDiffuse.y = ( ot + r_turbsin( os * 0.125 + u_RealTime )) * (1.0 / 64.0);
#endif

	// setup lightstyles
#if defined( BMODEL_APPLY_STYLE0 )
	var_TexLight0.xy = attr_TexCoord1.xy;
	var_TexLight0.z = u_LightStyleValues[int( attr_LightStyles[0] )];
#endif

#if defined( BMODEL_APPLY_STYLE1 )
	var_TexLight1.xy = attr_TexCoord1.zw;
	var_TexLight1.z = u_LightStyleValues[int( attr_LightStyles[1] )];
#endif

#if defined( BMODEL_APPLY_STYLE2 )
	var_TexLight2.xy = attr_TexCoord2.xy;
	var_TexLight2.z = u_LightStyleValues[int( attr_LightStyles[2] )];
#endif

#if defined( BMODEL_APPLY_STYLE3 )
	var_TexLight3.xy = attr_TexCoord2.zw;
	var_TexLight3.z = u_LightStyleValues[int( attr_LightStyles[3] )];
#endif

#if defined( BMODEL_REFLECTION_PLANAR ) || defined( BMODEL_WATER_PLANAR ) || defined( BMODEL_PORTAL )
	var_TexMirror = ( Mat4Texture( 0.5 ) * gl_TextureMatrix[0] ) * position;
#endif
	var_ViewVec = ( u_ViewOrigin.xyz - worldpos.xyz );
	var_Normal = srcN * tbn;

	var_TexGlobal = attr_TexCoord0.zw;

#if defined( REFLECTION_CUBEMAP ) || defined( BMODEL_INTERIOR )
	var_Position = worldpos.xyz;
	var_WorldNormal = srcN;
#endif

	var_MatrixTBN = tbn;
}