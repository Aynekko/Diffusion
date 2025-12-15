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

// global scale for all images, unrelated to HUD scaling
// (it's easier than to rescale the images themselves manually)
#define CROSSHAIR_SCALE 0.25f

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

void CHudCrosshairStatic::VidInitCrosshairs( void )
{
	// load default crosshairs and hitmarkers
	crosshair_default[0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy.dds", NULL, 0, 0 );
	if( crosshair_default[0].texture )
	{
		crosshair_default[0].w = RENDER_GET_PARM( PARM_TEX_WIDTH, crosshair_default[0].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
		crosshair_default[0].h = RENDER_GET_PARM( PARM_TEX_HEIGHT, crosshair_default[0].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
	}

	crosshair_default[1].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_simple.dds", NULL, 0, 0 );
	if( crosshair_default[1].texture )
	{
		crosshair_default[1].w = RENDER_GET_PARM( PARM_TEX_WIDTH, crosshair_default[0].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
		crosshair_default[1].h = RENDER_GET_PARM( PARM_TEX_HEIGHT, crosshair_default[0].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
	}

	hitmarker[0].texture = LOAD_TEXTURE( "textures/!crosshair/hitmarker.dds", NULL, 0, 0 );
	if( hitmarker[0].texture )
	{
		hitmarker[0].w = RENDER_GET_PARM( PARM_TEX_WIDTH, hitmarker[0].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
		hitmarker[0].h = RENDER_GET_PARM( PARM_TEX_HEIGHT, hitmarker[0].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
	}

	hitmarker[1].texture = LOAD_TEXTURE( "textures/!crosshair/hitmarker_simple.dds", NULL, 0, 0 );
	if( hitmarker[1].texture )
	{
		hitmarker[1].w = RENDER_GET_PARM( PARM_TEX_WIDTH, hitmarker[1].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
		hitmarker[1].h = RENDER_GET_PARM( PARM_TEX_HEIGHT, hitmarker[1].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
	}

	// load special crosshairs for weapons, if any
	// regular
	int i;
	for( i = 0; i < TOTAL_WEAPONS; i++ )
	{
		crosshair[i][0].texture = 0;
		switch( i )
		{
			case WEAPON_DEAGLE:		crosshair[i][0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_deagle.dds", NULL, 0, 0 ); break;
			case WEAPON_RPG:		crosshair[i][0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_rpg.dds", NULL, 0, 0 ); break;
			case WEAPON_TRIPMINE:
			case WEAPON_KNIFE:		crosshair[i][0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_nade.dds", NULL, 0, 0 ); break;
			case WEAPON_CROSSBOW:	crosshair[i][0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_xbow.dds", NULL, 0, 0 ); break;
			case WEAPON_SHOTGUN_XM:
			case WEAPON_SHOTGUN:	crosshair[i][0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_shotgun.dds", NULL, 0, 0 ); break;
			case WEAPON_AR2:		crosshair[i][0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_ar2.dds", NULL, 0, 0 ); break;
			case WEAPON_DRONE:		crosshair[i][0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_drone.dds", NULL, 0, 0 ); break;

			case WEAPON_SNIPER:
			case WEAPON_HANDGRENADE:
			case WEAPON_SMOKEGRENADE:
			case WEAPON_SATCHEL:
			case WEAPON_SENTRY:
			case WEAPON_GAUSS:
				// no crosshair for those weapons
				break;

			default: crosshair[i][0].texture = LOAD_TEXTURE( "textures/!crosshair/crossy.dds", NULL, 0, 0 ); break; // default crosshair
		}

		if( crosshair[i][0].texture )
		{
			crosshair[i][0].w = RENDER_GET_PARM( PARM_TEX_WIDTH, crosshair[i][0].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
			crosshair[i][0].h = RENDER_GET_PARM( PARM_TEX_HEIGHT, crosshair[i][0].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
		}
	}

	// simple
	for( i = 0; i < TOTAL_WEAPONS; i++ )
	{
		crosshair[i][1].texture = 0;
		switch( i )
		{
		case WEAPON_DRONE:		crosshair[i][1].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_drone.dds", NULL, 0, 0 ); break;

		case WEAPON_SNIPER:
		case WEAPON_HANDGRENADE:
		case WEAPON_SMOKEGRENADE:
		case WEAPON_SATCHEL:
		case WEAPON_SENTRY:
		case WEAPON_GAUSS:
			// no crosshair for those weapons
			break;

		default: crosshair[i][1].texture = LOAD_TEXTURE( "textures/!crosshair/crossy_simple.dds", NULL, 0, 0 ); break; // default crosshair
		}

		if( crosshair[i][1].texture )
		{
			crosshair[i][1].w = RENDER_GET_PARM( PARM_TEX_WIDTH, crosshair[i][1].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
			crosshair[i][1].h = RENDER_GET_PARM( PARM_TEX_HEIGHT, crosshair[i][1].texture ) * CROSSHAIR_SCALE * gHUD.fScale;
		}
	}
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

	// load crosshair images
	VidInitCrosshairs();

	// gauss-specific
	const int Gframe = gHUD.GetSpriteIndex( "hud_gausniper_frame" );
	const int Gcharge = gHUD.GetSpriteIndex( "hud_gausniper_charge" );
	const int Gammo = gHUD.GetSpriteIndex( "hud_gausniper_ammo" );
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
	BEGIN_READ( pszName, pbuf, iSize );

	const int ShouldEnable = READ_BYTE();

	if( ShouldEnable > 0 )
	{
		WeaponID = READ_BYTE();
		ConfirmedHit = READ_BYTE();
		DamageDealt = READ_SHORT();
		DroneControl = (READ_BYTE() > 0);
		m_iFlags |= HUD_ACTIVE;
		if( ConfirmedHit > 0 )
		{
			if( cl_hitmarker->value > 0 )
			{
				EnableHitMarker = true;
				HMTransparency = 255;
			}
			// play hitmarker sound, if set
			if( cl_hitmarker->value > 1 )
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

	pglEnable( GL_MULTISAMPLE_ARB );
	DrawCrosshairs( flTime );
	pglDisable( GL_MULTISAMPLE_ARB );

	return 1;
}

int CHudCrosshairStatic::DrawCrosshairs( float flTime )
{
	crosshair_t cur_hitmarker = cl_crosshair->value == 2 ? hitmarker[1] : hitmarker[0];
	crosshair_t cur_crosshair = cl_crosshair->value == 2 ? crosshair[WeaponID][1] : crosshair[WeaponID][0];
	if( WeaponID == WEAPON_DRONE && !DroneControl )
		cur_crosshair.texture = 0;

	float r, g, b, a;

	// hitmarker is allowed at all times
	if( EnableHitMarker )
	{
		x = (ScreenWidth - cur_hitmarker.w) * 0.5f;
		y = (ScreenHeight - cur_hitmarker.h) * 0.5f;

		switch( ConfirmedHit )
		{
		case 1:
			r = 1.0f; g = 1.0f; b = 1.0f;
			HMTransparency -= 666 * g_fFrametime;
			break;
		case 2:
			r = 1.0f; g = 0.1f; b = 0.1f;
			HMTransparency -= 250 * g_fFrametime;
			break;
		default:
			// FIXME weapon changed while hitmarker was still visible, causing it to update (changing color and not being removed from screen)
			HMTransparency = 0;
			break;
		}

		if( HMTransparency > 0.0f )
		{
			a = HMTransparency / 255.0f;
			gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
			gEngfuncs.pTriAPI->Color4f( r, g, b, a );

			if( cur_hitmarker.texture )
			{
				GL_Bind( GL_TEXTURE0, cur_hitmarker.texture );
				gEngfuncs.pTriAPI->Begin( TRI_QUADS );
				DrawQuad( x, y, x + cur_hitmarker.w, y + cur_hitmarker.h );
				gEngfuncs.pTriAPI->End();
			}

			// also draw the amount of damage
			if( DamageDealt > 0 && cl_showdamage->value > 0 )
			{
				static int width = 0;
				if( DamageDealt != TempDamageDealt )
				{
					Q_snprintf( dmg, sizeof( dmg ), "%d", DamageDealt );
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

				int R = r * 255;
				int G = g * 255;
				int B = b * 255;
				ScaleColors( R, G, B, HMTransparency );
				DrawString( (int)((ScreenWidth - width) * 0.5f), y - 5 * gHUD.fScale, dmg, R, G, B );
			}
		}
		else
			EnableHitMarker = false;
	}

	if( (gHUD.m_iHideHUDDisplay & (HIDEHUD_WPNS | HIDEHUD_ALL | HIDEHUD_HEALTH | HIDEHUD_WPNS_HOLDABLEITEM | HIDEHUD_WPNS_CUSTOM)) )
		return 1;

	if( gHUD.IsZoomed ) // draw sniper scope
	{
		const float YawSpeed = (PrevViewAngles - tr.viewparams.viewangles).Length() / g_fFrametime;
		const float Velocity = gEngfuncs.GetLocalPlayer()->curstate.velocity.Length();
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

		x = (ScreenWidth - cur_crosshair.w) * 0.5f;
		y = (ScreenHeight - cur_crosshair.h) * 0.5f;
		r = 0.275f;// 70 / 255;
		g = 0.663f;// 169 / 255;
		b = 1.0f;
		a = 0.6f; // 150 / 255

		gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
		gEngfuncs.pTriAPI->Color4f( r, g, b, a );

		if( cur_crosshair.texture )
		{
			GL_Bind( GL_TEXTURE0, cur_crosshair.texture );
			gEngfuncs.pTriAPI->Begin( TRI_QUADS );
			DrawQuad( x, y, x + cur_crosshair.w, y + cur_crosshair.h );
			gEngfuncs.pTriAPI->End();
		}
	}

	if( WeaponID == WEAPON_GAUSS )
		DrawGaussZoomedHUD();

	return 1;
}

//===========================================================================
// gauss-specific
// I also make particles and weapon effects here. So if HUD is off, they
// probably won't work.
//===========================================================================
void CHudCrosshairStatic::DrawGaussZoomedHUD(void)
{
	// change weapon skin while charging
	const int skin = bound( 0, 9 - GaussCharge, 9 );
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
	const int offs = 200 - GaussCharge * 20; // GaussCharge is locked at 9 max
	rc.right -= offs;
	rc.left += offs;
	SPR_Set( m_hGCharge, r, g, b );
	SPR_DrawAdditive( 0, x + offs, y, &rc );
}
