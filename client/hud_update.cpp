/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   Use, distribution, and modification of this source code and/or resulting
*   object code is restricted to non-commercial enhancements to products from
*   Valve LLC.  All other use, distribution, or modification is prohibited
*   without written permission from Valve LLC.
*
****/

#include "hud.h"
#include "utils.h"

int CHud::UpdateClientData(client_data_t *cdata, float time)
{
	m_vecOrigin = cdata->origin;
	m_vecAngles = cdata->viewangles;
	m_iKeyBits = CL_ButtonBits( 0 );
//	m_iWeaponBits = cdata->iWeaponBits;

	Think();

	static float AddFov = 0.0f;
	if( gHUD.CarAddFovMult < 1.0f )
		gHUD.CarAddFovMult = CL_UTIL_Approach( 1.0f, gHUD.CarAddFovMult, g_fFrametime ); // this jerks fov when switching gears
	if( gHUD.InCar )
	{
		AddFov = CL_UTIL_Approach( gHUD.CarSpeed * 0.25f * gHUD.CarAddFovMult, AddFov, (10 / gHUD.CarAddFovMult) * g_fFrametime ); // just for smoothing
		AddFov = bound( 0, AddFov, 30 );
		cdata->fov = (m_flFOV * 0.75f) + AddFov;
	}
	else
	{
		AddFov = 0.0f;
		cdata->fov = m_flFOV;
	}

	v_idlescale = m_iConcussionEffect;
	CL_ResetButtonBits( m_iKeyBits );

	// return 1 if in anything in the client_data struct has been changed, 0 otherwise
	return 1;
}