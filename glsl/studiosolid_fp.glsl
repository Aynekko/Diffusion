/*
StudioSolid_fp.glsl - draw solid and alpha-test surfaces
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
#if defined( STUDIO_SPECULAR )
uniform float		u_GlossScale;
uniform float		u_GlossSmoothness;
#endif
#if defined( STUDIO_EMBOSS )
uniform float		u_EmbossScale;
#endif
uniform vec2		u_ReflectScale;
uniform vec3		u_MeshParams[3];
#define MeshOrigin	u_MeshParams[0]
uniform vec4		u_StudioParams[3];
#define u_ViewOrigin	u_StudioParams[0].xyz
#define u_FogParams		u_StudioParams[2]

#if defined( STUDIO_TEXTURE_BLEND )
uniform sampler2D	u_BlendTexture;
#define u_BlendAmount u_MeshParams[2].y
#endif

// shared variables
#if defined( STUDIO_VERTEX_LIGHTING ) && !defined( STUDIO_FULLBRIGHT )
	varying vec3		var_LightDiffuse[4];
	varying vec3		var_LightVec[4];
	uniform vec4		u_StudioLighting[2];	
	#define u_LightStyles	u_StudioLighting[0]	
#else
	varying vec3		var_LightDiffuse;
	varying vec3		var_LightVec;
#endif

varying vec2		var_TexDiffuse;
varying vec3		var_ViewVec;
varying vec3		var_Normal;
varying float		var_Distance;

#if defined( REFLECTION_CUBEMAP ) || defined( STUDIO_INTERIOR )
varying vec3		var_Position;
varying mat3		var_MatrixTBN;
#endif

#if defined( REFLECTION_CUBEMAP )
uniform float		u_Fresnel;
#endif

void main( void )
{		
	vec4 diffuse = texture2D( u_ColorMap, var_TexDiffuse );

#if defined( ALPHA_RESCALING )
	diffuse.a = AlphaRescaling( u_ColorMap, var_TexDiffuse, diffuse.a );
#endif

#if !defined( STUDIO_INTERIOR )
	#if !defined( STUDIO_DEFAULTALPHATEST )
		if( diffuse.a < 0.5f )
			discard;
	#endif
#endif

	// compute the diffuse, emboss and specular term
#if defined( STUDIO_TEXTURE_BLEND )
	vec4 blend_texture = texture2D( u_BlendTexture, var_TexDiffuse );
	// blend them together
	diffuse.rgb = mix( diffuse.rgb, blend_texture.rgb, u_BlendAmount );
#endif

	// apply emboss filter
	#if defined( STUDIO_EMBOSS )
		vec3 emboss = EmbossFilter( u_ColorMap, var_TexDiffuse, u_EmbossScale );
		diffuse.rgb *= emboss;
	#endif

#if defined( STUDIO_HAS_COLORMASK )
	vec4 colormask = texture2D( u_ColorMask, var_TexDiffuse );
	if( colormask.a > 0.5 )
		diffuse.rgb *= u_RenderColor.rgb;
#else
	diffuse.rgb *= u_RenderColor.rgb; // kRenderTransColor
#endif

	// compute the normal term
	vec3 N;	
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
	N = gl_FrontFacing ? -N : N;
	
	vec3 V = normalize( var_ViewVec );
	vec3 light_diffuse;		

#if defined( STUDIO_VERTEX_LIGHTING ) && !defined( STUDIO_FULLBRIGHT )
	vec3 light_diffuse_sum = vec3(0.0);
	vec3 gloss_sum = vec3(0.0);
	#if defined( STUDIO_SPECULAR )
		vec3 glossmap = DiffuseToGlossmap( u_ColorMap, var_TexDiffuse );
	#endif

	// apply lightstyles
	if( u_LightStyles[0] > 0.0 )
	{
		vec3 L = normalize( var_LightVec[0] );		
			
		#if defined( STUDIO_BUMP )
			float NdotB = ComputeStaticBump( L, N );
			light_diffuse = var_LightDiffuse[0] * NdotB;
		#else	
			light_diffuse = var_LightDiffuse[0];
		#endif			
			
		#if defined( STUDIO_SPECULAR )
			float NdotLGloss = saturate( dot( N, L ));				
			vec3 gloss = ComputeSpecular( N, V, L, glossmap, u_GlossSmoothness, u_GlossScale ) * NdotLGloss;				
			gloss_sum += gloss * light_diffuse;			
			light_diffuse *= saturate( 1.0 - GetLuminance( gloss ));
		#endif
			
		light_diffuse_sum += light_diffuse;
	}

	if( u_LightStyles[1] > 0.0 )
	{
		vec3 L = normalize( var_LightVec[1] );		
			
		#if defined( STUDIO_BUMP )
			float NdotB = ComputeStaticBump( L, N );
			light_diffuse = var_LightDiffuse[1] * NdotB;
		#else	
			light_diffuse = var_LightDiffuse[1];
		#endif			
			
		#if defined( STUDIO_SPECULAR )
			float NdotLGloss = saturate( dot( N, L ));				
			vec3 gloss = ComputeSpecular( N, V, L, glossmap, u_GlossSmoothness, u_GlossScale ) * NdotLGloss;				
			gloss_sum += gloss * light_diffuse;			
			light_diffuse *= saturate( 1.0 - GetLuminance( gloss ));
		#endif
			
		light_diffuse_sum += light_diffuse;
	}

	if( u_LightStyles[2] > 0.0 )
	{
		vec3 L = normalize( var_LightVec[2] );		
			
		#if defined( STUDIO_BUMP )
			float NdotB = ComputeStaticBump( L, N );
			light_diffuse = var_LightDiffuse[2] * NdotB;
		#else	
			light_diffuse = var_LightDiffuse[2];
		#endif			
			
		#if defined( STUDIO_SPECULAR )
			float NdotLGloss = saturate( dot( N, L ));				
			vec3 gloss = ComputeSpecular( N, V, L, glossmap, u_GlossSmoothness, u_GlossScale ) * NdotLGloss;				
			gloss_sum += gloss * light_diffuse;			
			light_diffuse *= saturate( 1.0 - GetLuminance( gloss ));
		#endif
			
		light_diffuse_sum += light_diffuse;
	}

	if( u_LightStyles[3] > 0.0 )
	{
		vec3 L = normalize( var_LightVec[3] );		
			
		#if defined( STUDIO_BUMP )
			float NdotB = ComputeStaticBump( L, N );
			light_diffuse = var_LightDiffuse[3] * NdotB;
		#else	
			light_diffuse = var_LightDiffuse[3];
		#endif			
			
		#if defined( STUDIO_SPECULAR )
			float NdotLGloss = saturate( dot( N, L ));				
			vec3 gloss = ComputeSpecular( N, V, L, glossmap, u_GlossSmoothness, u_GlossScale ) * NdotLGloss;				
			gloss_sum += gloss * light_diffuse;			
			light_diffuse *= saturate( 1.0 - GetLuminance( gloss ));
		#endif
			
		light_diffuse_sum += light_diffuse;
	}

	light_diffuse = light_diffuse_sum;
	diffuse.rgb *= light_diffuse; // apply lighting		
#else
	vec3 L = normalize( var_LightVec );	
	#if defined( STUDIO_BUMP )			
		float NdotB = ComputeStaticBump( L, N );
		light_diffuse = var_LightDiffuse * NdotB;
	#else	
		light_diffuse = var_LightDiffuse;
	#endif
	diffuse.rgb *= light_diffuse; // apply lighting
#endif

#if defined( STUDIO_INTERIOR )
	if( diffuse.a < 0.98 )
		diffuse = InteriorMapping( diffuse, var_TexDiffuse, N, 0, var_ViewVec, var_Position ); // u_realtime is currently not used
#endif

	// apply specular lighting
#if defined( STUDIO_SPECULAR ) && !defined( STUDIO_FULLBRIGHT )
	#if defined( STUDIO_VERTEX_LIGHTING )
		#if defined( STUDIO_EMBOSS )
			gloss_sum *= emboss;
		#endif	
		diffuse.rgb += gloss_sum;
	#else
		float NdotLGloss = saturate( dot( N, L ));
		vec3 glossmap = DiffuseToGlossmap( u_ColorMap, var_TexDiffuse );
		vec3 gloss = ComputeSpecular( N, V, L, glossmap, u_GlossSmoothness, u_GlossScale ) * NdotLGloss;
		diffuse.rgb *= saturate( 1.0 - GetLuminance( gloss ));
		#if defined( STUDIO_EMBOSS )
			gloss *= emboss;
		#endif
		diffuse.rgb += gloss * light_diffuse;
	#endif	
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
	float fresnel = GetFresnel( VW, NW, u_Fresnel, u_ReflectScale.x );
	vec3 cubemap_reflection = CubemapReflectionProbe( MeshOrigin, R, u_ReflectScale.y ) * u_ReflectScale.x;
	diffuse.rgb += cubemap_reflection * fresnel * light_diffuse;
#endif

	// apply global fog
	if( u_FogParams.w > 0.0 )
	{
		float dist = var_Distance;
		float fogFactor = exp( -dist * u_FogParams.w );
		fogFactor = clamp( fogFactor, 0.0, 1.0 );
		diffuse.rgb = mix( u_FogParams.xyz, diffuse.rgb, fogFactor );
	}

	// compute final color
	gl_FragColor = vec4( diffuse.rgb, diffuse.a * u_RenderColor.a );
}