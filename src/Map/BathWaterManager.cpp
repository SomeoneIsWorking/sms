#include <Map/BathWaterManager.hpp>
#include <JSystem/ResTIMG.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/Components/J3DTexMtx.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DVertex.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTevs.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <System/Resolution.hpp>
#include <MarioUtil/MtxUtil.hpp>
#include <MarioUtil/ScreenUtil.hpp>
#include <MoveBG/MapObjCorona.hpp>
#include <Player/MarioAccess.hpp>
#include <Camera/Camera.hpp>
#include <dolphin/mtx.h>
#include <dolphin/gx.h>
#include <dolphin/gd/GDBase.h>
#include <dolphin/gx/GXCommandList.h>

#include <MSound/MSound.hpp>

// rogue includes needed for matching sinit & bss
#include <MSound/MSSetSound.hpp>
#include <MSound/MSoundBGM.hpp>
#include <M3DUtil/InfectiousStrings.hpp>

// NOTE: the main tragedy of this file is that it looks like a bunch of classes
// were defined right inside of the cpp file and all their methods were defined
// in-line, and furthermore, some methods were defined in-line inside of a
// NESTED class, in which case MWCC **always** inlines the contents independent
// of any heuristics. Hence, a LOT here is dumb inlining guesswork.
// Most inlines are fabricated.

TBathWaterParams::TBathWaterParams(const char* path)
    : TParams(path)
    , PARAM_INIT(suppliesDrops, 1)
    , PARAM_INIT(bathtubGravity, 1)
    , PARAM_INIT(intersects, 1)
    , PARAM_INIT(isVisible, 1)
    , PARAM_INIT(checksMario, 1)
    , PARAM_INIT(numDrops, 120)
    , PARAM_INIT(dropRadius, 300.0f)
    , PARAM_INIT(texScale, 3.0f)
    , PARAM_INIT(hitScale, 5.0f)
    , PARAM_INIT(modelScale, 1.5f)
    , PARAM_INIT(modelScale2, 1.0f)
    , PARAM_INIT(modelScaleY, 1.0f)
    , PARAM_INIT(gravity, 18.0f)
    , PARAM_INIT(bounceY, 0.05f)
    , PARAM_INIT(bounceXZ, 0.5f)
    , PARAM_INIT(damp, 0.985f)
    , PARAM_INIT(jump, 65.0f)
    , PARAM_INIT(overGravity, 0.0f)
    , PARAM_INIT(emitVel, 20.0f)
    , PARAM_INIT(lifeTime, 0)
{
	TParams::load(mPrmPath);
}

TBathWaterGlobalParams::TBathWaterGlobalParams()
    : TParams("/MapObj/bathwaterglobal.prm")
    , PARAM_INIT(regR, 0)
    , PARAM_INIT(regG, 0)
    , PARAM_INIT(regB, 0)
    , PARAM_INIT(regA, 255)
    , PARAM_INIT(kRegR, 144)
    , PARAM_INIT(kRegG, 24)
    , PARAM_INIT(kRegB, 0)
    , PARAM_INIT(kRegA, 255)
    , PARAM_INIT(polygonR, 255)
    , PARAM_INIT(polygonG, 255)
    , PARAM_INIT(polygonB, 151)
    , PARAM_INIT(indTexScale, 1.5f)
    , PARAM_INIT(showsCap, 1)
    , PARAM_INIT(bendsNormal, 0)
    , PARAM_INIT(showsMist, 0)
    , PARAM_INIT(clearsAlpha, 1)
    , PARAM_INIT(alpha, 200)
    , PARAM_INIT(scrolls, 1)
    , PARAM_INIT(displaysMesh, 0)
    , PARAM_INIT(mode, 0)
    , PARAM_INIT(mask, 1)
    , PARAM_INIT(indirectScale, -3)
    , PARAM_INIT(scrollSpan, 60)
    , PARAM_INIT(meshTexWidth, 80)
    , PARAM_INIT(envMapScale, 0.6f)
    , PARAM_INIT(capHeight, 150.0f)
    , PARAM_INIT(meshWidth, 7000.0f)
{
	TParams::load(mPrmPath);
}

static void minmax_set(JGeometry::TBox3<f32>&, const JGeometry::TVec3<f32>&) { }

