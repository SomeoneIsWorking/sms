#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTransform.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DPacket.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
extern "C" int sb_boot_capture_phase();   // SB_DBHEAD_DBG: which pass this drawHead flush is in
#endif

J3DDrawBuffer::sortFunc J3DDrawBuffer::sortFuncTable[6] = {
	&J3DDrawBuffer::entryMatSort,     &J3DDrawBuffer::entryMatAnmSort,
	&J3DDrawBuffer::entryZSort,       &J3DDrawBuffer::entryModelSort,
	&J3DDrawBuffer::entryInvalidSort, &J3DDrawBuffer::entryNonSort,
};
J3DDrawBuffer::drawFunc J3DDrawBuffer::drawFuncTable[2] = {
	&J3DDrawBuffer::drawHead,
	&J3DDrawBuffer::drawTail,
};
int J3DDrawBuffer::entryNum;

J3DDrawBuffer::J3DDrawBuffer(u32 size)
{
	mBuffer = new (0x20) J3DPacket*[size];
	mSize   = size;

	mDrawType = DRAW_HEAD;
	mSortType = SORT_MAT;

	mZNear = 1.0f;
	mZFar  = 10000.0f;

	mZMtx           = nullptr;
	mCallBackPacket = nullptr;

	mZRatio = (mZFar - mZNear) / mSize;

	frameInit();
}

#ifdef SMS_NATIVE_PLATFORM
// SB_FI_TRACE: scene_drive registers the scene's two draw buffers here; frameInit() then
// backtraces whenever EITHER is reset, so we can find what clears the map mid-conductor-walk.
extern "C" { J3DDrawBuffer* g_sb_fi_watch[2] = { nullptr, nullptr }; }
extern "C" void sb_fi_watch_register(J3DDrawBuffer* opa, J3DDrawBuffer* xlu)
{
	g_sb_fi_watch[0] = opa;
	g_sb_fi_watch[1] = xlu;
}
#include <execinfo.h>
#include <cstdlib>
#include <cstdio>
#endif

void J3DDrawBuffer::frameInit()
{
#ifdef SMS_NATIVE_PLATFORM
	if ((this == g_sb_fi_watch[0] || this == g_sb_fi_watch[1]) && getenv("SB_FI_TRACE")) {
		static int s_fi = 0;
		if (s_fi < 12) { ++s_fi;
			std::fprintf(stderr, "[fi-trace] frameInit on WATCHED scene buf %p:\n", this);
			void* frames[24]; int nf = backtrace(frames, 24);
			backtrace_symbols_fd(frames, nf, 2);
		}
	}
#endif
	for (int i = 0; i < mSize; ++i)
		mBuffer[i] = nullptr;

	mCallBackPacket = nullptr;
}

bool J3DDrawBuffer::entryMatSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();

	J3DTexture* texture = j3dSys.getTexture();
	u32 hash;
	u16 texNo = packet->getMaterial()->getTexNo(0);
	if (texNo == 0xFFFF) {
		hash = 0;
	} else {
		hash = (u32)(uintptr_t)texture->getResTIMG(texNo);
	}

	if (packet->unk3C & 0x80000000) {
#ifdef SMS_NATIVE_PLATFORM
		// PC-engine draw-list invariant: this is the "no-merge, always prepend to
		// slot 0" path for unsorted/indirect materials (unk3C bit31). Unlike the
		// hashed branch below (which dedups via isSame), it does NOT guard against
		// re-entering the SAME packet. When an object enters a model via both
		// entry() and update() in one frameInit window (e.g. TShimmer::perform does
		// unk48->entry() for flag bit 0x4 AND unk48->update() for bit 0x200, and the
		// indirect-scene flag 0x40000204 sets both), the packet gets prepended twice.
		//
		// The original eeccde1 fix only checked the SLOT HEAD (mBuffer[0] == packet),
		// which catches a packet entered twice CONSECUTIVELY (the TShimmer self-loop).
		// But with the NPC population MANY objects enter twice and interleave: enter P
		// (head=P), enter Q (head=Q, Q->next=P), enter P AGAIN — P is no longer the
		// head, so the head-check misses it, P->next=Q and head=P → P->next=Q,
		// Q->next=P = a MULTI-node cycle → drawHead()'s walk spins forever (the
		// SB_NPC_ON gameplay hang; SB_DRAWBUF_CHECK reports "CYCLE ... selfloop=0").
		// The correct invariant is "enter each packet at most once per frameInit
		// window": skip the prepend if the packet is ALREADY anywhere in the slot
		// chain (re-entering an entered packet is a pure no-op). The chain is the
		// unsorted/indirect set (small), so the walk is cheap and can never form any
		// cycle. See debug_journal/2026-06-25_delfino_indirect_drawbuf_selfloop.md.
		for (J3DPacket* p = mBuffer[0]; p; p = p->getNextPacket())
			if (p == packet)
				return true;
#endif
		packet->setNextPacket(mBuffer[0]);
		mBuffer[0] = packet;
		return true;
	} else {
		u32 slot = hash & (mSize - 1);
		if (mBuffer[slot] == NULL) {
			mBuffer[slot] = packet;
			return true;
		} else {
			for (J3DMatPacket* pkt = (J3DMatPacket*)mBuffer[slot]; pkt != NULL;
			     pkt               = (J3DMatPacket*)pkt->getNextPacket()) {
				if (pkt->isSame(packet)) {
					pkt->addShapePacket(packet->getShapePacket());
					return false;
				}
			}

			packet->setNextPacket(mBuffer[slot]);
			mBuffer[slot] = packet;
			return true;
		}
	}
}

