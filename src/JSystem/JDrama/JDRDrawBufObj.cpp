#include <JSystem/JDrama/JDRDrawBufObj.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DDrawBuffer.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DSys.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#include <cstring>
// WEAK: only defined inside the sms-boot executable (native/render/sms_boot_j3d_capture.cpp), not
// in any linkable library — a test target that links this file without the render-capture
// pipeline (e.g. sms-j3dload_test) must still link cleanly. Call sites below null-check first.
extern "C" void sb_boot_capture_set_drawbuf(const char*) __attribute__((weak));  // overbright harness: name the active buffer
extern "C" int  sb_boot_capture_phase() __attribute__((weak));                   // which perform-list phase (6 = GXPost/display)
#endif

using namespace JDrama;

TDrawBufObj::TDrawBufObj()
    : TViewObj("<DrawBufObj>")
    , mDrawBuffer(nullptr)
    , mDrawBufferSize(0)
    , unk18(7)
{
}

TDrawBufObj::TDrawBufObj(u32 param_1, u32 param_2, const char* name)
    : TViewObj(name)
    , mDrawBufferSize(param_2)
    , unk18(param_1)
{
	mDrawBuffer = new J3DDrawBuffer(mDrawBufferSize);
}

#ifdef SMS_NATIVE_PLATFORM
// Registry mapping J3DDrawBuffer* -> owning TDrawBufObj name. Populated at first perform()
// of each buffer; used by SB_ENTRY_MAT (J3DDrawBuffer::entryMatSort) to name the buffer a
// packet is being entered into. Small fixed table (there are ~30 named buffers in the game).
namespace {
	struct DbRegEntry { const void* buf; const char* name; };
	constexpr int kDbRegMax = 64;
	DbRegEntry g_dbreg[kDbRegMax];
	int g_dbreg_n = 0;
	void register_drawbuf(const void* buf, const char* name) {
		if (!buf || !name) return;
		for (int i = 0; i < g_dbreg_n; ++i)
			if (g_dbreg[i].buf == buf) { g_dbreg[i].name = name; return; }
		if (g_dbreg_n < kDbRegMax) { g_dbreg[g_dbreg_n].buf = buf; g_dbreg[g_dbreg_n].name = name; ++g_dbreg_n; }
	}
}
extern "C" const char* sb_boot_drawbuf_name(const void* buf) {
	for (int i = 0; i < g_dbreg_n; ++i)
		if (g_dbreg[i].buf == buf) return g_dbreg[i].name;
	return nullptr;
}
#endif

void TDrawBufObj::load(JSUMemoryInputStream& stream)
{
	TNameRef::load(stream);
	// The on-disc stream is BIG-ENDIAN; these are u32 scalars, so use the byteswap-
	// aware readU32() (not the raw read(), which copies BE bytes into a host LE u32 ->
	// a garbage huge mDrawBufferSize -> J3DDrawBuffer over-allocates and overflows the
	// SolidHeap). On GC (non-native) readU32 is a no-op, matching the original read().
	unk18           = stream.readU32();
	mDrawBufferSize = stream.readU32();
	mDrawBuffer = new J3DDrawBuffer(mDrawBufferSize);
}

