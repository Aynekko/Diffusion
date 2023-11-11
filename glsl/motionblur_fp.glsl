// it's not really a motion blur shader, it's a copypaste of sunshafts with a ray from center of the screen

#include "const.h"
#include "mathlib.h"

uniform sampler2D		u_ScreenMap;
uniform sampler2D		u_ColorMap;
uniform vec3			u_MotionBlur;

varying vec2       		var_TexCoord;

void main( void )
{
	float Multiplier = u_MotionBlur.z * 0.1;
	
	vec2 Vec = saturate( u_MotionBlur.xy ) - var_TexCoord;
	vec2 tc = var_TexCoord;

	vec4 accum = texture2D( u_ColorMap, tc );

	accum += texture2D( u_ColorMap, tc + Vec.xy * Multiplier * 1 ) * (1.0 - 1.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + Vec.xy * Multiplier * 2 ) * (1.0 - 2.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + Vec.xy * Multiplier * 3 ) * (1.0 - 3.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + Vec.xy * Multiplier * 4 ) * (1.0 - 4.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + Vec.xy * Multiplier * 5 ) * (1.0 - 5.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + Vec.xy * Multiplier * 6 ) * (1.0 - 6.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + Vec.xy * Multiplier * 7 ) * (1.0 - 7.0 / 8.0);
	accum += texture2D( u_ColorMap, tc + Vec.xy * Multiplier * 8 ) * (1.0 - 8.0 / 8.0);
	accum /= 8.0;
	
 	vec3 cScreen = texture2D( u_ScreenMap, var_TexCoord ).rgb;

	vec3 outColor = cScreen * 0.4 + accum.rgb;
	gl_FragColor = vec4( outColor, 1.0 );
}