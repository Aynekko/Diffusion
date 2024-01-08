#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "parsemsg.h"
#include "triangleapi.h"

DECLARE_MESSAGE( m_UseIcon, UseIcon );

int UseIconImage = 0;
float Rotation = 0.0f;

int CUseIcon::Init( void )
{
	HOOK_MESSAGE( UseIcon );
	gHUD.AddHudElem( this );
	return 1;
}

int CUseIcon::VidInit( void )
{
	UseableEntOrigin = g_vecZero;
	if( !UseIconImage )
		UseIconImage = LOAD_TEXTURE( "sprites/use.dds", NULL, 0, 0 );
	Rotation = 0.0f;
	return 1;
}

int CUseIcon::Draw( float flTime )
{	
	int r, g, b;

	if( UseableEntOrigin.IsNull() )
		return 1;
	
//	x = (ScreenWidth / 2) - 32;  // 64x64 icon, drawing on top of crosshair
//	y = (ScreenHeight / 2) - 80;

	Vector screen;
	R_WorldToScreen( UseableEntOrigin, screen );
	x = XPROJECT( screen[0] );
	y = YPROJECT( screen[1] );
		
	switch( UsePressed )
	{
		case 2: UnpackRGB( r, g, b, 0x0046FF71 ); // green
			break;
		case 3: UnpackRGB( r, g, b, 0x00FF8A14 ); // orange
			break;
		case 4: UnpackRGB( r, g, b, 0x00FF3232 ); // red
			break;
		default:
			UnpackRGB( r, g, b, 0x0046A9FF ); // blue
			break;
	}

	ScaleColors(r,g,b,150); 

	// for "can use" and "pressed" I use dds image
	if( UsePressed == 1 || UsePressed == 2 )
	{
		if( UseIconImage )
		{
			GL_Bind( 0, UseIconImage );
			pglPushMatrix();
			pglTranslatef( x, y, 0 );
			pglRotatef( Rotation, 0, 0, 1 );
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			GL_Color4f( r / 255.0f, g / 255.0f, b / 255.0f, 1.0f );
			DrawQuad( -32, -32, 32, 32 );
			gEngfuncs.pTriAPI->End();
			pglPopMatrix();
			Rotation += 100 * g_fFrametime;
			if( Rotation >= 360 )
				Rotation = 0;
		}
	}
	else
	{
		SPR_Set( m_UseIcon.spr, r, g, b );
		SPR_DrawAdditive( 0, x - m_UseIcon.rc.right * 0.5f, y - m_UseIcon.rc.bottom * 0.5f, &m_UseIcon.rc );
	}

	return 1;
}

int CUseIcon::MsgFunc_UseIcon( const char *pszName, int iSize, void *pbuf )
{

	BEGIN_READ( pszName, pbuf, iSize );

	UsePressed = READ_BYTE();

	if( UsePressed > 0 )
	{
		UseableEntOrigin.x = READ_COORD();
		UseableEntOrigin.y = READ_COORD();
		UseableEntOrigin.z = READ_COORD();
		EnableIcon( UsePressed );
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		UseableEntOrigin = g_vecZero;
		m_iFlags &= ~HUD_ACTIVE;
	}

	END_READ();

	return 1;
}

void CUseIcon::EnableIcon( int UsePressed )
{	
	char *pszIconName;
	
	switch( UsePressed )
	{
	case 1: pszIconName = "use"; break;
	case 2: pszIconName = "use_yes"; break;
	case 3: pszIconName = "use_busy"; break;
	case 4: pszIconName = "use_locked"; break;
	}
	
	// the sprite must be listed in hud.txt
	UseIconSprite = gHUD.GetSpriteIndex( pszIconName );
	m_UseIcon.spr = gHUD.GetSprite( UseIconSprite );
	m_UseIcon.rc = gHUD.GetSpriteRect( UseIconSprite );
	Q_strcpy( m_UseIcon.szSpriteName, pszIconName );
}
