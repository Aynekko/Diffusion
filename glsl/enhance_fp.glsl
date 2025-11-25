#include "const.h"
#include "mathlib.h"
#include "texfetch.h"

uniform sampler2D   u_ColorMap;
uniform vec2        u_ScreenSizeInv;

varying vec2        var_TexCoord;

#define SATURATE
//#define SHARPEN

void main( void )
{
	vec4 color = texture2D( u_ColorMap, var_TexCoord, 0.0 );
	
	// saturate
#if defined( SATURATE )
	const float Saturation = 0.55f;
	vec3 col_hsv = RGBtoHSV( color.rgb );
	col_hsv.y *= ( Saturation * 2.0 );
	vec3 col_rgb = HSVtoRGB( col_hsv.rgb );
	color.rgb = col_rgb;
#endif

	// sharpen
#if defined( SHARPEN )
	vec4 sharpen = sharpen( u_ColorMap, var_TexCoord, u_ScreenSizeInv );
	color.rgb = mix( color.rgb, sharpen.rgb, 0.2 );
#endif

	gl_FragColor = color;
}