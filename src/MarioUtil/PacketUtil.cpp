#include <MarioUtil/PacketUtil.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/J3D/J3DGraphBase/Blocks/J3DPEBlocks.hpp>

// Retail (0x80235f04) writes these overrides as raw WGPIPE BP/XF commands
// mid-shape-draw; the plain GX immediate calls emit the same commands into
// the fifo at the same point, so they are the faithful native equivalents.
static void FifoSetChanMatColor(GXChannelID chan, GXColor col) { GXSetChanMatColor(chan, col); }

static void FifoSetTevColorS10(GXTevRegID reg, GXColorS10 col) { GXSetTevColorS10(reg, col); }

static void FifoSetTevKColor(GXTevKColorID reg, GXColor col) { GXSetTevKColor(reg, col); }

static void FifoSetFogRangeAdj(u8 enable, u16 center, GXFogAdjTable* table)
{
	GXSetFogRangeAdj(enable, center, table);
}

static void FifoSetFog(GXFogType type, float startZ, float endZ, float nearZ, float farZ, GXColor col)
{
	GXSetFog(type, startZ, endZ, nearZ, farZ, col);
}

static void SetFogBase(const J3DFogInfo* fog)
{
	FifoSetFog((GXFogType)fog->mType, fog->mStartZ, fog->mEndZ, fog->mNearZ, fog->mFarZ, fog->mColor);
	FifoSetFogRangeAdj(fog->mAdjEnable, fog->mCenter, (GXFogAdjTable*)fog->mFogAdjTable);
}

static void ShapePacketCallBackFunc(J3DCallBackPacket*, int); // impl at end of file (needs the user-data structs)

static J3DShapePacket* InitPacket_Sub(J3DModel* model, u16 mat_idx)
{
	J3DMaterial* mat = model->getModelData()->getMaterialNodePointer(mat_idx);
	return model->getShapePacket(mat->getShape()->getIndex());
}

// fabricated
struct PacketUserData_MatColor {
	u32 unk0;
	GXChannelID unk4;
	const GXColor* unk8;
};

void SMS_InitPacket_MatColor(J3DModel* param_1, u16 param_2,
                             GXChannelID param_3, const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_MatColor* userData = new PacketUserData_MatColor;

	userData->unk0 = 0;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevColor {
	u32 unk0;
	GXTevRegID unk4;
	const GXColorS10* unk8;
};

void SMS_InitPacket_OneTevColor(J3DModel* param_1, u16 param_2,
                                GXTevRegID param_3, const GXColorS10* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevColor* userData = new PacketUserData_OneTevColor;

	userData->unk0 = 1;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	const GXColorS10* unkC;
	const GXColorS10* unk10;
};

void SMS_InitPacket_TwoTevColor(J3DModel* param_1, u16 param_2,
                                GXTevRegID param_3, const GXColorS10* param_4,
                                GXTevRegID param_5, const GXColorS10* param_6)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevColor* userData = new PacketUserData_TwoTevColor;

	userData->unk0  = 2;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_ThreeTevColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	GXTevRegID unkC;
	const GXColorS10* unk10;
	const GXColorS10* unk14;
	const GXColorS10* unk18;
};

void SMS_InitPacket_ThreeTevColor(J3DModel* param_1, u16 param_2,
                                  GXTevRegID param_3, const GXColorS10* param_4,
                                  GXTevRegID param_5, const GXColorS10* param_6,
                                  GXTevRegID param_7, const GXColorS10* param_8)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_ThreeTevColor* userData = new PacketUserData_ThreeTevColor;

	userData->unk0  = 3;
	userData->unk4  = param_3;
	userData->unk10 = param_4;
	userData->unk8  = param_5;
	userData->unk14 = param_6;
	userData->unkC  = param_7;
	userData->unk18 = param_8;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_Fog {
	u32 unk0;
	J3DFog* unk4;
};

void SMS_InitPacket_Fog(J3DModel* param_1, u16 param_2)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	J3DFog* fog = param_1->getModelData()
	                  ->getMaterialNodePointer(param_2)
	                  ->getPEBlock()
	                  ->getFog();

	PacketUserData_Fog* userData = new PacketUserData_Fog;
	userData->unk0               = 5;
	userData->unk4               = fog;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevKColor {
	u32 unk0;
	GXTevKColorID unk4;
	const GXColor* unk8;
};

