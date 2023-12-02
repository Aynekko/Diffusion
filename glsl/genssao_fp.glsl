#include "const.h"
#include "mathlib.h"

uniform sampler2D   u_DepthMap;
uniform sampler2D   u_RotateMap;

varying vec2        var_TexCoord;
uniform float       u_zFar;

const vec4 rndTable[8] = vec4[8] 
    (
	vec4 ( -0.5, -0.5, -0.5, 0.0 ),
	vec4 (  0.5, -0.5, -0.5, 0.0 ),
	vec4 ( -0.5,  0.5, -0.5, 0.0 ),
	vec4 (  0.5,  0.5, -0.5, 0.0 ),
	vec4 ( -0.5, -0.5,  0.5, 0.0 ),
	vec4 (  0.5, -0.5,  0.5, 0.0 ),
	vec4 ( -0.5,  0.5,  0.5, 0.0 ),
	vec4 (  0.5,  0.5,  0.5, 0.0 )
    );

const float znear = 1.0; // camera clipping start
const float distScale = 2.0;

void main( void )
{
    float zfar = u_zFar;	// camera clipping end	
    vec2 tx = var_TexCoord;
    float radius = 0.5;

    float fSampledDepth = texture2D( u_DepthMap, tx ).r;
    fSampledDepth = linearizeDepth( fSampledDepth, znear, zfar ); // get z-eye

    vec3 noise = 2.0 * texture2D( u_RotateMap, fract( gl_FragCoord.xy * 0.125 )).xyz - vec3( 1.0 );
    float ssao = 0.0;
    
    for( int i = 0; i < 8; i++ )
    {   
        vec3 sample = reflect( rndTable[i].xyz, noise );

        float zSample = texture2D( u_DepthMap, tx + radius * sample.xy / fSampledDepth ).r;
        zSample = linearizeDepth( zSample, znear, zfar );
   
        float dist = max( zSample - fSampledDepth, 0.0 ) / distScale;    
        float occl = 15 * max( dist * ( 1.5 - dist ), 0.0 );
      
        ssao += 1.0 / (( 1.0 + occl * occl ));
        radius += 0.125;
    }
    
    ssao = clamp( ssao * 0.25, 0.0, 1.0 );
    
    gl_FragColor = vec4( ssao );
}