#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DOrthoGraph.hpp>
#include <JSystem/J2D/J2DWindow.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/JKernel/JKRArchive.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <dolphin/gx.h>
#include <dolphin/os.h>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdlib>
#endif

J2DScreen::~J2DScreen() { }

void J2DScreen::makeHiearachyPanes(J2DPane* parent,
                                   JSURandomInputStream* stream,
                                   bool allow_user_panes, bool we_are_root,
                                   bool is_ex, s32* out_next_pane_offset)
{
	s32 nextPaneOffset;
	J2DPane* nextParent = parent;
	if (we_are_root) {
		u32 magic;
		stream->peek(&magic, 4);
		magic  = JSU_BE32(magic); // FourCC read raw (not via readU32) -> swap on LE
		is_ex  = magic == 'SCRN' ? true : false;
		mColor = 0;
		if (is_ex) {
			stream->skip(4);
			u32 magic2 = stream->readU32();
			if (magic2 != 'blo1')
				return;
			stream->skip(0x18);
			u32 magic3 = stream->readU32();
			if (magic3 != 'INF1')
				return;
			stream->skip(4);
			s16 w                = stream->readS16();
			s16 h                = stream->readS16();
			mBounds              = JUTRect(0, 0, w, h);
			mColor               = stream->readU32();
			out_next_pane_offset = &nextPaneOffset;
		}
	}

	while (true) {
		if (is_ex) {
			u32 magic;
			if (stream->read(&magic, 4) != 4) {
				OSPanic("J2DScreen.cpp", 0x91, "SCRN resource is broken.\n");
			}
			magic = JSU_BE32(magic); // FourCC read raw -> swap on LE
			u32 size;
			if (stream->peek(&size, 4) != 4) {
				OSPanic("J2DScreen.cpp", 0x96, "SCRN resource is broken.\n");
			}
			size = JSU_BE32(size); // big-endian block size read raw -> swap on LE
			stream->skip(-4);
			*out_next_pane_offset = size + stream->getPosition();

			switch (magic) {
			case 'EXT1':
				return;
			case 'BGN1':
				stream->seek(*out_next_pane_offset, JSUStreamSeekFrom_SET);
				makeHiearachyPanes(nextParent, stream, allow_user_panes, false,
				                   is_ex, out_next_pane_offset);
				break;
			case 'END1':
				stream->seek(*out_next_pane_offset, JSUStreamSeekFrom_SET);
				return;
			case 'PAN1':
				nextParent = new J2DPane(parent, stream, is_ex);
				break;
			case 'WIN1':
				nextParent = new J2DWindow(parent, stream, is_ex);
				break;
			case 'PIC1':
				nextParent = new J2DPicture(parent, stream, is_ex);
				break;
			case 'TBX1':
				nextParent = new J2DTextBox(parent, stream, is_ex);
				break;
			default:
				if (allow_user_panes)
					nextParent = makeUserPane(mKind, parent, stream);
				break;
			}
			stream->seek(*out_next_pane_offset, JSUStreamSeekFrom_SET);
		} else {
			u16 tag;
			if (stream->peek(&tag, 2) != 2) {
				OSPanic("J2DScreen.cpp", 199, "SCRN resource is broken.\n");
			}
			tag = JSU_BE16(tag); // big-endian tag read raw -> swap on LE
			switch (tag) {
			case 0:
				return;
			case 1:
				stream->skip(4);
				makeHiearachyPanes(nextParent, stream, allow_user_panes, false,
				                   is_ex, nullptr);
				break;
			case 2:
				stream->skip(4);
				return;
			case 0x10:
				nextParent = new J2DPane(parent, stream, is_ex);

				if (we_are_root)
					mBounds = JUTRect(0, 0, nextParent->getWidth(),
					                  nextParent->getHeight());
				break;
			case 0x11:
				nextParent = new J2DWindow(parent, stream, is_ex);
				break;
			case 0x12:
				nextParent = new J2DPicture(parent, stream, is_ex);
				break;
			case 0x13:
				nextParent = new J2DTextBox(parent, stream, is_ex);
				break;
			default:
				nextParent = allow_user_panes
				                 ? makeUserPane(tag, parent, stream)
				                 : stop();
				break;
			}
		}
	}
}

J2DPane* J2DScreen::makeUserPane(u16, J2DPane*, JSURandomInputStream*)
{
	OSPanic(__FILE__, 270, "There is a unknown pane in SCRN resource\n");
	return nullptr;
}

J2DPane* J2DScreen::makeUserPane(u32, J2DPane*, JSURandomInputStream*)
{
	return nullptr;
}

J2DPane* J2DScreen::stop()
{
	OSPanic(__FILE__, 311, "There is a unknown pane in SCRN resource\n");
	return nullptr;
}

