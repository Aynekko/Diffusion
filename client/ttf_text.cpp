//==================================================================
// ttf_text.cpp - client-side Unicode/TrueType text drawing
//==================================================================
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <unordered_map>
#include <vector>

#include "hud.h"

#ifdef STB_TRUETYPE_IMPLEMENTATION // the define is in hud.h

#ifdef _WIN32
#include <windows.h>
#endif

#include "render/r_opengl.h"
#include "ttf_stb.h"

float       ttf_hudscale = 1.0f;
ttf_font_t *g_pTTFDefault = NULL;

// flags used by the pfn-style wrappers (text is usually always additive)
#define TTF_PFN_FLAGS ( TTF_DRAW_HUD | TTF_DRAW_ADDITIVE )

static const unsigned char g_ttfColors[8][3] =
{
	{ 0, 0, 0 }, { 255, 0, 0 }, { 0, 255, 0 }, { 255, 255, 0 },
	{ 0, 0, 255 }, { 0, 255, 255 }, { 255, 0, 255 }, { 240, 180, 24 },
};

struct ttf_glyph_t
{
	short ax, ay, w, h;   // atlas rect
	short offx, offy;     // bitmap offset from pen x / baseline y
	float advance;        // in pixels at this size
};

struct ttf_face_t
{
	stbtt_fontinfo info;
	unsigned char *data;
};

struct ttf_font_t
{
	std::vector<ttf_face_t> faces;
	int pixelHeight;
	float scale;
	unsigned atlasGen;
	std::unordered_map<uint64_t, ttf_glyph_t> cache;
};

// atlas
static const int ATLAS_SIZE = 1024;
static struct
{
	GLuint tex;
	int x, y, rowH;
	unsigned gen;
} g_atlas = { 0, 0, 0, 0, 1 };

// batch
struct ttf_vert_t
{
	float x, y, s, t;
	unsigned char c[4];
};

static std::vector<ttf_vert_t> g_batch;
static bool g_batchAdd = false;
static unsigned char g_lastColor[4] = { 255, 255, 255, 255 };
static const size_t BATCH_MAX_VERTS = 4 * 2048;

