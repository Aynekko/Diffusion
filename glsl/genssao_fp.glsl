#include "const.h"
#include "mathlib.h"

uniform sampler2D   u_DepthMap;
uniform float       u_zFar;

varying vec2        var_TexCoord;

const float znear = 1.0; // camera clipping start
const float distScale = 2.0;

const float Samples = 8.0;
const float InvSamples = ( 1.0 / Samples );

vec2 VogelDiskSampleAO( float sampleIndex, float phi )
{
	float r = sqrt( sampleIndex * InvSamples );
	float theta = sampleIndex * 2.4 + phi;  //2.4 - GoldenAngle
	return r * vec2( cos( theta ), sin( theta ));
}

float Bayer2( vec2 a ) 
{
	a = floor( a );
	return fract( a.x * 0.5 + a.y * a.y * 0.75 );
}

float Bayer4( vec2 a ) 
{
	return Bayer2( 0.5 * a ) * 0.25 + Bayer2( a );
}

float Bayer8( vec2 a ) 
{
	return Bayer4( 0.5 * a ) * 0.25 + Bayer2( a );
}

float Bayer16( vec2 a ) 
{
	return Bayer8( 0.5 * a ) * 0.25 + Bayer2( a );
}

void main( void )
{
	float zfar = u_zFar;	// camera clipping end	
	vec2 tx = var_TexCoord;
	float radius = 0.4;

	float fSampledDepth = texture2D( u_DepthMap, tx ).r;
	fSampledDepth = linearizeDepth( fSampledDepth, znear, zfar ); // get z-eye

	float ssao = 0.0;
	float rotation = Bayer16( fract( gl_FragCoord.xy / 16.0 ) * 16.0 ) * 6.2831853;	
    
	for( float i = 0.5; i < Samples; i++ )
	{   
		vec2 offset = VogelDiskSampleAO( float( i ), rotation );

		float zSample = texture2D( u_DepthMap, tx + radius * offset / fSampledDepth ).r;
		zSample = linearizeDepth( zSample, znear, zfar );
   
		float dist = max( zSample - fSampledDepth, 0.0 ) / distScale;    
		float occl = 15 * max( dist * ( 1.5 - dist ), 0.0 );
      
		ssao += 1.0 / (( 1.0 + occl * occl ));
		radius += 0.125;
	}
    
    ssao = clamp( ssao * 0.25, 0.0, 1.0 );
    
    gl_FragColor = vec4( ssao );
}