#pragma once
#include <cstdint>
#include <cstddef>
#include <cmath>
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Vector2 { float x, y; } Vector2;
typedef struct Vector3 { float x, y, z; } Vector3;
typedef struct Vector4 { float x, y, z, w; } Vector4;
typedef struct Rectangle { float x, y, width, height; } Rectangle;
typedef struct Color { unsigned char r,g,b,a; } Color;
typedef struct Texture { unsigned int id; int width,height,mipmaps,format; } Texture;
typedef Texture Texture2D;
typedef struct RenderTexture { unsigned int id; Texture texture; Texture depth; } RenderTexture;
typedef RenderTexture RenderTexture2D;
typedef struct GlyphInfo { int value; int offsetX,offsetY,advanceX; void* image; } GlyphInfo;
typedef struct Font { int baseSize; int glyphCount; int glyphPadding; Texture2D texture; Rectangle* recs; GlyphInfo* glyphs; } Font;
typedef struct Shader { unsigned int id; int* locs; } Shader;
typedef struct Camera2D { Vector2 offset; Vector2 target; float rotation; float zoom; } Camera2D;
typedef struct NPatchInfo { Rectangle source; int left, top, right, bottom; int layout; } NPatchInfo;

static const Color LIGHTGRAY = {200,200,200,255};
static const Color GRAY = {130,130,130,255};
static const Color DARKGRAY = {80,80,80,255};
static const Color YELLOW = {253,249,0,255};
static const Color GOLD = {255,203,0,255};
static const Color ORANGE = {255,161,0,255};
static const Color PINK = {255,109,194,255};
static const Color RED = {230,41,55,255};
static const Color MAROON = {190,33,55,255};
static const Color GREEN = {0,228,48,255};
static const Color LIME = {0,158,47,255};
static const Color DARKGREEN = {0,117,44,255};
static const Color SKYBLUE = {102,191,255,255};
static const Color BLUE = {0,121,241,255};
static const Color DARKBLUE = {0,82,172,255};
static const Color PURPLE = {200,122,255,255};
static const Color VIOLET = {135,60,190,255};
static const Color DARKPURPLE = {112,31,126,255};
static const Color BEIGE = {211,176,131,255};
static const Color BROWN = {127,106,79,255};
static const Color DARKBROWN = {76,63,47,255};
static const Color WHITE = {255,255,255,255};
static const Color BLACK = {0,0,0,255};
static const Color BLANK = {0,0,0,0};
static const Color MAGENTA = {255,0,255,255};
static const Color RAYWHITE = {245,245,245,255};

