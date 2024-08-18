#include "hud.h"
#include "utils.h"
#include "triangleapi.h"
#include "r_local.h"

// diffusion - draw mouse cursor

int cursor_img = 0;
#define CURSOR_SIZE 24

int CMouseCursor::Init( void )
{
	x = 0;
	y = 0;

	return 1;
}

int CMouseCursor::VidInit( void )
{
	m_iFlags = 0;
	cursor_img = LOAD_TEXTURE( "sprites/diffusion/cursor.dds", NULL, 0, 0 );
	x = ScreenWidth / 2;
	y = ScreenHeight / 2;

	return 1;
}

bool CMouseCursor::GetMousePosition( void )
{
	gEngfuncs.GetMousePosition( &x, &y );

	if( IS_NAN( x ) || IS_NAN( y ) )
		return false;

	return true;
}

//========================================================================================================================
// DrawCursor: call this function in any of your custom HUD last to draw the cursor (as gHUD.m_Cursor.DrawCursor)
//========================================================================================================================
void CMouseCursor::DrawCursor( void )
{
	if( cursor_img )
	{
		GL_Bind( GL_TEXTURE0, cursor_img );
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		GL_Color4f( 1, 1, 1, 1 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( x, y, x + CURSOR_SIZE, y + CURSOR_SIZE );
		gEngfuncs.pTriAPI->End();
	}
	else // no image?
		gEngfuncs.pfnFillRGBA( x, y, 10, 10, 255, 255, 255, 255 );
}