#include "smaa.h"

varying vec2 var_TexCoord;
varying vec2 var_PixCoord;
varying vec4 var_Offset[3];

void main( void )
{
	var_TexCoord = gl_MultiTexCoord0.xy;//gl_Vertex.xy;// + vec2(1.0);

	var_PixCoord = var_TexCoord * SMAA_RT_METRICS.zw;

	// We will use these offsets for the searches later on (see @PSEUDO_GATHER4):
	var_Offset[0] = mad( SMAA_RT_METRICS.xyxy, vec4( -0.25, API_V_DIR( -0.125 ), 1.25, API_V_DIR( -0.125 ) ), var_TexCoord.xyxy );
	var_Offset[1] = mad( SMAA_RT_METRICS.xyxy, vec4( -0.125, API_V_DIR( -0.25 ), -0.125, API_V_DIR( 1.25 ) ), var_TexCoord.xyxy );

	// And these for the searches, they indicate the ends of the loops:
	var_Offset[2] = mad(
		SMAA_RT_METRICS.xxyy,
		vec4( -2.0, 2.0, API_V_DIR( -2.0 ), API_V_DIR( 2.0 ) ) * float( SMAA_MAX_SEARCH_STEPS ),
		vec4( var_Offset[0].xz, var_Offset[1].yw )
	);

	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
}