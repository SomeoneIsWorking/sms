#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DStruct.hpp>
#include <JSystem/JMath.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#endif

J3DTransformInfo const j3dDefaultTransformInfo
    = { { 1.0f, 1.0f, 1.0f }, { 0, 0, 0 }, { 0.0f, 0.0f, 0.0f } };

// clang-format off
Mtx const j3dDefaultMtx = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	};
// clang-format on

f32 PSMulUnit01[2] = { 0.0f, -1.0f };

f32 Unit01[2] = { 0.0f, 1.0f };

#define qr0 0

#pragma push
#pragma fp_contract off
// TODO: several other functions in this TU use fp_contract,
// but this one for some reason doesn't?!
f32 J3DCalcZValue(MtxPtr m, Vec v)
{
	/* Nonmatching */
	return m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z + m[2][3];
}
#pragma pop

// Native reimplementation of the Metrowerks paired-single inverse-transpose
// kernel (a copy of PSMTXInverse that also transposes). Computes the
// inverse-transpose of the upper-left 3x3 of `src` and stores it into `dst`
// (a ROMtx, rows of 3). Used to transform normals. Returns false (and leaves
// `dst` untouched) when the matrix is singular.
bool J3DPSCalcInverseTranspose(MtxPtr src, ROMtxPtr dst)
{
	f32 m00 = src[0][0], m01 = src[0][1], m02 = src[0][2];
	f32 m10 = src[1][0], m11 = src[1][1], m12 = src[1][2];
	f32 m20 = src[2][0], m21 = src[2][1], m22 = src[2][2];

	f32 c00 =  (m11 * m22 - m12 * m21);
	f32 c01 = -(m10 * m22 - m12 * m20);
	f32 c02 =  (m10 * m21 - m11 * m20);

	f32 det = m00 * c00 + m01 * c01 + m02 * c02;
	if (det == 0.0f)
		return false;

	f32 inv = 1.0f / det;

	// inverse-transpose = cofactor / det
	dst[0][0] = c00 * inv;
	dst[0][1] = c01 * inv;
	dst[0][2] = c02 * inv;

	dst[1][0] = -(m01 * m22 - m02 * m21) * inv;
	dst[1][1] =  (m00 * m22 - m02 * m20) * inv;
	dst[1][2] = -(m00 * m21 - m01 * m20) * inv;

	dst[2][0] =  (m01 * m12 - m02 * m11) * inv;
	dst[2][1] = -(m00 * m12 - m02 * m10) * inv;
	dst[2][2] =  (m00 * m11 - m01 * m10) * inv;

	return true;
}

void J3DGetTranslateRotateMtx(const J3DTransformInfo& tx, Mtx dst)
{
#ifdef SMS_NATIVE_PLATFORM
	// One-shot sanity check of the JMath sin/cos table: cos(0) must be 1.
	// A zero/garbled table zeroes every J3D joint rotation while leaving
	// translation intact (the invisible-backdrop signature).
	{
		static int checked = 0;
		if (!checked) {
			checked = 1;
			fprintf(stderr, "[jmath-check] jmaSinShift=%u cos(0)=%f sin(0)=%f sin(0x4000)=%f table=%p\n",
			        jmaSinShift, JMASCos(0), JMASSin(0), JMASSin(0x4000), (void*)jmaSinTable);
		}
	}
#endif
	f32 sx = JMASSin(tx.mRotation.x), cx = JMASCos(tx.mRotation.x);
	f32 sy = JMASSin(tx.mRotation.y), cy = JMASCos(tx.mRotation.y);
	f32 sz = JMASSin(tx.mRotation.z), cz = JMASCos(tx.mRotation.z);

	dst[2][0] = -sy;
	dst[0][0] = cz * cy;
	dst[1][0] = sz * cy;
	dst[2][1] = cy * sx;
	dst[2][2] = cy * cx;

	f32 cxsz  = cx * sz;
	f32 sxcz  = sx * cz;
	dst[0][1] = sxcz * sy - cxsz;
	dst[1][2] = cxsz * sy - sxcz;

	f32 sxsz  = sx * sz;
	f32 cxcz  = cx * cz;
	dst[0][2] = cxcz * sy + sxsz;
	dst[1][1] = sxsz * sy + cxcz;

	dst[0][3] = tx.mTranslate.x;
	dst[1][3] = tx.mTranslate.y;
	dst[2][3] = tx.mTranslate.z;
}