int TTF_DrawString_Compat( int x, int y, const char *s, int r, int g, int b )
{
	unsigned char color[4] = { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
	return TTF_DrawString( (float)x, (float)y, s, color, g_pTTFDefault, TTF_PFN_FLAGS );
}

int TTF_DrawStringReverse_Compat( int x, int y, const char *s, int r, int g, int b )
{
	unsigned char color[4] = { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
	return TTF_DrawStringReverse( (float)x, (float)y, s, color, g_pTTFDefault, TTF_PFN_FLAGS );
}

// Sends everything queued so far to GL: one state save/restore and one glBegin/glEnd for the whole batch.
void TTF_FlushText( void )
{
	if( g_batch.empty() ) return;

	if( g_atlas.tex )
	{
		pglPushAttrib( GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TEXTURE_BIT | GL_CURRENT_BIT );

		// GL_BindShader( NULL );

		pglEnable( GL_TEXTURE_2D );
		pglEnable( GL_BLEND );
		pglDisable( GL_ALPHA_TEST );
		pglDisable( GL_DEPTH_TEST );
		pglBlendFunc( GL_SRC_ALPHA, g_batchAdd ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA );
		pglTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		pglBindTexture( GL_TEXTURE_2D, g_atlas.tex );

		pglBegin( GL_QUADS );
		for( size_t i = 0; i < g_batch.size(); i++ )
		{
			const ttf_vert_t &v = g_batch[i];
			pglColor4ubv( v.c );
			pglTexCoord2f( v.s, v.t );
			pglVertex2f( v.x, v.y );
		}
		pglEnd();

		pglPopAttrib(); // restores bound texture too, so the engine's texture-bind cache stays valid
	}

	g_batch.clear();
}

static void TTF_AtlasReset( void )
{
	g_atlas.x = g_atlas.y = g_atlas.rowH = 0;
	g_atlas.gen++; // fonts lazily drop their caches when they see a new generation
}

static void TTF_AtlasInit( void )
{
	std::vector<unsigned char> zero( ATLAS_SIZE * ATLAS_SIZE, 0 );
	GLint prevTex = 0;
	pglGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );

	pglGenTextures( 1, &g_atlas.tex );
	pglBindTexture( GL_TEXTURE_2D, g_atlas.tex );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
	pglTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	pglTexImage2D( GL_TEXTURE_2D, 0, GL_ALPHA8, ATLAS_SIZE, ATLAS_SIZE, 0, GL_ALPHA, GL_UNSIGNED_BYTE, &zero[0] );

	pglBindTexture( GL_TEXTURE_2D, (GLuint)prevTex );
	TTF_AtlasReset();
}

static bool TTF_AtlasAlloc( int w, int h, int *ox, int *oy )
{
	w += 1; h += 1; // 1px gap so bilinear sampling never bleeds between glyphs

	if( w > ATLAS_SIZE || h > ATLAS_SIZE )
		return false;

	if( g_atlas.x + w > ATLAS_SIZE )
	{
		g_atlas.x = 0;
		g_atlas.y += g_atlas.rowH;
		g_atlas.rowH = 0;
	}

	if( g_atlas.y + h > ATLAS_SIZE )
		return false;

	*ox = g_atlas.x;
	*oy = g_atlas.y;
	g_atlas.x += w;

	if( h > g_atlas.rowH )
		g_atlas.rowH = h;

	return true;
}

// Self-contained: saves and restores the binding / pixel-store state it touches. Only runs on a glyph cache miss.
static void TTF_UploadGlyph( int x, int y, int w, int h, const unsigned char *pixels )
{
	GLint prevTex = 0, prevAlign = 4;
	pglGetIntegerv( GL_TEXTURE_BINDING_2D, &prevTex );
	pglGetIntegerv( GL_UNPACK_ALIGNMENT, &prevAlign );
	pglPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
	pglBindTexture( GL_TEXTURE_2D, g_atlas.tex );
	pglTexSubImage2D( GL_TEXTURE_2D, 0, x, y, w, h, GL_ALPHA, GL_UNSIGNED_BYTE, pixels );
	pglBindTexture( GL_TEXTURE_2D, (GLuint)prevTex );
	pglPixelStorei( GL_UNPACK_ALIGNMENT, prevAlign );
}

void TTF_OnVidRestart( void )
{
	// the old GL context (and texture) is gone; don't delete the id, it may now belong to something else
	g_batch.clear();
	g_atlas.tex = 0;
	TTF_AtlasReset();
}

// font loading
ttf_font_t *TTF_LoadFont( const char *const *files, int numFiles, int pixelHeight )
{
	ttf_font_t *f = new ttf_font_t;
	f->pixelHeight = pixelHeight;
	f->scale = 1.0f;
	f->atlasGen = 0;

	for( int i = 0; i < numFiles; i++ )
	{
		int len = 0;
		byte *buf = gEngfuncs.COM_LoadFile( files[i], 5, &len );
		if( !buf ) continue;

		ttf_face_t face;
		face.data = (unsigned char *)malloc( len ); // stb keeps pointing into this, so keep our own copy
		memcpy( face.data, buf, len );
		gEngfuncs.COM_FreeFile( buf );

		if( !stbtt_InitFont( &face.info, face.data, stbtt_GetFontOffsetForIndex( face.data, 0 ) ) )
		{
			free( face.data );
			continue;
		}

		f->faces.push_back( face );
	}

	if( f->faces.empty() )
	{
		delete f;
		return NULL;
	}

	return f;
}

int TTF_GetCharHeight( void )
{
	return gHUD.iCharHeight;
}

void TTF_FreeFont( ttf_font_t *f )
{
	if( !f )
		return;

	for( size_t i = 0; i < f->faces.size(); i++ )
		free( f->faces[i].data );

	if( g_pTTFDefault == f )
		g_pTTFDefault = NULL;

	delete f;
}

void TTF_SetFontScale( ttf_font_t *f, float scale )
{
	if( f && scale > 0.0f ) f->scale = scale;
}

// Valid UTF-8 is decoded normally. An invalid byte is treated as a legacy CP1251 byte, so old single-byte
// Russian strings keep working until they are converted
static unsigned TTF_Cp1251( unsigned b )
{
	if( b >= 0xC0 ) return 0x410 + (b - 0xC0);
	if( b == 0xA8 ) return 0x401;
	if( b == 0xB8 ) return 0x451;
	return b; // good enough for the rest (latin-1 range)
}

static unsigned TTF_DecodeUtf8( const unsigned char **ps )
{
	const unsigned char *s = *ps;
	unsigned c = *s;
	int extra = 0;
	unsigned cp = 0, minv = 0;

	if( c < 0x80 ) { *ps = s + 1; return c; }
	else if( (c & 0xE0) == 0xC0 ) { extra = 1; cp = c & 0x1F; minv = 0x80; }
	else if( (c & 0xF0) == 0xE0 ) { extra = 2; cp = c & 0x0F; minv = 0x800; }
	else if( (c & 0xF8) == 0xF0 ) { extra = 3; cp = c & 0x07; minv = 0x10000; }

	if( extra )
	{
		bool ok = true;
		for( int i = 1; i <= extra; i++ )
		{
			if( (s[i] & 0xC0) != 0x80 ) { ok = false; break; }
			cp = (cp << 6) | (s[i] & 0x3F);
		}
		if( ok && cp >= minv && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF) )
		{
			*ps = s + extra + 1;
			return cp;
		}
	}

	*ps = s + 1;

	return TTF_Cp1251( c );
}