class TBathWater : public THitActor {
public:
	class TDrop;

	TBathWater()
	    : THitActor("HitActor")
	    , unk68(0)
	{
		unk70 = 500;
		unk88 = new TDrop[unk70];
		unk8C = 0;
	}

	void initialize(TBathWaterParams* params, const TBathtubData& data)
	{
		unk8C = params;
		unk74 = params->numDrops.get();

		int i = 0;
		for (TBathWater::TDrop *drop = unk88, *end = unk88 + unk70; drop < end;
		     ++drop) {
			drop->reset(data.getPos(i++, unk70, unk8C->dropRadius.get()),
			            unk68.get_float01());
		}

		initHitActor(0x4000025B, 1, 0x80000000, unk8C->dropRadius.get(),
		             unk8C->dropRadius.get() * 2.0f, 0.0f, 0.0f);
		onHitFlag(HIT_FLAG_NO_COLLISION);
		onHitFlag(HIT_FLAG_CANNOT_GET_HIT);
		unk78.set(0.0f, 0.0f, 0.0f);
		unk84 = 0.0f;
	}

	void addDrop(const JGeometry::TVec3<f32>& param_1, f32 param_2)
	{
		if (unk74 < unk8C->numDrops.get()) {

			unk88[unk74].reset(param_1, unk68.get_float01());
			unk88[unk74].unkC.y = param_2;

			// Main hint at inlining + classes in cpp being employed:
			// otherwise you couldn't hope to have this on line 655.
			OSReport("BathWaterManager.cpp(%d): ...\n", 655, unk74++);
		}
	}

	bool eraseDrop(TDrop* drop)
	{
		int idx = drop - unk88;
		if (idx < unk74) {
			unk74--;
			if (idx < unk74)
				*drop = unk88[unk74];
			return true;
		} else {
			return false;
		}
	}

	bool tryHitMario(THitActor* mario)
	{
		TBathWaterParams* params = unk8C;
		f32 rr    = params->dropRadius.get() * params->hitScale.get();
		f32 upper = mario->mDamageHeight + rr;
		f32 radH  = rr + mario->mDamageRadius;
		f32 radH2 = radH * radH;
		f32 mx    = mario->mPosition.x;
		f32 my    = mario->mPosition.y;
		f32 mz    = mario->mPosition.z;

		for (TBathWater::TDrop* drop = unk88; drop < unk88 + unk74; ++drop) {
			f32 dy = drop->unk0.y - my;
			if (!(dy > upper) && !(dy < -rr)) {
				f32 dx = mx - drop->unk0.x;
				f32 dz = mz - drop->unk0.z;
				if (dx * dx + dz * dz < radH2) {
					setAttackRadius(unk8C->dropRadius.get());
					setAttackRadius(unk8C->dropRadius.get() * 2.0f);
					mPosition.set(drop->unk0);
					mPosition.y -= unk8C->dropRadius.get();
					mario->receiveMessage(this, HIT_MESSAGE_UNKA);
					return true;
				}
			}
		}
		return false;
	}

	bool tryHitMario2(THitActor* mario, const TBathtubData& data)
	{
		f32 mx  = mario->mPosition.x;
		f32 my  = mario->mPosition.y;
		f32 mz  = mario->mPosition.z;
		f32 mdh = mario->mDamageHeight;

		JGeometry::TVec3<f32> capCenter = data.getThing();

		f32 rad = JGeometry::TUtil<f32>::sqrt(data.unk3C * data.unk3C
		                                      - data.unk44 * data.unk44);
		bool hit;
		if (capCenter.y < my)
			return false;

		f32 h = data.unk3C - data.unk44;
		if (my + mdh < capCenter.y - h)
			return false;

		f32 dx = mx - capCenter.x;
		f32 dz = mz - capCenter.z;
		if (dx * dx + dz * dz >= rad * rad)
			return false;

		setAttackRadius(rad);
		mPosition.set(data.getThing());
		mPosition.y -= h;
		setAttackHeight(h);
		mario->receiveMessage(this, HIT_MESSAGE_UNKA);
		return true;
	}

	class TDrop {
	public:
		TDrop() { }

		void reset(const JGeometry::TVec3<f32>& param_1, f32 param_2)
		{
			unk48 = param_2;
			unk0.set(param_1);
			unkC.zero();
			unk18.i.zero();
			unk18.f.zero();
			unk30.i.zero();
			unk30.f.zero();
			unk4C = 0;
		}

