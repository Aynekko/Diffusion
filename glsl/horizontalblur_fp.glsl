uniform sampler2D	u_ColorMap;
uniform float	u_Accum;
varying vec2	var_TexCoord;

void main( void )
{
	float h = u_Accum * 0.01;
	vec4 sum = vec4(0.0);
	vec2 pixel = var_TexCoord;

	sum += texture2D(u_ColorMap, vec2(pixel.x - 4.0*h, pixel.y) ) * 0.05;
	sum += texture2D(u_ColorMap, vec2(pixel.x - 3.0*h, pixel.y) ) * 0.09;
	sum += texture2D(u_ColorMap, vec2(pixel.x - 2.0*h, pixel.y) ) * 0.12;
	sum += texture2D(u_ColorMap, vec2(pixel.x - 1.0*h, pixel.y) ) * 0.15;
	sum += texture2D(u_ColorMap, vec2(pixel.x + 0.0*h, pixel.y) ) * 0.16;
	sum += texture2D(u_ColorMap, vec2(pixel.x + 1.0*h, pixel.y) ) * 0.15;
	sum += texture2D(u_ColorMap, vec2(pixel.x + 2.0*h, pixel.y) ) * 0.12;
	sum += texture2D(u_ColorMap, vec2(pixel.x + 3.0*h, pixel.y) ) * 0.09;
	sum += texture2D(u_ColorMap, vec2(pixel.x + 4.0*h, pixel.y) ) * 0.05;

    gl_FragColor = sum / 0.98;
}