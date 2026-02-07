#include "smaa.h"

varying vec2 var_TexCoord;
varying vec4 var_Offset;

void main( void )
{
	var_TexCoord = gl_MultiTexCoord0.xy;//gl_Vertex.xy;
	var_Offset = mad( SMAA_RT_METRICS.xyxy, vec4( 1.0, 0.0, 0.0, API_V_DIR( 1.0 ) ), var_TexCoord.xyxy );

	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}
