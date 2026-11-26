#pragma once
#ifndef TTF_TEXT_H
#define TTF_TEXT_H

#ifdef STB_TRUETYPE_IMPLEMENTATION

struct ttf_font_t;

extern const char *const g_FontFiles[];
extern const int g_NumFonts;

enum
{
	TTF_DRAW_HUD = (1 << 0), // scale position/size by ttf_hudscale (engine FONT_DRAW_HUD analogue)
	TTF_DRAW_FORCECOL = (1 << 1), // ignore ^N colour codes inside the string
	TTF_DRAW_NOLF = (1 << 2), // ignore '\n'
	TTF_DRAW_RESETCOLORONLF = (1 << 3), // restore base colour after a newline
	TTF_DRAW_NOCOLOR = (1 << 4), // DrawCharacter: reuse the previous colour instead of the colour argument
	TTF_DRAW_KERNING = (1 << 5), // apply legacy 'kern' table kerning (primary face only)
	TTF_DRAW_ADDITIVE = (1 << 6), // additive blending (white-on-black atlas look)
	TTF_DRAW_DEFER = (1 << 7), // DrawString: don't flush at the end, you will call TTF_FlushText() yourself
};

// files[0] is the primary face, the rest are fallbacks searched in order for missing glyphs.
// pixelHeight is the line height in pixels at scale 1.0.
ttf_font_t *TTF_LoadFont( const char *const *files, int numFiles, int pixelHeight );
void        TTF_FreeFont( ttf_font_t *font );
void        TTF_SetFontScale( ttf_font_t *font, float scale );

// Call when the GL context is recreated (vid_restart / renderer reload).
void        TTF_OnVidRestart( void );

extern float        ttf_hudscale;  // used with TTF_DRAW_HUD, default 1.0
extern ttf_font_t *g_pTTFDefault; // used by the Pfn* wrappers below

// ---------------------------------------------------------------------------------------------
// Drawing is BATCHED. Glyph quads are queued and sent to GL in one go by TTF_FlushText().
//  - TTF_DrawString flushes by itself at the end (unless TTF_DRAW_DEFER is set).
//  - TTF_DrawCharacter / TTF_PfnDrawCharacter only QUEUE. You must call TTF_FlushText():
//      * at the end of every function that draws text char-by-char (e.g. CHudMessage::MessageDrawScan), and
//      * once more at the end of HUD redraw as a safety net.
//    Flush while the 2D HUD projection is still active, and before drawing anything that must appear on top.
// ---------------------------------------------------------------------------------------------
void TTF_FlushText( void );

// 'number' is a Unicode codepoint, strings are UTF-8, colour is RGBA (4 bytes) or NULL for white.
// Return value is the advance / line width in unscaled pixels.
int  TTF_DrawCharacter( float x, float y, int number, const unsigned char *color, ttf_font_t *font, int flags );
int  TTF_DrawString_Compat( int x, int y, const char *s, int r, int g, int b );
int  TTF_DrawString( float x, float y, const char *s, const unsigned char *color, ttf_font_t *font, int flags );
int  TTF_DrawStringReverse( float x, float y, const char *s, const unsigned char *color, ttf_font_t *font, int flags );
int  TTF_DrawStringReverse_Compat( int x, int y, const char *s, int r, int g, int b );
void TTF_StringLen( ttf_font_t *font, const char *s, int flags, int *width, int *height );

// Codepoint-aware helpers for width loops
float    TTF_CodepointAdvance( ttf_font_t *font, unsigned cp, int flags ); // unscaled pixels, not rounded
unsigned TTF_NextCodepoint( const char **s );                              // decode one UTF-8 char, advance *s

// State of the byte-by-byte UTF-8 collector used by TTF_PfnDrawCharacter.
// Call before every run of per-byte TextMessageDrawChar / pfnDrawCharacter calls that forms one string/line.
void TTF_ResetUtf8State( void );

// pfn-style wrappers to assign into your gEngfuncs copy. Adjust signatures to match your engine header.
int  TTF_PfnDrawCharacter( int x, int y, int number, int r, int g, int b );
int  TTF_PfnDrawString( int x, int y, const char *str, int r, int g, int b );
int  TTF_PfnDrawConsoleString( int x, int y, const char *string );
void TTF_PfnSetTextColor( float r, float g, float b );
void TTF_PfnDrawConsoleStringLen( const char *string, int *length, int *height );
int TTF_GetCharHeight( void );

#endif // STB_TRUETYPE_IMPLEMENTATION

#endif // TTF_TEXT_H