unsigned TTF_NextCodepoint( const char **s )
{
	const unsigned char *p = (const unsigned char *)*s;
	unsigned cp = TTF_DecodeUtf8( &p );
	*s = (const char *)p;
	return cp;
}

// glyphs
static int TTF_PixelSize( ttf_font_t *f, float hudscale )
{
	int px = (int)floorf( f->pixelHeight * f->scale * hudscale + 0.5f );
	if( px < 1 ) px = 1;
	if( px > 512 ) px = 512;
	return px;
}

static float TTF_Ascent( ttf_font_t *f, int px )
{
	int asc, desc, gap;
	const stbtt_fontinfo *info = &f->faces[0].info;
	stbtt_GetFontVMetrics( info, &asc, &desc, &gap );
	return floorf( asc * stbtt_ScaleForPixelHeight( info, (float)px ) + 0.5f );
}

static ttf_face_t *TTF_FindFace( ttf_font_t *f, unsigned cp, int *glyphIndex )
{
	for( size_t i = 0; i < f->faces.size(); i++ )
	{
		int gi = stbtt_FindGlyphIndex( &f->faces[i].info, (int)cp );
		if( gi ) { *glyphIndex = gi; return &f->faces[i]; }
	}

	*glyphIndex = 0; // .notdef of the primary face

	return &f->faces[0];
}

static const ttf_glyph_t *TTF_GetGlyph( ttf_font_t *f, int px, unsigned cp )
{
	if( f->atlasGen != g_atlas.gen )
	{
		f->cache.clear();
		f->atlasGen = g_atlas.gen;
	}

	uint64_t key = ((uint64_t)px << 32) | cp;
	std::unordered_map<uint64_t, ttf_glyph_t>::iterator it = f->cache.find( key );
	if( it != f->cache.end() )
		return &it->second;

	if( !g_atlas.tex )
		TTF_AtlasInit();

	ttf_glyph_t g;
	memset( &g, 0, sizeof( g ) );

	int gi;
	ttf_face_t *face = TTF_FindFace( f, cp, &gi );
	float s = stbtt_ScaleForPixelHeight( &face->info, (float)px );

	int adv, lsb, x0, y0, x1, y1;
	stbtt_GetGlyphHMetrics( &face->info, gi, &adv, &lsb );
	g.advance = adv * s;

	stbtt_GetGlyphBitmapBox( &face->info, gi, s, s, &x0, &y0, &x1, &y1 );
	int w = x1 - x0, h = y1 - y0;

	if( w > 0 && h > 0 )
	{
		int ax, ay;
		if( !TTF_AtlasAlloc( w, h, &ax, &ay ) )
		{
			// atlas full: draw what is queued (it still references the old contents), then start over
			TTF_FlushText();
			TTF_AtlasReset();
			f->cache.clear();
			f->atlasGen = g_atlas.gen;
			if( !TTF_AtlasAlloc( w, h, &ax, &ay ) ) w = h = 0; // glyph larger than the atlas, skip it
		}

		if( w > 0 )
		{
			std::vector<unsigned char> bmp( (size_t)w * h );
			stbtt_MakeGlyphBitmap( &face->info, &bmp[0], w, h, w, s, s, gi );
			TTF_UploadGlyph( ax, ay, w, h, &bmp[0] );
			g.ax = (short)ax; g.ay = (short)ay;
			g.w = (short)w;   g.h = (short)h;
			g.offx = (short)x0;
			g.offy = (short)(y0 + (int)TTF_Ascent( f, px )); // baked in: no per-draw metrics lookup
		}
	}

	return &(f->cache[key] = g);
}

