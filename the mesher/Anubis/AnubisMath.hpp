#pragma once
#include <random>
#include "math.h"
namespace Math {

    typedef struct Matrix {
        float m0, m4, m8, m12;
        float m1, m5, m9, m13;
        float m2, m6, m10, m14;
        float m3, m7, m11, m15;
    } Matrix;

    struct IVector2 {
        union {
            struct { int x, y; };
            struct { int width, height;  };

        };
    };
    struct IVector3 {
        union {
            struct { int x, y, z; };
            struct { int width, height, depth; };
        };
    };
    struct Vector2 {
        union {
            struct { double x, y; };
            struct { double width, height; };
        };

    };
    struct Vector3 {
        union {
            struct { double x, y, z; };
            struct { double width, height, depth; };
        };
        
    };
    struct Vector4 {
        union {
            struct { double x, y, z, w; };
            struct { double width, height, depth, time; };
        };
    };
    inline static double dot(Vector3 vec1, Vector3 vec2) {
        return vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z;
    }
    inline static Vector3 scale(Vector3 vec1, float scale) {
        return { vec1.x * scale, vec1.y * scale, vec1.z * scale };
    }
    inline static Vector2 DivideVector2(Vector2 vec1, Vector2 vec2) {
        return Vector2{ vec1.x / vec2.x,vec1.y / vec2.y };
    }
    inline static  Vector2 MultiplyVector2(Vector2 vec1, Vector2 vec2) {
        return Vector2{ vec1.x * vec2.x,vec1.y * vec2.y };
    }
    inline static  Vector2 AddVector2(Vector2 vec1, Vector2 vec2) {
        return Vector2{ vec1.x + vec2.x,vec1.y + vec2.y };
    }
    inline static Vector2 SubtractVector2(Vector2 vec1, Vector2 vec2) {
        return Vector2{ vec1.x - vec2.x,vec1.y - vec2.y };
    }
    inline static  Vector3 DivideVector3(Vector3 vec1, Vector3 vec2) {
        return Vector3{ vec1.x / vec2.x,vec1.y / vec2.y, vec1.z / vec2.z };
    }
    inline static  Vector3 MultiplyVector3(Vector3 vec1, Vector3 vec2) {
        return Vector3{ vec1.x * vec2.x,vec1.y * vec2.y, vec1.z * vec2.z };
    }
    inline static Vector3 AddVector3(Vector3 vec1, Vector3 vec2) {
        return Vector3{ vec1.x + vec2.x,vec1.y + vec2.y , vec1.z + vec2.z };
    }
    inline static  Vector3 SubtractVector3(Vector3 vec1, Vector3 vec2) {
        return Vector3{ vec1.x - vec2.x,vec1.y - vec2.y, vec1.z - vec2.z };
    }
    inline static Vector3 CrossVector3(Vector3 vector1, Vector3 vector2) {
        return Vector3{
            vector1.y * vector2.z - vector1.z * vector2.y,
            vector1.z * vector2.x - vector1.x * vector2.z,
            vector1.x * vector2.y - vector1.y * vector2.x
        };
    }
    inline static Vector3 MultiplyVector3ByFloat(Vector3 vector, float val) {
        return {
            vector.x * val,
            vector.y * val,
            vector.z * val
        };
    }
    inline static  Vector3 DivideVector3ByFloat(Vector3 vector, float val) {
        return Vector3{
            vector.x / val,
            vector.y / val,
            vector.z / val
        };
    }
    inline Vector3 NormalizeVector3(Vector3 vec) {
        float magnitude = std::sqrt(vec.x * vec.x + vec.y * vec.y + vec.z * vec.z);
        return magnitude > 0 ? Vector3{ vec.x / magnitude, vec.y / magnitude, vec.z / magnitude } : Vector3{ 0, 0, 0 };
    }
    inline static float DotVector3(Vector3 a, Vector3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    inline static Vector3 RotateVector3(Vector3 vec, Vector3 axis, float angle) {
        Vector3 normalizedAxis = NormalizeVector3(axis);
        float cosTheta = std::cos(angle);
        float sinTheta = std::sin(angle);
        Vector3 term1 = scale(vec, cosTheta);
        Vector3 term2 = scale(CrossVector3(normalizedAxis, vec), sinTheta);
        Vector3 term3 = scale(normalizedAxis, dot(normalizedAxis, vec) * (1 - cosTheta));
        return AddVector3(AddVector3(term1, term2), term3);
    }
    inline static float Radians(float degrees) {
        return degrees * (3.14159f / 180);
    }
    inline static float Clamp(float val, float min, float max) {
        if (val > max) return max;
        if (val < min) return min;
        return val;
    }
    inline static uint64_t NextU64(uint64_t& s)
    {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return s;
    }
    inline static int GetRandomValueStreamlined(int min, int max)
    {
        thread_local std::mt19937 engine{ std::random_device{}() };
        std::uniform_int_distribution<int> dist(min, max);
        return dist(engine);
    }
    inline static int GetRandomValueStreamlinedFast(int min, int max)
    {
        thread_local uint64_t s = 0x9E3779B97F4A7C15ULL;

        uint64_t range = (uint64_t)(max - min) + 1;
        uint64_t r = NextU64(s);
        return min + (int)(r % range);
    }
}