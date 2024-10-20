//=================================================================================================================
// diffusion
// this is instead of half-life crosshair system. now, for better or worse, each crosshair is a separate sprite.
// this code simply puts the sprite in the dead center of the screen
// the current crosshair to view is handled on server side, depending on which weapon is currently equipped
//=================================================================================================================

#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "weaponinfo.h"
#include "triangleapi.h"
#include "r_cvars.h"
#include "event_api.h"
#include "r_local.h"
#include "r_quakeparticle.h"

DECLARE_MESSAGE( m_CrosshairStatic, CrosshairStatic );
DECLARE_MESSAGE( m_CrosshairStatic, GaussHUD );

int CurCrosshair = 0;
int TempDamageDealt = 0;
char dmg[8];

int CHudCrosshairStatic::Init( void )
{
	HOOK_MESSAGE( CrosshairStatic );
	HOOK_MESSAGE( GaussHUD );
	gHUD.AddHudElem( this );
	SniperCrosshairImage = 0;
	CrossbowCrosshairImage = 0;
	AlienCrosshairImage = 0;
	Reset();
	return 1;
}

int CHudCrosshairStatic::VidInit( void )
{
	SniperCrosshairImage = LOAD_TEXTURE( "sprites/scope_sniper.dds", NULL, 0, 0 );
	CrossbowCrosshairImage = LOAD_TEXTURE( "sprites/scope_crossbow.dds", NULL, 0, 0 );
	AlienCrosshairImage = LOAD_TEXTURE( "sprites/scope_alien.dds", NULL, 0, 0 );
	G36CCrosshairImage = LOAD_TEXTURE( "sprites/scope_g36c.dds", NULL, 0, 0 );
	ZoomBlur = 0.0f;
	CurCrosshair = cl_crosshair->value;
	DamageDealt = 0;

	// gauss-specific
	int Gframe = gHUD.GetSpriteIndex( "hud_gausniper_frame" );
	int Gcharge = gHUD.GetSpriteIndex( "hud_gausniper_charge" );
	int Gammo = gHUD.GetSpriteIndex( "hud_gausniper_ammo" );
	m_hGFrame = gHUD.GetSprite( Gframe );
	m_hGCharge = gHUD.GetSprite( Gcharge );
	m_hGAmmo = gHUD.GetSprite( Gammo );
	m_prc_Gframe = &gHUD.GetSpriteRect( Gframe );
	m_prc_Gcharge = &gHUD.GetSpriteRect( Gcharge );
	m_prc_Gammo = &gHUD.GetSpriteRect( Gammo );
	return 1;
}

void CHudCrosshairStatic::Reset( void )
{
	m_iFlags &= ~HUD_ACTIVE;
}

int CHudCrosshairStatic::MsgFunc_CrosshairStatic( const char *pszName, int iSize, void *pbuf )
{
	int ShouldEnable;

	BEGIN_READ( pszName, pbuf, iSize );

	ShouldEnable = READ_BYTE();

	if( ShouldEnable > 0 )
	{
		WeaponID = READ_BYTE();
		ConfirmedHit = READ_BYTE();
		DamageDealt = READ_SHORT();
		DroneControl = (READ_BYTE() > 0);
		LoadCrosshairForWeapon( WeaponID );
		m_iFlags |= HUD_ACTIVE;
		if( ConfirmedHit > 0 )
		{
			EnableHitMarker = true;
			HMTransparency = 255;
			// play hitmarker sound, if set
			if( CVAR_TO_BOOL(cl_hitsound) )
			{
				switch( ConfirmedHit )
				{
				case 1: gEngfuncs.pEventAPI->EV_PlaySound( 0, Vector(0,0,0), CHAN_STATIC, "misc/hitmarker.wav", 1.0, 0, 0, 100 ); break;
				case 2: gEngfuncs.pEventAPI->EV_PlaySound( 0, Vector(0,0,0), CHAN_STATIC, "misc/hitmarker_kill.wav", 1.0, 0, 0, 100 ); break;
				}
			}
		}
	}
	else
		m_iFlags &= ~HUD_ACTIVE;

	END_READ();

	return 1;
}

int CHudCrosshairStatic::MsgFunc_GaussHUD( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	GaussAmmo = READ_BYTE();
	GaussCharge = READ_BYTE();
	END_READ();

	return 1;
}

