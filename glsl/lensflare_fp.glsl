// lens flare by Yusef28 (2016)
// adjusted for my purposes, original at https://www.shadertoy.com/view/Xlc3D2

uniform vec2 u_ScreenSizeInv; // not inverted
uniform float u_Accum;
uniform vec2 u_LightOrigin;

varying vec2 var_TexCoord;

float regShape( vec2 p )
{
	float a = atan( p.x, p.y ) + 0.2;
	const float b = 1.047;
	return smoothstep( 0.5, 0.51, cos( floor( a * 0.95 + 0.5 ) * b - a ) * length( p.xy ) );
}

vec3 circle( vec2 p, float size, vec3 color, float dist, vec2 mouse, float hexShape, float Ring, float Small, float Big )
{
	// l is used for making rings.I get the length and pass it through a sinwave
	// but I also use a pow function. pow function + sin function , from 0 and up, = a pulse, at least
	// if you return the max of that and 0.0.

	float l = length( p + mouse * (dist * 4.0) ) + size * 0.5;

	color = 0.5 + 0.5 * sin( color );
	color = cos( vec3( 3.52, 1.92, 1.6 ) + dist * 4.0 ) * 0.5 + 0.5;
	vec3 f = vec3( 0.0 );

	// these are circles
	// if the input (Small, Big...) is 0.0 it won't calculate which is a good optimize
	float big = Big * max( 00.01 - pow( length( p + mouse * dist ), size * 1.4 ), 0.0 ) * 50.0;
	f += big * color;
	float ring = Ring * max( 0.001 - pow( l - 0.3, 0.025 ) + sin( l * 30.0 ), 0.0 ) * 5.0;
	f += ring * color;
	float small = Small * max( 0.04 / pow( length( p - mouse * dist * 0.5 ), 1.0 ), 0.0 ) * 0.05;
	f += small * color;
	float hex = hexShape * max( 00.01 - pow( regShape( p * 5.0 + mouse * dist * 5.0 + 0.9 ), 1.0 ), 0.0 ) * 10.0;
	f += hex * color;

	return f - 0.01;
}

void main( void )
{	
	// I have to use [x - floor(x)] instead of [fract(x)]
	// because Nvidia driver 341.81 is bitching about "non constant expression in initialization"
	const float rnd20 = (( sin( 20.0 ) * 1000.0 ) - floor( sin( 20.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd40 = (( sin( 40.0 ) * 1000.0 ) - floor( sin( 40.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd60 = (( sin( 60.0 ) * 1000.0 ) - floor( sin( 60.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd80 = (( sin( 80.0 ) * 1000.0 ) - floor( sin( 80.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd100 = (( sin( 100.0 ) * 1000.0 ) - floor( sin( 100.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd120 = (( sin( 120.0 ) * 1000.0 ) - floor( sin( 120.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd140 = (( sin( 140.0 ) * 1000.0 ) - floor( sin( 140.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd160 = (( sin( 160.0 ) * 1000.0 ) - floor( sin( 160.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd180 = (( sin( 180.0 ) * 1000.0 ) - floor( sin( 180.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd200 = (( sin( 200.0 ) * 1000.0 ) - floor( sin( 200.0 ) * 1000.0 )) * 3.0 - 0.3;
	const float rnd2000 = pow( (( sin( 2000.0 ) * 1000.0 ) - floor( sin( 2000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd4000 = pow( (( sin( 4000.0 ) * 1000.0 ) - floor( sin( 4000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd6000 = pow( (( sin( 6000.0 ) * 1000.0 ) - floor( sin( 6000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd8000 = pow( (( sin( 8000.0 ) * 1000.0 ) - floor( sin( 8000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd10000 = pow( (( sin( 10000.0 ) * 1000.0 ) - floor( sin( 10000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd12000 = pow( (( sin( 12000.0 ) * 1000.0 ) - floor( sin( 12000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd14000 = pow( (( sin( 14000.0 ) * 1000.0 ) - floor( sin( 14000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd16000 = pow( (( sin( 16000.0 ) * 1000.0 ) - floor( sin( 16000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd18000 = pow( (( sin( 18000.0 ) * 1000.0 ) - floor( sin( 18000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	const float rnd20000 = pow( (( sin( 20000.0 ) * 1000.0 ) - floor( sin( 20000.0 ) * 1000.0 )), 2.0 ) + 1.41;
	
	vec2 uv = gl_FragCoord.xy / u_ScreenSizeInv - 0.5;
	//uv=uv*2.-1.0;
	uv.x *= u_ScreenSizeInv.x / u_ScreenSizeInv.y;

	vec2 mm = u_LightOrigin.xy / u_ScreenSizeInv - 0.5;
	mm.x *= u_ScreenSizeInv.x / u_ScreenSizeInv.y;

	const vec3 circColor = vec3( 0.9, 0.2, 0.1 );

	vec3 color = vec3( 0.0 ); // initialize black screen

	// this calls the function which adds three circle types every time through the loop based on parameters I
	// got by trying things out. rnd i*2000. and rnd i*20 are just to help randomize things more
	color += circle( uv, 1.41, circColor, -0.3, mm, 1.0, 1.0, 0.0, 1.0 );
	color += circle( uv, rnd2000, circColor + 1, rnd20, mm, 0.0, 0.0, 0.0, 1.0 );
	color += circle( uv, rnd4000, circColor + 2, rnd40, mm, 0.0, 0.0, 1.0, 1.0 );
	color += circle( uv, rnd6000, circColor + 3, rnd60, mm, 0.0, 0.0, 0.0, 1.0 );
	color += circle( uv, rnd8000, circColor + 4, rnd80, mm, 0.0, 0.0, 1.0, 1.0 );
	color += circle( uv, rnd10000, circColor + 5, rnd100, mm, 0.0, 0.0, 0.0, 1.0 );
	color += circle( uv, rnd12000, circColor + 6, rnd120, mm, 0.0, 0.0, 1.0, 1.0 );
	color += circle( uv, rnd14000, circColor + 7, rnd140, mm, 0.0, 0.0, 0.0, 1.0 );
	color += circle( uv, rnd16000, circColor + 8, rnd160, mm, 0.0, 0.0, 1.0, 1.0 );
	color += circle( uv, rnd18000, circColor + 9, rnd180, mm, 0.0, 0.0, 0.0, 1.0 );
	color += circle( uv, rnd20000, circColor + 10, rnd200, mm, 0.0, 0.0, 1.0, 1.0 );

	// multiply by the exponetial e^x ? of 1.0-length which kind of masks the brightness more so that
	// there is a sharper roll of of the light decay from the sun. 
	color *= exp( 1.0 - length( uv - mm ) ) * 0.2;

	gl_FragColor = vec4( color, u_Accum );
}