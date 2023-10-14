#include "animatex.h"
#include "hud.h"
#include "r_local.h"
#include "triangleapi.h"
#include "utils.h"
#include "r_opengl.h"

void CAnimatex::Init( char *Tex )
{
	fCurFrame = 0.0f;
	xmin = 0;
	xmax = 100;
	ymin = 0;
	ymax = 100;
	iTotalFrames = 0;
	iFrame = 0;
	memset( Texture, 0, sizeof( Texture ) );

	// load frames
	char tmp[MAX_PATH];
	Q_strcpy( tmp, Tex );
	COM_StripExtension( tmp );
	char Path[128];
	int i;

	for( i = 0; i < MAX_ANIMATEX_FRAMES; i++ )
	{
		sprintf_s( Path, "%s_%i", tmp, i );
		if( IMAGE_EXISTS( Path ) )
			Texture[i] = LOAD_TEXTURE( Path, NULL, 0, 0 );
		else
		{
			if( i == 0 )
				ConPrintf( "^3Error:^7 CAnimatex can't load any frames for \"%s\".\n", Tex );

			break;
		}

		// successfully loaded a frame
		iTotalFrames++;
	}

	ConPrintf( "^2Animatex:^7 loaded %i frames for \"%s\".\n", iTotalFrames, Tex );
}

bool CAnimatex::Initialized( void )
{
	return (Texture[0] > 0);
}

void CAnimatex::Free( void )
{
	if( !iTotalFrames )
		return;

	for( int i = 0; i < iTotalFrames; i++ )
	{
		if( Texture[i] )
			FREE_TEXTURE( Texture[i] );
	}
}

void CAnimatex::SetPos( int x_min, int y_min, int x_max, int y_max )
{
	xmin = x_min;
	ymin = y_min;
	xmax = x_max;
	ymax = y_max;
}

int CAnimatex::GetTotalFrames( void )
{
	return iTotalFrames;
}

int CAnimatex::GetCurFrame( void )
{
	return (int)fCurFrame;
}

void CAnimatex::DrawFrame( int Frame )
{
	GL_Bind( 0, Texture[Frame] );
	pglBegin( GL_QUADS );
	DrawQuad( xmin, ymin, xmax, ymax );
	pglEnd();
}

void CAnimatex::DrawAnimate( float Speed )
{
	if( !Initialized() )
		return;

	fCurFrame += Speed * g_fFrametime;
	if( fCurFrame >= GetTotalFrames() )
		fCurFrame = 0;

	DrawFrame( (int)fCurFrame );
}

void CAnimatex::SetColor( int r, int g, int b, int a )
{
	float R = r / 255.0f;
	float G = g / 255.0f;
	float B = b / 255.0f;
	float A = a / 255.0f;

	pglColor4f( R, G, B, A );
}

void CAnimatex::SetRenderMode( int RenderMode )
{
	gEngfuncs.pTriAPI->RenderMode( RenderMode );
}