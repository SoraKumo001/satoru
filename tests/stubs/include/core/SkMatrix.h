#pragma once
// Minimal SkMatrix stub for native tests (no Skia dependency)

struct SkMatrix {
    float fMat[9]{1,0,0, 0,1,0, 0,0,1};  // identity

    void setAll(float scaleX, float skewX, float transX,
                float skewY, float scaleY, float transY,
                float pers0, float pers1, float pers2) {
        fMat[0] = scaleX; fMat[1] = skewX;  fMat[2] = transX;
        fMat[3] = skewY;  fMat[4] = scaleY; fMat[5] = transY;
        fMat[6] = pers0;  fMat[7] = pers1;  fMat[8] = pers2;
    }

    void setScaleTranslate(float sx, float sy, float tx, float ty) {
        fMat[0] = sx;  fMat[1] = 0;   fMat[2] = tx;
        fMat[3] = 0;   fMat[4] = sy;  fMat[5] = ty;
        fMat[6] = 0;   fMat[7] = 0;   fMat[8] = 1;
    }

    void setTranslate(float dx, float dy) {
        fMat[0] = 1;  fMat[1] = 0;  fMat[2] = dx;
        fMat[3] = 0;  fMat[4] = 1;  fMat[5] = dy;
        fMat[6] = 0;  fMat[7] = 0;  fMat[8] = 1;
    }

    void preTranslate(float dx, float dy) {
        fMat[2] += dx;
        fMat[5] += dy;
    }

    bool operator==(const SkMatrix& o) const {
        for (int i = 0; i < 9; ++i)
            if (fMat[i] != o.fMat[i]) return false;
        return true;
    }
    bool operator!=(const SkMatrix& o) const { return !(*this == o); }
};