enum { LOG_ALL=0, LOG_TRACE, LOG_DEBUG, LOG_INFO, LOG_WARNING, LOG_ERROR, LOG_FATAL, LOG_NONE };
enum { MOUSE_BUTTON_LEFT=0, MOUSE_BUTTON_RIGHT=1, MOUSE_BUTTON_MIDDLE=2, MOUSE_BUTTON_SIDE=3, MOUSE_BUTTON_EXTRA=4, MOUSE_BUTTON_FORWARD=5, MOUSE_BUTTON_BACK=6 };
enum { MOUSE_CURSOR_DEFAULT=0, MOUSE_CURSOR_ARROW=0, MOUSE_CURSOR_IBEAM=1, MOUSE_CURSOR_CROSSHAIR=2, MOUSE_CURSOR_POINTING_HAND=3, MOUSE_CURSOR_RESIZE_EW=4, MOUSE_CURSOR_RESIZE_NS=5, MOUSE_CURSOR_RESIZE_NWSE=6, MOUSE_CURSOR_RESIZE_NESW=7, MOUSE_CURSOR_RESIZE_ALL=8, MOUSE_CURSOR_NOT_ALLOWED=9 };
enum {
 KEY_NULL=0, KEY_APOSTROPHE=39, KEY_COMMA=44, KEY_MINUS=45, KEY_PERIOD=46, KEY_SLASH=47,
 KEY_ZERO=48, KEY_ONE=49, KEY_TWO=50, KEY_THREE=51, KEY_FOUR=52, KEY_FIVE=53, KEY_SIX=54, KEY_SEVEN=55, KEY_EIGHT=56, KEY_NINE=57,
 KEY_SEMICOLON=59, KEY_EQUAL=61, KEY_A=65, KEY_B=66, KEY_C=67, KEY_D=68, KEY_E=69, KEY_F=70, KEY_G=71, KEY_H=72, KEY_I=73, KEY_J=74, KEY_K=75, KEY_L=76, KEY_M=77, KEY_N=78, KEY_O=79, KEY_P=80, KEY_Q=81, KEY_R=82, KEY_S=83, KEY_T=84, KEY_U=85, KEY_V=86, KEY_W=87, KEY_X=88, KEY_Y=89, KEY_Z=90,
 KEY_SPACE=32, KEY_ESCAPE=256, KEY_ENTER=257, KEY_TAB=258, KEY_BACKSPACE=259, KEY_INSERT=260, KEY_DELETE=261, KEY_RIGHT=262, KEY_LEFT=263, KEY_DOWN=264, KEY_UP=265, KEY_PAGE_UP=266, KEY_PAGE_DOWN=267, KEY_HOME=268, KEY_END=269,
 KEY_CAPS_LOCK=280, KEY_SCROLL_LOCK=281, KEY_NUM_LOCK=282, KEY_PRINT_SCREEN=283, KEY_PAUSE=284,
 KEY_F1=290, KEY_F2=291, KEY_F3=292, KEY_F4=293, KEY_F5=294, KEY_F6=295, KEY_F7=296, KEY_F8=297, KEY_F9=298, KEY_F10=299, KEY_F11=300, KEY_F12=301,
 KEY_LEFT_SHIFT=340, KEY_LEFT_CONTROL=341, KEY_LEFT_ALT=342, KEY_LEFT_SUPER=343, KEY_RIGHT_SHIFT=344, KEY_RIGHT_CONTROL=345, KEY_RIGHT_ALT=346, KEY_RIGHT_SUPER=347
};

typedef int KeyboardKey;
typedef int MouseButton;
typedef int MouseCursor;

static inline Vector2 GetMousePosition(void) { return (Vector2){0,0}; }
static inline Vector2 GetMouseDelta(void) { return (Vector2){0,0}; }
static inline float GetMouseWheelMove(void) { return 0.0f; }
static inline Vector2 GetMouseWheelMoveV(void) { return (Vector2){0,0}; }
static inline int IsMouseButtonPressed(int b) { (void)b; return 0; }
static inline int IsMouseButtonDown(int b) { (void)b; return 0; }
static inline int IsMouseButtonReleased(int b) { (void)b; return 0; }
static inline int IsKeyPressed(int k) { (void)k; return 0; }
static inline int IsKeyDown(int k) { (void)k; return 0; }
static inline int GetKeyPressed(void) { return 0; }
static inline int GetCharPressed(void) { return 0; }
static inline void SetMouseCursor(int c) { (void)c; }
static inline int CheckCollisionPointRec(Vector2 p, Rectangle r) { return p.x >= r.x && p.y >= r.y && p.x <= r.x+r.width && p.y <= r.y+r.height; }
static inline int CheckCollisionRecs(Rectangle a, Rectangle b) { return a.x < b.x+b.width && a.x+a.width > b.x && a.y < b.y+b.height && a.y+a.height > b.y; }
static inline Rectangle GetCollisionRec(Rectangle a, Rectangle b) { float x=fmaxf(a.x,b.x), y=fmaxf(a.y,b.y), r=fminf(a.x+a.width,b.x+b.width), d=fminf(a.y+a.height,b.y+b.height); return (Rectangle){x,y,fmaxf(0,r-x),fmaxf(0,d-y)}; }
static inline double GetTime(void) { static double t=0.0; t += 0.016; return t; }
static inline float GetFrameTime(void) { return 1.0f/60.0f; }
static inline int GetScreenWidth(void) { return 1280; }
static inline int GetScreenHeight(void) { return 720; }
static inline Font GetFontDefault(void) { Font f={20,0,0,{0,1,1,1,0},0,0}; return f; }
static inline Vector2 MeasureTextEx(Font f,const char* text,float size,float spacing) { (void)f; size_t n=text?strlen(text):0; return (Vector2){(float)n*(size*0.5f+spacing),size}; }
static inline int MeasureText(const char* text,int size) { return (int)(text?strlen(text):0)*size/2; }
static inline const char* TextFormat(const char* fmt, ...) { return fmt; }
static inline void TraceLog(int level,const char* fmt, ...) { (void)level;(void)fmt; }
static inline char* LoadFileText(const char* p) { (void)p; return 0; }
static inline void UnloadFileText(char* p) { (void)p; }
static inline int FileExists(const char* p) { (void)p; return 0; }