		void doThing(f32 damp)
		{
			unk0.add(unk18.i);
			unk0.add(unk18.f);
			unkC.scale(damp);
			unkC.add(unk30.i);
			unkC.add(unk30.f);
		}

		void calcBathtub(const TBathtubData& data, f32 radius,
		                 const JGeometry::TVec3<f32>& grav1,
		                 const JGeometry::TVec3<f32>& grav2, int& count,
		                 JGeometry::TVec3<f32>& accum)
		{
			JGeometry::TVec3<f32> m(data.unk18.at(1, 0), data.unk18.at(1, 1),
			                        data.unk18.at(1, 2));
			JGeometry::TVec3<f32> delta;
			delta.sub(unk0, data.mPos);
			f32 outerR = data.unk40 + radius;
			f32 innerR = data.unk3C - radius;
			f32 distSq = delta.squared();
			f32 proj   = m.dot(delta);

			if (distSq <= outerR * outerR) {
				if (proj < 0.0f) {
					if (distSq >= innerR * innerR) {
						f32 dist = JGeometry::TUtil<f32>::sqrt(distSq);
						f32 pen  = dist - innerR;
						f32 inv  = -1.0f / dist;
						JGeometry::TVec3<f32> n;
						n.scale(inv, delta);
						JGeometry::TVec3<f32> point;
						point.scale(pen, n);
						unk18.extend(point);

						f32 c = -n.dot(unkC);
						if (c < 0.0f)
							c = 0.0f;

						JGeometry::TVec3<f32> point2;
						point2.scale(c, n);
						point2 += data.unk58;
						unk30.extend(point2);
					} else {
						f32 f = radius + (data.mPos.y - data.unk44);
						if (unk0.y < f) {
							f32 pen = f - unk0.y;
							JGeometry::TVec3<f32> point(0.0f, pen, 0.0f);
							unk18.extend(point);
							JGeometry::TVec3<f32> point2(0.0f, -1.0f * unkC.y,
							                             0.0f);
							point2 += data.unk58;
							unk30.extend(point2);
						} else {
							count++;
							accum.add(unk0);
						}
					}
				} else {
					count++;
					accum.add(unk0);
				}
				unk30.extend(grav1);
			} else {
				if (proj > 0.0f && proj < radius + data.unk48
				    && distSq > innerR * innerR && distSq < outerR * outerR) {
					JGeometry::TVec3<f32> point;
					point.scale((radius + data.unk48) - proj, m);
					unk18.extend(point);

					JGeometry::TVec3<f32> r;
					r.scale(1.5f * -m.dot(unkC), m);
					JGeometry::TVec3<f32> thing;
					thing.set(delta);
					thing.setLength(0.01f * radius);
					r.add(thing);
					unk30.extend(r);
					unk30.extend(grav2);
				} else {
					unk30.extend(grav2);
					count++;
					accum.add(unk0);
				}
			}
		}

