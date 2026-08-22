#ifndef J3DTRANSFORM_H
#define J3DTRANSFORM_H

#include <dolphin/mtx.h>

struct J3DTextureSRTInfo;

struct J3DTransformInfo {
	/* 0x00 */ Vec mScale;
	/* 0x0C */ S16Vec mRotation;
	/* 0x14 */ Vec mTranslate;

	inline J3DTransformInfo& operator=(const J3DTransformInfo& b)
	{
		mScale.x     = b.mScale.x;
		mScale.y     = b.mScale.y;
		mScale.z     = b.mScale.z;
		mRotation.x  = b.mRotation.x;
		mRotation.y  = b.mRotation.y;
		mRotation.z  = b.mRotation.z;
		mTranslate.x = b.mTranslate.x;
		mTranslate.y = b.mTranslate.y;
		mTranslate.z = b.mTranslate.z;
		return *this;
	}
}; // Size: 0x20

extern J3DTransformInfo const j3dDefaultTransformInfo;
extern Vec const j3dDefaultScale;
extern Mtx const j3dDefaultMtx;
extern f32 PSMulUnit01[];

f32 J3DCalcZValue(MtxPtr m, Vec v);
bool J3DPSCalcInverseTranspose(MtxPtr, ROMtxPtr);
void J3DGetTranslateRotateMtx(J3DTransformInfo const&, Mtx);
void J3DGetTranslateRotateMtx(s16, s16, s16, f32, f32, f32, Mtx);
void J3DGetTextureMtx(const J3DTextureSRTInfo&, Vec, MtxPtr);
void J3DGetTextureMtxOld(const J3DTextureSRTInfo&, Vec, MtxPtr);
void J3DGetTextureMtxMaya(const J3DTextureSRTInfo&, MtxPtr);
void J3DGetTextureMtxMayaOld(const J3DTextureSRTInfo&, MtxPtr);
void J3DScaleNrmMtx33(ROMtxPtr, const Vec&);
void J3DMtxProjConcat(MtxPtr, MtxPtr, MtxPtr);
void J3DPSMtx33Copy(ROMtxPtr src, ROMtxPtr dst);
void J3DPSMtx33CopyFrom34(MtxPtr src, ROMtxPtr dst);
void J3DPSMtxArrayCopy(MtxPtr, MtxPtr, u32);
void J3DMTXConcatArrayIndexedSrc(const float (*)[4], const float (*)[3][4],
                                 const u16*, float (*)[3][4], u32);
void J3DPSMtxArrayConcat(Mtx, Mtx, Mtx, u32);

// TODO: is this used in sms? Probably, but totally inlined

// regalloc issues
// SMS_NATIVE_PLATFORM builds as C++17, where the legacy register keyword is
// invalid.
inline void J3DPSMulMtxVec(MtxPtr mtx, Vec* vec, Vec* dst)
{
	f32 fr12;
	f32 fr11;
	f32 fr10;
	f32 fr9;
	f32 fr8;
	f32 fr6;
	f32 fra6;
	f32 fr5;
	f32 fra5;
	f32 fra4;
	f32 fr4;
	f32 fr3;
	f32 fr2;
	f32 fra2;
	f32 fr01;
	f32 fr00;
#ifdef __MWERKS__ // clang-format off
    asm {
        psq_l f0, 0(vec), 0, 0
        psq_l f2, 0(mtx), 0, 0
        psq_l f1, 8(vec), 1, 0
        ps_mul f4, f2, f0
        psq_l f3, 8(mtx), 0, 0
        ps_madd f5, f3, f1, f4
        psq_l f8, 16(mtx), 0, 0
        ps_sum0 f6, f5, f6, f5
        psq_l f9, 24(mtx), 0, 0
        ps_mul f10, f8, f0
        psq_st f6, 0(dst), 1, 0
        ps_madd f11, f9, f1, f10
        psq_l f2, 32(mtx), 0, 0
        ps_sum0 f12, f11, f12, f11
        psq_l f3, 40(mtx), 0, 0
        ps_mul f4, f2, f0
        psq_st f12, 4(dst), 1, 0
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_st f6, 8(dst), 1, 0
    }
#endif // clang-format on
}

