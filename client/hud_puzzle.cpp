#include "hud.h"
#include "utils.h"
#include "parsemsg.h"
#include "r_local.h"

DECLARE_MESSAGE( m_Puzzle, Puzzle )

//#define PUZZLE_DEBUG_MSG

float current_bar_width = 0;
float background_alpha = 0.5f;
Vector2D current_active_block_coord{ 0, 0 };
float next_move_time = 0;
CAnimatex checkmark;
short entidx = 0;
byte sign = 0;

int CHudPuzzle::Init( void )
{
	HOOK_MESSAGE( Puzzle );
	gHUD.AddHudElem( this );
	return 1;
};

int CHudPuzzle::VidInit( void )
{
	if( m_iFlags & HUD_ACTIVE )
		m_iFlags &= ~HUD_ACTIVE;

	if( !checkmark.Initialized() )
		checkmark.Init( "sprites/diffusion/checkmark/frame" );

	return 1;
};

void CHudPuzzle::MoveActiveBlock( int button )
{
	switch( button )
	{
	case IN_MOVELEFT:
		if( active_block_id.y > 0 )
			active_block_id.y--;
		break;
	case IN_MOVERIGHT:
		if( active_block_id.y < field_size - 1 )
			active_block_id.y++;
		break;
	case IN_FORWARD:
		if( active_block_id.x > 0 )
			active_block_id.x--;
		break;
	case IN_BACK:
		if( active_block_id.x < field_size - 1 )
			active_block_id.x++;
		break;
	}
}

void CHudPuzzle::MoveCorrectBlock( int direction )
{
	switch( direction )
	{
	case 0:
		if( correct_block_id.y > 0 )
			correct_block_id.y--;
		break;
	case 1:
		if( correct_block_id.y < field_size - 1 )
			correct_block_id.y++;
		break;
	case 2:
		if( correct_block_id.x > 0 )
			correct_block_id.x--;
		break;
	case 3:
		if( correct_block_id.x < field_size - 1 )
			correct_block_id.x++;
		break;
	}

	// don't solve itself!
	if( correct_block_id.x == active_block_id.x && correct_block_id.y == active_block_id.y )
		MoveCorrectBlock( RANDOM_LONG( 0, 3 ) );

}

void CHudPuzzle::Start( void )
{
	current_bar_width = 0;
	move_time = bound( 0.5f, move_time, 10.0f );
	next_move_time = tr.time + move_time;
	current_active_block_coord.x = 0;
	current_active_block_coord.y = 0;
	solved = false;
	background_alpha = 0.5f;

	// make a good shuffle...
	active_block_id.x = RANDOM_LONG( 0, field_size - 1 );
	active_block_id.x = RANDOM_LONG( 0, field_size - 1 );
	active_block_id.x = RANDOM_LONG( 0, field_size - 1 );
	active_block_id.y = RANDOM_LONG( 0, field_size - 1 );
	active_block_id.y = RANDOM_LONG( 0, field_size - 1 );
	active_block_id.y = RANDOM_LONG( 0, field_size - 1 );
	correct_block_id.x = RANDOM_LONG( 0, field_size - 1 );
	correct_block_id.y = RANDOM_LONG( 0, field_size - 1 );
	// prevent infinite loops
	int tries = 0;
	while( abs( correct_block_id.x - active_block_id.x ) < int( field_size * 0.5 ) && tries < 20 )
	{
		#ifdef PUZZLE_DEBUG_MSG
		Msg( "correct is too close...shuffling\n" );
		#endif
		correct_block_id.x = RANDOM_LONG( 0, field_size - 1 );
		tries++;
	}
	tries = 0;
	while( abs( correct_block_id.y - active_block_id.y ) < int( field_size * 0.5 ) && tries < 20 )
	{
		#ifdef PUZZLE_DEBUG_MSG
		Msg( "correct is too close...shuffling\n" );
		#endif
		correct_block_id.y = RANDOM_LONG( 0, field_size - 1 );
		tries++;
	}

	#ifdef PUZZLE_DEBUG_MSG
	Msg( "correct is %f %f\n", correct_block_id.x, correct_block_id.y );
	#endif

	checkmark.SetCurFrame( 0 );
	checkmark.flags |= ATX_STOPATLASTFRAME;

	m_iFlags |= HUD_ACTIVE;
}

int CHudPuzzle::MsgFunc_Puzzle( const char *pszName, int iSize, void *pbuf )
{
	BEGIN_READ( pszName, pbuf, iSize );
	entidx = READ_SHORT();
	field_size = READ_BYTE();
	move_time = READ_BYTE() * 0.1f;
	sign = READ_BYTE();
	END_READ();

	Start();
	
	return 1;
}