int CHudCrosshairStatic::Draw( float flTime )
{
	if( !CVAR_TO_BOOL( cl_crosshair ) )
		return 1;

	if( CurCrosshair != cl_crosshair->value )
	{
		LoadCrosshairForWeapon( WeaponID );
		CurCrosshair = cl_crosshair->value;
	}

	int r, g, b;

	// hitmarker is allowed at all times
	if( EnableHitMarker )
	{
		x = (ScreenWidth / 2) - (m_HM.rc.right / 2);
		y = (ScreenHeight / 2) - (m_HM.rc.bottom / 2);

		switch( ConfirmedHit )
		{
		case 1:
			UnpackRGB( r, g, b, 0x00FFFFFF );
			HMTransparency -= 666 * g_fFrametime;
			break;
		case 2:
			UnpackRGB( r, g, b, 0x00FF0000 );
			HMTransparency -= 250 * g_fFrametime;
			break;
		default:
			// FIXME weapon changed while hitmarker was still visible, causing it to update (changing color and not being removed from screen)
			HMTransparency = 0;
			break;
		}

		ScaleColors( r, g, b, HMTransparency );
		SPR_Set( m_HM.spr, r, g, b );
		SPR_DrawAdditive( 0, x, y, &m_HM.rc );
		if( HMTransparency <= 0 )
			EnableHitMarker = false;

		// also draw the amount of damage
		if( DamageDealt > 0 && cl_showdamage->value > 0 )
		{
			static int width = 0;
			if( DamageDealt != TempDamageDealt )
			{
				_snprintf_s( dmg, sizeof( dmg ), "%d", DamageDealt );
				// calculate width to align center...
				const char *buf;
				width = 0;
				buf = dmg;
				while( *buf )
				{
					width += gHUD.m_scrinfo.charWidths[*buf];
					buf++;
				}

				TempDamageDealt = DamageDealt; // cache to avoid doing text trickery every frame
			}

			DrawString( (int)((ScreenWidth - width) * 0.5f), y - 10, dmg, r, g, b );
		}
	}
	
	if(( gHUD.m_iHideHUDDisplay & ( HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_HEALTH | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM)))
		return 1;
	
	if( gHUD.IsZoomed ) // draw sniper scope
	{
		float YawSpeed = (PrevViewAngles - tr.viewparams.viewangles).Length() / g_fFrametime;
		float Velocity = gEngfuncs.GetLocalPlayer()->curstate.velocity.Length();
		if( gHUD.IsZooming )
		//	ZoomBlur += 5 * g_fFrametime;
			ZoomBlur = 0.1f;
		else
		{
			// we are standing still but moving mouse
			if( Velocity < 50 )
			{
				if( YawSpeed > 20 )
					ZoomBlur += 0.6 * g_fFrametime;
				else
					ZoomBlur -= g_fFrametime;
			}
			else
				ZoomBlur = Velocity * 0.0005;
		}
		ZoomBlur = bound( 0.0f, ZoomBlur, 0.1f );
		
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		gEngfuncs.pTriAPI->Brightness( 1.0f );
		gEngfuncs.pTriAPI->Color4f( 1, 1, 1, 1.0f );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE );

		switch( WeaponID )
		{
		case WEAPON_CROSSBOW:
			GL_Bind( GL_TEXTURE0, CrossbowCrosshairImage );
			break;
		case WEAPON_SNIPER:
			GL_Bind( GL_TEXTURE0, SniperCrosshairImage );
			break;
		case WEAPON_GAUSS:
			GL_Bind( GL_TEXTURE0, AlienCrosshairImage );
			break;
		case WEAPON_G36C:
			GL_Bind( GL_TEXTURE0, G36CCrosshairImage );
			break;
		}

		// draw a square image with the scope crosshair
		int x1 = (ScreenWidth / 2) - (ScreenHeight / 2);
		int x2 = (ScreenWidth / 2) + (ScreenHeight / 2);

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( x1, 0, x2, ScreenHeight );
		gEngfuncs.pTriAPI->End();

		// draw blackness to the left
		gEngfuncs.pfnFillRGBABlend( 0, 0, x1, ScreenHeight, 0, 0, 0, 255 );

		// draw blackness to the right
		gEngfuncs.pfnFillRGBABlend( x2, 0, x1, ScreenHeight, 0, 0, 0, 255 );
	}
	else // draw normal crosshairs
	{
		if( tr.time == tr.oldtime ) // paused
			return 1;

		if( CVAR_TO_BOOL( ui_is_active ) )
			return 0;

		ZoomBlur = 0.0f; // reset blur

		x = (ScreenWidth / 2) - (m_CrosshairStatic.rc.right / 2);
		y = (ScreenHeight / 2) - (m_CrosshairStatic.rc.bottom / 2);
		UnpackRGB( r, g, b, 0x0046A9FF );
		ScaleColors( r, g, b, 150 );

		SPR_Set( m_CrosshairStatic.spr, r, g, b );
		SPR_DrawAdditive( 0, x, y, &m_CrosshairStatic.rc );
	}

	if( WeaponID == WEAPON_GAUSS )
		DrawGaussZoomedHUD();

	return 1;
}

