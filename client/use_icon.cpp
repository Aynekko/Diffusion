#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "parsemsg.h"
#include "triangleapi.h"

DECLARE_MESSAGE( m_UseIcon, UseIcon );

int UseIconImage = 0;
float Rotation = 0.0f;
int UseInteractImage = 0;
float alpha = 0;
Vector InteractPos = g_vecZero;

int CUseIcon::Init( void )
{
	HOOK_MESSAGE( UseIcon );
	gHUD.AddHudElem( this );
	return 1;
}

int CUseIcon::VidInit( void )
{
	UseableEntOrigin = g_vecZero;
	InteractPos = g_vecZero;
	if( !UseIconImage )
		UseIconImage = LOAD_TEXTURE( "sprites/use.dds", NULL, 0, 0 );
	if( !UseInteractImage )
		UseInteractImage = LOAD_TEXTURE( "sprites/use_interact.dds", NULL, 0, 0 );
	Rotation = 0.0f;
	alpha = 0.0f;
	return 1;
}

int CUseIcon::Draw( float flTime )
{	
	if( cl_useicon->value <= 0 )
	{
		UseableEntOrigin = g_vecZero;
		InteractPos = g_vecZero;
		alpha = 0.0f;
		if( m_iFlags & HUD_ACTIVE )
			m_iFlags &= ~HUD_ACTIVE;

		return 1;
	}

	if( tr.time == tr.oldtime ) // paused
		return 1;
	
	int r, g, b;
	Vector blue;
	Vector screen;
	
	// decay the alpha
	if( alpha > 0.0f )
	{
		UnpackRGB( r, g, b, 0x0046A9FF );
		blue = Vector( r, g, b );
		// draw interactive circle
		if( UseInteractImage )
		{
			R_WorldToScreen( InteractPos, screen );
			x = XPROJECT( screen[0] );
			y = YPROJECT( screen[1] );
			pglPushMatrix();
			pglTranslatef( x, y, 0 );
			float scale = 100.0f / (1.0f + alpha);
			GL_Bind( 0, UseInteractImage );
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			GL_Color4f( blue.x / 255.0f, blue.y / 255.0f, blue.z / 255.0f, alpha );
			DrawQuad( -scale, -scale, scale, scale );
			gEngfuncs.pTriAPI->End();
			pglPopMatrix();
		}
		alpha -= 2.0f * g_fFrametime;
	}
	
	if( UseableEntOrigin.IsNull() )
		return 1;
	
//	x = (ScreenWidth / 2) - 32;  // 64x64 icon, drawing on top of crosshair
//	y = (ScreenHeight / 2) - 80;

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

	int LastUsePressed = UsePressed;
	UsePressed = READ_BYTE();

	if( UsePressed > 0 )
	{
		if( UsePressed == 5 ) // interaction circle
		{
			InteractPos.x = READ_COORD();
			InteractPos.y = READ_COORD();
			InteractPos.z = READ_COORD();
			alpha = 1.0f;
			UsePressed = LastUsePressed; // do not override it, so icon would not change the color...
		}
		else
		{
			UseableEntOrigin.x = READ_COORD();
			UseableEntOrigin.y = READ_COORD();
			UseableEntOrigin.z = READ_COORD();
			EnableIcon( UsePressed );
		}
		
		m_iFlags |= HUD_ACTIVE;
	}
	else
	{
		UseableEntOrigin = g_vecZero;
		InteractPos = g_vecZero;
		alpha = 0.0f;
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
