
// diffusion
// just a place for all custom on-screen effects

#include "hud.h"
#include "utils.h"
#include "triangleapi.h"
#include "r_local.h"

#include "pm_defs.h"
#include "r_efx.h"
#include "r_world.h"
#include "event_api.h"


#define SPEED_ARROW_STEP 2
#define SPEED_ARROW_FRAMES 121
#define SPEEDOMETER_Y_OFFSET 5

int CScreenEffects::Init( void )
{
	gHUD.AddHudElem( this );
	m_iFlags |= HUD_ACTIVE; // always active
	return 1;
}

int CScreenEffects::VidInit(void)
{
	Vignette = LOAD_TEXTURE( "sprites/ef_vignette.dds", NULL, 0, 0 );
	DroneScreen = LOAD_TEXTURE( "sprites/ef_dronescreen.dds", NULL, 0, 0 );
	Speedometer = LOAD_TEXTURE( "sprites/diffusion/speedometer.dds", NULL, 0, 0 );
	SpeedometerArrows = LoadSprite( "sprites/diffusion/speed_arrows.spr" );
	SpeedometerGears = LoadSprite( "sprites/diffusion/speed_gears.spr" );
	LastOrigin = g_vecZero;
	SaveIcon = LoadSprite( "sprites/diffusion/gamesaved.spr" );
	return 1;
}

int CScreenEffects::Draw( float flTime )
{
	DrawSpeedometer();
	DrawShieldVignette();
	DrawVignette();
	DrawCinematicBorder();
	DrawDroneScreen();
	DrawGameSaved();

	return 1;
}

//===============================================
// draw "game saved" icon
//===============================================
void CScreenEffects::DrawGameSaved(void)
{
	if( SaveIcon_Reset )
	{
		SaveIcon_Frame = 0;
		SaveIcon_Brightness = 0.0f;
		SaveIcon_Reset = false;
	}
	
	if( !ShouldDrawGameSaved )
		return;

	if( tr.time == tr.oldtime ) // not in paused
		return;

	const model_s *pSaveIcon = gEngfuncs.GetSpritePointer( SaveIcon );
	int total_frames = SPR_Frames( SaveIcon );
	if( SaveIcon_Frame >= total_frames - 1 )
	{
		SaveIcon_Frame = total_frames - 1;
		if( SaveIcon_Brightness > 0.0f )
			SaveIcon_Brightness -= 300 * g_fFrametime;

		if( SaveIcon_Brightness <= 0.0f )
		{
			ShouldDrawGameSaved = false;
			SaveIcon_Frame = 0;
			SaveIcon_Brightness = 0.0f;
			return;
		}
	}
	else
	{
		if( SaveIcon_Brightness < 255.0f )
			SaveIcon_Brightness += 350 * g_fFrametime;

		SaveIcon_Frame += 100 * g_fFrametime;
	}

	if( !gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)pSaveIcon, (int)SaveIcon_Frame ) )
		return;

	int SQUARE_SIZE = 150;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, SaveIcon_Brightness / 255.0f );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( (ScreenWidth * 0.5f) - (SQUARE_SIZE/2), ScreenHeight - 75 - SQUARE_SIZE, (ScreenWidth * 0.5f) + (SQUARE_SIZE/2), ScreenHeight - 75 );
	gEngfuncs.pTriAPI->End();
}

//===============================================
// draw speedometer when in car or boat
//===============================================
void CScreenEffects::DrawSpeedometer(void)
{	
	if( !gHUD.InCar )
	{
		gHUD.CarSpeed = 0.0f;
		if( SpeedArrowFrame > 0 )
			SpeedArrowFrame = 0;
		return;
	}

	if( tr.time == tr.oldtime ) // not in paused
		return;
	
	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );
	GL_SelectTexture( 0 );
	GL_Bind( 0, Speedometer );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( ScreenWidth - 320, ScreenHeight - 320, ScreenWidth - 20, ScreenHeight - 20 );
	gEngfuncs.pTriAPI->End();

	// kilometers
	float Distance = (gEngfuncs.GetLocalPlayer()->prevstate.origin - gEngfuncs.GetLocalPlayer()->curstate.origin).Length() * 0.01905f * 0.001f;
	float timedelta = g_fFrametime;
	if( GET_MAX_CLIENTS() > 1 )
		timedelta = gEngfuncs.GetLocalPlayer()->curstate.msg_time - gEngfuncs.GetLocalPlayer()->prevstate.msg_time;
	
	gHUD.CarSpeed = Distance / (timedelta / 3600.0f);