		// TODO: break up into pieces and move into TBathWater.
		// Placed here ONLY to force-inline it abusing an MWCC bug.
		static void calcWaterModel(TBathWater* bw, const TBathtubData& data)
		{
			f32 gravity = bw->unk8C->gravity.get();
			JGeometry::TVec3<f32> gravVec1;
			gravVec1.scale(-gravity,
			               data.getGravityDir(bw->unk8C->overGravity.get()));
			JGeometry::TVec3<f32> gravVec2(0.0f, -gravity, 0.0f);

			TDrop* end2    = bw->unk88 + bw->unk74;
			f32 dropRadius = bw->unk8C->dropRadius.get();
			int count      = 0;

			JGeometry::TVec3<f32> accum(0.0f, 0.0f, 0.0f);

			if (data.unk18.at(1, 1) > 0.0f) {
				for (TDrop* drop = bw->unk88; drop < end2; ++drop) {
					drop->unk0.add(drop->unkC);
					drop->unk18.i.zero();
					drop->unk18.f.zero();
					drop->unk30.i.zero();
					drop->unk30.f.zero();
					drop->calcBathtub(data, dropRadius, gravVec1, gravVec2,
					                  count, accum);
				}
			} else {
				for (TDrop* drop = bw->unk88; drop < end2; ++drop) {
					drop->unk0.add(drop->unkC);
					drop->unk18.i.zero();
					drop->unk18.f.zero();
					drop->unk30.i.zero();
					drop->unk30.f.zero();
					drop->unk30.extend(gravVec2);
					accum.add(drop->unk0);
					count++;
				}
			}

			f32 heightVal;
			if (count * 30 > bw->unk74) {
				f32 inv = 1.0f / (f32)count;
				accum *= inv;
				f32 t     = 3.0f * (f32)count;
				t         = t / (f32)bw->unk74;
				heightVal = JGeometry::TUtil<f32>::sqrt(t);
				if (heightVal > 1.0f)
					heightVal = 1.0f;
			} else {
				heightVal = 0.0f;
			}
			bw->unk78.set(accum);
			bw->unk84 = heightVal;

			if (bw->unk8C->intersects.get()) {
				f32 dropRadius = bw->unk8C->dropRadius.get();
				f32 twoR       = 2.0f * dropRadius;
				f32 sep2       = 4.0f * (dropRadius * dropRadius);
				for (TDrop* a = bw->unk88; a < end2; a++) {
					for (TDrop* b = a + bw->unk8C->intersects.get(); b < end2;
					     b += bw->unk8C->intersects.get()) {
						JGeometry::TVec3<f32> local_2D8;
						local_2D8.sub(b->unk0, a->unk0);
						(void)&local_2D8;
						if (!(local_2D8.squared() > sep2)) {
							f32 dist = local_2D8.length();
							JGeometry::TVec3<f32> local_2E4;
							local_2E4.scale(1.0f / dist, local_2D8);

							(void)&local_2E4;

							f32 half = (twoR - dist) / 2.0f;

							local_2D8.x = local_2E4.x * half;
							local_2D8.z = local_2E4.z * half;

							f32 hny = half * local_2E4.y;

							local_2D8.set(local_2E4.x * half,
							              (local_2E4.y + 1.0f) * hny,
							              local_2E4.z * half);
							b->unk18.extend(local_2D8);
							local_2D8.x = -local_2D8.x;
							local_2D8.z = -local_2D8.z;
							local_2D8.y = (local_2E4.y - 1.0f) * hny;
							a->unk18.extend(local_2D8);

							f32 mag = hny;
							if (mag < twoR - dist)
								mag = twoR - dist;

							local_2E4.x *= mag * bw->unk8C->bounceXZ.get();
							local_2E4.y *= mag * bw->unk8C->bounceY.get();
							local_2E4.z *= mag * bw->unk8C->bounceXZ.get();
							b->unk30.extend(local_2E4);
							local_2E4.negate();
							a->unk30.extend(local_2E4);
						}
					}
				}
			}

			f32 threshold = data.mPos.y - data.unk3C;
			if (data.unk65)
				threshold = data.mPos.y - 8.0f * data.unk3C;

			int respawnIdx = 0;
			for (TDrop* drop = bw->unk88; drop < end2; drop++) {
				if (drop->unk0.y < threshold) {
					if (bw->unk8C->suppliesDrops.get() && !data.unk65) {
						drop->reset(
						    data.getPos(respawnIdx++, bw->unk70, dropRadius),
						    bw->unk68.get_float01());
					} else if (bw->eraseDrop(drop)) {
						end2--;
						drop->doThing(bw->unk8C->damp.get());
					}
				} else {
					drop->doThing(bw->unk8C->damp.get());
				}
			}

			int lifeTime = bw->unk8C->lifeTime.get();
			if (lifeTime > 0) {
				for (TDrop* drop = bw->unk88; drop < end2; --end2, ++drop) {
					drop->unk4C++;
					if (drop->unk4C > lifeTime)
						bw->eraseDrop(drop);
				}
			}

			if (bw->unk74 < bw->unk8C->numDrops.get()) {
				if (bw->unk8C->suppliesDrops.get() && !data.unk65) {
					bw->unk88[bw->unk74++].reset(
					    data.getPos(respawnIdx++, bw->unk70, dropRadius),
					    bw->unk68.get_float01());
				}
			} else if (bw->unk74 > bw->unk8C->numDrops.get()) {
				bw->unk74 = bw->unk8C->numDrops.get();
			}
		}