int CHudPuzzle::Draw( float flTime )
{
	if( tr.time == tr.oldtime )
		return 1;

	if( solved )
	{
		const int checkmark_half_size = 100;
		checkmark.xmin = (ScreenWidth * 0.5f) - checkmark_half_size;
		checkmark.xmax = (ScreenWidth * 0.5f) + checkmark_half_size;
		checkmark.ymin = (ScreenHeight * 0.5f) - checkmark_half_size;
		checkmark.ymax = (ScreenHeight * 0.5f) + checkmark_half_size;
		checkmark.SetRenderMode( kRenderTransAdd );
		checkmark.SetTransparency( background_alpha * 2.0f * 255.0f );
		checkmark.DrawAnimate( 30 );

		background_alpha = CL_UTIL_Approach( 0.0f, background_alpha, (0.5f / background_alpha) * g_fFrametime * 0.1f );
		if( background_alpha <= 0.0f )
			m_iFlags &= ~HUD_ACTIVE;

		return 1;
	}

	// main field (background) is 80% of the screen height
	float square_dimension = ScreenHeight * 0.8f;
	// blocks take 75%
	float blocks_dimension = ScreenHeight * 0.75f;
	float borderLR = (square_dimension - blocks_dimension) * 0.5f;
	float borderT = borderLR;
	float borderB = borderT;
	Vector2D background_start_coord;
	background_start_coord.x = ((float)ScreenWidth - square_dimension) * 0.5f;
	background_start_coord.y = ((float)ScreenHeight - square_dimension) * 0.5f;

	// draw background
	FillRoundedRGBA( background_start_coord.x, background_start_coord.y, square_dimension, square_dimension, 10, Vector4D( 0.5f, 0.5f, 0.5f, background_alpha ) );

	// draw blocks
	/*
	==========================================================
	x = cell width
	x / 4 = cell margin
	z = total cells (ammo)
	y = total width of the whole thing
	y = zx - (z-1)*(x/4)
	cell width: x = 1 / ((z + (z-1)*4) / y);
	==========================================================
	*/
	int total_cells = field_size; // length of 1 line
	float cell_width = 1.0f / ((total_cells + ((total_cells - 1) / 4.0f)) / blocks_dimension);
	float cell_height = cell_width;
	float cell_margin = cell_width * 0.25f;
	float cell_start_x = background_start_coord.x + borderLR;
	float cell_start_y = background_start_coord.y + borderT;
	const float act_cell_r = 70.f / 255.f;
	const float act_cell_g = 169.f / 255.f;
	const float act_cell_b = 1.0f;

	Vector2D active_block_coord;
	Vector2D correct_block_coord;
	Vector2D min_coord = Vector2D( cell_start_x, cell_start_y );
	Vector2D max_coord;

	// drawing the block field...
	for( int i = 0; i < field_size; i++ )
	{
		for( int j = 0; j < field_size; j++ )
		{
			if( i == active_block_id.x && j == active_block_id.y )
				active_block_coord = Vector2D( cell_start_x, cell_start_y );
			if( i == correct_block_id.x && j == correct_block_id.y )
				correct_block_coord = Vector2D( cell_start_x, cell_start_y );
			if( i == field_size - 1 && j == field_size - 1 )
				max_coord = Vector2D( cell_start_x, cell_start_y );
			FillRoundedRGBA( cell_start_x, cell_start_y, cell_width, cell_height, 3, Vector4D( 0.5f, 0.5f, 0.5f, 0.5f ) );
			cell_start_x += cell_width + cell_margin;
		}

		// restore X start for the next line
		cell_start_x = background_start_coord.x + borderLR;
		cell_start_y += cell_height + cell_margin;
	}

	// draw the active block
	if( current_active_block_coord.IsNull() )
		current_active_block_coord = active_block_coord;
	current_active_block_coord.x = lerp( current_active_block_coord.x, active_block_coord.x, g_fFrametime * 10.f );
	current_active_block_coord.y = lerp( current_active_block_coord.y, active_block_coord.y, g_fFrametime * 10.f );
	FillRoundedRGBA( current_active_block_coord.x, current_active_block_coord.y, cell_width, cell_height, 3, Vector4D( act_cell_r, act_cell_g, act_cell_b, 0.5f ) );

	if( active_block_id.x == correct_block_id.x && active_block_id.y == correct_block_id.y )
	{
		solved = true;
		// send result to server
		char szbuf[64];
		Q_snprintf( szbuf, sizeof( szbuf ), "solvepuzzle %i %i\n", (int)entidx, (int)sign );
		ClientCmd( szbuf );
	}
#ifdef PUZZLE_DEBUG_MSG
	else
		gEngfuncs.Con_NPrintf( 1, "act %.f %.f\ncorrect %.f %.f\n", active_block_id.x, active_block_id.y, correct_block_id.x, correct_block_id.y );
#endif

	// draw bar
	// background
	float bar_h = borderB * 0.5f;
	float bar_w = blocks_dimension;
	float bar_pos_x = background_start_coord.x + borderLR;
	float bar_pos_y = background_start_coord.y + square_dimension - borderB * 0.75f;
	FillRoundedRGBA( bar_pos_x, bar_pos_y, bar_w, bar_h, 3, Vector4D( 0.5f, 0.5f, 0.5f, 0.5f ) );
	// filled
	float total_length = (max_coord - min_coord).Length();
	float current_length = (active_block_coord - correct_block_coord).Length();
#ifdef PUZZLE_DEBUG_MSG
	gEngfuncs.Con_NPrintf( 3, "total length %.f, cur %.f\n", total_length, current_length );
#endif
	bar_pos_x += 3;
	bar_pos_y += 3;
	bar_h -= 7;
	bar_w = bar_w - ((bar_w / total_length) * current_length);
	current_bar_width = lerp( current_bar_width, bar_w, g_fFrametime * 7.5f );
	FillRoundedRGBA( bar_pos_x, bar_pos_y, current_bar_width, bar_h, 3, Vector4D( act_cell_r, act_cell_g, act_cell_b, 0.5f ) );

	// move correct block every second - otherwise it's too easy! :)
	if( tr.time > next_move_time )
	{
		MoveCorrectBlock( RANDOM_LONG( 0, 3 ) );
		// a chance to do it again
		if( RANDOM_LONG(0,9) > 6 )
			MoveCorrectBlock( RANDOM_LONG( 0, 3 ) );
		next_move_time = tr.time + move_time;
	}

	return 1;
}