// Queues one glyph quad at pen position (x,y = top of the line, screen pixels). Returns advance in screen pixels.
static float TTF_QueueGlyph( ttf_font_t *f, int px, unsigned cp, float x, float y, const unsigned char *col, int flags )
{
	const ttf_glyph_t *g = TTF_GetGlyph( f, px, cp );

	if( g->w > 0 )
	{
		const bool add = (flags & TTF_DRAW_ADDITIVE) != 0;

		if( !g_batch.empty() && (add != g_batchAdd || g_batch.size() + 4 > BATCH_MAX_VERTS) )
			TTF_FlushText();

		g_batchAdd = add;

		const float gx = floorf( x + 0.5f ) + g->offx;
		const float gy = floorf( y + 0.5f ) + g->offy;
		const float s1 = (float)g->ax / ATLAS_SIZE;
		const float t1 = (float)g->ay / ATLAS_SIZE;
		const float s2 = (float)(g->ax + g->w) / ATLAS_SIZE;
		const float t2 = (float)(g->ay + g->h) / ATLAS_SIZE;

		ttf_vert_t v[4] =
		{
			{ gx,         gy,         s1, t1, { col[0], col[1], col[2], col[3] } },
			{ gx + g->w,  gy,         s2, t1, { col[0], col[1], col[2], col[3] } },
			{ gx + g->w,  gy + g->h,  s2, t2, { col[0], col[1], col[2], col[3] } },
			{ gx,         gy + g->h,  s1, t2, { col[0], col[1], col[2], col[3] } },
		};

		g_batch.insert( g_batch.end(), v, v + 4 );
	}
	return g->advance;
}

// ---------------------------------------------------------------- measuring (no GL needed)
static float TTF_MeasureAdvance( ttf_font_t *f, int px, unsigned cp, float x, unsigned prev, int flags )
{
	int gi;

	if( cp == '\t' || cp == ' ' || cp > 32 )
	{
		ttf_face_t *face = TTF_FindFace( f, cp == '\t' ? ' ' : cp, &gi );
		int adv, lsb;
		float s = stbtt_ScaleForPixelHeight( &face->info, (float)px );
		stbtt_GetGlyphHMetrics( &face->info, gi, &adv, &lsb );

		if( cp == '\t' )
		{
			float space = adv * s;
			float tab = space * 4.0f;
			float stop = tab - fmodf( x, tab );
			if( stop < space ) stop += tab;
			return stop;
		}

		float kern = 0.0f;
		if( (flags & TTF_DRAW_KERNING) && prev )
		{
			const stbtt_fontinfo *pi = &f->faces[0].info;
			kern = stbtt_GetCodepointKernAdvance( pi, (int)prev, (int)cp ) * stbtt_ScaleForPixelHeight( pi, (float)px );
		}
		return adv * s + kern;
	}

	return 0.0f; // other control characters
}

float TTF_CodepointAdvance( ttf_font_t *font, unsigned cp, int flags )
{
	if( !font )
		return 0.0f;

	const float hs = (flags & TTF_DRAW_HUD) ? ttf_hudscale : 1.0f;
	return TTF_MeasureAdvance( font, TTF_PixelSize( font, hs ), cp, 0.0f, 0, 0 ) / hs;
}

static inline bool TTF_IsColorCode( const char *s )
{
	return s[0] == '^' && s[1] >= '0' && s[1] <= '9';
}

// Only queues the glyph: call TTF_FlushText() when you are done with a run of characters.
int TTF_DrawCharacter( float x, float y, int number, const unsigned char *color, ttf_font_t *font, int flags )
{
	if( !font || number <= 0 )
		return 0;

	if( number < 32 && number != '\t' )
		return 0;

	const float hs = (flags & TTF_DRAW_HUD) ? ttf_hudscale : 1.0f;
	const int px = TTF_PixelSize( font, hs );
	float adv;

	if( number == '\t' )
		adv = TTF_MeasureAdvance( font, px, '\t', x * hs, 0, 0 );
	else if( x >= gHUD.m_scrinfo.iWidth || y >= gHUD.m_scrinfo.iHeight )
		adv = TTF_MeasureAdvance( font, px, (unsigned)number, 0, 0, 0 ); // off-screen: width only, no GL, nothing queued
	else
	{
		if( !(flags & TTF_DRAW_NOCOLOR) )
		{
			static const unsigned char white[4] = { 255, 255, 255, 255 };
			memcpy( g_lastColor, color ? color : white, 4 );
		}

		adv = TTF_QueueGlyph( font, px, (unsigned)number, x * hs, y * hs, g_lastColor, flags );
	}

	return (int)floorf( adv / hs + 0.5f );
}

