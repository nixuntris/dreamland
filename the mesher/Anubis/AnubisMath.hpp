#pragma once
#include "math.h"
namespace Math {

    typedef struct Matrix {
        float m0, m4, m8, m12;  // Matrix first row (4 components)
        float m1, m5, m9, m13;  // Matrix second row (4 components)
        float m2, m6, m10, m14; // Matrix third row (4 components)
        float m3, m7, m11, m15; // Matrix fourth row (4 components)
    } Matrix;

    struct IVector2 {
        int x;
        int y;
    };
    struct IVector3 {
        int x;
        int y;
        int z;
    };
    struct Vector2 {
        double x;
        double y;
    };
    struct Vector3 {
        double x;
        double y;
        double z;
    };
    struct Vector4 {
        double x;
        double y;
        double z;
        double w;
    };
    inline Vector3 IVec3ToVec3(IVector3 vec) {
        return { float(vec.x), float(vec.y), float(vec.z) };
    }
    inline IVector3 Vec3ToIVec3(Vector3 vec) {
        return { int(vec.x), int(vec.y), int(vec.z) };
    }
    inline double dot(Vector3 vec1, Vector3 vec2) {
        return vec1.x * vec2.x + vec1.y * vec2.y + vec1.z * vec2.z;
    }
    inline Vector3 scale(Vector3 vec1, float scale) {
        return { vec1.x * scale, vec1.y * scale, vec1.z * scale };
    }
    inline Vector2 DivideVector2(Vector2 vec1, Vector2 vec2) {
        return Vector2{ vec1.x / vec2.x,vec1.y / vec2.y };
    }
    inline Vector2 MultiplyVector2(Vector2 vec1, Vector2 vec2) {
        return Vector2{ vec1.x * vec2.x,vec1.y * vec2.y };
    }
    inline Vector2 AddVector2(Vector2 vec1, Vector2 vec2) {
        return Vector2{ vec1.x + vec2.x,vec1.y + vec2.y };
    }
    inline Vector2 SubtractVector2(Vector2 vec1, Vector2 vec2) {
        return Vector2{ vec1.x - vec2.x,vec1.y - vec2.y };
    }
    inline Vector3 DivideVector3(Vector3 vec1, Vector3 vec2) {
        return Vector3{ vec1.x / vec2.x,vec1.y / vec2.y, vec1.z / vec2.z };
    }
    inline Vector3 MultiplyVector3(Vector3 vec1, Vector3 vec2) {
        return Vector3{ vec1.x * vec2.x,vec1.y * vec2.y, vec1.z * vec2.z };
    }
    inline Vector3 AddVector3(Vector3 vec1, Vector3 vec2) {
        return Vector3{ vec1.x + vec2.x,vec1.y + vec2.y , vec1.z + vec2.z };
    }
    inline Vector3 SubtractVector3(Vector3 vec1, Vector3 vec2) {
        return Vector3{ vec1.x - vec2.x,vec1.y - vec2.y, vec1.z - vec2.z };
    }
    inline Vector3 CrossVector3(Vector3 vector1, Vector3 vector2) {
        return Vector3{
            vector1.y * vector2.z - vector1.z * vector2.y,
            vector1.z * vector2.x - vector1.x * vector2.z,
            vector1.x * vector2.y - vector1.y * vector2.x
        };
    }
    inline Vector3 MultiplyVector3ByFloat(Vector3 vector, float val) {
        return {
            vector.x * val,
            vector.y * val,
            vector.z * val
        };
    }
    inline Vector3 DivideVector3ByFloat(Vector3 vector, float val) {
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
    inline float DotVector3(Vector3 a, Vector3 b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    inline Vector3 RotateVector3(Vector3 vec, Vector3 axis, float angle) {
        Vector3 normalizedAxis = NormalizeVector3(axis);
        float cosTheta = std::cos(angle);
        float sinTheta = std::sin(angle);
        Vector3 term1 = scale(vec, cosTheta);
        Vector3 term2 = scale(CrossVector3(normalizedAxis, vec), sinTheta);
        Vector3 term3 = scale(normalizedAxis, dot(normalizedAxis, vec) * (1 - cosTheta));
        return AddVector3(AddVector3(term1, term2), term3);
    }
    inline float Radians(float degrees) {
        return degrees * (3.14159f / 180);
    }
    inline float Clamp(float val, float min, float max) {
        if (val > max) return max;
        if (val < min) return min;
        return val;
    }
    inline int GetRandomValueStreamlined(int min, int max)
    {
        int value = 0;

        int range = (max - min) + 1;

        const unsigned long c = (unsigned long)RAND_MAX + 1UL;  // number of possible rand() results
        unsigned long m = (unsigned long)range;           // size of the target interval
        unsigned long t = c - (c % m);                    // largest multiple of m <= c
        unsigned long r = 0;

        for (;;)
        {
            r = (unsigned long)rand();
            if (r < t) break;
        }

        value = min + (int)(r % m);

        return value;
    }
}