#include <JSystem/J3D/J3DGraphBase/J3DPacket.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <dolphin/os.h>
#include <dolphin/gd.h>
#include <dolphin/gx.h>

#ifdef SMS_NATIVE_PLATFORM
#include <cstdlib>
#include <cstdio>
extern "C" u32 aurora_gx_scan_dl(const u8* data, u32 size, u32 allowDraws);

static u32 sb_dl_validate_enabled()
{
	static int dbg = -1;
	if (dbg < 0) { const char* e = getenv("SB_DL_VALIDATE"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
	return (u32)dbg;
}
#endif

int J3DDrawPacket::sInterruptFlag;

void J3DDisplayListObj::newDisplayList(u32 param_1)
{
	unkC = param_1 + 0x1f & 0xffffffe0;
	unk0 = new (0x20) char[unkC];
	unk4 = new (0x20) char[unkC];
	unk8 = 0;
#ifdef SMS_NATIVE_PLATFORM
	// Poison fresh DL buffers with an invalid fifo opcode. Replaying a
	// buffer that was never built (double-buffer bookkeeping bug, stale
	// size) must FAIL FAST at the drain with an unambiguous 0xEE signature
	// instead of executing heap junk as GX commands.
	memset(unk0, 0xEE, unkC);
	memset(unk4, 0xEE, unkC);
#endif
}

void J3DDisplayListObj::swapBuffer()
{
	void* tmp = unk0;
	unk0      = unk4;
	unk4      = tmp;
}

void J3DDisplayListObj::callDL()
{
#ifdef SMS_NATIVE_PLATFORM
	// SB_DL_VALIDATE=1 (replay side): the same structural scan that endDL
	// runs at build time. Build-clean + replay-corrupt brackets the
	// corruption window to [endDL .. callDL]; the OSPanic backtrace names
	// the material/model driving this replay.
	{
		if (sb_dl_validate_enabled()) {
			u32 bad = aurora_gx_scan_dl((const u8*)unk0, unk8, 0);
			if (bad != 0xFFFFFFFFu) {
				const u8* d = (const u8*)unk0;
				fprintf(stderr, "[dl-validate] MALFORMED at REPLAY dl=%p size=%u off=%u:", unk0, unk8, bad);
				for (u32 i = (bad > 16 ? bad - 16 : 0); i < bad + 16 && i < unk8; ++i)
					fprintf(stderr, " %02x%s", d[i], i == bad ? "<" : "");
				fprintf(stderr, "\n");
				OSPanic(__FILE__, __LINE__, "callDL replaying a malformed display list");
			}
		}
	}
#endif
	GXCallDisplayList(unk0, unk8);
}

bool J3DPacket::isSame(J3DMatPacket*) const { return false; }

bool J3DPacket::entry(J3DDrawBuffer*) { return true; }

void J3DPacket::addChildPacket(J3DPacket* packet)
{
	if (unk8 == nullptr) {
		unk8 = packet;
		return;
	}
	packet->unk4 = unk8;
	unk8         = packet;
}

void J3DCallBackPacket::draw()
{
	if (unk10 != nullptr) {
		unk10(this, 0);
	}
	for (J3DPacket* packet = unk8; packet != nullptr; packet = packet->unk4) {
		packet->draw();
	}
	if (unk10 != nullptr) {
		unk10(this, 1);
	}
}

J3DDrawPacket::J3DDrawPacket()
{
	unk10 = 0;
	unk30 = nullptr;
}

J3DDrawPacket::~J3DDrawPacket() { }

void J3DDrawPacket::draw() { GXCallDisplayList(unk30->unk0, unk30->unk8); }

void J3DDrawPacket::beginDL()
{
	unk30->swapBuffer();
	sInterruptFlag = OSDisableInterrupts();
	GDInitGDLObj(&unk20, unk30->unk0, unk30->unkC);
	__GDCurrentDL = &unk20;
}

u32 J3DDrawPacket::endDL()
{
	GDPadCurr32();
	OSRestoreInterrupts(sInterruptFlag);
	unk30->unk8 = unk20.ptr - unk20.start;
	GDFlushCurrToMem();
	__GDCurrentDL = 0;
#ifdef SMS_NATIVE_PLATFORM
	// SB_DL_VALIDATE=1: structurally validate the just-built material DL.
	// A malformed buffer HERE names the builder while its context is live;
	// the same bytes failing later in the fifo drain names nobody.
	{
		static int dbg = -1;
		if (dbg < 0) { const char* e = getenv("SB_DL_VALIDATE"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dbg) {
			u32 bad = aurora_gx_scan_dl((const u8*)unk30->unk0, unk30->unk8, 0);
			if (bad != 0xFFFFFFFFu) {
				const u8* d = (const u8*)unk30->unk0;
				fprintf(stderr, "[dl-validate] MALFORMED material DL %p size=%u at off=%u:", unk30->unk0,
				        unk30->unk8, bad);
				for (u32 i = (bad > 16 ? bad - 16 : 0); i < bad + 16 && i < unk30->unk8; ++i)
					fprintf(stderr, " %02x%s", d[i], i == bad ? "<" : "");
				fprintf(stderr, "\n");
				OSPanic(__FILE__, __LINE__, "endDL built a malformed display list (see [dl-validate])");
			}
		}
	}
#endif
	return unk30->unk8;
}

J3DMatPacket::J3DMatPacket()
{
	unk38    = 0;
	unk3C    = -1;
	mTexture = 0;
	unk44    = 0;
}

J3DMatPacket::~J3DMatPacket() { }

void J3DMatPacket::addShapePacket(J3DShapePacket* packet)
{
	if (unk34 == nullptr) {
		unk34 = packet;
		return;
	}
	packet->unk4 = unk34;
	unk34        = packet;
}

inline bool checkThing(J3DShapePacket* p)
{
	bool ret = true;

	for (J3DShapePacket* i = p; i != nullptr; i = (J3DShapePacket*)i->unk4) {
		if (i->unk30 != 0) {
			ret = false;
			break;
		}
	}

	return ret;
}

void J3DMatPacket::draw()
{
	char trash[0x20];
	if (!checkThing(unk34)) {
		j3dSys.mTexture   = mTexture;
		j3dSys.mMatPacket = this;
		unk38->load();
		for (J3DPacket* j = unk34; j != nullptr; j = j->unk4) {
			j->draw();
		}
	}
}

J3DShapePacket::J3DShapePacket()
{
	unk14 = 0;
	unk18 = 0;
	unk1C = 0;
	unk20 = &j3dDefaultViewNo;
	unk24 = 0;
	unk28 = 0;
	unk2C = 0;
	unk30 = 1;
}

J3DShapePacket::~J3DShapePacket() { }

void J3DShapePacket::draw()
{
	char
	    trash[0x20]; // TODO: probably shares inlines w/ J3DCallBackPacket::draw
	if ((unk14 != 0) && (unk30 != 0)) {
		if (unk10 != nullptr) {
			unk10(this, 0);
		}
		j3dSys.unk10C         = unk24;
		j3dSys.unk110         = unk28;
		j3dSys.unk114         = unk2C;
		unk14->mDrawMatrices  = unk18;
		unk14->mNormMatrices  = unk1C;
		unk14->mCurrentViewNo = unk20;
		unk14->draw();
		if (unk10 != nullptr) {
			unk10(this, 1);
		}
	}
}
