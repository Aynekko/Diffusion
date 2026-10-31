/*
genhbao_fp.glsl - HBAO+ ambient occlusion
NVIDIA GFSDK_SSAO / nvpro-samples gl_ssao
*/

#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_DepthMap;
uniform vec2		u_HBAOParams[5];
#define FocalLen	u_HBAOParams[0]		// 1/tan(fovx/2), 1/tan(fovy/2)
#define UVToViewA	u_HBAOParams[1]
#define UVToViewB	u_HBAOParams[2]
#define LinMAD		u_HBAOParams[3]
#define ScreenRes	u_HBAOParams[4]		// depth buffer size (full res)

varying vec2		var_TexCoord;

#define AO_RADIUS		32.0	// view-space radius in map units
#define AO_BIAS			0.1	// hides self occlusion on flat / low-poly surfaces (HBAO+ Bias)
#define AO_POWER		1.5	// final exponent (HBAO+ PowerExponent)
#define AO_DIRS			8	// directions per pixel
#define AO_STEPS		4	// steps per direction
#define AO_MAX_FOOTPRINT	0.1	// max kernel radius, fraction of the screen height

const float NegInvR2 = -1.0 / (AO_RADIUS * AO_RADIUS);
const float AOMultiplier = 1.0 / (1.0 - AO_BIAS);

float ViewSpaceZFromDepth( float d )
{
	// [0,1] -> [-1,1] clip space
	d = d * 2.0 - 1.0;

	// view space Z (negative, camera looks down -Z)
	return -1.0 / (LinMAD.x * d + LinMAD.y);
}

vec3 UVToViewSpace( vec2 uv, float z )
{
	uv = UVToViewA * uv + UVToViewB;
	return vec3( uv * z, z );
}

vec2 SnapToTexel( vec2 uv )
{
	// + 0.25: at half resolution the AO pixel centre sits on the corner of 4 depth texels
	// fAOScale = 0.5 in r_postprocess.cpp
	return (floor( uv * ScreenRes + 0.25 ) + 0.5) / ScreenRes;
}

vec3 FetchViewPos( vec2 uv )
{
	vec2 halfTexel = 0.5 / ScreenRes;
	uv = clamp( uv, halfTexel, 1.0 - halfTexel );

	float z = ViewSpaceZFromDepth( texture2D( u_DepthMap, uv ).r );
	return UVToViewSpace( uv, z );
}

vec3 MinDiff( vec3 P, vec3 Pr, vec3 Pl )
{
	vec3 V1 = Pr - P;
	vec3 V2 = P - Pl;
	return (dot( V1, V1 ) < dot( V2, V2 )) ? V1 : V2;
}

vec3 ReconstructNormal( vec2 uv, vec3 P )
{
	vec2 invRes = 1.0 / ScreenRes;

	vec3 Pr = FetchViewPos( uv + vec2( invRes.x, 0.0 ) );
	vec3 Pl = FetchViewPos( uv + vec2( -invRes.x, 0.0 ) );
	vec3 Pt = FetchViewPos( uv + vec2( 0.0, invRes.y ) );
	vec3 Pb = FetchViewPos( uv + vec2( 0.0, -invRes.y ) );

	vec3 N = normalize( cross( MinDiff( P, Pr, Pl ), MinDiff( P, Pt, Pb ) ) );

	if( dot( N, P ) > 0.0 )
		N = -N;

	return N;
}

float Bayer2( vec2 p )
{
	return 2.0 * p.x + 3.0 * p.y - 4.0 * p.x * p.y;
}

float Bayer4( vec2 p )
{
	p = mod( floor( p ), 4.0 );
	return 4.0 * Bayer2( mod( p, 2.0 ) ) + Bayer2( floor( p * 0.5 ) );
}

vec2 GetJitter( void )
{
	float b = Bayer4( gl_FragCoord.xy );
	return vec2( b / 16.0 + 1.0 / 32.0, mod( b * 7.0, 16.0 ) / 16.0 + 1.0 / 32.0 );
}

// P = view-space position at the kernel centre
// N = view-space normal at the kernel centre
// S = view-space position of the sample
float ComputeAO( vec3 P, vec3 N, vec3 S )
{
	vec3 V = S - P;
	float VdotV = dot( V, V );
	float NdotV = dot( N, V ) * inversesqrt( max( VdotV, Epsilon ) );

	return saturate( NdotV - AO_BIAS ) * saturate( VdotV * NegInvR2 + 1.0 );
}

void main( void )
{
	vec2 uv = SnapToTexel( var_TexCoord );

	// check depth to prevent from appearing in the sky box
	float fSampledDepth = texture2D( u_DepthMap, uv ).r;
	if( fSampledDepth > 0.999 )
	{
		gl_FragColor = vec4( 1.0 );
		return;
	}

	vec3 P = UVToViewSpace( uv, ViewSpaceZFromDepth( fSampledDepth ) );
	vec3 N = ReconstructNormal( uv, P );

	// projected radius of the hemisphere in depth-buffer pixels
	float radiusPixels = AO_RADIUS * FocalLen.y * 0.5 * ScreenRes.y / -P.z;
	radiusPixels = min( radiusPixels, AO_MAX_FOOTPRINT * ScreenRes.y );

	// footprint below one pixel: nothing to evaluate
	if( radiusPixels < 1.0 )
	{
		gl_FragColor = vec4( 1.0 );
		return;
	}

	// divide by (steps + 1) so that the farthest samples are not fully attenuated
	float stepPixels = radiusPixels / (float( AO_STEPS ) + 1.0);

	vec2 jitter = GetJitter();
	vec2 invRes = 1.0 / ScreenRes;
	const float alpha = 2.0 * M_PI / float( AO_DIRS );

	float ao = 0.0;

	for( int d = 0; d < AO_DIRS; d++ )
	{
		float angle = alpha * (float( d ) + jitter.x);
		vec2 dir = vec2( cos( angle ), sin( angle ) );

		// jitter the first sample inside the first step
		float rayPixels = jitter.y * stepPixels + 1.0;

		for( int s = 0; s < AO_STEPS; s++ )
		{
			// snap to a whole pixel offset = exact depth texel
			vec2 offset = floor( rayPixels * dir + 0.5 );
			vec3 S = FetchViewPos( uv + offset * invRes );

			rayPixels += stepPixels;
			ao += ComputeAO( P, N, S );
		}
	}

	ao *= AOMultiplier / float( AO_DIRS * AO_STEPS );
	ao = saturate( 1.0 - ao * 2.0 );

	gl_FragColor = vec4( pow( ao, AO_POWER ) );
}