int TTF_DrawString( float x, float y, const char *s, const unsigned char *color, ttf_font_t *font, int flags )
{
	if( !font || !s )
		return 0;

	const float hs = (flags & TTF_DRAW_HUD) ? ttf_hudscale : 1.0f;
	const int px = TTF_PixelSize( font, hs );
	float sx = x * hs, sy = y * hs, pen = 0.0f;
	unsigned char base[4] = { 255, 255, 255, 255 }, cur[4];
	unsigned prev = 0;

	if( color )
		memcpy( base, color, 4 );

	memcpy( cur, base, 4 );

	const unsigned char *p = (const unsigned char *)s;
	while( *p )
	{
		if( *p == '\n' )
		{
			p++;
			if( !*p ) break;
			if( !(flags & TTF_DRAW_NOLF) )
			{
				pen = 0.0f;
				sy += px;
			}
			if( flags & TTF_DRAW_RESETCOLORONLF )
				memcpy( cur, base, 4 );
			prev = 0;
			continue;
		}

		if( TTF_IsColorCode( (const char *)p ) )
		{
			if( !(flags & TTF_DRAW_FORCECOL) )
				memcpy( cur, g_ttfColors[(p[1] - '0') & 7], 3 ); // keep base alpha
			p += 2;
			continue;
		}

		unsigned cp = TTF_DecodeUtf8( &p );
		if( cp == '\t' )
		{
			pen += TTF_MeasureAdvance( font, px, cp, sx + pen, 0, 0 );
			prev = 0;
			continue;
		}
		if( cp < 32 )
			continue;

		float kern = 0.0f;
		if( (flags & TTF_DRAW_KERNING) && prev )
			kern = TTF_MeasureAdvance( font, px, cp, 0, prev, flags ) - TTF_MeasureAdvance( font, px, cp, 0, 0, 0 );

		pen += kern;
		pen += TTF_QueueGlyph( font, px, cp, sx + pen, sy, cur, flags );
		prev = cp;
	}

	if( !(flags & TTF_DRAW_DEFER) )
		TTF_FlushText();

	return (int)floorf( pen / hs + 0.5f );
}

int TTF_DrawStringReverse( float x, float y, const char *s, const unsigned char *color, ttf_font_t *font, int flags )
{
	if( !font || !s )
		return 0;

	const float hs = (flags & TTF_DRAW_HUD) ? ttf_hudscale : 1.0f;
	const float lineH = TTF_PixelSize( font, hs ) / hs;
	int widest = 0;
	std::string line;

	const char *p = s;
	for( ;; )
	{
		const char *e = p;
		while( *e && (*e != '\n' || (flags & TTF_DRAW_NOLF)) ) e++; // with NOLF the whole string is one line

		line.assign( p, e - p );

		int w = 0;
		TTF_StringLen( font, line.c_str(), flags, &w, NULL );
		TTF_DrawString( x - w, y, line.c_str(), color, font, flags | TTF_DRAW_DEFER );
		if( w > widest ) widest = w;

		if( !*e ) break;
		p = e + 1;
		if( !*p ) break; // trailing newline, nothing more to draw
		y += lineH;
	}

	if( !(flags & TTF_DRAW_DEFER) )
		TTF_FlushText();

	return widest;
}

void TTF_StringLen( ttf_font_t *font, const char *s, int flags, int *width, int *height )
{
	if( width ) *width = 0;
	if( height ) *height = 0;
	if( !font || !s ) return;

	const float hs = (flags & TTF_DRAW_HUD) ? ttf_hudscale : 1.0f;
	const int px = TTF_PixelSize( font, hs );
	float pen = 0.0f, maxw = 0.0f;
	int lines = 1;
	unsigned prev = 0;

	const unsigned char *p = (const unsigned char *)s;
	while( *p )
	{
		if( *p == '\n' )
		{
			p++;
			if( !*p ) break;
			if( !(flags & TTF_DRAW_NOLF) ) { pen = 0.0f; lines++; }
			prev = 0;
			continue;
		}
		if( TTF_IsColorCode( (const char *)p ) ) { p += 2; continue; }

		unsigned cp = TTF_DecodeUtf8( &p );
		if( cp < 32 && cp != '\t' ) continue;
		pen += TTF_MeasureAdvance( font, px, cp, pen, prev, flags );
		prev = cp;
		if( pen > maxw ) maxw = pen;
	}

	if( width ) *width = (int)floorf( maxw / hs + 0.5f );
	if( height ) *height = (int)floorf( lines * px / hs + 0.5f );
}

