#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_AOMap;
uniform sampler2D   u_DepthMap;
uniform float		u_GenericCondition;
uniform vec4		u_FogParams;
uniform float		u_zFar;

varying vec2		var_TexCoord;

const float lumInfluence = 1.25;

void main( void )
{
    vec3 color = texture2D( u_ScreenMap, var_TexCoord ).rgb;
    float ssao = texture2D( u_AOMap, var_TexCoord ).r;

    float lum = GetLuminance( color.rgb );   
   
    vec3 final;
    if( bool( u_GenericCondition == 0.0f )) // normal mode
	{
		if( u_FogParams.w > 0.0 ) // fog on
		{
			float fSampledDepth = texture2D( u_DepthMap, var_TexCoord ).r;
			fSampledDepth = linearizeDepth( fSampledDepth, Z_NEAR , u_zFar ); // get z-eye
			float fogFactor = exp( -fSampledDepth * u_FogParams.w );
			fogFactor = clamp( fogFactor, 0.0, 1.0 );

			ssao = mix( ssao, 1.0, lum * lumInfluence );
			final = mix( u_FogParams.xyz * ( 1.0 - fogFactor ), color, ssao );
		}
		else // fog off
			final = vec3( color * mix( ssao, 1.0, lum * lumInfluence ));
	}
    else // only AO for test (gl_ssao_debug 1)
		final = vec3( mix( ssao, 1.0, lum * lumInfluence )) * 0.28;
   

    gl_FragColor = vec4( final, 1.0 );
}


