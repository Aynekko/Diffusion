#include "const.h"
#include "mathlib.h"

uniform sampler2D   u_ColorMap;
uniform vec2        u_ScreenSizeInv;
uniform float       u_GenericCondition;
uniform float       u_GenericCondition2;

varying vec2        var_TexCoord;

vec3 RGBtoHSV( in vec3 RGB )
{
    vec4 P = ( RGB.g < RGB.b ) ? vec4( RGB.bg, -1.0, 2.0/3.0 ) : vec4( RGB.gb, 0.0, -1.0/3.0 );
    vec4 Q = ( RGB.r < P.x ) ? vec4( P.xyw, RGB.r ) : vec4( RGB.r, P.yzx );
    float C = Q.x - min(Q.w, Q.y);
    float H = abs(( Q.w - Q.y ) / ( 6.0 * C + Epsilon ) + Q.z );
    vec3 HCV = vec3( H, C, Q.x );
    float S = HCV.y / ( HCV.z + Epsilon );
    return vec3( HCV.x, S, HCV.z );
}

vec3 HSVtoRGB( in vec3 HSV )
{
    float H = HSV.x;
    float R = abs( H * 6.0 - 3.0 ) - 1.0;
    float G = 2.0 - abs( H * 6.0 - 2.0 );
    float B = 2.0 - abs( H * 6.0 - 4.0 );
    vec3 RGB = clamp( vec3( R,G,B ), 0.0, 1.0 );
    return(( RGB - 1.0 ) * HSV.y + 1.0 ) * HSV.z;
}

vec4 sharpen( in sampler2D tex, in vec2 coords )
{
    float dx = u_ScreenSizeInv.x;
    float dy = u_ScreenSizeInv.y;
    vec4 sum = vec4( 0.0 );
    sum +=-1. * texture2DLod( tex, coords + vec2(-1.0 * dx , 0.0 * dy ), 0.0 );
    sum +=-1. * texture2DLod( tex, coords + vec2( 0.0 * dx ,-1.0 * dy ), 0.0 );
    sum += 5. * texture2DLod( tex, coords + vec2( 0.0 * dx , 0.0 * dy ), 0.0 );
    sum +=-1. * texture2DLod( tex, coords + vec2( 0.0 * dx , 1.0 * dy ), 0.0 );
    sum +=-1. * texture2DLod( tex, coords + vec2( 1.0 * dx , 0.0 * dy ), 0.0 );
    return sum;
}

void main( void )
{
    vec4 color = texture2D( u_ColorMap, var_TexCoord, 0.0 );
	
    // saturate
    if( bool( u_GenericCondition == 1.0f ))
    {
	float Saturation = 0.55f;
	vec3 col_hsv = RGBtoHSV( color.rgb );
	col_hsv.y *= ( Saturation * 2.0 );
	vec3 col_rgb = HSVtoRGB( col_hsv.rgb );
        color.rgb = col_rgb;
    }

    // sharpen
    if( bool( u_GenericCondition2 == 1.0f ))
    {
	vec4 sharpen = sharpen( u_ColorMap,var_TexCoord );
	color.rgb = mix( color.rgb, sharpen.rgb, 0.2 );
    }

    gl_FragColor = color;
}