/*
StudioDlight_fp.glsl - fragment uber shader for all dlight types for studio meshes
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
#include "mathlib.h"
#include "texfetch.h"
#include "matrix.h"
#if defined( STUDIO_EMBOSS )
#include "emboss.h"
#endif
#if defined( STUDIO_SPECULAR )
#include "specular.h"
#endif
#if defined( STUDIO_INTERIOR )
#include "interior.h"
#endif

uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;

#if defined( STUDIO_HAS_COLORMASK )
	uniform sampler2D	u_ColorMask;
#endif

#if defined( STUDIO_LIGHT_PROJECTION )
uniform sampler2D	u_ProjectMap;
#elif defined( STUDIO_LIGHT_OMNIDIRECTIONAL )
uniform samplerCube	u_ProjectMap;
#endif

#if defined( STUDIO_HAS_SHADOWS )
	#if defined( STUDIO_LIGHT_PROJECTION )
		#include "shadow_proj.h"
	#elif defined( STUDIO_LIGHT_OMNIDIRECTIONAL )
		#include "shadow_omni.h"
	#endif
#endif

uniform vec4		u_LightParams[7];
#define u_LightDir		u_LightParams[0]
#define u_LightDiffuse	u_LightParams[1]
#define u_ShadowParams	u_LightParams[2]
#define u_LightOrigin	u_LightParams[3]
#define u_FogParams		u_LightParams[6]
uniform vec4		u_RenderColor;
#if defined( STUDIO_SPECULAR )
uniform float	        u_GlossSmoothness;
uniform float	        u_GlossScale;
#endif
#if defined( STUDIO_EMBOSS )
uniform float	        u_EmbossScale;
#endif
uniform float		u_DynLightBrightness;
uniform float		u_RealTime;
uniform vec3		u_MeshParams[3];

// shared variables
#if defined( STUDIO_LIGHT_PROJECTION )
varying vec4		var_ProjCoord;
#endif

#if defined( STUDIO_HAS_SHADOWS )
varying vec4		var_ShadowCoord;
#endif

#if defined( STUDIO_TEXTURE_BLEND )
uniform sampler2D	u_BlendTexture;
#define u_BlendAmount u_MeshParams[2].y
#endif

varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;
varying vec3		var_ViewVec;
varying vec3		var_Position;
varying vec3		var_Normal;

#if defined( STUDIO_BUMP ) || defined( STUDIO_INTERIOR )
varying mat3	        var_MatrixTBN;
#endif

void main( void )
{
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );

#if !defined( STUDIO_INTERIOR )
	if( diffuse.a == 0.0 ) discard;
	#if !defined( STUDIO_DEFAULTALPHATEST )
		if( diffuse.a < 0.5 ) discard;
	#endif
#endif

	float dist = length( var_LightVec );
	float atten = 1.0 - saturate( pow( dist * u_LightOrigin.w, u_LightDiffuse.a ));
	if( atten <= 0.0 ) discard; // fast reject
	vec3 L = vec3( 0.0 );
	float shadow = 1.0;

#if defined( STUDIO_LIGHT_PROJECTION )
	L = normalize( var_LightVec );
	
	// spot attenuation
	float spotDot = dot( normalize( u_LightDir.xyz ), L );
	float fov = ( u_LightDir.w * FOV_MULT * ( M_PI / 180.0 ));
	float spotCos = cos( fov + fov );
	if( spotDot < spotCos ) discard;

	vec4 tex_projection = texture2DProj( u_ProjectMap, var_ProjCoord );

	// ignore black pixels of the flashlight/projection texture
	if( length( tex_projection.rgb ) == 0.0 )
		discard;

	#if defined( STUDIO_HAS_SHADOWS )
		// compute shadow early to ignore the rest of the shader
		shadow = ShadowProj( var_ShadowCoord, u_ShadowParams.xy, dot( normalize( var_Normal ), L ));
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#elif defined( STUDIO_LIGHT_OMNIDIRECTIONAL )
	L = normalize( var_LightVec );

	#if defined( STUDIO_HAS_SHADOWS )
		vec3 ShadowVec = VectorRotate( var_LightVec, u_MeshParams[1] ); // rotate entity angles to worldspace
		shadow = ShadowOmni( -ShadowVec, u_ShadowParams );
		if( shadow <= 0.0 ) discard; // fast reject
	#endif
#endif

	vec3 V = normalize( var_ViewVec );
	vec3 N;

	// compute the normal term
#if defined( STUDIO_BUMP )
	N = normalmap2D( u_NormalMap, var_TexDiffuse );
#else
	N = normalize( var_Normal );
#endif

	// two side materials support
	if( bool( gl_FrontFacing )) N = -N;

#if defined( STUDIO_TEXTURE_BLEND )
	vec4 blend_texture = texture2D( u_BlendTexture, var_TexDiffuse );
	// blend them together
	diffuse.rgb = mix( diffuse.rgb, blend_texture.rgb, u_BlendAmount );
#endif

#if defined( STUDIO_HAS_COLORMASK )
	vec4 colormask = texture2D( u_ColorMask, var_TexDiffuse );
	if( colormask.a > 0.5 )
		diffuse.rgb *= u_RenderColor.rgb;
#else
	diffuse.rgb *= u_RenderColor.rgb; // kRenderTransColor
#endif

#if defined( STUDIO_INTERIOR )
	if( diffuse.a < 0.98 )
		diffuse = InteriorMapping( diffuse, var_TexDiffuse, N, 0, var_ViewVec * var_MatrixTBN, var_Position ); // u_realtime is currently not used
#endif

#if defined( STUDIO_BUMP )
	// now, rotate normalmap to worldspace
	N = normalize( var_MatrixTBN * N );
#endif

	vec3 light = vec3( 1.0 );

#if defined( STUDIO_LIGHT_PROJECTION )
	light = u_LightDiffuse.rgb * DLIGHT_SCALE;	// light color
	// texture or procedural spotlight
	light *= 2.0 * u_DynLightBrightness * tex_projection.rgb;
#elif defined( STUDIO_LIGHT_OMNIDIRECTIONAL )
	light = u_LightDiffuse.rgb * DLIGHT_SCALE;
	light *= 2 * u_DynLightBrightness * textureCube( u_ProjectMap, -var_LightVec ).rgb;
#endif

	// do modified hemisperical lighting
	float NdotL = saturate( dot( N, L ));
	if( NdotL <= 0.0 ) discard; // fast reject

	// apply emboss filter
#if defined( STUDIO_EMBOSS )
	vec3 emboss = EmbossFilter( u_ColorMap, var_TexDiffuse, u_EmbossScale );
	diffuse.rgb *= emboss;
#endif

	diffuse.rgb *= light.rgb * NdotL * atten * shadow;

	// !!! remove this if HDR is implemented
	// this is a hack to remove overbright on brighter textures
	float hack = 1.0 - (0.5 * length( diffuse.rgb ));
	if( hack < 0.5 ) hack = 0.5;
	diffuse.rgb *= hack;

	// apply specular lighting
#if defined( STUDIO_SPECULAR )
	vec3 glossmap = DiffuseToGlossmap( u_ColorMap, var_TexDiffuse );
	vec3 gloss = ComputeSpecular( N, V, L, glossmap, u_GlossSmoothness, u_GlossScale ) * ( light * 0.5 ) * NdotL * atten * shadow;
	#if defined( STUDIO_EMBOSS )
		gloss *= emboss;
	#endif
	diffuse.rgb += gloss * shadow;
#endif

	if( u_FogParams.w > 0.0 )
	{
		float dist = length( var_ViewVec );
		float fogFactor = exp( -dist * u_FogParams.w );
		fogFactor = clamp( fogFactor, 0.0, 1.0 );
		atten = mix( 0.0, atten, fogFactor );
	}

	// compute final color
	gl_FragColor = diffuse;
}