// regalloc issues
inline void J3DPSMulMtxVec(MtxPtr mtx, S16Vec* vec, S16Vec* dst)
{
	f32 fr12;
	f32 fr11;
	f32 fr10;
	f32 fr9;
	f32 fr8;
	f32 fr6;
	f32 fra6;
	f32 fr5;
	f32 fra5;
	f32 fra4;
	f32 fr4;
	f32 fr3;
	f32 fr2;
	f32 fra2;
	f32 fr01;
	f32 fr00;
#ifdef __MWERKS__ // clang-format off
    asm {
        psq_l f0, 0(vec), 0, 7
        psq_l f2, 0(mtx), 0, 0
        psq_l f1, 4(vec), 1, 7
        ps_mul f4, f2, f0
        psq_l f3, 8(mtx), 0, 0
        ps_madd f5, f3, f1, f4
        psq_l f8, 16(mtx), 0, 0
        ps_sum0 f6, f5, f6, f5
        psq_l f9, 24(mtx), 0, 0
        ps_mul f10, f8, f0
        psq_st f6, 0(dst), 1, 7
        ps_madd f11, f9, f1, f10
        psq_l f2, 32(mtx), 0, 0
        ps_sum0 f12, f11, f12, f11
        psq_l f3, 40(mtx), 0, 0
        ps_mul f4, f2, f0
        psq_st f12, 2(dst), 1, 7
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_st f6, 4(dst), 1, 7
    }
#endif // clang-format on
}

// regalloc issues
inline void J3DPSMulMtxVec(ROMtxPtr mtx, Vec* vec, Vec* dst)
{
	f32* punit;
	f32 unit;
	f32 fr12;
	f32 fr11;
	f32 fr10;
	f32 fr9;
	f32 fr8;
	f32 fr6;
	f32 fr5;
	f32 fr4;
	f32 fr3;
	f32 fr2;
	f32 fr01;
	f32 fr00;
#ifdef __MWERKS__ // clang-format off
    asm {
        addis r6, r0, PSMulUnit01@ha
        psq_l f0, 0(vec), 0, 0
        addi r6, r6, PSMulUnit01@l
        psq_l f2, 0(mtx), 0, 0
        psq_l f13, 0(r6), 0, 0
        psq_l f1, 8(vec), 1, 0
        ps_add f1, f13, f1
        psq_l f3, 8(mtx), 1, 0
        ps_mul f4, f2, f0
        psq_l f8, 12(mtx), 0, 0
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_l f9, 20(mtx), 1, 0
        ps_mul f10, f8, f0
        psq_st f6, 0(dst), 1, 0
        ps_madd f11, f9, f1, f10
        psq_l f2, 24(mtx), 0, 0
        ps_sum0 f12, f11, f12, f11
        psq_l f3, 32(mtx), 1, 0
        ps_mul f4, f2, f0
        psq_st f12, 4(dst), 1, 0
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_st f6, 8(dst), 1, 0
    }
#endif // clang-format on
}

// regalloc issues
inline void J3DPSMulMtxVec(ROMtxPtr mtx, S16Vec* vec, S16Vec* dst)
{
	f32* punit;
	f32 unit;
	f32 fr6;
	f32 fr5;
	f32 fr4;
	f32 fr3;
	f32 fr2;
	f32 fr01;
	f32 fr00;
#ifdef __MWERKS__ // clang-format off
    asm {
        addis r6, r0, PSMulUnit01@ha
        psq_l f0, 0(vec), 0, 7
        addi r6, r6, PSMulUnit01@l
        psq_l f2, 0(mtx), 0, 0
        psq_l f13, 0(r6), 0, 0
        psq_l f1, 4(vec), 1, 7
        ps_add f1, f13, f1
        psq_l f3, 8(mtx), 1, 0
        ps_mul f4, f2, f0
        psq_l f8, 12(mtx), 0, 0
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_l f9, 20(mtx), 1, 0
        ps_mul f10, f8, f0
        psq_st f6, 0(dst), 1, 7
        ps_madd f11, f9, f1, f10
        psq_l f2, 24(mtx), 0, 0
        ps_sum0 f12, f11, f12, f11
        psq_l f3, 32(mtx), 1, 0
        ps_mul f4, f2, f0
        psq_st f12, 2(dst), 1, 7
        ps_madd f5, f3, f1, f4
        ps_sum0 f6, f5, f6, f5
        psq_st f6, 4(dst), 1, 7
    }
#endif // clang-format on
}

#endif