void J3DGetTranslateRotateMtx(s16 rx, s16 ry, s16 rz, f32 tx, f32 ty, f32 tz,
                              Mtx dst)
{
	f32 sx = JMASSin(rx), cx = JMASCos(rx);
	f32 sy = JMASSin(ry), cy = JMASCos(ry);
	f32 sz = JMASSin(rz), cz = JMASCos(rz);

	dst[2][0] = -sy;
	dst[0][0] = cz * cy;
	dst[1][0] = sz * cy;
	dst[2][1] = cy * sx;
	dst[2][2] = cy * cx;

	f32 cxsz  = cx * sz;
	f32 sxcz  = sx * cz;
	dst[0][1] = sxcz * sy - cxsz;
	dst[1][2] = cxsz * sy - sxcz;

	f32 sxsz  = sx * sz;
	f32 cxcz  = cx * cz;
	dst[0][2] = cxcz * sy + sxsz;
	dst[1][1] = sxsz * sy + cxcz;

	dst[0][3] = tx;
	dst[1][3] = ty;
	dst[2][3] = tz;
}

void J3DGetTextureMtx(const J3DTextureSRTInfo& srt, Vec center, Mtx dst)
{
	f32 sr = JMASSin(srt.mRotation), cr = JMASCos(srt.mRotation);

	dst[0][0] = srt.mScaleX * cr;
	dst[0][1] = -srt.mScaleX * sr;
	dst[0][2] = (-srt.mScaleX * cr * center.x + srt.mScaleX * sr * center.y)
	            + center.x + srt.mTranslationX;

	dst[1][0] = srt.mScaleY * sr;
	dst[1][1] = srt.mScaleY * cr;
	dst[1][2] = (-srt.mScaleY * sr * center.x - srt.mScaleY * cr * center.y)
	            + center.y + srt.mTranslationY;

	dst[2][3] = 0.0f;
	dst[2][1] = 0.0f;
	dst[2][0] = 0.0f;
	dst[1][3] = 0.0f;
	dst[0][3] = 0.0f;
	dst[2][2] = 1.0f;
}

void J3DGetTextureMtxOld(const J3DTextureSRTInfo& srt, Vec center, Mtx dst)
{
	f32 sr = JMASSin(srt.mRotation), cr = JMASCos(srt.mRotation);

	dst[0][0] = srt.mScaleX * cr;
	dst[0][1] = -srt.mScaleX * sr;
	dst[0][3] = (-srt.mScaleX * cr * center.x + srt.mScaleX * sr * center.y)
	            + center.x + srt.mTranslationX;

	dst[1][0] = srt.mScaleY * sr;
	dst[1][1] = srt.mScaleY * cr;
	dst[1][3] = (-srt.mScaleY * sr * center.x - srt.mScaleY * cr * center.y)
	            + center.y + srt.mTranslationY;

	dst[2][3] = 0.0f;
	dst[2][1] = 0.0f;
	dst[2][0] = 0.0f;
	dst[1][2] = 0.0f;
	dst[0][2] = 0.0f;
	dst[2][2] = 1.0f;
}

void J3DGetTextureMtxMaya(const J3DTextureSRTInfo& srt, MtxPtr dst)
{
	dst[0][0] = srt.mScaleX * JMASCos(srt.mRotation);
	dst[0][1] = srt.mScaleY * JMASSin(srt.mRotation);
	dst[0][2]
	    = (srt.mTranslationX - 0.5f) * JMASCos(srt.mRotation)
	      - JMASSin(srt.mRotation) * ((srt.mTranslationY - 0.5f) + srt.mScaleY)
	      + 0.5f;
	dst[0][3] = 0.0f;

	dst[1][0] = -srt.mScaleX * JMASSin(srt.mRotation);
	dst[1][1] = srt.mScaleY * JMASCos(srt.mRotation);
	dst[1][2]
	    = -(srt.mTranslationX - 0.5f) * JMASSin(srt.mRotation)
	      - JMASCos(srt.mRotation) * ((srt.mTranslationY - 0.5f) + srt.mScaleY)
	      + 0.5f;
	dst[1][3] = 0.0f;

	dst[2][0] = 0.0f;
	dst[2][1] = 0.0f;
	dst[2][2] = 1.0f;
	dst[2][3] = 0.0f;
}

