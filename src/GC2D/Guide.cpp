#include <GC2D/Guide.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <System/Application.hpp>
#include <System/MarDirector.hpp>
#include <System/MarioGamePad.hpp>
#include <System/StageUtil.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/J2D/J2DPane.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <GC2D/ExPane.hpp>
#include <GC2D/BoundPane.hpp>
#include <GC2D/ScrnFader.hpp>
#include <sb_log.h>
#include <sms_boot_guide.h>
#include <JSystem/JDrama/JDRViewObj.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>

// Native port of the parts of TGuide (the pause-menu guide/map screen) that are fully determined
// by the US GMSE01 disassembly. Guide.cpp is empty upstream, so everything here is cold RE.
//
// WHAT IS PORTED: setup (0x8017b464), startMoveCursor (0x8017b450), and the screen/pane ownership
// half of load (0x8017c180). The input/marker state machine is still being ported from the DOL.

// ── the mount-state byte at r13-0x63a8 (guest 0x8040de18) ──────────────────────────────────────
//
// Identified by exhaustive reference scan of the DOL rather than guessed: exactly five
// instructions in the whole image touch it, and all five are in TGuide — the destructor
// (0x801791ec lbz, 0x801791fc stb, 0x80179200 lbz), setup (0x8017b498 stb) and load (0x8017c1cc
// stb). A global that only one class reads is that class's file-scope static, which is why it
// lives here and not in a header.
//
// Semantics, from those five sites: setup and load set it to 0x10 when there is NO archive to
// mount, and it is decremented once per call by TGuide::perform, which on reaching zero calls
// SMSSwitch2DArchive(<rodata string>, gArBkGuide). It is therefore a FRAME DELAY — sixteen frames
// after a mountless load, the shared 2D archive is switched.
//
// CORRECTED 2026-08-12, same day: this first said the DESTRUCTOR decrements it. It does not. The
// three reads/writes sit at 0x801791ec..0x80179208, which is past the end of __dt__6TGuideFv —
// the dtor is only 0x74 bytes and TGuide::perform starts at 0x801791d0. perform is WEAK and so
// absent from the US symbol list, which is why the disassembly around those instructions rendered
// as "__dt__6TGuideFv+0x..." and read as destructor code. Resolving perform's real entry through
// its vtable slot (index 6, shared with TConsoleStr, both direct JDrama::TViewObj subclasses)
// settled it, and 0x801791d0 begins mflr/stwu/stmw with three incoming arguments — a function
// entry, not a continuation.
//
// perform is NOT ported here, so nothing decrements this yet; it is defined so the two functions
// that WRITE it are faithful, and so the next porter finds the analysis instead of redoing it.
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
// The retail body next allocates a J2DSetScreen for guide_1.blo and then resolves every pane that
// the guide state machine owns.  The screen and the stable pane tables are constructed here first;
// the animated markers/textures are filled in by the remaining state-machine port.
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

	unkBC = new J2DSetScreen("guide_1.blo", shared);

	// US GMSE01 creates the same screen then assigns the system font to these four static labels
	// before the per-location labels are populated.  Do this as a table so every failed lookup is
	// surfaced by J2DScreen::search rather than silently leaving a label with stale font state.
	const u32 font_panes[] = { 'a_ic', 'a_tx', 'b_ic', 'b_tx' };
	for (u32 tag : font_panes) {
		J2DPane* pane = unkBC->search(tag);
		if (pane != nullptr)
			static_cast<J2DTextBox*>(pane)->setFont(gpSystemFont);
	}

	// The guide has thirteen location panes and ten selectable point panes.  These are the exact
	// four-character tag progressions from 0x8017c2f4..0x8017c4a8; the final location pane at
	// [13] is the separate 0x19c entry in retail and intentionally remains part of unk168.
	for (u32 i = 0; i < 13; ++i) {
		const u32 row = (i / 10) * 0xF6 + i;
		unk168[i]  = unkBC->search(0x3030 + row);
		unk1C0[i]  = new TExPane(unkBC, (0x3030 + row) << 16 | 0x5F30);
		unk218[i] = unk1C0[i]->getInitialBounds();
		unk378[i]  = new TExPane(unkBC, (0x3030 + row) << 16 | 0x5F31);
	}
	unk168[13] = unkBC->search('20');
	unk1F4 = new TExPane(unkBC, 'lwin');
	unk2E8 = unk1F4->getInitialBounds();
	unk3AC = new TExPane(unkBC, 'llin');
	for (u32 i = 0; i < 10; ++i)
		unk44c[i] = unkBC->search(0x706E3030 + i);

	unk124 = unkBC->search('s_mn');
	unk128[0] = new TExPane(unkBC, 'cu_a');
	unk128[1] = new TExPane(unkBC, 'cu_b');
	unk430 = unkBC->search('01mi');
	unk434 = unkBC->search('01_9')->getBounds();
	unk444 = new TBoundPane(unkBC, '20');
	unk478 = new TExPane(unkBC, 'mark');
	unkC5 = 1;
	SB_LOGC("guide", "load screen=%p cursor=(%p,%p) panes=(%p,%p)", (void*)unkBC,
	        (void*)unk128[0], (void*)unk128[1], (void*)unk168[0], (void*)unk168[13]);
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

