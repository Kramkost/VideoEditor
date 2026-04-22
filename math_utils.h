#pragma once
#include <cmath>
#include "logger.h"

struct TransformMatrix {
    float m[3][3];

    TransformMatrix() {
        m[0][0] = 1.0f; m[0][1] = 0.0f; m[0][2] = 0.0f;
        m[1][0] = 0.0f; m[1][1] = 1.0f; m[1][2] = 0.0f;
        m[2][0] = 0.0f; m[2][1] = 0.0f; m[2][2] = 1.0f;
    }


    TransformMatrix operator*(const TransformMatrix& other) const {
        TransformMatrix res;
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 3; ++col) {
                res.m[row][col] = m[row][0] * other.m[0][col] + 
                                  m[row][1] * other.m[1][col] + 
                                  m[row][2] * other.m[2][col];
            }
        }
        return res;
    }

    // Generate local matrix from TRS (Translate, Rotate, Scale)
    static TransformMatrix CreateTRS(float x, float y, float angleDeg, float scale) {
        TransformMatrix mat;
        float rad = angleDeg * (3.1415926535f / 180.0f);
        float c = std::cos(rad);
        float s = std::sin(rad);

        mat.m[0][0] = scale * c;  mat.m[0][1] = scale * -s; mat.m[0][2] = x;
        mat.m[1][0] = scale * s;  mat.m[1][1] = scale * c;  mat.m[1][2] = y;
        return mat;
    }

    
    void Decompose(float& outX, float& outY, float& outAngleDeg, float& outScale) const {
        outX = m[0][2];
        outY = m[1][2];
 
        outScale = std::sqrt(m[0][0] * m[0][0] + m[1][0] * m[1][0]); 
  
        outAngleDeg = std::atan2(m[1][0], m[0][0]) * (180.0f / 3.1415926535f);
    }
};