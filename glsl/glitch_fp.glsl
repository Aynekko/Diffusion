// author:piyushslayer, https://www.shadertoy.com/view/wld3WN
// modified, simplified (removed unneeded stuff)

uniform sampler2D u_ColorMap;  // screen
varying vec2 var_TexCoord;
uniform vec4 u_Glitch;
#define u_ScreenSizeInv u_Glitch.xy
#define u_RealTime		u_Glitch.z
#define u_GlitchAmount	u_Glitch.w

// amount of seconds for which the glitch loop occurs
#define DURATION 1.0
// percentage of the duration for which the glitch is triggered
#define AMT 1.0 

#define SS(a, b, x) (smoothstep(a, b, x) * smoothstep(b, a, x))

#define UI0 1597334673U
#define UI1 3812015801U
#define UI2 uvec2(UI0, UI1)
#define UI3 uvec3(UI0, UI1, 2798796415U)

// Hash by David_Hoskins
vec3 hash33(vec3 p)
{
	uvec3 q = uvec3(ivec3(p)) * UI3;
	q = (q.x ^ q.y ^ q.z)*UI3;
	return -1. + 2. * vec3(q) * (1. / float(0xffffffffU));
}

// Gradient noise by iq
float gnoise(vec3 x)
{
    // grid
    vec3 p = floor(x);
    vec3 w = fract(x);
    
    // quintic interpolant
    vec3 u = w * w * w * (w * (w * 6. - 15.) + 10.);
    
    // gradients
    vec3 ga = hash33(p + vec3(0., 0., 0.));
    vec3 gb = hash33(p + vec3(1., 0., 0.));
    vec3 gc = hash33(p + vec3(0., 1., 0.));
    vec3 gd = hash33(p + vec3(1., 1., 0.));
    vec3 ge = hash33(p + vec3(0., 0., 1.));
    vec3 gf = hash33(p + vec3(1., 0., 1.));
    vec3 gg = hash33(p + vec3(0., 1., 1.));
    vec3 gh = hash33(p + vec3(1., 1., 1.));
    
    // projections
    float va = dot(ga, w - vec3(0., 0., 0.));
    float vb = dot(gb, w - vec3(1., 0., 0.));
    float vc = dot(gc, w - vec3(0., 1., 0.));
    float vd = dot(gd, w - vec3(1., 1., 0.));
    float ve = dot(ge, w - vec3(0., 0., 1.));
    float vf = dot(gf, w - vec3(1., 0., 1.));
    float vg = dot(gg, w - vec3(0., 1., 1.));
    float vh = dot(gh, w - vec3(1., 1., 1.));
	
    // interpolation
    float gNoise = va + u.x * (vb - va) + 
           		u.y * (vc - va) + 
           		u.z * (ve - va) + 
           		u.x * u.y * (va - vb - vc + vd) + 
           		u.y * u.z * (va - vc - ve + vg) + 
           		u.z * u.x * (va - vb - ve + vf) + 
           		u.x * u.y * u.z * (-va + vb + vc - vd + ve - vf - vg + vh);
    
    return 2. * gNoise;
}

void main( void )
{
	vec4 original = texture2D( u_ColorMap, var_TexCoord );

    vec2 uv = gl_FragCoord.xy * u_ScreenSizeInv;
    float t = u_RealTime;
    
    // smoothed interval for which the glitch gets triggered
    float glitchAmount = 0.1;//SS(DURATION * .001, DURATION * AMT, mod(t, DURATION));  
	float displayNoise = 0.0;
    vec3 col = vec3(0.0);
    vec2 eps = vec2(5.0 / u_ScreenSizeInv.y, 0.0);
    vec2 st = vec2(0.0);

    // analog distortion
    float y = uv.y * u_ScreenSizeInv.x;
    float distortion = gnoise(vec3(0., y * .01, t * 500.)) * (glitchAmount * 4. + .1);
    distortion *= gnoise(vec3(0., y * .02, t * 250.)) * (glitchAmount * 2. + .025);

    ++displayNoise;
    distortion += smoothstep(.999, 1., sin((uv.y + t * 1.6) * 2.)) * .02;
    distortion -= smoothstep(.999, 1., sin((uv.y + t) * 2.)) * .02;
    st = uv + vec2(distortion, 0.);
    // chromatic aberration
    col.r += textureLod(u_ColorMap, st + eps + distortion, 0.).r;
    col.g += textureLod(u_ColorMap, st, 0.).g;
    col.b += textureLod(u_ColorMap, st - eps - distortion, 0.).b;
    
    // white noise + scanlines
    displayNoise = clamp(displayNoise, 0.0, 1.0) * 0.5;
    col += (.15 + .65 * glitchAmount) * (hash33(vec3(gl_FragCoord.xy, mod(float(200), 1000.))).r) * displayNoise;
    col -= (.25 + .75 * glitchAmount) * (sin(4. * t + uv.y * u_ScreenSizeInv.x * 1.75)) * displayNoise;

	float outAmount = u_GlitchAmount; // visibility controlled here
	gl_FragColor = vec4( mix( original.rgb, col.rgb, outAmount ), 1.0 );
}