// TGuide::perform — US 0x801791d0, draw and mount-delay spine.
//
// Retail deliberately keeps the guide screen out of the draw pass while its archive switch and
// wipe are in flight (states 8/9).  Once the director's wipe admits the guide state, it draws the
// `guide_1.blo` hierarchy through the normal J2D orthographic graph.  The previous native stub
// omitted this entire shared path, which is why the C/Z guide transition could only remain black.
// The state-specific cursor/map marker branches are ported below this spine as their object and
// flag contracts are recovered; keeping the draw gate retail-shaped means those branches cannot
// accidentally expose a partially-built screen during the transition.
void TGuide::perform(u32 cue, JDrama::TGraphics* graphics)
{
	if (sGuideMountCountdown != 0) {
		--sGuideMountCountdown;
		if (sGuideMountCountdown == 0) {
			SMSSwitch2DArchive("guide", gArBkGuide);
			unkC4 = 0;
		}
	}

	if ((cue & 8) != 0 && unkBC != nullptr && unk10 != 8 && unk10 != 9) {
		SB_LOG_EVERY("guide", 30, "draw state=%u screen=%p", unk10, (void*)unkBC);
		J2DOrthoGraph graph(graphics->getViewport());
		graph.setup2D();
		unkBC->draw(0, 0, &graph);
		graphics->setScissor(graphics->getScissor());
	}

	if ((cue & 1) != 0) {
		if (unk10 == 9) {
			// Retail keeps both cursor layers pinned to the current shine-stage pane while
			// the outgoing gameplay wipe covers the scene (US 0x80179620..0x801796b0).
			const u32 stage = SMS_getShineStage(gpApplication.mCurrArea.getStage());
			if (stage < 14) {
				const JUTRect& bounds = unk168[stage]->getBounds();
				unk128[0]->getPane()->move(bounds.x1 + 6, bounds.y1 - 1);
				unk128[1]->getPane()->move(bounds.x1 + 6, bounds.y1 - 1);
			}
		}
		const bool closeRequested = unk10 == 0
		    && ((unkC0->mEnabledFrameMeaning & TMarioGamePad::MEANING_0x40) != 0
		        || (unkC0->mButton.mTrigger & PAD_TRIGGER_Z) != 0);
		if (unk10 == 0)
			unkC0->onFlag(TMarioGamePad::PAD_FLAG_0x80);

		const sb::guide::Transition transition = sb::guide::step_transition(
		    unk10, unkC5 != 0, gpApplication.mFader->isFullyFadedOut(),
		    gpApplication.mFader->isFullyFadedIn(), closeRequested);
		const u32 oldState = unk10;
		unk10 = transition.next_state;

		if (transition.wipe == sb::guide::kWipeIn5)
			gpApplication.mFader->startWipe(5, 1.0f, 0.0f);
		else if (transition.wipe == sb::guide::kWipeOut6) {
			gpApplication.mFader->startWipe(6, 1.0f, 0.0f);
			unkC0->offFlag(TMarioGamePad::PAD_FLAG_0x80);
		}
		if (transition.clear_selection) {
			unk424 = nullptr;
			unk428 = nullptr;
			unk128[0]->getPane()->setAlpha(0xff);
			unk128[1]->getPane()->setAlpha(0x50);
		}
		if (transition.return_to_gameplay) {
			if (unk424 != nullptr && unk424->getPane()->isVisible())
				unk424->getPane()->hide();
			if (unk428 != nullptr && unk428->getPane()->isVisible())
				unk428->getPane()->hide();
			unkC4 = 1;
		}
		if (unk10 != oldState)
			SB_LOGC("guide", "state %u -> %u", oldState, unk10);
	}
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