void J3DGetTextureMtxMayaOld(const J3DTextureSRTInfo& srt, Mtx dst)
{
	dst[0][0] = srt.mScaleX * JMASCos(srt.mRotation);
	dst[0][1] = srt.mScaleY * JMASSin(srt.mRotation);
	dst[0][2] = 0.0f;
	dst[0][3]
	    = (srt.mTranslationX - 0.5f) * JMASCos(srt.mRotation)
	      - JMASSin(srt.mRotation) * ((srt.mTranslationY - 0.5f) + srt.mScaleY)
	      + 0.5f;

	dst[1][0] = -srt.mScaleX * JMASSin(srt.mRotation);
	dst[1][1] = srt.mScaleY * JMASCos(srt.mRotation);
	dst[1][2] = 0.0f;
	dst[1][3]
	    = -(srt.mTranslationX - 0.5f) * JMASSin(srt.mRotation)
	      - JMASCos(srt.mRotation) * ((srt.mTranslationY - 0.5f) + srt.mScaleY)
	      + 0.5f;

	dst[2][0] = 0.0f;
	dst[2][1] = 0.0f;
	dst[2][2] = 1.0f;
	dst[2][3] = 0.0f;
}

// Native reimplementation: scale each row of the 3x3 (ROMtx layout, rows of 3)
// component-wise by the scale vector: mtx[i][j] *= scl[j].
void J3DScaleNrmMtx33(ROMtxPtr mtx, const Vec& scl)
{
	for (int i = 0; i < 3; i++) {
		mtx[i][0] *= scl.x;
		mtx[i][1] *= scl.y;
		mtx[i][2] *= scl.z;
	}
}

// Native reimplementation of the paired-single projection concat:
//   result(3x4) = param_1(3x4) x param_2(4x4)
// param_1's 4th column multiplies param_2's 4th row (full 4x4 second operand).
void J3DMtxProjConcat(MtxPtr param_1, MtxPtr param_2, MtxPtr result)
{
	for (int i = 0; i < 3; i++) {
		for (int k = 0; k < 4; k++) {
			result[i][k] = param_1[i][0] * param_2[0][k]
			             + param_1[i][1] * param_2[1][k]
			             + param_1[i][2] * param_2[2][k]
			             + param_1[i][3] * param_2[3][k];
		}
	}
}

// Native reimplementation: copy a 3x3 matrix (ROMtx, 9 contiguous floats).
void J3DPSMtx33Copy(ROMtxPtr src, ROMtxPtr dst)
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			dst[i][j] = src[i][j];
}

// Native reimplementation: copy the upper-left 3x3 of a 3x4 matrix (`src`,
// MtxPtr) into a 3x3 ROMtx (`dst`).
void J3DPSMtx33CopyFrom34(MtxPtr src, ROMtxPtr dst)
{
	for (int i = 0; i < 3; i++)
		for (int j = 0; j < 3; j++)
			dst[i][j] = src[i][j];
}

// Native reimplementation: copy an array of `size` 3x4 matrices (each 12 floats)
// from `src` to `dst`.
void J3DPSMtxArrayCopy(MtxPtr src, MtxPtr dst, u32 size)
{
	for (u32 m = 0; m < size; m++) {
		for (int i = 0; i < 3; i++)
			for (int j = 0; j < 4; j++)
				dst[m * 3 + i][j] = src[m * 3 + i][j];
	}
}

// Native reimplementation: for each k, dst[k] = mat1 x mat2[param_3[k]], an
// affine 3x4 concat where mat1 is a single 3x4 matrix, mat2 is an array of 3x4
// matrices indexed by the u16 index array param_3 (one index per output).
void J3DMTXConcatArrayIndexedSrc(const float (*mat1)[4],
                                 const float (*mat2)[3][4],
                                 const u16* param_3, Mtx* dst, u32 count)
{
	for (u32 k = 0; k < count; k++) {
		const float (*src)[4] = mat2[param_3[k]];
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; j++) {
				f32 v = mat1[i][0] * src[0][j]
				      + mat1[i][1] * src[1][j]
				      + mat1[i][2] * src[2][j];
				if (j == 3)
					v += mat1[i][3];
				dst[k][i][j] = v;
			}
		}
	}
}

// Native reimplementation: concatenate the single 3x4 affine matrix `fst` with
// each of the `size` matrices in the `snd` array, writing dst[k] = fst x snd[k].
// Each matrix is a 3x4 affine (implicit 4th row [0,0,0,1]).
void J3DPSMtxArrayConcat(Mtx fst, Mtx snd, Mtx dst, u32 size)
{
	MtxPtr s = (MtxPtr)snd;
	MtxPtr d = (MtxPtr)dst;
	for (u32 m = 0; m < size; m++) {
		MtxPtr sm = s + m * 3;
		MtxPtr dm = d + m * 3;
		for (int i = 0; i < 3; i++) {
			for (int j = 0; j < 4; j++) {
				f32 v = fst[i][0] * sm[0][j]
				      + fst[i][1] * sm[1][j]
				      + fst[i][2] * sm[2][j];
				if (j == 3)
					v += fst[i][3];
				dm[i][j] = v;
			}
		}
	}
}
