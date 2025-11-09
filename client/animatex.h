//============================================================================
// diffusion - simple animating texture class to use instead of sprites
//============================================================================

#ifndef _WIN32
#include <limits.h>
#define MAX_PATH PATH_MAX
#endif // _WIN32

#define MAX_ANIMATEX_FRAMES 256
// flags
#define ATX_STOPATLASTFRAME BIT(0)

class CAnimatex
{
public:
	int iTotalFrames;
	int iFrame;
	int framerate;
	int Texture[MAX_ANIMATEX_FRAMES];
	float fCurFrame;
	int xmin, ymin, xmax, ymax;
	float r, g, b, a;
	int flags;

	void Init( const char *Tex );
	bool Initialized( void );
	void SetPos( int x_min, int y_min, int x_max, int y_max );
	int GetTotalFrames( void );
	int GetCurFrame( void );
	void DrawFrame( int Frame );
	void DrawAnimate( float Speed = 10.0f );
	void SetColor( int R, int G, int B );
	void SetTransparency( int A );
	void SetRenderMode( int RenderMode );
	void Free( void );
	void SetCurFrame( int Frame );
	int GetAnimationCurFrame( void );
	void AdvanceFrame( float Speed );
	bool IsLastFrame( void );

	char start_tex[MAX_PATH]; // texture which was put into Init() - just for caching purposes for studio models to reference
};