static inline void BeginScissorMode(int x,int y,int w,int h){(void)x;(void)y;(void)w;(void)h;}
static inline void EndScissorMode(void){}
static inline void DrawRectangle(int x,int y,int w,int h,Color c){(void)x;(void)y;(void)w;(void)h;(void)c;}
static inline void DrawRectangleRec(Rectangle r,Color c){(void)r;(void)c;}
static inline void DrawRectangleLinesEx(Rectangle r,float t,Color c){(void)r;(void)t;(void)c;}
static inline void DrawRectangleRounded(Rectangle r,float roundness,int segments,Color c){(void)r;(void)roundness;(void)segments;(void)c;}
static inline void DrawRectangleRoundedLinesEx(Rectangle r,float roundness,int segments,float thickness,Color c){(void)r;(void)roundness;(void)segments;(void)thickness;(void)c;}
static inline void DrawRectangleGradientH(int x,int y,int w,int h,Color a,Color b){(void)x;(void)y;(void)w;(void)h;(void)a;(void)b;}
static inline void DrawRectangleGradientV(int x,int y,int w,int h,Color a,Color b){(void)x;(void)y;(void)w;(void)h;(void)a;(void)b;}
static inline void DrawLineV(Vector2 a,Vector2 b,Color c){(void)a;(void)b;(void)c;}
static inline void DrawLineEx(Vector2 a,Vector2 b,float t,Color c){(void)a;(void)b;(void)t;(void)c;}
static inline void DrawCircleV(Vector2 p,float r,Color c){(void)p;(void)r;(void)c;}
static inline void DrawCircleLines(int x,int y,float r,Color c){(void)x;(void)y;(void)r;(void)c;}
static inline void DrawTriangle(Vector2 a,Vector2 b,Vector2 c,Color d){(void)a;(void)b;(void)c;(void)d;}
static inline void DrawText(const char* t,int x,int y,int s,Color c){(void)t;(void)x;(void)y;(void)s;(void)c;}
static inline void DrawTextEx(Font f,const char* t,Vector2 p,float s,float sp,Color c){(void)f;(void)t;(void)p;(void)s;(void)sp;(void)c;}
static inline void DrawTexturePro(Texture2D t,Rectangle s,Rectangle d,Vector2 o,float rot,Color tint){(void)t;(void)s;(void)d;(void)o;(void)rot;(void)tint;}
static inline Color Fade(Color c,float a){ c.a=(unsigned char)(c.a*a); return c; }

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
#ifndef TEXTURE_FILTER_BILINEAR
#define TEXTURE_FILTER_BILINEAR 1
#endif
static inline Font LoadFontEx(const char* p,int s,int* cps,int n){(void)p;(void)cps;(void)n; Font f=GetFontDefault(); f.baseSize=s; f.texture.id=1; return f;}
static inline void UnloadFont(Font f){(void)f;}
static inline void SetTextureFilter(Texture2D t,int filter){(void)t;(void)filter;}
#ifdef __cplusplus
}
#endif
#ifndef TEXTURE_FILTER_POINT
#define TEXTURE_FILTER_POINT 0
#endif