	public:
		/* 0x0 */ JGeometry::TVec3<f32> unk0;
		/* 0xC */ JGeometry::TVec3<f32> unkC;
		/* 0x18 */ JGeometry::TBox3<f32> unk18;
		/* 0x30 */ JGeometry::TBox3<f32> unk30;
		/* 0x48 */ f32 unk48;
		/* 0x4C */ int unk4C;
	};

public:
	/* 0x68 */ JMath::TRandomFast unk68;
	/* 0x6C */ u32 unk6C;
	/* 0x70 */ s32 unk70;
	/* 0x74 */ s32 unk74;
	/* 0x78 */ JGeometry::TVec3<f32> unk78;
	/* 0x84 */ f32 unk84;
	/* 0x88 */ TDrop* unk88;
	/* 0x8C */ TBathWaterParams* unk8C;
};

static void initScreen2D(s16, s16) { }

static void drawCap(const JGeometry::TVec3<f32>& pos, f32 radius)
{
	static f32 delta = 2.0f * 3.1415927f / 30.0f;
	f32 angle;
	f32 r;

	r     = radius / cosf(0.5f * delta);
	angle = 0.0f;
	GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, 30);
	for (int i = 0; i < 30; i++) {
		GXPosition3f32(r * cosf(angle) + pos.x, pos.y, r * sinf(angle) + pos.z);
		GXTexCoord2u8(0x40, 0x40);
		angle += delta;
	}
	GXEnd();
}

namespace {
void clearEFB_alpha(s16 x, s16 y, s16 wd, s16 ht, u8 alpha)
{
	Mtx44 m;
	Mtx pmtx;

	if (wd <= 0)
		wd = SMSGetGameRenderWidth();
	if (ht <= 0)
		ht = SMSGetGameRenderHeight();

	f32 fx      = x;
	f32 fwd     = wd;
	f32 fy      = y;
	f32 fht     = ht;
	f32 fright  = fx + fwd;
	f32 fbottom = fy + fht;

	C_MTXOrtho(m, fy, fbottom, fx, fright, 0.0f, 1.0f);
	PSMTXIdentity(pmtx);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 0);
	GXSetNumChans(1);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetNumTexGens(0);
	GXSetNumTevStages(1);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
	              GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO,
	                GX_CC_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);
	GXSetProjection(m, GX_ORTHOGRAPHIC);
	GXSetViewport(fx, fy, fwd, fht, 0.0f, 1.0f);
	GXSetScissor(x, y, wd, ht);
	GXSetBlendMode(GX_BM_NONE, GX_BL_ZERO, GX_BL_ZERO, GX_LO_NOOP);
	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GXSetColorUpdate(GX_FALSE);
	GXSetAlphaUpdate(GX_TRUE);
	GXSetDstAlpha(GX_TRUE, alpha);
	GXLoadPosMtxImm(pmtx, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetCullMode(GX_CULL_NONE);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2s16(x, y);
	GXPosition2s16(x + wd, y);
	GXPosition2s16(x + wd, y + ht);
	GXPosition2s16(x, y + ht);
	GXEnd();

	GXSetColorUpdate(GX_TRUE);
	GXSetDstAlpha(GX_FALSE, alpha);
}
} // namespace

void set_light_pos_nrm(GXLightObj*, MtxPtr, JGeometry::TVec3<f32>*,
                       JGeometry::TVec3<f32>*)
{
}

static void init_draw_light(MtxPtr, JGeometry::TVec3<f32>*, GXColor*) { }

