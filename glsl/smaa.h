//------------------------------------------
#define SMAA_PRESET_ULTRA
//------------------------------------------

uniform vec4 u_ScreenSizeInv;

#define SMAA_RT_METRICS u_ScreenSizeInv

#define API_V_DIR(v) -(v)
#define API_V_COORD(v) (1.0 - v)
#define API_V_BELOW(v1, v2)	v1 < v2
#define API_V_ABOVE(v1, v2)	v1 > v2

// Presets
#if defined(SMAA_PRESET_LOW)
#define SMAA_THRESHOLD 0.15
#define SMAA_MAX_SEARCH_STEPS 4
#define SMAA_DISABLE_DIAG_DETECTION
#define SMAA_DISABLE_CORNER_DETECTION
#elif defined(SMAA_PRESET_MEDIUM)
#define SMAA_THRESHOLD 0.1
#define SMAA_MAX_SEARCH_STEPS 8
#define SMAA_DISABLE_DIAG_DETECTION
#define SMAA_DISABLE_CORNER_DETECTION
#elif defined(SMAA_PRESET_HIGH)
#define SMAA_THRESHOLD 0.1
#define SMAA_MAX_SEARCH_STEPS 16
#define SMAA_MAX_SEARCH_STEPS_DIAG 8
#define SMAA_CORNER_ROUNDING 25
#elif defined(SMAA_PRESET_ULTRA)
#define SMAA_THRESHOLD 0.05
#define SMAA_MAX_SEARCH_STEPS 32
#define SMAA_MAX_SEARCH_STEPS_DIAG 16
#define SMAA_CORNER_ROUNDING 25
#endif

// Defaults
#ifndef SMAA_THRESHOLD
#define SMAA_THRESHOLD 0.1
#endif
#ifndef SMAA_MAX_SEARCH_STEPS
#define SMAA_MAX_SEARCH_STEPS 16
#endif
#ifndef SMAA_MAX_SEARCH_STEPS_DIAG
#define SMAA_MAX_SEARCH_STEPS_DIAG 8
#endif
#ifndef SMAA_CORNER_ROUNDING
#define SMAA_CORNER_ROUNDING 25
#endif

// Non-Configurable Defines
#define SMAA_AREATEX_MAX_DISTANCE 16
#define SMAA_AREATEX_MAX_DISTANCE_DIAG 20
#define SMAA_AREATEX_PIXEL_SIZE (1.0 / vec2(160.0 , 560.0))
#define SMAA_AREATEX_SUBTEX_SIZE (1.0 / 7.0)
#define SMAA_SEARCHTEX_SIZE vec2(66.0  , 33.0)
#define SMAA_SEARCHTEX_PACKED_SIZE vec2(64.0 , 16.0)
#define SMAA_CORNER_ROUNDING_NORM (float(SMAA_CORNER_ROUNDING) / 100.0)

#define SMAA_LOCAL_CONTRAST_ADAPTATION_FACTOR 2.0

#define mad(a, b, c) (a * b + c)
#define saturate(a) clamp(a, 0.0 , 1.0)

#define SMAARound(v) round(v)
//floor((v) + 0.5)
#define SMAASampleLevelZeroOffset(tex, coord, offset) textureLodOffset(tex, coord , 0.0 , offset)