void J2DScreen::draw(int x, int y, const J2DGrafContext* pCtx)
{
	if (pCtx) {
		J2DGrafContext ctx(*pCtx);
		J2DPane::draw(x, y, pCtx, mbClipToParent);
		ctx.setScissor();
	} else {
		J2DOrthoGraph graph(0, 0, 640, 480);
		graph.setPort();
		J2DPane::draw(x, y, &graph, mbClipToParent);
		graph.setScissor();
	}
	GXSetNumTexGens(0);
	GXSetNumTevStages(1);
	GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GXSetChanCtrl(GX_COLOR0A0, 0, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetVtxDesc(GX_VA_TEX0, GX_NONE);
	GXSetCullMode(GX_CULL_NONE);
}

#ifdef SMS_NATIVE_PLATFORM
#include <new>
// Region-tolerant pane fallback. The disc is GMSE01 (US) but the engine is the
// GMSJ01/PAL decomp, so US UI archives lack many panes the loaders look up by tag
// (e.g. big_tx_1.blo has 6 'sg' panes vs the 9 the code reserves; option/card screens
// differ further). Rather than null-deref across the hundreds of
// `screen->search(tag)->method()` sites in the GC2D loaders, J2DScreen::search returns
// this shared, hidden, inert pane for a tag that isn't present. It is over-allocated
// past the largest J2D pane type (J2DPicture ~0x160) so the loaders' NON-virtual
// derived accesses — (J2DPicture*)p->setBlendKonstColor / changeTexture / ->mWhite,
// (J2DTextBox*)p->setString / setFont — land within the buffer; base J2DPane virtuals
// dispatch correctly via its real vtable; it is hidden (mVisible=false) and never
// linked into a screen's child tree, so it never draws. Only J2DScreen::search (the
// screen-level entry the loaders use) falls back; the recursive J2DPane::search still
// returns null, so tree traversal and the few null-checking callers are unaffected.
static J2DPane* getRegionTolerantDummyPane()
{
	static unsigned char s_buf[0x200] __attribute__((aligned(16)));
	static J2DPane* s_dummy = nullptr;
	if (!s_dummy) {
		for (unsigned i = 0; i < sizeof(s_buf); ++i)
			s_buf[i] = 0;
		s_dummy = new (s_buf) J2DPane();
		s_dummy->hide();
	}
	return s_dummy;
}
#endif

J2DPane* J2DScreen::search(u32 tag)
{
	if (tag == 0)
		return nullptr;
#ifdef SMS_NATIVE_PLATFORM
	if (J2DPane* p = J2DPane::search(tag))
		return p;
	// FAIL-LOUD (not silent): the dummy-pane fallback masks ANY missing pane, so a
	// genuine bug (wrong archive, parse error, wrong tag) looks identical to a real
	// US-region absence. We can't hard-crash (US archives legitimately lack panes),
	// but a tolerated sentinel must be LOUD — log each missing tag ONCE so masked
	// root causes are visible. SB_PANE_DBG escalates to a panic to pin a specific tag.
	{
		char t[5] = { (char)(tag >> 24), (char)(tag >> 16), (char)(tag >> 8),
		              (char)tag, 0 };
		static u32 s_seen[256];
		static int s_n = 0;
		bool known = false;
		for (int i = 0; i < s_n; ++i)
			if (s_seen[i] == tag) { known = true; break; }
		if (!known) {
			if (s_n < 256)
				s_seen[s_n++] = tag;
			OSReport("[j2d] J2DScreen::search MISSING pane tag '%s' (0x%08x) -> "
			         "region-tolerant dummy\n", t, tag);
			if (getenv("SB_PANE_DBG"))
				OSPanic(__FILE__, __LINE__,
				        "J2DScreen::search: pane '%s' (0x%08x) absent - SB_PANE_DBG", t, tag);
		}
	}
	return getRegionTolerantDummyPane(); // pane absent in a US (GMSE01) UI archive
#else
	return J2DPane::search(tag);
#endif
}

J2DSetScreen::J2DSetScreen(const char* name, JKRArchive* arch)
    : J2DScreen()
{
	u8* res = (u8*)JKRGetNameResource(name, arch);
#ifdef SMS_NATIVE_PLATFORM
	if (::getenv("SB_ARC_DBG"))
		OSReport("[SBDBG] J2DSetScreen('%s', arch=%p) res=%p\n", name, (void*)arch,
		         (void*)res);
#endif
	if (res) {
		u32 sz = JKRFileLoader::getResSize(res, nullptr);
		// Probably an assert
		(void)!res;
		JSUMemoryInputStream stream(res, sz);
		makeHiearachyPanes(this, &stream, false, true, false, nullptr);
	}
#ifdef SMS_NATIVE_PLATFORM
	if (::getenv("SB_BLO_DBG")) {
		// Count panes reachable from the screen root + max tree depth, to detect
		// a truncated/broken hierarchy (created panes that search can't reach).
		struct C { static void walk(J2DPane* p, int d, int& n, int& md) {
			++n; if (d > md) md = d;
			for (JSUTreeIterator<J2DPane> it = p->mPaneTree.getFirstChild();
			     it != p->mPaneTree.getEndChild(); ++it)
				walk(it.getObject(), d + 1, n, md);
		}};
		int n = 0, md = 0; C::walk(this, 0, n, md);
		OSReport("[blo] '%s' reachable panes=%d maxDepth=%d\n", name, n, md);
	}
#endif
	mbClipToParent = false;
}

void J2DScreen::drawSelf(int x, int y, Mtx* mtx)
{
	int trash[0x4]; // ???

	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_SET);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3s16(0, 0, 0);
	GXColor1u32(mColor);
	GXPosition3s16((u32)getWidth(), 0, 0);
	GXColor1u32(mColor);
	GXPosition3s16((u32)getWidth(), (u32)getHeight(), 0);
	GXColor1u32(mColor);
	GXPosition3s16(0, (u32)getHeight(), 0);
	GXColor1u32(mColor);
	GXEnd();
}

static void dummy(J2DSetScreen* s) { s->~J2DSetScreen(); }
