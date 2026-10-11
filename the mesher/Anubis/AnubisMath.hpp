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
    inline int RandRange(int max, int min) {


        int range = max - min + 1;
        int num = std::rand() % range + min;

        return num;
    }
    inline int mod(int x, int y) {
        return x - y * std::floor(x / y);
    }
    inline float Length(Math::Vector3 pos) {
        return std::sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
    }Matrix MatrixPerspective(double fovY, double aspect, double nearPlane, double farPlane)
    {
        Matrix result = { 0 };

        double top = nearPlane * tan(fovY * 0.5);
        double bottom = -top;
        double right = top * aspect;
        double left = -right;

        // MatrixFrustum(-right, right, -top, top, near, far);
        float rl = (float)(right - left);
        float tb = (float)(top - bottom);
        float fn = (float)(farPlane - nearPlane);

        result.m0 = ((float)nearPlane * 2.0f) / rl;
        result.m5 = ((float)nearPlane * 2.0f) / tb;
        result.m8 = ((float)right + (float)left) / rl;
        result.m9 = ((float)top + (float)bottom) / tb;
        result.m10 = -((float)farPlane + (float)nearPlane) / fn;
        result.m11 = -1.0f;
        result.m14 = -((float)farPlane * (float)nearPlane * 2.0f) / fn;

        return result;
    }
    Matrix MatrixLookAt(Vector3 eye, Vector3 target, Vector3 up)
    {
        Matrix result = { 0 };

        float length = 0.0f;
        float ilength = 0.0f;

        // Vector3Subtract(eye, target)
        Vector3 vz = { eye.x - target.x, eye.y - target.y, eye.z - target.z };

        // Vector3Normalize(vz)
        Vector3 v = vz;
        length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        if (length == 0.0f) length = 1.0f;
        ilength = 1.0f / length;
        vz.x *= ilength;
        vz.y *= ilength;
        vz.z *= ilength;

        // Vector3CrossProduct(up, vz)
        Vector3 vx = { up.y * vz.z - up.z * vz.y, up.z * vz.x - up.x * vz.z, up.x * vz.y - up.y * vz.x };

        // Vector3Normalize(x)
        v = vx;
        length = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
        if (length == 0.0f) length = 1.0f;
        ilength = 1.0f / length;
        vx.x *= ilength;
        vx.y *= ilength;
        vx.z *= ilength;

        // Vector3CrossProduct(vz, vx)
        Vector3 vy = { vz.y * vx.z - vz.z * vx.y, vz.z * vx.x - vz.x * vx.z, vz.x * vx.y - vz.y * vx.x };

        result.m0 = vx.x;
        result.m1 = vy.x;
        result.m2 = vz.x;
        result.m3 = 0.0f;
        result.m4 = vx.y;
        result.m5 = vy.y;
        result.m6 = vz.y;
        result.m7 = 0.0f;
        result.m8 = vx.z;
        result.m9 = vy.z;
        result.m10 = vz.z;
        result.m11 = 0.0f;
        result.m12 = -(vx.x * eye.x + vx.y * eye.y + vx.z * eye.z);   // Vector3DotProduct(vx, eye)
        result.m13 = -(vy.x * eye.x + vy.y * eye.y + vy.z * eye.z);   // Vector3DotProduct(vy, eye)
        result.m14 = -(vz.x * eye.x + vz.y * eye.y + vz.z * eye.z);   // Vector3DotProduct(vz, eye)
        result.m15 = 1.0f;

        return result;
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