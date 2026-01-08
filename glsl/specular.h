#ifndef SPECULAR_H
#define SPECULAR_H

vec3 DiffuseToGlossmap( vec3 rgb )
{
	vec3 glossmap = vec3( clamp( dot( rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );

	return glossmap * glossmap;
}

#if defined( BMODEL_MULTI_LAYERS )
vec3 DiffuseToGlossmapTerrain( sampler2DArray ColorMap, vec2 texcoord, vec4 mask0, vec4 mask1, vec4 mask2, vec4 mask3 )
{
	vec4 res = vec4( 0.0 );
	vec3 glossmap = vec3( 0.0 );

#if TERRAIN_NUM_LAYERS >= 1
	if( mask0.r > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 0.0 ) ) * mask0.r;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 2
	if( mask0.g > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 1.0 ) ) * mask0.g;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 3
	if( mask0.b > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 2.0 ) ) * mask0.b;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 4
	if( mask0.a > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 3.0 ) ) * mask0.a;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 5
	if( mask1.r > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 4.0 ) ) * mask1.r;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 6
	if( mask1.g > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 5.0 ) ) * mask1.g;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 7
	if( mask1.b > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 6.0 ) ) * mask1.b;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 8
	if( mask1.a > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 7.0 ) ) * mask1.a;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 9
	if( mask0.r > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 8.0 ) ) * mask2.r;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 10
	if( mask0.g > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 9.0 ) ) * mask2.g;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 11
	if( mask0.b > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 10.0 ) ) * mask2.b;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 12
	if( mask0.a > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 11.0 ) ) * mask2.a;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 13
	if( mask1.r > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 12.0 ) ) * mask3.r;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 14
	if( mask1.g > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 13.0 ) ) * mask3.g;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 15
	if( mask1.b > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 14.0 ) ) * mask3.b;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

#if TERRAIN_NUM_LAYERS >= 16
	if( mask1.a > 0.0 )
		res += texture2DArray( ColorMap, vec3( texcoord, 15.0 ) ) * mask3.a;
	glossmap = vec3( clamp( dot( res.rgb, vec3( 0.33 ) ), 0.0, 1.0 ) );
#endif

	return glossmap * glossmap;
}
#endif // /BMODEL_MULTI_LAYERS

float SpecularAntialiasing( vec3 N, float roughness )
{
	const float sigma2 = 0.5; // default value is 0.25
	const float kappa = 0.18;
	vec3 dndu = dFdx( N );
	vec3 dndv = dFdy( N );
	float variance = sigma2 * (dot( dndu, dndu ) + dot( dndv, dndv ));
	float kernelRoughness2 = min( variance, kappa );

	return saturate( roughness + kernelRoughness2 );
}

vec3 SpecularBRDF( vec3 N, vec3 V, vec3 L, vec3 f0, float m, float scale )
{
	vec3 H = normalize( V + L );
	float m2 = m * m;

	// GGX NDF
	float NdotH = saturate( dot( N, H ) );
	float spec = (NdotH * m2 - NdotH) * NdotH + 1.0;
	spec = m2 / (spec * spec) * scale;

	float NdotL = saturate( dot( N, L ) );
	float NdotV = saturate( dot( N, V ) );
	float r = m + 1.0;
	float k = r * r * 0.125;
	spec /= (4.0 * (NdotV * (1.0 - k) + k) * (NdotL * (1.0 - k) + k));

	// Fresnel (Schlick approximation)
	float f90 = saturate( dot( f0, vec3( 0.33333 ) ) / 0.02 );  // Assume micro-occlusion when reflectance is below 2%
	vec3 fresnel = mix( f0, vec3( f90 ), pow( 1.0 - saturate( dot( L, H ) ), 5.0 ) );

	return fresnel * spec;
}

vec3 ComputeSpecular( vec3 N, vec3 V, vec3 L, vec3 glossmap, float smoothness, float scale )
{
	float sm_to_rgh = 1.0 - smoothness; // to roughness
	float roughness = max( sm_to_rgh * sm_to_rgh, 0.001 );  // Prevent highlights from getting too tiny without area lights
	roughness = SpecularAntialiasing( N, roughness );
	return SpecularBRDF( N, V, L, glossmap, roughness, scale );
}

#endif//SPECULAR_H