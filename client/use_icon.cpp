
#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "triangleapi.h"

DECLARE_MESSAGE( m_UseIcon, UseIcon );

int CUseIcon::Init( void )
{
	HOOK_MESSAGE( UseIcon );
	gHUD.AddHudElem( this );
	return 1;
}

int CUseIcon::VidInit( void )
{
	return 1;
}

int CUseIcon::Draw( float flTime )
{	
	int r, g, b;
	
	x = (ScreenWidth / 2) - 32;  // 64x64 icon, drawing on top of crosshair
	y = (ScreenHeight / 2) - 80;

	if( UsePressed == 2 )
		UnpackRGB(r,g,b,0x0046FF71); // green
	else if( UsePressed == 3 )
		UnpackRGB(r,g,b,0x00FF8A14); // orange
	else if( UsePressed == 4 )
		UnpackRGB(r,g,b,0x00FF3232); // red
	else
		UnpackRGB(r,g,b,0x0046A9FF); // blue

	ScaleColors(r,g,b,150); 

	SPR_Set( m_UseIcon.spr, r, g, b );
			
	SPR_DrawAdditive( 0, x, y, &m_UseIcon.rc );

	//	gEngfuncs.Con_NPrintf( 1, "%i %i\n", x, y );
	return 1;
}

int CUseIcon::MsgFunc_UseIcon( const char *pszName, int iSize, void *pbuf )
{

	BEGIN_READ( pszName, pbuf, iSize );

	UsePressed = READ_BYTE();
//	UseableEntIndex = READ_SHORT();

	if( UsePressed > 0 )
	{
		EnableIcon( UsePressed );
		m_iFlags |= HUD_ACTIVE;
	}
	else
		m_iFlags &= ~HUD_ACTIVE;

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