static void draw_mist(u16 x, u16 y, u16 wd, u16 ht, void* buffer)
{
	Mtx e_m;
	Mtx44 m;
	GXTexObj tex_obj;

	GXColor tev_color = { 0x03, 0x03, 0x03, 0x00 };
	u8 vFilter[7]     = { 0x15, 0x00, 0x00, 0x16, 0x00, 0x00, 0x15 };

	f32 f_left   = x;
	f32 f_wd     = wd;
	f32 f_top    = y;
	f32 f_ht     = ht;
	f32 f_right  = f_left + f_wd;
	f32 f_bottom = f_top + f_ht;
	f32 offset_x = (4.0f / f_wd);
	f32 offset_y = (2.0f / f_ht);

	C_MTXOrtho(m, f_top, f_bottom, f_left, f_right, 0.0f, 1.0f);
	PSMTXIdentity(e_m);
	GXSetTexCopySrc(x, y, wd, ht);
	GXSetCopyFilter(GX_FALSE, 0, GX_TRUE, vFilter);
	GXSetTexCopyDst(wd >> 1, ht >> 1, GX_TF_RGB565, GX_TRUE);
	GXCopyTex(buffer, GX_FALSE);
	GXPixModeSync();
	GXInitTexObj(&tex_obj, buffer, wd >> 1, ht >> 1, GX_TF_RGB565, GX_CLAMP,
	             GX_CLAMP, 0);
	GXInitTexObjLOD(&tex_obj, GX_LINEAR, GX_LINEAR, 0.0, 0.0, 0.0, GX_FALSE,
	                GX_FALSE, GX_ANISO_1);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX1, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX2, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX3, GX_DIRECT);

	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XY, GX_U16, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX1, GX_TEX_ST, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX2, GX_TEX_ST, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX3, GX_TEX_ST, GX_F32, 0);

	GXSetNumChans(0);
	GXSetChanCtrl(GX_COLOR0A0, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_FALSE, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);

	GXSetNumTexGens(4);
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, GX_FALSE,
	                  0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX1, 0x3c, GX_FALSE,
	                  0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD2, GX_TG_MTX2x4, GX_TG_TEX2, 0x3c, GX_FALSE,
	                  0x7d);
	GXSetTexCoordGen2(GX_TEXCOORD3, GX_TG_MTX2x4, GX_TG_TEX3, 0x3c, GX_FALSE,
	                  0x7d);

	GXLoadTexObj(&tex_obj, GX_TEXMAP0);

	GXSetNumTevStages(4);

	// Stage 0
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF, GX_CC_C0);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_FALSE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevDirect(GX_TEVSTAGE0);

	// Stage 1
	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF,
	                GX_CC_CPREV);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_FALSE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevDirect(GX_TEVSTAGE1);

	// Stage 2
	GXSetTevOrder(GX_TEVSTAGE2, GX_TEXCOORD2, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE2, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF,
	                GX_CC_CPREV);
	GXSetTevColorOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_FALSE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE2, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE2, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevDirect(GX_TEVSTAGE2);

	// Stage 3
	GXSetTevOrder(GX_TEVSTAGE3, GX_TEXCOORD3, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetTevColorIn(GX_TEVSTAGE3, GX_CC_ZERO, GX_CC_TEXC, GX_CC_HALF,
	                GX_CC_CPREV);
	GXSetTevColorOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_DIVIDE_2,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaIn(GX_TEVSTAGE3, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO,
	                GX_CA_ZERO);
	GXSetTevAlphaOp(GX_TEVSTAGE3, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
	                GX_TRUE, GX_TEVPREV);
	GXSetTevDirect(GX_TEVSTAGE3);

	GXSetAlphaCompare(GX_ALWAYS, 0, GX_AOP_OR, GX_ALWAYS, 0);

	GXSetTevColor(GX_TEVREG0, tev_color);
	GXSetProjection(m, GX_ORTHOGRAPHIC);
	GXSetViewport(f_left, f_top, f_wd, f_ht, 0.0, 1.0);
	GXSetScissor(x, y, wd, ht);

	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GXSetAlphaUpdate(GX_FALSE);
	GXSetColorUpdate(GX_TRUE);
	GXLoadPosMtxImm(e_m, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	GXSetCullMode(GX_CULL_NONE);
	GXSetBlendMode(GX_BM_SUBTRACT, GX_BL_ZERO, GX_BL_ZERO, GX_LO_NOOP);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2u16(x, y);
	GXTexCoord2f32(-offset_x, 0.0f);
	GXTexCoord2f32(offset_x, 0.0f);
	GXTexCoord2f32(0.0f, -offset_y);
	GXTexCoord2f32(0.0f, offset_y);
	GXPosition2u16(x + wd, y);
	GXTexCoord2f32(1.0f - offset_x, 0.0f);
	GXTexCoord2f32(1.0f + offset_x, 0.0f);
	GXTexCoord2f32(1.0f, -offset_y);
	GXTexCoord2f32(1.0f, offset_y);
	GXPosition2u16(x + wd, y + ht);
	GXTexCoord2f32(1.0f - offset_x, 1.0f);
	GXTexCoord2f32(1.0f + offset_x, 1.0f);
	GXTexCoord2f32(1.0f, 1.0f - offset_y);
	GXTexCoord2f32(1.0f, 1.0f + offset_y);
	GXPosition2u16(x, y + ht);
	GXTexCoord2f32(-offset_x, 1.0f);
	GXTexCoord2f32(+offset_x, 1.0f);
	GXTexCoord2f32(0.0f, 1.0f - offset_y);
	GXTexCoord2f32(0.0f, 1.0f + offset_y);
	GXEnd();

	GXSetBlendMode(GX_BM_BLEND, GX_BL_ONE, GX_BL_ONE, GX_LO_NOOP);
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition2u16(x, y);
	GXTexCoord2f32(-offset_x, 0.0f);
	GXTexCoord2f32(offset_x, 0.0f);
	GXTexCoord2f32(0.0f, -offset_y);
	GXTexCoord2f32(0.0f, offset_y);
	GXPosition2u16(x + wd, y);
	GXTexCoord2f32(1.0f - offset_x, 0.0f);
	GXTexCoord2f32(1.0f + offset_x, 0.0f);
	GXTexCoord2f32(1.0f, -offset_y);
	GXTexCoord2f32(1.0f, offset_y);
	GXPosition2u16(x + wd, y + ht);
	GXTexCoord2f32(1.0f - offset_x, 1.0f);
	GXTexCoord2f32(1.0f + offset_x, 1.0f);
	GXTexCoord2f32(1.0f, 1.0f - offset_y);
	GXTexCoord2f32(1.0f, 1.0f + offset_y);
	GXPosition2u16(x, y + ht);
	GXTexCoord2f32(-offset_x, 1.0f);
	GXTexCoord2f32(+offset_x, 1.0f);
	GXTexCoord2f32(0.0f, 1.0f - offset_y);
	GXTexCoord2f32(0.0f, 1.0f + offset_y);
	GXEnd();
}

// Native port of TBathWaterPreprocessor::perform (@0x801aa5a8). Thin dispatch: on the DRAW
// flag bit, forward to the manager's renderer->render(graphics, bathtubData, waters, params, 2).
// RE: scratch/decomp_bathwater/801aa5a8.c.
//
// vtable slot resolution: TBathWaterRenderer's virtuals in declaration order are prerender[0]
// / render[1] / getHeight[2]. CodeWarrior emits the dtor as vtable[0] (byte 0), so the
// virtuals sit at byte offsets 4/8/12 respectively. The RE's `*(*(int**)(mgr+0x30) + 8)`
// dereferences vtable[8/4] = the 3rd slot with a dtor prefix = render. Args
// (graphics, bathtubData, TBathWater** waters, TBathWaterParams** params, count=2) match
// TBathWaterRenderer::render's signature exactly, confirming the pick.
//
// The dispatch gate is the pure sb::bath_water_preprocessor_should_dispatch predicate
// (native/render/sms_boot_bath_water.h), unit-tested in bath_water_preprocessor_test.cpp.
// The shipping port routes through it so the test validates the real gate.
#include "sms_boot_bath_water.h"

void TBathWaterPreprocessor::perform(u32 flags, JDrama::TGraphics* graphics)
{
	// mgr->unk24 is declared u32 in the header but the RE treats it as a POINTER to a
	// TBathtubData-adjacent structure (the RE's `mgr->unk24 + 0x170` yields the
	// TBathtubData& passed to render). We match that here — a subtle typing mismatch that
	// only matters at this seam; unk24 is written by other parts of the manager as a raw
	// pointer address. If the header is ever retyped, remove this cast.
	TBathWaterManager* mgr = unk10;
	const bool bathtub_present  = mgr && mgr->unk24 != 0;
	const bool renderer_present = mgr && mgr->unk30 != nullptr;
	if (!sb::bath_water_preprocessor_should_dispatch(flags, mgr != nullptr,
	                                                 bathtub_present, renderer_present)) {
		return;
	}
	const TBathtubData* bathtub = reinterpret_cast<const TBathtubData*>((uintptr_t)mgr->unk24 + 0x170);
	mgr->unk30->render(graphics, *bathtub, mgr->unk20, mgr->unk14, 2);
}