bool J3DDrawBuffer::entryMatAnmSort(J3DMatPacket* packet)
{
	J3DMaterialAnm* pMaterialAnm = packet->unk44;
	u32 slot                     = (u32)((uintptr_t)pMaterialAnm & (mSize - 1));

	if (pMaterialAnm == NULL) {
		return entryMatSort(packet);
	} else {
		packet->drawClear();
		packet->getShapePacket()->drawClear();
		if (mBuffer[slot] == NULL) {
			mBuffer[slot] = packet;
			return true;
		} else {
			for (J3DMatPacket* pkt = (J3DMatPacket*)mBuffer[slot]; pkt != NULL;
			     pkt               = (J3DMatPacket*)pkt->getNextPacket()) {
				if (pkt->unk44 == pMaterialAnm) {
					pkt->addShapePacket(packet->getShapePacket());
					return false;
				}
			}

			packet->setNextPacket(mBuffer[slot]);
			mBuffer[slot] = packet;
			return true;
		}
	}
}

bool J3DDrawBuffer::entryZSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();

	Vec tmp;
	tmp.x = mZMtx[0][3];
	tmp.y = mZMtx[1][3];
	tmp.z = mZMtx[2][3];

	f32 value = -J3DCalcZValue(j3dSys.getViewMtx(), tmp);

	u32 idx;
	if (mZNear + mZRatio < value) {
		if (mZFar - mZRatio > value) {
			idx = value / mZRatio;
		} else {
			idx = mSize - 1;
		}
	} else {
		idx = 0;
	}

	idx = (mSize - 1) - idx;
	packet->setNextPacket(mBuffer[idx]);
	mBuffer[idx] = packet;

	return true;
}

bool J3DDrawBuffer::entryModelSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();

	if (mCallBackPacket != nullptr) {
		mCallBackPacket->addChildPacket(packet);
		return true;
	}

	return false;
}

bool J3DDrawBuffer::entryInvalidSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();

	if (mCallBackPacket != nullptr) {
		mCallBackPacket->addChildPacket(packet->getShapePacket());
		return true;
	}

	return false;
}

bool J3DDrawBuffer::entryNonSort(J3DMatPacket* packet)
{
	packet->drawClear();
	packet->getShapePacket()->drawClear();

	packet->setNextPacket(mBuffer[0]);
	mBuffer[0] = packet;

	return true;
}

bool J3DDrawBuffer::entryImm(J3DPacket* packet, u16 index)
{
	packet->setNextPacket(mBuffer[index]);
	mBuffer[index] = packet;

	return true;
}

void J3DDrawBuffer::draw() const
{
	drawFunc func = drawFuncTable[mDrawType];
	(this->*func)();
}

void J3DDrawBuffer::drawHead() const
{
#ifdef SMS_NATIVE_PLATFORM
	// SB_DRAWBUF_CHECK=1: opt-in defensive cycle detector. A packet entered twice
	// into the same slot via a no-merge prepend self-links (pkt->next = mBuffer[slot]
	// = pkt), making this walk spin forever. entryMatSort now guards against that
	// (the real fix), so this is OFF by default and kept only as a tripwire for any
	// future no-merge entry path that re-introduces a self-link. Floyd tortoise/hare
	// per slot; names the offending packet + buffer, then aborts.
	{
		static int chk = -1;
		if (chk < 0) { const char* e = getenv("SB_DRAWBUF_CHECK"); chk = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (chk) {
			for (u32 i = 0; i < mSize; i++) {
				J3DPacket* slow = mBuffer[i];
				J3DPacket* fast = mBuffer[i];
				while (fast && fast->getNextPacket()) {
					slow = slow->getNextPacket();
					fast = fast->getNextPacket()->getNextPacket();
					if (slow == fast) {
						fprintf(stderr,
						        "[drawbuf] CYCLE buf=%p mBuffer[%u]: packet=%p vtable=%p "
						        "selfloop=%d sortType=%d\n",
						        (const void*)this, i, (void*)slow,
						        slow ? *(void**)slow : nullptr,
						        (int)(mBuffer[i] == mBuffer[i]->getNextPacket()),
						        (int)mSortType);
						abort();
					}
				}
			}
		}
	}
#endif
#ifdef SMS_NATIVE_PLATFORM
	// SB_DBHEAD_DBG: trace each drawHead flush — buffer ptr + phase + packet count. Cross-ref the
	// buffer ptr with SB_DRAWBUF_INV to see WHICH named buffer (e.g. MapXlu) is flushed in WHICH pass.
	// This nails the MapXlu-mask pass-routing divergence (native flushes it in ph1+ph6; GC in main).
	if (const char* e = std::getenv("SB_DBHEAD_DBG"); e && e[0] && e[0] != '0') {
		long np = 0; for (u32 i = 0; i < mSize; i++)
			for (J3DPacket* p = mBuffer[i]; p; p = p->getNextPacket()) ++np;
		if (np > 0)
			std::fprintf(stderr, "[dbhead] phase=%d buf=%p packets=%ld\n",
			             sb_boot_capture_phase(), (const void*)this, np);
	}
#endif
	for (u32 i = 0; i < mSize; i++) {
		for (J3DPacket* packet = mBuffer[i]; packet != nullptr;
		     packet            = packet->getNextPacket()) {
			packet->draw();
		}
	}
}

void J3DDrawBuffer::drawTail() const
{
	for (int i = mSize - 1; i >= 0; i--) {
		for (J3DPacket* packet = mBuffer[i]; packet != nullptr;
		     packet            = packet->getNextPacket()) {
			packet->draw();
		}
	}
}
