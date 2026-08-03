#pragma once
#include "raylib.h"
#include <cmath>
static inline Vector2 Vector2Zero(void){return {0,0};}
static inline Vector2 Vector2Add(Vector2 a,Vector2 b){return {a.x+b.x,a.y+b.y};}
static inline Vector2 Vector2Subtract(Vector2 a,Vector2 b){return {a.x-b.x,a.y-b.y};}
static inline Vector2 Vector2Scale(Vector2 a,float s){return {a.x*s,a.y*s};}
static inline float Vector2Length(Vector2 a){return std::sqrt(a.x*a.x+a.y*a.y);}
static inline float Vector2Distance(Vector2 a,Vector2 b){return Vector2Length(Vector2Subtract(a,b));}
static inline float Lerp(float a,float b,float t){return a+(b-a)*t;}
