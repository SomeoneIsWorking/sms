#include <GC2D/Guide.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <System/Application.hpp>
#include <System/MarDirector.hpp>
#include <JSystem/J2D/J2DPane.hpp>
#include <sms_boot_guide.h>
#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>

// Native port of the parts of TGuide (the pause-menu guide/map screen) that are fully determined
// by the US GMSE01 disassembly. Guide.cpp is empty upstream, so everything here is cold RE.
//
// WHAT IS PORTED: setup (0x8017b464) and startMoveCursor (0x8017b450), both complete, plus the
// archive-mount half of load (0x8017c180). WHAT IS NOT: the pane construction that the rest of
// load does, and perform — those remain the LOUD stubs they were, see sms-boot/boot_stubs.

// ── the mount-state byte at r13-0x63a8 (guest 0x8040de18) ──────────────────────────────────────
//
// Identified by exhaustive reference scan of the DOL rather than guessed: exactly five
// instructions in the whole image touch it, and all five are in TGuide — the destructor
// (0x801791ec lbz, 0x801791fc stb, 0x80179200 lbz), setup (0x8017b498 stb) and load (0x8017c1cc
// stb). A global that only one class reads is that class's file-scope static, which is why it
// lives here and not in a header.
//
// Semantics, from those five sites: setup and load set it to 0x10 when there is NO archive to
// mount, and the destructor decrements it and, on reaching zero, calls
// SMSSwitch2DArchive(<rodata string>, gArBkGuide) — i.e. it is a countdown that defers releasing
// the shared "guide" 2D archive. The destructor is NOT ported here (it is large and branches into
// pane teardown), so nothing decrements this yet; it is defined so the two functions that WRITE it
// are faithful, and so the next porter finds the analysis instead of redoing it.
static u8 sGuideMountCountdown;

// TGuide::setup — US 0x8017b464, 0x5C bytes.
//
//   8017b474  or.  r31, r4, r4      ; test the archive argument
//   8017b480  beq  ...              ; null -> take the countdown branch
//   8017b48c  bl   SMSMountAramArchive(r31, r13-0x6010)
//   8017b494  li   r0, 0x10 ; stb r0, -0x63a8(r13)
//   8017b4a0  stb  r0, 0xc4(r30)    ; unkC4 = 0 on BOTH paths
//
// r13-0x6010 is gArBkGuide. That identification is not assumed from the name: the DOL forms that
// address in exactly five places — TGuide's dtor/setup/load and TApplication::mountStageArchive
// twice — and at the mountStageArchive sites the object is filled by
// JKRDvdAramRipper::loadToAram into +0x0 with a u8 flag written at +0x4, which is TARAMBlock's
// layout. It is also the same global the already-ported TGuide::load mounts as gArBkGuide.
void TGuide::setup(JKRMemArchive* archive)
{
	if (archive != nullptr) {
		SMSMountAramArchive(archive, gArBkGuide);
	} else {
		sGuideMountCountdown = 0x10;
	}
	unkC4 = 0;
}

// Prologue made FAITHFUL to US 0x8017c180 (2026-08-12), from the disassembly:
//
//   8017c1a0  stb  r0, 0xc5(r3)          ; unkC5 = 0, BEFORE the base load
//   8017c1a4  bl   JDrama::TViewObj::load
//   8017c1a8  lwz  r3, -0x6048(r13)      ; gpMarDirector
//   8017c1ac  lwz  r24, 0xd8(r3)         ; ->unkD8, the shared 2D archive
//   8017c1b0  cmplwi r24, 0 ; beq        ; MOUNT ONLY IF NON-NULL
//   8017c1bc  bl   SMSMountAramArchive(r24, gArBkGuide)
//   8017c1c8  li r0,0x10 ; stb -0x63a8(r13)   ; else: the countdown declared above
//   8017c1d4  stb  r0, 0xc4(r31)         ; unkC4 = 0 on both paths
//
// The previous version mounted UNCONDITIONALLY and set neither byte, so a null shared archive
// dereferenced instead of taking the branch retail has for exactly that case.
//
// STILL UNPORTED, deliberately: everything from 0x8017c1d8 on, which allocates (li r3,0xf4) and
// builds this screen's panes. That is why perform() below is still a stub and the screen draws
// nothing — the gap is in the pane build, not in the mount.
void TGuide::load(JSUMemoryInputStream& stream)
{
	unkC5 = 0;
	JDrama::TViewObj::load(stream); // read NameRef header so search("ガイド画面") finds it
	JKRMemArchive* shared = gpMarDirector->unkD8;
	if (shared != nullptr) {
		SMSMountAramArchive(shared, gArBkGuide);
	} else {
		sGuideMountCountdown = 0x10;
	}
	unkC4 = 0;
}

// TGuide::startMoveCursor — US 0x8017b450, 0x14 bytes, complete:
//
//   li r0, 9    ; stw r0, 0x10(r3)
//   li r0, 0    ; stb r0, 0x164(r3)
//   blr
//
// Both fields are otherwise untouched by any ported code, so no behaviour depends on them yet —
// this is the writer half of a state machine whose reader (perform) is still a stub. It is ported
// because it is complete and unambiguous, not because it is currently observable.
void TGuide::startMoveCursor()
{
	unk10  = 9;
	unk164 = 0;
}

// TGuide::checkPoint — US 0x8017a6bc, 0x134 bytes. Which pane, if any, contains the point.
//
//   pass one   14 panes at this+0x168, rect copied from pane+0x14
//   pass two   10 panes at this+0x44c, same, only if pass one found nothing
//   gate       0 <= hit < 10 and pass-two pane not visible -> -1
//
// Retail copies each JUTRect to the stack before comparing (bl JUTRect::copy at 0x8017a6f4 and
// 0x8017a760) rather than reading the pane's rect in place. That copy is not reproduced: it is a
// compiler artefact of taking a JUTRect by value, and it cannot be observed — nothing writes the
// rect during the loop. The comparisons, which CAN be observed, are reproduced exactly, including
// that every edge is strict; see sms-boot/shims/sms_boot_guide.h.
int TGuide::checkPoint(int x, int y)
{
	int hit = -1;

	for (int i = 0; i < sb::guide::kPassOnePanes; ++i) {
		const JUTRect& r = unk168[i]->mBounds;
		if (sb::guide::point_in_rect(x, y, r.x1, r.y1, r.x2, r.y2)) {
			hit = i;
			break;
		}
	}

	if (hit == -1) {
		for (int i = 0; i < sb::guide::kPassTwoPanes; ++i) {
			const JUTRect& r = unk44c[i]->mBounds;
			if (sb::guide::point_in_rect(x, y, r.x1, r.y1, r.x2, r.y2)) {
				hit = i;
				break;
			}
		}
	}

	// NOTE the index, not the pass, decides whether the gate applies — a pass-one hit below 10
	// is validated against the pass-TWO pane at the same index. That is what 0x8017a7ac does.
	const bool visible
	    = (hit >= 0 && hit < sb::guide::kPassTwoPanes) ? unk44c[hit]->mVisible : false;
	return sb::guide::gate_hit(hit, visible);
}