// pfn-style wrappers
static unsigned char s_textColor[4] = { 255, 255, 255, 255 };

void TTF_PfnSetTextColor( float r, float g, float b )
{
	s_textColor[0] = (unsigned char)(r * 255.0f + 0.5f);
	s_textColor[1] = (unsigned char)(g * 255.0f + 0.5f);
	s_textColor[2] = (unsigned char)(b * 255.0f + 0.5f);
	s_textColor[3] = 255;
}

static unsigned char s_utfBuf[4];
static int s_utfLen = 0, s_utfNeed = 0;

void TTF_ResetUtf8State( void )
{
	s_utfLen = s_utfNeed = 0;
}

static int TTF_FlushPendingAsLegacy( int x, int y, const unsigned char *col )
{
	int adv = 0;
	for( int i = 0; i < s_utfLen; i++ )
		adv += TTF_DrawCharacter( (float)(x + adv), (float)y, (int)TTF_Cp1251( s_utfBuf[i] ), col, g_pTTFDefault, TTF_PFN_FLAGS );
	s_utfLen = s_utfNeed = 0;
	return adv;
}

int TTF_PfnDrawCharacter( int x, int y, int number, int r, int g, int b )
{
	const unsigned char col[4] = { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
	const unsigned b0 = (unsigned)number & 255;
	int adv = 0;

	// pending sequence interrupted by a non-continuation byte: it was legacy text
	if( s_utfLen && (b0 & 0xC0) != 0x80 )
		adv += TTF_FlushPendingAsLegacy( x, y, col );

	if( b0 >= 0xC2 && b0 <= 0xF4 ) // lead byte of a multi-byte sequence
	{
		s_utfBuf[0] = (unsigned char)b0;
		s_utfLen = 1;
		s_utfNeed = (b0 >= 0xF0) ? 4 : (b0 >= 0xE0) ? 3 : 2;
		return adv; // nothing drawn yet, caller advances by 0
	}

	if( s_utfLen && (b0 & 0xC0) == 0x80 ) // continuation byte
	{
		s_utfBuf[s_utfLen++] = (unsigned char)b0;
		if( s_utfLen < s_utfNeed ) return adv;

		const unsigned char *p = s_utfBuf;
		unsigned cp = TTF_DecodeUtf8( &p );
		if( p != s_utfBuf + s_utfLen ) // overlong / invalid sequence
			return adv + TTF_FlushPendingAsLegacy( x, y, col );

		s_utfLen = s_utfNeed = 0;
		return adv + TTF_DrawCharacter( (float)(x + adv), (float)y, (int)cp, col, g_pTTFDefault, TTF_PFN_FLAGS );
	}

	// plain ASCII or a stray byte
	unsigned cp = (b0 < 0x80) ? b0 : TTF_Cp1251( b0 );
	return adv + TTF_DrawCharacter( (float)(x + adv), (float)y, (int)cp, col, g_pTTFDefault, TTF_PFN_FLAGS );
}

int TTF_PfnDrawString( int x, int y, const char *str, int r, int g, int b )
{
	unsigned char c[4] = { (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
	return x + TTF_DrawString( (float)x, (float)y, str, c, g_pTTFDefault, TTF_PFN_FLAGS | TTF_DRAW_NOLF );
}

int TTF_PfnDrawConsoleString( int x, int y, const char *string )
{
	return x + TTF_DrawString( (float)x, (float)y, string, s_textColor, g_pTTFDefault, TTF_PFN_FLAGS );
}

void TTF_PfnDrawConsoleStringLen( const char *string, int *length, int *height )
{
	TTF_StringLen( g_pTTFDefault, string, TTF_DRAW_HUD, length, height );
}

#else // stub functions
void TTF_ResetUtf8State( void ){}
void TTF_FlushText( void ){}
#endif // STB_TRUETYPE_IMPLEMENTATION