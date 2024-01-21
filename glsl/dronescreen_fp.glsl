// author:mensab, https://www.shadertoy.com/view/XttfWH
// modified by Aynekko

#include "const.h"
#include "mathlib.h"

uniform sampler2D u_ColorMap;  // screen
uniform sampler2D u_NormalMap;  // gfx/noise.dds
varying vec2 var_TexCoord;
uniform vec4 u_DroneScreen;
#define u_ScreenSize	u_DroneScreen.xy
#define u_RealTime		u_DroneScreen.z
#define u_OutAmount		u_DroneScreen.w

const float wobble_intensity = 0.002;
const float grade_intensity = 0.25;
const float line_intensity = 2.;
const float vignette_intensity = 0.2;

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

float sample_noise(vec2 fragCoord)
{
	vec2 uv = mod(gl_FragCoord.xy + vec2(0, 100. * u_RealTime), u_ScreenSize.xy);
	float value = texture(u_NormalMap, uv / u_ScreenSize.xy).r;
	return pow(value, 7.); // sharper ramp
}

void main( void )
{
	vec4 original = texture2D( u_ColorMap, var_TexCoord );

    vec2 uv = gl_FragCoord.xy / u_ScreenSize.xy;
    
    // wobble
    vec2 wobbl = vec2( 0.0 ); // disabled this // vec2(wobble_intensity * rand(vec2(u_RealTime, gl_FragCoord.y)), 0.);
    
    // band distortion
    float t_val = tan(0.25 * u_RealTime + uv.y * M_PI * .67);
    vec2 tan_off = vec2(wobbl.x * min(0., t_val), 0.);
    
    // chromab
    vec4 color1 = texture2D(u_ColorMap, uv + wobbl + tan_off);
    vec4 color2 = texture2D(u_ColorMap, (uv + (wobbl * 1.5) + (tan_off * 1.3)) * 1.005);
    // combine + grade
    vec4 color = vec4(color2.rg, pow(color1.b, .67), 1.);
    color.rgb = mix(texture2D(u_ColorMap, uv + tan_off).rgb, color.rgb, grade_intensity * u_OutAmount);
    
    // scanline sim
    float s_val = ((sin(2. * M_PI * uv.y + u_RealTime * 20.) + sin(2. * M_PI * uv.y)) / 2.) * .015 * sin(u_RealTime);
    color += s_val;
    
    // noise lines
    float ival = u_ScreenSize.y / 4.;
    float r = rand(vec2(u_RealTime, gl_FragCoord.y));
    // dirty hack to avoid conditional
    float on = floor(float(int(gl_FragCoord.y + (u_RealTime * r * 1000.)) % int(ival + (line_intensity * u_OutAmount * 0.5f))) / ival);
    float wh = sample_noise(gl_FragCoord.xy) * on;
    color = vec4(min(1., color.r + wh), 
                 min(1., color.g + wh),
                 min(1., color.b + wh), 1.);
    
    float vig = 1. - sin(M_PI * uv.x) * sin(M_PI * uv.y);
    
    vec4 output = color - (vig * vignette_intensity);

	gl_FragColor = vec4( mix( original.rgb, output.rgb, 1.0 ), 1.0 );
}