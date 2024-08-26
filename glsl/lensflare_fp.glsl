// lens flare by Yusef28 (2016)
// adjusted for my purposes, original at https://www.shadertoy.com/view/Xlc3D2

uniform vec2 u_ScreenSizeInv; // not inverted
uniform float u_Accum;
uniform vec2 u_LightOrigin;
uniform vec3 u_LightColor; // not used

varying vec2 var_TexCoord;

float regShape( vec2 p, int N )
{
	float a = atan( p.x, p.y ) + 0.2;
	float b = 6.28319 / float( N );
	return smoothstep( 0.5, 0.51, cos( floor( 0.5 + a / b ) * b - a ) * length( p.xy ) );
}

vec3 circle( vec2 p, float size, vec3 color, vec3 color2, float dist, vec2 mouse )
{
	// l is used for making rings.I get the length and pass it through a sinwave
	// but I also use a pow function. pow function + sin function , from 0 and up, = a pulse, at least
	// if you return the max of that and 0.0.

	float l = length( p + mouse * (dist * 4.0) ) + size / 2.0;

	///these are circles, big, rings, and  tiny respectively
	float c = max( 00.01 - pow( length( p + mouse * dist ), size * 1.4 ), 0.0 ) * 50.;
	float c1 = max( 0.001 - pow( l - 0.3, 1. / 40. ) + sin( l * 30. ), 0.0 ) * 3.;
	float c2 = max( 0.04 / pow( length( p - mouse * dist / 2.0 ) * 1., 1. ), 0.0 ) / 20.;
	float s = max( 00.01 - pow( regShape( p * 5. + mouse * dist * 5. + 0.9, 6 ), 1. ), 0.0 ) * 5.;

	color = 0.5 + 0.5 * sin( color );
	color = cos( vec3( 3.52, 1.92, 1.6 ) + dist * 4. ) * 0.5 + .5;
	vec3 f = c * color;
	f += c1 * color;
	f += c2 * color;
	f += s * color;
	return f - 0.01;
}

void main( void )
{
	const float rnd20 = fract( sin( 20.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd40 = fract( sin( 40.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd60 = fract( sin( 60.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd80 = fract( sin( 80.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd100 = fract( sin( 100.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd120 = fract( sin( 120.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd140 = fract( sin( 140.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd160 = fract( sin( 160.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd180 = fract( sin( 180.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd200 = fract( sin( 200.0 ) * 1000.0 ) * 3.0 - 0.3;
	const float rnd2000 = pow( fract( sin( 2000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd4000 = pow( fract( sin( 4000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd6000 = pow( fract( sin( 6000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd8000 = pow( fract( sin( 8000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd10000 = pow( fract( sin( 10000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd12000 = pow( fract( sin( 12000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd14000 = pow( fract( sin( 14000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd16000 = pow( fract( sin( 16000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd18000 = pow( fract( sin( 18000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	const float rnd20000 = pow( fract( sin( 20000.0 ) * 1000.0 ), 2.0 ) + 1.41;
	
	vec2 uv = gl_FragCoord.xy / u_ScreenSizeInv - 0.5;
	//uv=uv*2.-1.0;
	uv.x *= u_ScreenSizeInv.x / u_ScreenSizeInv.y;

	vec2 mm = u_LightOrigin.xy / u_ScreenSizeInv - 0.5;
	mm.x *= u_ScreenSizeInv.x / u_ScreenSizeInv.y;

	const vec3 circColor = vec3( 0.9, 0.2, 0.1 );
	const vec3 circColor2 = vec3( 0.3, 0.1, 0.5 );

	vec3 color = vec3( 0.0 ); // initialize black screen

	// this calls the function which adds three circle types every time through the loop based on parameters I
	// got by trying things out. rnd i*2000. and rnd i*20 are just to help randomize things more
	color += circle( uv, 1.41, circColor, circColor2, -0.3, mm );
	color += circle( uv, rnd2000, circColor + 1, circColor2 + 1, rnd20, mm );
	color += circle( uv, rnd4000, circColor + 2, circColor2 + 2, rnd40, mm );
	color += circle( uv, rnd6000, circColor + 3, circColor2 + 3, rnd60, mm );
	color += circle( uv, rnd8000, circColor + 4, circColor2 + 4, rnd80, mm );
	color += circle( uv, rnd10000, circColor + 5, circColor2 + 5, rnd100, mm );
	color += circle( uv, rnd12000, circColor + 6, circColor2 + 6, rnd120, mm );
	color += circle( uv, rnd14000, circColor + 7, circColor2 + 7, rnd140, mm );
	color += circle( uv, rnd16000, circColor + 8, circColor2 + 8, rnd160, mm );
	color += circle( uv, rnd18000, circColor + 9, circColor2 + 9, rnd180, mm );
	color += circle( uv, rnd20000, circColor + 10, circColor2 + 10, rnd200, mm );
/*
	// get angle and length of the sun (uv - mouse)
	float a = atan( uv.y - mm.y, uv.x - mm.x );
	float l = max( 1.0 - length( uv - mm ) - 0.84, 0.0 );

	float bright = 0.01;//+0.1/1/3.;//add brightness based on how the sun moves so that it is brightest
	// when it is lined up with the center

	// add the sun with the frill things
	color += u_LightColor * max( 0.1 / pow( length( uv - mm ) * 5.0, 5.0 ), 0.0 ) * abs( sin( a * 5.0 + cos( a * 9.0 ) ) ) * 0.05 * 0.05;
	color += u_LightColor * max( 0.1 / pow(length(uv-mm) * 10.0, 1.0 / 20.0), 0.0) + abs( sin(a * 3.0 + cos(a * 9.0))) / 8.0 * (abs(sin(a * 9.0))) / 1.0;

	// add another sun in the middle (to make it brighter)  with the20color I want, and bright as the numerator.
	color += (max(bright/pow(length(uv-mm)*4., 1./2.), 0.0)*4.)*vec3(0.2, 0.21, 0.3)*4.;
	   // * (0.5+.5*sin(vec3(0.4, 0.2, 0.1) + vec3(a*2., 00., a*3.)+1.3));
*/
	// multiply by the exponetial e^x ? of 1.0-length which kind of masks the brightness more so that
	// there is a sharper roll of of the light decay from the sun. 
	color *= exp( 1.0 - length( uv - mm ) ) / 5.0;

	gl_FragColor = vec4( color, u_Accum );
}