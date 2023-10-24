#include "hud.h"
#include "utils.h"
#include "r_local.h"
#include "r_cvars.h"
#include "r_world.h"
#include "pm_movevars.h"
#include "event_api.h"
#include "triangleapi.h"
#include "pm_defs.h"

int SunFlareTexture = 0;

int CHudLensflare::Init( void )
{
	m_iFlags |= HUD_ACTIVE;
	gHUD.AddHudElem( this );
	BlendMult = 0.0f;
	return 1;
}

int CHudLensflare::VidInit( void )
{
	SunFlareTexture = LOAD_TEXTURE( "sprites/lens/sunflare.dds", NULL, 0, 0 );
	tex[0] = SPR_Load( "sprites/lens/lens1.spr" );
	tex[1] = SPR_Load( "sprites/lens/lens2.spr" );
	tex[2] = SPR_Load( "sprites/lens/glow1.spr" );
	tex[3] = SPR_Load( "sprites/lens/glow2.spr" );
	tex[4] = SPR_Load( "sprites/lens/lens3.spr" );
	tex[5] = tex[1];
	tex[6] = tex[1];
	tex[7] = tex[2];
	tex[8] = tex[1];
	tex[9] = tex[0];
	BlendMult = 0.0f;
	return 1;
}

int CHudLensflare::Draw( float flTime )
{
	if( gl_lensflare->value <= 0 )
		return 1;

	if( !(world->features & WORLD_HAS_SKYBOX) )
		return 1; // don't waste time on tracing

	if( tr.time == tr.oldtime )
		return 1; // not in paused

	if( tr.shader_modifier != NULL )
	{
		// mapper disabled the lensflare on this map
		if( tr.shader_modifier->curstate.iuser2 == 1 )
			return 1;
	}

	if( CVAR_TO_BOOL( ui_is_active ) && !CVAR_GET_FLOAT( "cl_background" ) )
		return 1;

	Vector sunangles, sundir, suntarget;
	Vector v_forward, v_right, v_up, angles;
	Vector forward, right, up, screen;
	Vector v_angles, v_origin;
	pmtrace_t trace;
	float DotP;

	// Sun position
	VectorAngles( -tr.sky_normal, sunangles );

	v_angles = tr.viewparams.viewangles;
	v_origin = tr.viewparams.vieworg;
	AngleVectors( v_angles, forward, NULL, NULL );
	AngleVectors( sunangles, sundir, NULL, NULL );
	suntarget = v_origin + sundir * 135000; // maps can be this big :)

	DotP = DotProduct( forward, sundir );
	if( BlendMult <= 0.0f && DotP < 0.8 )
		return 1;

	//	gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
	//	gEngfuncs.pEventAPI->EV_PlayerTrace( v_origin, suntarget, PM_GLASS_IGNORE, -1, &trace );
	int maxLoops = 4;
	int ignoreent = -1;	// first, ignore no entity
	cl_entity_t *ent = NULL;
	Vector vecStart = v_origin;
	while( maxLoops > 0 )
	{
		gEngfuncs.pEventAPI->EV_SetTraceHull( 2 );
		gEngfuncs.pEventAPI->EV_PlayerTrace( vecStart, suntarget, PM_TRACELINE_PHYSENTSONLY, ignoreent, &trace );

		if( trace.ent <= 0 ) break; // we hit the world or nothing, stop trace

		ent = GET_ENTITY( PM_GetPhysEntInfo( trace.ent ) );
		if( ent == NULL ) break;

		if( ent->curstate.rendermode == kRenderNormal )
			break;

		if( ent->curstate.rendermode == kRenderTransTexture && ent->curstate.renderamt == 0 ) // invisible brush "collision", so it's likely a model
			break;

		if( ent->curstate.rendermode == kRenderTransColor && ent->curstate.renderamt == 255 )
			break;

		// if close enough to end pos, stop, otherwise continue trace
		if( (suntarget - trace.endpos).Length() < 1.0f )
			break;
		else
		{
			ignoreent = trace.ent;	// ignore last hit entity
			vecStart = trace.endpos;
		}
		maxLoops--;
	}

	if( (POINT_CONTENTS(trace.endpos) == CONTENTS_SKY) && DotP > 0.8 )
		BlendMult += 5 * g_fFrametime;
	else
		BlendMult -= g_fFrametime;

	BlendMult = bound( 0, BlendMult, 1 - gHUD.ScreenDrips_DripIntensity );

	if( BlendMult <= 0.0f )
		return 1;

	red[0] = green[0] = blue[0] = 1.0;
	scale[0] = 45;
	multi[0] = -0.45;

	red[1] = green[0] = blue[0] = 1.0;
	scale[1] = 25;
	multi[1] = 0.2;

	red[2] = 132 / 255;
	green[2] = 1.0;
	blue[2] = 153 / 255;
	scale[2] = 35;
	multi[2] = 0.3;

	red[3] = 1.0;
	green[3] = 164 / 255;
	blue[3] = 164 / 255;
	scale[3] = 40;
	multi[3] = 0.46;

	red[4] = 1.0;
	green[4] = 164 / 255;
	blue[4] = 164 / 255;
	scale[4] = 52;
	multi[4] = 0.5;

	red[5] = green[5] = blue[5] = 1.0;
	scale[5] = 31;
	multi[5] = 0.54;

	red[6] = 0.6;
	green[6] = 1.0;
	blue[6] = 0.6;
	scale[6] = 26;
	multi[6] = 0.64;

	red[7] = 0.5;
	green[7] = 1.0;
	blue[7] = 0.5;
	scale[7] = 20;
	multi[7] = 0.77;

	flPlayerBlend = 0.0;
	flPlayerBlend2 = 0.0;

	flPlayerBlend = max( DotP - 0.85, 0.0 ) * 6.8 * BlendMult;
	if( flPlayerBlend > 1.0 )
		flPlayerBlend = 1.0;

	flPlayerBlend4 = max( DotP - 0.90, 0.0 ) * 6.6 * BlendMult;
	if( flPlayerBlend4 > 1.0 )
		flPlayerBlend4 = 1.0;

	flPlayerBlend6 = max( DotP - 0.80, 0.0 ) * 6.7 * BlendMult;
	if( flPlayerBlend6 > 1.0 )
		flPlayerBlend6 = 1.0;

	flPlayerBlend2 = flPlayerBlend6 * 140.0 * BlendMult;
	flPlayerBlend3 = flPlayerBlend * 190.0 * BlendMult;
	flPlayerBlend5 = flPlayerBlend4 * 222.0 * BlendMult;

	Vector normal, point, origin;

	normal = tr.viewparams.viewangles;
	gEngfuncs.pfnAngleVectors( normal, forward, right, up );

	origin = trace.endpos;

	R_WorldToScreen( trace.endpos, screen );

	Suncoordx = XPROJECT( screen[0] );
	Suncoordy = YPROJECT( screen[1] );

	if( Suncoordx < XRES( -10 ) || Suncoordx > XRES( 650 ) || Suncoordy < YRES( -10 ) || Suncoordy > YRES( 490 ) )
		return 1;

	Screenmx = ScreenWidth / 2;
	Screenmy = ScreenHeight / 2;

	Sundistx = Screenmx - Suncoordx;
	Sundisty = Screenmy - Suncoordy;

	gEngfuncs.pTriAPI->RenderMode( kRenderTransAdd );
//	gEngfuncs.pTriAPI->CullFace( TRI_FRONT );

	if( SunFlareTexture > 0 )
	{
		int crd = 190;
		gEngfuncs.pTriAPI->Color4f( tr.movevars->skycolor_r / 255, tr.movevars->skycolor_g / 255, tr.movevars->skycolor_b / 255, flPlayerBlend2 / 355.0 );
		gEngfuncs.pTriAPI->Brightness( flPlayerBlend2 / 355.0 );
		GL_SelectTexture( 0 );
		GL_Bind( 0, SunFlareTexture );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS );
		DrawQuad( Suncoordx + crd, Suncoordy + crd, Suncoordx - crd, Suncoordy - crd );
		gEngfuncs.pTriAPI->End();
		GL_CleanUpTextureUnits( 0 );
	}

	if( tex[3] > 0 )
	{
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[3] ), 0 );
		gEngfuncs.pTriAPI->Color4f( 1.0, 1.0, 1.0, flPlayerBlend3 / 255.0 );
		gEngfuncs.pTriAPI->Brightness( flPlayerBlend3 / 255.0 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Suncoordx + 160, Suncoordy + 160, 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Suncoordx + 160, Suncoordy - 160, 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Suncoordx - 160, Suncoordy - 160, 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Suncoordx - 160, Suncoordy + 160, 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	gEngfuncs.pTriAPI->Brightness( flPlayerBlend2 / 255.0 );

	int i = 1;
	if( tex[i] > 0 )
	{
		Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
		Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Color4f( red[i], green[i], green[i], flPlayerBlend2 / 255.0 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] + scale[i], 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] - scale[i], 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] - scale[i], 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] + scale[i], 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	i++;
	if( tex[i] > 0 )
	{
		Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
		Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Color4f( red[i], green[i], green[i], flPlayerBlend2 / 255.0 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] + scale[i], 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] - scale[i], 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] - scale[i], 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] + scale[i], 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	i++;
	if( tex[i] > 0 )
	{
		Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
		Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Color4f( red[i], green[i], green[i], flPlayerBlend2 / 255.0 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] + scale[i], 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] - scale[i], 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] - scale[i], 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] + scale[i], 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	i++;
	if( tex[i] > 0 )
	{
		Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
		Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Color4f( red[i], green[i], green[i], flPlayerBlend2 / 255.0 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] + scale[i], 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] - scale[i], 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] - scale[i], 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] + scale[i], 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	i++;
	if( tex[i] > 0 )
	{
		Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
		Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Color4f( red[i], green[i], green[i], flPlayerBlend2 / 255.0 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] + scale[i], 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] - scale[i], 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] - scale[i], 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] + scale[i], 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	i++;
	{
		Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
		Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Color4f( red[i], green[i], green[i], flPlayerBlend2 / 255.0 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] + scale[i], 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] - scale[i], 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] - scale[i], 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] + scale[i], 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	i++;
	if( tex[i] > 0 )
	{
		Lensx[i] = (Suncoordx + (Sundistx * multi[i]));
		Lensy[i] = (Suncoordy + (Sundisty * multi[i]));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Color4f( red[i], green[i], green[i], flPlayerBlend2 / 255.0 );
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] + scale[i], 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] + scale[i], Lensy[i] - scale[i], 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] - scale[i], 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx[i] - scale[i], Lensy[i] + scale[i], 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	i++;
	int scale1 = 32;
	int Lensx1, Lensy1 = 0;

	gEngfuncs.pTriAPI->Color4f( 0.9, 0.9, 0.9, flPlayerBlend2 / 255.0 );

	if( tex[i] > 0 )
	{
		Lensx1 = (Suncoordx + (Sundistx * 0.88));
		Lensy1 = (Suncoordy + (Sundisty * 0.88));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx1 + scale1, Lensy1 + scale1, 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx1 + scale1, Lensy1 - scale1, 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx1 - scale1, Lensy1 - scale1, 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx1 - scale1, Lensy1 + scale1, 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	i++;
	if( tex[i] > 0 )
	{
		scale1 = 140;
		Lensx1 = (Suncoordx + (Sundistx * 1.1));
		Lensy1 = (Suncoordy + (Sundisty * 1.1));
		gEngfuncs.pTriAPI->SpriteTexture( (struct model_s *)gEngfuncs.GetSpritePointer( tex[i] ), 0 ); //hotglow, or any other sprite for the texture
		gEngfuncs.pTriAPI->Begin( TRI_QUADS ); //start our quad
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx1 + scale1, Lensy1 + scale1, 0 ); //top left
		gEngfuncs.pTriAPI->TexCoord2f( 0.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx1 + scale1, Lensy1 - scale1, 0 ); //bottom left
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 0.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx1 - scale1, Lensy1 - scale1, 0 ); //bottom right
		gEngfuncs.pTriAPI->TexCoord2f( 1.0f, 1.0f ); gEngfuncs.pTriAPI->Vertex3f( Lensx1 - scale1, Lensy1 + scale1, 0 ); //top right
		gEngfuncs.pTriAPI->End(); //end our list of vertexes
	}

	gEngfuncs.pTriAPI->RenderMode( kRenderNormal );

	return 1;
}