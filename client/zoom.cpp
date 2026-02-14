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
		gHUD.IsZooming = false;
	//	gHUD.m_flFOV = 0.0f;
	}
	
	return 1;
}

void CZoom::Think( void )
{
	float FOV = gHUD.m_flFOV;

	if( ZoomMode == 0 )
	{
		FOV = 0;
		gHUD.IsZooming = false;
		gHUD.IsZoomed = false;
		gHUD.SetFOV( 0 );
		m_iFlags &= ~HUD_ACTIVE;
		return;
	}
	
	//=====================================
	// zoom function for crossbow
	//=====================================
	if( WeaponID == WEAPON_CROSSBOW )
	{
		if( ZoomMode == 1 ) // zooming in
		{
			FOV = CL_UTIL_Approach( CROSSBOW_MAX_ZOOM, FOV, 200 * g_fFrametime );
			gHUD.m_flFOV = (int)FOV;

			if( FOV == CROSSBOW_MAX_ZOOM )
			{
				gHUD.IsZooming = false;
				gHUD.IsZoomed = true;
				gHUD.SetFOV( CROSSBOW_MAX_ZOOM );
				m_iFlags &= ~HUD_ACTIVE;
				return;
			}
		}
	}
	
	//=====================================
	// zoom function for sniper rifle
	//=====================================
	if( (WeaponID == WEAPON_SNIPER) || (WeaponID == WEAPON_GAUSS) )
	{
		if( ZoomMode == 1 ) // zooming in
		{
			FOV = CL_UTIL_Approach( SNIPER_MID_ZOOM, FOV, 200 * g_fFrametime );
			gHUD.m_flFOV = (int)FOV;

			if( FOV == SNIPER_MID_ZOOM )
			{
				gHUD.IsZooming = false;
				gHUD.IsZoomed = true;
				gHUD.SetFOV( SNIPER_MID_ZOOM );
				m_iFlags &= ~HUD_ACTIVE;
				return;
			}
		}
		else if( ZoomMode == 2 ) // zooming in to max
		{
			FOV = CL_UTIL_Approach( SNIPER_MAX_ZOOM, FOV, 200 * g_fFrametime );
			gHUD.m_flFOV = (int)FOV;

			if( FOV <= SNIPER_MAX_ZOOM )
			{
				gHUD.IsZooming = false;
				gHUD.IsZoomed = true;
				gHUD.SetFOV( SNIPER_MAX_ZOOM );
				m_iFlags &= ~HUD_ACTIVE;
				return;
			}
		}
	}

	//=====================================
	// zoom function for G36C
	//=====================================
	if( WeaponID == WEAPON_G36C )
	{
		if( ZoomMode == 1 ) // zooming in
		{
			FOV = CL_UTIL_Approach( G36C_MAX_ZOOM, FOV, 175 * g_fFrametime );
			gHUD.m_flFOV = (int)FOV;

			if( FOV == G36C_MAX_ZOOM )
			{
				gHUD.IsZooming = false;
				gHUD.IsZoomed = true;
				gHUD.SetFOV( G36C_MAX_ZOOM );
				m_iFlags &= ~HUD_ACTIVE;
				return;
			}
		}
	}
}