//	gEngfuncs.Con_NPrintf( 1,"%i\n", (int)kph );

	const model_s *pArrows = gEngfuncs.GetSpritePointer( SpeedometerArrows );
	// choose arrow frame, each step equals 2 kph
	SpeedArrowFrame = CL_UTIL_Approach( gHUD.CarSpeed / SPEED_ARROW_STEP, SpeedArrowFrame, 25 * g_fFrametime );
	SpeedArrowFrame = bound( 0, SpeedArrowFrame, SPEED_ARROW_FRAMES - 1  );
	if( !gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)pArrows, (int)SpeedArrowFrame ) )
		return;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( ScreenWidth - 320, ScreenHeight - 320 - SPEEDOMETER_Y_OFFSET, ScreenWidth - 20, ScreenHeight - 20 - SPEEDOMETER_Y_OFFSET );
	gEngfuncs.pTriAPI->End();

	// draw gears
	const model_s *pGears = gEngfuncs.GetSpritePointer( SpeedometerGears );
	// 9 frames in total (EMPTY-1-2-3-4-5-6-7-R)
	int GearFrame = Gear;
	GearFrame = bound( 0, GearFrame, 9 );
	if( !gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)pGears, GearFrame ) )
		return;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( ScreenWidth - 320, ScreenHeight - 320 - SPEEDOMETER_Y_OFFSET, ScreenWidth - 20, ScreenHeight - 20 - SPEEDOMETER_Y_OFFSET );
	gEngfuncs.pTriAPI->End();
}

//===============================================
// draw cinematic border (spectator mode and trigger_camera using this)
//===============================================
void CScreenEffects::DrawCinematicBorder( void )
{
//	if( tr.time == tr.oldtime ) // paused
//		return;

	int BorderTransparency = 0;

	if( g_iUser1 )
		BorderTransparency = 125;

	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERUSINGCAMERA )
		BorderTransparency = 200;

	if( BorderTransparency > 0 )
	{
		gEngfuncs.pfnFillRGBABlend( 0, ScreenHeight - (ScreenHeight / 10), ScreenWidth, ScreenHeight / 10, 0, 0, 0, BorderTransparency ); // bottom border
		gEngfuncs.pfnFillRGBABlend( 0, 0, ScreenWidth, ScreenHeight / 10, 0, 0, 0, BorderTransparency ); // top border
	}
}

//===============================================
// draw a vignette when crouched or paused
//===============================================
void CScreenEffects::DrawVignette( void )
{
	if( (g_bDucked && tr.viewparams.onground) || (tr.time == tr.oldtime) )
	{
		if( VignetteAlpha < 1 )
			VignetteAlpha += 2 * g_fFrametime;
	}
	else
	{
		if( VignetteAlpha > 0 )
			VignetteAlpha -= 2 * g_fFrametime;
	}

	VignetteAlpha = bound( 0, VignetteAlpha, 1 );

	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERDRONECONTROL )
		return;

	if( VignetteAlpha > 0 )
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, VignetteAlpha );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE );

		GL_SelectTexture( 0 );

		GL_Bind( 0, Vignette );

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( 0, 0, ScreenWidth, ScreenHeight );
		gEngfuncs.pTriAPI->End();
	}
}

//===============================================
// draw the drone screen texture
//===============================================
void CScreenEffects::DrawDroneScreen( void )
{
	if( !(gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERDRONECONTROL) )
		return;

	if( gEngfuncs.GetLocalPlayer()->curstate.effects & EF_PLAYERUSINGCAMERA )
		return;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
	gEngfuncs.pTriAPI->Color4f( 1.0f, 1.0f, 1.0f, 1.0f );

	GL_SelectTexture( 0 );

	GL_Bind( 0, DroneScreen );

	gEngfuncs.pTriAPI->Begin( TRI_QUADS );
	DrawQuad( 0, 0, ScreenWidth, ScreenHeight );
	gEngfuncs.pTriAPI->End();
}

void CScreenEffects::DrawShieldVignette(void)
{
	if( !gHUD.ShieldOn )
	{
		if( ShieldAlpha == 0.0f )
			return;
		
		if( ShieldAlpha > 0 )
		{
			ShieldAlpha -= 3 * g_fFrametime;
			if( ShieldAlpha <= 0 )
				ShieldAlpha = 0;
		}
	}
	else
	{
		if( ShieldAlpha < 1 )
			ShieldAlpha += 3 * g_fFrametime;
	}

	ShieldAlpha = bound( 0, ShieldAlpha, 1 );

	if( ShieldAlpha > 0 )
	{
		gEngfuncs.pTriAPI->RenderMode( kRenderTransAlpha );
		gEngfuncs.pTriAPI->Color4f( 70 / 255.0f, 169 / 255.0f, 1.0f, ShieldAlpha );
		gEngfuncs.pTriAPI->CullFace( TRI_NONE );

		GL_SelectTexture( 0 );

		GL_Bind( 0, LOAD_TEXTURE( "sprites/ef_vignette_shield.dds", NULL, 0, 0 ) );

		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( 0, 0, ScreenWidth, ScreenHeight );
		gEngfuncs.pTriAPI->End();
	}
}