void SMS_InitPacket_OneTevKColor(J3DModel* param_1, u16 param_2,
                                 GXTevKColorID param_3, const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevKColor* userData = new PacketUserData_OneTevKColor;

	userData->unk0 = 6;
	userData->unk4 = param_3;
	userData->unk8 = param_4;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevKColor {
	u32 unk0;
	GXTevKColorID unk4;
	GXTevKColorID unk8;
	const GXColor* unkC;
	const GXColor* unk10;
};

void SMS_InitPacket_TwoTevKColor(J3DModel* param_1, u16 param_2,
                                 GXTevKColorID param_3, const GXColor* param_4,
                                 GXTevKColorID param_5, const GXColor* param_6)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevKColor* userData = new PacketUserData_TwoTevKColor;

	userData->unk0  = 7;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevKColorAndFog {
	u32 unk0;
	u32 unk4;
	GXTevKColorID unk8;
	const GXColor* unkC;
	u32 unk10;
	J3DFog* unk14;
};

void SMS_InitPacket_OneTevKColorAndFog(J3DModel* param_1, u16 param_2,
                                       GXTevKColorID param_3,
                                       const GXColor* param_4)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevKColorAndFog* userData
	    = new PacketUserData_OneTevKColorAndFog;

	userData->unk0 = 8;
	userData->unk4 = 6;
	userData->unk8 = param_3;

	if (param_4 != nullptr) {
		userData->unkC = param_4;
	} else {
		userData->unkC = &param_1->getModelData()
		                      ->getMaterialNodePointer(param_2)
		                      ->getTevBlock()
		                      ->getTevKColor(param_3)
		                      ->color;
	}

	J3DFog* fog = param_1->getModelData()
	                  ->getMaterialNodePointer(param_2)
	                  ->getPEBlock()
	                  ->getFog();

	userData->unk10 = 5;
	userData->unk14 = fog;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_OneTevColorAndOneTevKColor {
	u32 unk0;
	GXTevRegID unk4;
	const GXColorS10* unk8;
	const GXColor* unkC;
};

void SMS_InitPacket_OneTevColorAndOneTevKColor(J3DModel* param_1, u16 param_2,
                                               GXTevRegID param_3,
                                               const GXColorS10* param_4,
                                               const GXColor* param_5)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_OneTevColorAndOneTevKColor* userData
	    = new PacketUserData_OneTevColorAndOneTevKColor;

	userData->unk0 = 9;
	userData->unk4 = param_3;
	userData->unk8 = param_4;
	userData->unkC = param_5;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

// fabricated
struct PacketUserData_TwoTevColorAndOneTevKColor {
	u32 unk0;
	GXTevRegID unk4;
	GXTevRegID unk8;
	const GXColorS10* unkC;
	const GXColorS10* unk10;
	const GXColor* unk14;
};

void SMS_InitPacket_TwoTevColorAndOneTevKColor(J3DModel* param_1, u16 param_2,
                                               GXTevRegID param_3,
                                               const GXColorS10* param_4,
                                               GXTevRegID param_5,
                                               const GXColorS10* param_6,
                                               const GXColor* param_7)
{
	J3DShapePacket* packet = InitPacket_Sub(param_1, param_2);

	PacketUserData_TwoTevColorAndOneTevKColor* userData
	    = new PacketUserData_TwoTevColorAndOneTevKColor;

	userData->unk0  = 10;
	userData->unk4  = param_3;
	userData->unkC  = param_4;
	userData->unk8  = param_5;
	userData->unk10 = param_6;
	userData->unk14 = param_7;

	packet->setUserArea((uintptr_t)userData);
	packet->setCallback(&ShapePacketCallBackFunc);
}

void SMS_HideAllShapePacket(J3DModel* model)
{
	u16 mats = model->getModelData()->getMaterialNum();
	for (u16 i = 0; i < mats; ++i)
		model->getShapePacket(i)->hide();
}

void SMS_ShowAllShapePacket(J3DModel* model)
{
	u16 mats = model->getModelData()->getMaterialNum();
	for (u16 i = 0; i < mats; ++i)
		model->getShapePacket(i)->show();
}

// Retail ShapePacketCallBackFunc (@0x80235f04, Ghidra 2026-07-16): the
// per-shape-packet GX override dispatcher installed by every SMS_InitPacket_*
// above. J3DShapePacket::draw fires it with param 0 BEFORE the shape's
// display lists (apply the override on top of the — possibly LOCKED — material
// DL) and param 1 AFTER (restore global fog for the fog-carrying modes).
//
// This was an EMPTY STUB until 2026-07-16; with Mario's mat packets LOCKED
// (SMS_MakeDLAndLock) the baked TEV KONST colors from the BMD (K0.a=0x80)
// were never overridden by the live material state (TMario::addDirty keeps
// K0.a=mDirty=0), so the dirt/marking TEXA-compare stage fired over the
// marking regions -> black glove-back/chest/leg patches at file-select.
// Retail case order and register usage verified against the decompile:
// mode 9/10 write GX_KCOLOR0 specifically (0xe0800000 in the BP stream).
static void ShapePacketCallBackFunc(J3DCallBackPacket* packet, int stage)
{
	const u32* data = (const u32*)((J3DShapePacket*)packet)->getUserArea();
	if (data == nullptr)
		return;

	if (stage == 0) {
		switch (*data) {
		case 0: {
			const PacketUserData_MatColor* d = (const PacketUserData_MatColor*)data;
			FifoSetChanMatColor(d->unk4, *d->unk8);
		} break;
		case 1: {
			const PacketUserData_OneTevColor* d = (const PacketUserData_OneTevColor*)data;
			FifoSetTevColorS10(d->unk4, *d->unk8);
		} break;
		case 2: {
			const PacketUserData_TwoTevColor* d = (const PacketUserData_TwoTevColor*)data;
			FifoSetTevColorS10(d->unk4, *d->unkC);
			FifoSetTevColorS10(d->unk8, *d->unk10);
		} break;
		case 3: {
			const PacketUserData_ThreeTevColor* d = (const PacketUserData_ThreeTevColor*)data;
			FifoSetTevColorS10(d->unk4, *d->unk10);
			FifoSetTevColorS10(d->unk8, *d->unk14);
			FifoSetTevColorS10(d->unkC, *d->unk18);
		} break;
		case 4: {
			// Retail: GXCallDisplayList(data[1], data[2]). No SMS_InitPacket_*
			// setter for this mode survives in the decomp; layout from the
			// callback decompile.
			struct CallDL { u32 mode; void* list; u32 size; };
			const CallDL* d = (const CallDL*)data;
			GXCallDisplayList(d->list, d->size);
		} break;
		case 5: {
			const PacketUserData_Fog* d = (const PacketUserData_Fog*)data;
			SetFogBase(d->unk4);
		} break;
		case 6: {
			const PacketUserData_OneTevKColor* d = (const PacketUserData_OneTevKColor*)data;
			FifoSetTevKColor(d->unk4, *d->unk8);
		} break;
		case 7: {
			const PacketUserData_TwoTevKColor* d = (const PacketUserData_TwoTevKColor*)data;
			FifoSetTevKColor(d->unk4, *d->unkC);
			FifoSetTevKColor(d->unk8, *d->unk10);
		} break;
		case 8: {
			const PacketUserData_OneTevKColorAndFog* d = (const PacketUserData_OneTevKColorAndFog*)data;
			FifoSetTevKColor(d->unk8, *d->unkC);
			SetFogBase(d->unk14);
		} break;
		case 9: {
			const PacketUserData_OneTevColorAndOneTevKColor* d
			    = (const PacketUserData_OneTevColorAndOneTevKColor*)data;
			FifoSetTevColorS10(d->unk4, *d->unk8);
			FifoSetTevKColor(GX_KCOLOR0, *d->unkC);
		} break;
		case 10: {
			const PacketUserData_TwoTevColorAndOneTevKColor* d
			    = (const PacketUserData_TwoTevColorAndOneTevKColor*)data;
			FifoSetTevColorS10(d->unk4, *d->unkC);
			FifoSetTevColorS10(d->unk8, *d->unk10);
			FifoSetTevKColor(GX_KCOLOR0, *d->unk14);
		} break;
		}
	} else if (stage == 1) {
		// Post-draw: fog-carrying modes restore the global fog. Retail reads
		// SDA2[-0x15a4] (f32 0.0) and SDA2[-0x15a8] (GXColor 0x00000000) —
		// i.e. fog OFF with a black color.
		const u32 mode = *data;
		if (mode == 5 || mode == 8) {
			GXColor black = { 0, 0, 0, 0 };
			FifoSetFog(GX_FOG_NONE, 0.0f, 0.0f, 0.0f, 0.0f, black);
		}
	}
}
