#pragma once
// Minimal SkColorMatrix stub for native tests (no Skia dependency)
#include <cstring>
#include <cmath>

class SkColorMatrix {
public:
    float fMat[20];

    SkColorMatrix() { std::memset(fMat, 0, sizeof(fMat)); }

    void setRowMajor(const float src[20]) {
        std::memcpy(fMat, src, sizeof(fMat));
    }

    void setSaturation(float amount) {
        float r_lum = 0.2126f;
        float g_lum = 0.7152f;
        float b_lum = 0.0722f;
        float i_amount = 1.0f - amount;
        fMat[0]  = i_amount * r_lum + amount;
        fMat[1]  = i_amount * r_lum;
        fMat[2]  = i_amount * r_lum;
        fMat[3]  = 0; fMat[4]  = 0;
        fMat[5]  = i_amount * g_lum;
        fMat[6]  = i_amount * g_lum + amount;
        fMat[7]  = i_amount * g_lum;
        fMat[8]  = 0; fMat[9]  = 0;
        fMat[10] = i_amount * b_lum;
        fMat[11] = i_amount * b_lum;
        fMat[12] = i_amount * b_lum + amount;
        fMat[13] = 0; fMat[14] = 0;
        fMat[15] = 0; fMat[16] = 0; fMat[17] = 0; fMat[18] = 1; fMat[19] = 0;
    }
};