void CHudCrosshairStatic::LoadCrosshairForWeapon( int WeaponID )
{
	char* pszCrosshair;

	if( cl_crosshair->value == 2 ) // simple
	{
		switch( WeaponID )
		{
		case WEAPON_DRONE:
		{
			if( DroneControl )
				pszCrosshair = (char *)("crossy_drone");
			else
				pszCrosshair = (char *)("crossy_empty");
		} break;

		case WEAPON_SNIPER:
		case WEAPON_HANDGRENADE:
		case WEAPON_SATCHEL:
		case WEAPON_SENTRY:
		case WEAPON_GAUSS:
			pszCrosshair = (char *)("crossy_empty");
			break; // no crosshair for those weapons, but we need to set an empty one to display a hitmarker

		default: pszCrosshair = (char *)("crossy_simple"); break; // default crosshair
		}
	}
	else
	{
		switch( WeaponID )
		{
		case WEAPON_DEAGLE:		pszCrosshair = (char *)("crossy_deagle");	break;
		case WEAPON_RPG:		pszCrosshair = (char *)("crossy_rpg");		break;
		case WEAPON_TRIPMINE:	pszCrosshair = (char *)("crossy_nade");		break;
		case WEAPON_KNIFE:	pszCrosshair = (char *)("crossy_nade");		break;
		case WEAPON_CROSSBOW:	pszCrosshair = (char *)("crossy_xbow");		break;
		case WEAPON_SHOTGUN_XM:
		case WEAPON_SHOTGUN:	pszCrosshair = (char *)("crossy_sgun");		break;
		case WEAPON_AR2:		pszCrosshair = (char *)("crossy_ar2");		break;
		case WEAPON_DRONE:
		{
			if( DroneControl )
				pszCrosshair = (char *)("crossy_drone");
			else
				pszCrosshair = (char *)("crossy_empty");
		} break;

		case WEAPON_SNIPER:
		case WEAPON_HANDGRENADE:
		case WEAPON_SATCHEL:
		case WEAPON_SENTRY:
		case WEAPON_GAUSS:
			pszCrosshair = (char *)("crossy_empty");
			break; // no crosshair for those weapons, but we need to set an empty one to display a hitmarker

		default: pszCrosshair = (char *)("crossy"); break; // default crosshair
		}
	}
	
	// the sprite must be listed in hud.txt
	CrosshairSprite = gHUD.GetSpriteIndex(pszCrosshair);
	m_CrosshairStatic.spr = gHUD.GetSprite( CrosshairSprite );
	m_CrosshairStatic.rc = gHUD.GetSpriteRect( CrosshairSprite );
	Q_strcpy( m_CrosshairStatic.szSpriteName, pszCrosshair);

	// hitmarker
	if( cl_crosshair->value == 2 )
		HMSprite = gHUD.GetSpriteIndex( "crossyhit_simple" );
	else
		HMSprite = gHUD.GetSpriteIndex( "crossyhit" );
	m_HM.spr = gHUD.GetSprite( HMSprite );
	m_HM.rc = gHUD.GetSpriteRect( HMSprite );
	Q_strcpy( m_HM.szSpriteName, "crossyhit" );
}

//===========================================================================
// gauss-specific
// I also make particles and weapon effects here. So if HUD is off, they
// probably won't work.
//===========================================================================
void CHudCrosshairStatic::DrawGaussZoomedHUD(void)
{
	// change weapon skin while charging
	int skin = bound( 0, 9 - GaussCharge, 9 );
	GET_VIEWMODEL()->curstate.skin = skin;

	static int iPlaySound = 0;
	if( GaussCharge == 0 )
		iPlaySound = 1;
	else if( GaussCharge == 9 && iPlaySound == 2 )
		iPlaySound = 0;

	if( iPlaySound == 1 )
	{
		gEngfuncs.pEventAPI->EV_PlaySound( 0, Vector( 0, 0, 0 ), CHAN_STATIC, "weapons/gauss_charge.wav", 1.0, 0, 0, 100 );
		iPlaySound = 2;
	}

	if( !gHUD.IsZoomed )
		return; // this HUD is visible only when zoomed

	int r, g, b, x, y = 0;

	// main color
	UnpackRGB( r, g, b, 0x0046A9FF ); // 70,169,255 for Diffusion

	// draw the frame
	x = (ScreenWidth / 2) - (m_prc_Gframe->right / 2);
	y = ScreenHeight - 120;
	SPR_Set( m_hGFrame, r, g, b );
	SPR_DrawAdditive( 0, x, y, m_prc_Gframe );

	// draw the ammo
	x = (ScreenWidth / 2) - (m_prc_Gammo->right / 2);
	y = ScreenHeight - 103;
	wrect_t rcammo = *m_prc_Gammo;
	int offss = 100 - GaussAmmo * 10;
	rcammo.top += offss;
	SPR_Set( m_hGAmmo, r, g, b );
	SPR_DrawAdditive( 0, x, y + offss, &rcammo );

	// draw the charge
	x = (ScreenWidth / 2) - (m_prc_Gcharge->right / 2);
	y = ScreenHeight - 80;
	
	wrect_t rc = *m_prc_Gcharge;
	int offs = 200 - GaussCharge * 20; // GaussCharge is locked at 9 max
	rc.right -= offs;
	rc.left += offs;
	SPR_Set( m_hGCharge, r, g, b );
	SPR_DrawAdditive( 0, x + offs, y, &rc );
}