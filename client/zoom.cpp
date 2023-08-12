#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "weaponinfo.h"

//===========================================================================================
// diffusion - all weapon zooming is handled right here
// NOTENOTE: weapon disappearance is handled in V_CalcFirstPersonRefdef
//===========================================================================================

DECLARE_MESSAGE( m_Zoom, Zoom );

int CZoom::Init( void )
{
	HOOK_MESSAGE( Zoom );
	gHUD.AddHudElem( this );
	return 1;
}

int CZoom::MsgFunc_Zoom( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	ZoomMode = READ_BYTE();
	WeaponID = READ_BYTE();
	END_READ();

	m_iFlags |= HUD_ACTIVE;
	if( ZoomMode > 0 )
	{
		gHUD.IsZooming = true;
		gHUD.IsZoomed = true;
	}
	else
	{
		gHUD.IsZoomed = false;
		gHUD.m_flFOV = 0.0f;
	}
	
	return 1;
}

void CZoom::Think( void )
{
	float FOV = gHUD.m_flFOV;

	if (ZoomMode == 0)
	{
		FOV = 0;
		m_iFlags &= ~HUD_ACTIVE;
		gHUD.IsZooming = false;
		gHUD.IsZoomed = false;
		return;
	}
	
	//=====================================
	// zoom function for crossbow
	//=====================================
	if( WeaponID == WEAPON_CROSSBOW )
	{
		if( ZoomMode == 1 ) // zooming in
		{
			FOV -= 150 * g_fFrametime;
			gHUD.m_flFOV = (int)FOV;

			if( FOV <= 20 )
			{
				gHUD.IsZooming = false;
				gHUD.IsZoomed = true;
				gHUD.SetFOV( 20 );
				m_iFlags &= ~HUD_ACTIVE;
				return;
			}
		}
		else if( ZoomMode == 2 ) // zooming out (instant)
		{
			gHUD.IsZooming = false;
			gHUD.IsZoomed = false;
			gHUD.SetFOV( 0 );
			m_iFlags &= ~HUD_ACTIVE;
			return;
		}
	}
	
	//=====================================
	// zoom function for sniper rifle
	//=====================================
	if( (WeaponID == WEAPON_SNIPER) || (WeaponID == WEAPON_GAUSS) )
	{
		if( ZoomMode == 1 ) // zooming in
		{
			FOV -= 200 * g_fFrametime;
			gHUD.m_flFOV = (int)FOV;

			if( FOV <= 30 )
			{
				gHUD.IsZooming = false;
				gHUD.IsZoomed = true;
				gHUD.SetFOV( 30 );
				m_iFlags &= ~HUD_ACTIVE;
				return;
			}
		}
		else if( ZoomMode == 2 ) // zooming in to max
		{
			FOV -= 200 * g_fFrametime;
			gHUD.m_flFOV = (int)FOV;

			if( FOV <= 10 )
			{
				gHUD.IsZooming = false;
				gHUD.IsZoomed = true;
				gHUD.SetFOV( 10 );
				m_iFlags &= ~HUD_ACTIVE;
				return;
			}
		}
		else if( ZoomMode == 3 ) // zooming out (instant)
		{
			gHUD.IsZooming = false;
			gHUD.IsZoomed = false;
			gHUD.SetFOV( 0 );
			m_iFlags &= ~HUD_ACTIVE;
			return;
		}
	}

	//=====================================
	// zoom function for G36C
	//=====================================
	if( WeaponID == WEAPON_G36C )
	{
		if( ZoomMode == 1 ) // zooming in
		{
			FOV -= 175 * g_fFrametime;
			gHUD.m_flFOV = (int)FOV;

			if( FOV <= 45 )
			{
				gHUD.IsZooming = false;
				gHUD.IsZoomed = true;
				gHUD.SetFOV( 45 );
				m_iFlags &= ~HUD_ACTIVE;
				return;
			}
		}
		else if( ZoomMode == 2 ) // zooming out (instant)
		{
			gHUD.IsZooming = false;
			gHUD.IsZoomed = false;
			gHUD.SetFOV( 0 );
			m_iFlags &= ~HUD_ACTIVE;
			return;
		}
	}
}