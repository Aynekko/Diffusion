/*
StudioDiffuse_fp.glsl - draw solid and alpha-test surfaces
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
#include "texfetch.h"
#if defined( STUDIO_EMBOSS )
#include "emboss.h"
#endif
#include "specular.h"
#if defined( REFLECTION_CUBEMAP )
#include "cubemap.h"
#endif
#if defined( STUDIO_INTERIOR )
#include "interior.h"
#endif
#include "alpha2coverage.h"

uniform sampler2D	u_ColorMap;
uniform sampler2D	u_NormalMap;

#if defined( STUDIO_HAS_COLORMASK )
uniform sampler2D	u_ColorMask;
#endif

uniform vec4		u_RenderColor;
uniform float		u_GlossScale;
uniform float		u_GlossSmoothness;
uniform float		u_EmbossScale;
uniform float		u_ReflectScale;
uniform vec3		u_MeshParams[3];
uniform vec4		u_StudioParams[3];
#define u_ViewOrigin	u_StudioParams[0].xyz
#define u_FogParams		u_StudioParams[2]

#if defined( STUDIO_TEXTURE_BLEND )
uniform sampler2D	u_BlendTexture;
#define u_BlendAmount u_MeshParams[2].y
#endif

// shared variables
varying vec3		var_LightDiffuse;
varying vec2		var_TexDiffuse;
varying vec3		var_LightVec;
varying vec3		var_ViewVec;
varying vec3		var_Normal;

#if defined( REFLECTION_CUBEMAP ) || defined( STUDIO_INTERIOR )
varying vec3		var_Position;
varying mat3		var_MatrixTBN;
#endif

#if defined( REFLECTION_CUBEMAP )
uniform float		u_Fresnel;
#endif

void main( void )
{		
	vec3 L = normalize( var_LightVec );		
	vec3 V = normalize( var_ViewVec );
	vec3 N;
	vec3 MeshOrigin = u_MeshParams[0];

        // compute the normal term
#if defined( STUDIO_BUMP )
	#if defined( STUDIO_INTERIOR )
		N = normalmap2D( u_NormalMap, fract(var_TexDiffuse * vec2(u_InteriorParams.x,u_InteriorParams.y)));
	#else
		N = normalmap2D( u_NormalMap, var_TexDiffuse );
	#endif
#else
	N = normalize( var_Normal );
#endif
	
	// two side materials support
	if( bool( gl_FrontFacing )) N = -N;

	// compute the material defines
	float GlossScale = u_GlossScale; 
	float GlossSmoothness = u_GlossSmoothness; 
	float EmbossScale = u_EmbossScale; 

	// compute the diffuse, emboss and specular term
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );

#if defined( STUDIO_TEXTURE_BLEND )
	vec4 blend_texture = texture2D( u_BlendTexture, var_TexDiffuse );
	// blend them together
	diffuse.rgb = Q_mix( diffuse.rgb, blend_texture.rgb, u_BlendAmount );
#endif

	vec3 glossmap = vec3(1.0);
	#if defined( STUDIO_SPECULAR )
		glossmap = DiffuseToGlossmap( u_ColorMap, var_TexDiffuse );
	#endif
        // apply emboss filter
	#if defined( STUDIO_EMBOSS )
		vec3 emboss = EmbossFilter( u_ColorMap, var_TexDiffuse, EmbossScale );
		diffuse.rgb *= emboss;
	#endif

#if defined( STUDIO_HAS_COLORMASK )
	vec4 colormask = texture2D( u_ColorMask, var_TexDiffuse );
	if( colormask.a > 0.5 )
		diffuse.rgb *= u_RenderColor.rgb;
#else
	diffuse.rgb *= u_RenderColor.rgb; // kRenderTransColor
#endif

	diffuse.rgb *= var_LightDiffuse; // apply lighting

#if defined( STUDIO_INTERIOR )
	diffuse = InteriorMapping( diffuse, var_TexDiffuse, N, 0, var_ViewVec, var_Position ); // u_realtime is currently not used
#endif

#if defined( ALPHA_RESCALING )
	diffuse.a = AlphaRescaling( u_ColorMap, var_TexDiffuse, diffuse.a );
#endif

	if( diffuse.a < 0.5f )
		discard;

	// apply specular lighting
#if defined( STUDIO_SPECULAR )
	float NdotLGloss = saturate( dot( N, L ));
	vec3 gloss = ComputeSpecular( N, V, L, glossmap, GlossSmoothness, GlossScale ) * NdotLGloss;
	diffuse.rgb *= saturate( 1.0 - GetLuminance( gloss ));
	#if defined( STUDIO_EMBOSS )
		gloss *= emboss;
	#endif
	diffuse.rgb += gloss * var_LightDiffuse;
#endif

	// apply cubemap reflection
#if defined( REFLECTION_CUBEMAP )
	vec3 NW = N;
	vec3 VW = V;
	#if defined( STUDIO_BUMP )
		mat3 tbnBasis = mat3( normalize( var_MatrixTBN[0] ), normalize( var_MatrixTBN[1] ), normalize( var_MatrixTBN[2] ));
		NW = normalize( tbnBasis * NW );
		VW = normalize( tbnBasis * VW );
	#endif
	vec3 R = reflect( -VW, NW );
	float fresnel = GetFresnel( VW, NW, u_Fresnel, u_ReflectScale );
	vec3 cubemap_reflection = CubemapReflectionProbe( MeshOrigin, R, glossmap ) * u_ReflectScale;
	diffuse.rgb += cubemap_reflection * fresnel * var_LightDiffuse;
#endif

	// apply global fog
#if defined( STUDIO_FOG_EXP )
	float fogFactor = saturate( exp2( -u_FogParams.w * ( gl_FragCoord.z / gl_FragCoord.w )));
	diffuse.rgb = Q_mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
//	diffuse.a = Q_mix( 0.0, diffuse.a, fogFactor ); // comment out, this messes up models with alpha-channel textures
#endif

	// compute final color
	gl_FragColor = vec4( diffuse.rgb, diffuse.a * u_RenderColor.a );
}