void TDrawBufObj::perform(u32 cue, TGraphics* graphics)
{
#ifdef SMS_NATIVE_PLATFORM
	{
		static int dbg = -1;
		if (dbg < 0) { const char* e = getenv("SB_INDIRECT_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
		if (dbg) {
			const char* nm = getName();
			if (nm && std::strstr(nm, "Indirect")) {
				static long n = 0;
				if (n < 40) { ++n;
					fprintf(stderr, "[indirect] %s perform flag=0x%x (frameInit=%d setBuf=%d draw=%d) buf=%p\n",
					        nm, param_1, (int)!!(param_1 & 0x80), (int)!!(param_1 & 0x400),
					        (int)!!(param_1 & 8), (void*)mDrawBuffer);
				}
			}
		}
	}
#endif
#ifdef SMS_NATIVE_PLATFORM
	// Register (mDrawBuffer -> mName) so SB_ENTRY_MAT (in J3DDrawBuffer::entryMatSort) can name
	// the buffer a packet is being entered into. Idempotent; ~30 named buffers in the game.
	register_drawbuf((const void*)mDrawBuffer, getName());
#endif

	if (param_1 & 0x80)
		mDrawBuffer->frameInit();

	if (cue & CUE_SET_DRAW_BUFFER) {
		if (unk18 & 3)
			j3dSys.setDrawBuffer(mDrawBuffer, 0);

		if (unk18 & 4)
			j3dSys.setDrawBuffer(mDrawBuffer, 1);
	}

	if (cue & CUE_DRAW) {
		j3dSys.unk4C = unk18;
#ifdef SMS_NATIVE_PLATFORM
		// STOPGAP (file-select overbright): suppress the GXPost/display-pass (phase 6) redraw of the
		// map translucent buffer "DrawBuf MapXlu". On GC the frame renders across multiple EFB targets:
		// the whole scene (incl. this MapXlu buffer's sea-mask joint, drawn colorUpdate=FALSE as a Z/
		// silhouette mask) renders OFF-SCREEN in the main pass, and the GXPost/display pass RE-ENTERS
		// MapXlu with the reflective water-surface model (a tev=2 ~1352-vert draw) that native does NOT
		// reproduce. Because native never re-enters MapXlu, its buffer still holds the main-pass packets
		// (the sea-MASK, key eb5c8e74 / material c97c48, drawn colorUpdate=TRUE here) — so the display
		// pass paints that mask as a full-screen white overdraw = the file-select overbright wash.
		// VERIFIED by the GX-stream oracle (scratch/passes/oracle_gxdbg.log): the mask appears ONLY in
		// pass2 as [noC][noA] (colorUpdate=FALSE) and NEVER in pass3 (display); the mask is the same
		// packet native also draws (harmlessly, later covered) in the phase-1 scene flush. Until the GC
		// reflective-water re-entry is ported, drop native's stale phase-6 MapXlu redraw so the display
		// matches GC (the correct phase-1 sea shows through). PROPER FIX: port the water-reflection
		// MapXlu re-entry (TModelWaterManager / the pass2→pass3 EFB composite). A/B: SB_KEEP_PH6_MAPXLU=1.
		// 2026-07-02: gate below is DEAD CODE (debug_journal/2026-07-02_overbright_stopgap_is_dead_code.md).
		// SB_DRAWFLAG_DBG proves `DrawBuf MapXlu` fires only at ph0 (setup) and ph1 (unk40 flush),
		// NEVER at ph6 — perform-list routing moved map-translucent draws to `DrawBuf Map 半透明優先`
		// in ph6. Both SB_KEEP_PH6_MAPXLU=1 and unset measure identical mean|Δ|=58.2 on the
		// title-screen overbright harness. Kept commented so the intent survives; bandaid-free
		// fix = port TMapObjSeaIndirect::perform (EFB screen-texture composite).
		// if (&sb_boot_capture_phase && sb_boot_capture_phase() == 6) {
		// 	const char* nm = getName();
		// 	if (nm && std::strcmp(nm, "DrawBuf MapXlu") == 0) {
		// 		static int keep = -1;
		// 		if (keep < 0) { const char* e = getenv("SB_KEEP_PH6_MAPXLU"); keep = (e && e[0] && e[0] != '0') ? 1 : 0; }
		// 		if (!keep) return;
		// 	}
		// }
		// Attribute the shapes this flush captures to THIS draw buffer by name (overbright harness).
		if (&sb_boot_capture_set_drawbuf) sb_boot_capture_set_drawbuf(getName());
		// SB_DRAWFLAG_DBG: which phase actually sets the draw bit (0x8) for each named buffer —
		// settles whether ph1 and ph4 both genuinely draw() (double-buffer handoff, correct) or one
		// of them is a phase-mislabeling artifact.
		if (const char* e = std::getenv("SB_DRAWFLAG_DBG"); e && e[0] && e[0] != '0') {
			static long n = 0; if (n < 400) { ++n;
				std::fprintf(stderr, "[drawflag] phase=%d name='%s' flag=0x%x\n",
				             &sb_boot_capture_phase ? sb_boot_capture_phase() : -1, getName() ? getName() : "?", param_1);
			}
		}
#endif
#ifdef SMS_NATIVE_PLATFORM
		{
			static int dbg = -1;
			if (dbg < 0) { const char* e = getenv("SB_J3D_DBG"); dbg = (e && e[0] && e[0] != '0') ? 1 : 0; }
			static long s_n = 0;
			if (dbg && (++s_n % 2000) == 0)
				fprintf(stderr, "[drawbuf] TDrawBufObj::perform draw() calls=%ld\n", s_n);
		}
#endif
		mDrawBuffer->draw();
	}
}
