#include "const.h"
#include "mathlib.h"

uniform sampler2D	u_ScreenMap;
uniform sampler2D	u_AOMap;
uniform float           u_GenericCondition;

varying vec2	        var_TexCoord;

void main( void )
{
    vec2 tx = var_TexCoord;
    vec3 color = texture2D( u_ScreenMap, tx ).rgb;
    vec3 ssao = texture2D( u_AOMap, tx ).rgb;

    float lumInfluence = 1.0;   
    float lum = GetLuminance( color.rgb );
    vec3 luminance = vec3( lum, lum, lum );    
   
    vec3 final;
    if( bool( u_GenericCondition == 0.0f ))
		final = vec3( color * mix( vec3(ssao), vec3(1.0), luminance * lumInfluence )); 
    else
		final = vec3( mix(vec3(ssao), vec3(1.0), luminance * lumInfluence )) * 0.28; //only AO for test
   
    gl_FragColor = vec4( final, 1.0 );
}


