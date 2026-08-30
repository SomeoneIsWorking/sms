#include <JSystem/J2D/J2DWindow.hpp>
#include <JSystem/JSupport/JSURandomInputStream.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#include <JSystem/JUtility/JUTPalette.hpp>
#include <JSystem/JUtility/JUTResource.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>
#include <dolphin/gx.h>
#ifdef SMS_NATIVE_PLATFORM
#include <sb_native_j2d.h>
#endif

// NOTE: for .sdata ordering
static void dummy(float* f) { *f = 1.0f; }

J2DWindow::J2DWindow(const ResTIMG* timg1, const ResTIMG* timg2,
                     const ResTIMG* timg3, const ResTIMG* timg4)
{
}

J2DWindow::J2DWindow(const char* name1, const char* name2, const char* name3,
                     const char* name4)
{
}

J2DWindow::J2DWindow(const ResTIMG* timg, J2DTextureBase base) { }

J2DWindow::J2DWindow(const char* name, J2DTextureBase base) { }

J2DWindow::J2DWindow(J2DPane* parent, JSURandomInputStream* stream, bool is_ex)
    : J2DPane(parent, stream, is_ex)
    , mPalette(nullptr)
    , mFrameTextureTopLeft(nullptr)
    , mFrameTextureTopRight(nullptr)
    , mFrameTextureBottomLeft(nullptr)
    , mFrameTextureBottomRight(nullptr)
{
	JUTResReference res;
	mInfoTag = 0x11;
	if (is_ex) {
		u8 fields = stream->readU8();

		mContentsBounds.x1 = stream->readU16();
		mContentsBounds.y1 = stream->readU16();
		mContentsBounds.x2 = mContentsBounds.x1 + stream->readU16();
		mContentsBounds.y2 = mContentsBounds.y1 + stream->readU16();

		if (ResTIMG* timg = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
			mFrameTextureTopLeft = new Texture(timg);
		if (ResTIMG* timg = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
			mFrameTextureTopRight = new Texture(timg);
		if (ResTIMG* timg = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
			mFrameTextureBottomLeft = new Texture(timg);
		if (ResTIMG* timg = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
			mFrameTextureBottomRight = new Texture(timg);
		if (ResTLUT* tlut = (ResTLUT*)res.getResource(stream, 'TLUT', nullptr))
			mPalette = new JUTPalette(GX_TLUT0, tlut);

		mMirrorFlags = stream->readU8();
		mContentsColorTopLeft.set(stream->readU32());
		mContentsColorTopRight.set(stream->readU32());
		mContentsColorBottomLeft.set(stream->readU32());
		mContentsColorBottomRight.set(stream->readU32());
		fields -= 14;

		mContentsTexture = nullptr;
		if (fields) {
			if (ResTIMG* timg
			    = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
				mContentsTexture = new Texture(timg);
			fields--;
		}
		mFrameBlack = 0;
		mFrameWhite = 0xffffffff;
		if (fields) {
			mFrameBlack = stream->readU32();
			fields--;
		}
		if (fields) {
			mFrameWhite = stream->readU32();
		}
		stream->align(4);
	} else {
		mContentsBounds.x1 = stream->readU16();
		mContentsBounds.y1 = stream->readU16();
		mContentsBounds.x2 = mContentsBounds.x1 + stream->readU16();
		mContentsBounds.y2 = mContentsBounds.y1 + stream->readU16();

		if (ResTIMG* timg = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
			mFrameTextureTopLeft = new Texture(timg);
		if (ResTIMG* timg = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
			mFrameTextureTopRight = new Texture(timg);
		if (ResTIMG* timg = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
			mFrameTextureBottomLeft = new Texture(timg);
		if (ResTIMG* timg = (ResTIMG*)res.getResource(stream, 'TIMG', nullptr))
			mFrameTextureBottomRight = new Texture(timg);
		if (ResTLUT* tlut = (ResTLUT*)res.getResource(stream, 'TLUT', nullptr))
			mPalette = new JUTPalette(GX_TLUT0, tlut);

		mMirrorFlags = stream->readU8();
		mContentsColorTopLeft.set(stream->readU32());
		mContentsColorTopRight.set(stream->readU32());
		mContentsColorBottomLeft.set(stream->readU32());
		mContentsColorBottomRight.set(stream->readU32());

		stream->align(4);
		mContentsTexture = nullptr;
		mFrameBlack      = 0x0;
		mFrameWhite      = 0xffffffff;
	}
	if (mFrameTextureTopLeft && mFrameTextureTopRight && mFrameTextureBottomLeft
	    && mFrameTextureBottomRight) {
		mMinimumWidth  = mFrameTextureTopLeft->getWidth()
		                 + mFrameTextureTopRight->getWidth();
		mMinimumHeight = mFrameTextureTopLeft->getHeight()
		                 + mFrameTextureBottomLeft->getHeight();
	} else {
		mMinimumWidth  = 1;
		mMinimumHeight = 1;
	}
}

J2DWindow::J2DWindow(u32 tag, const JUTRect& bounds, const ResTIMG* timg1,
                     const ResTIMG* timg2, const ResTIMG* timg3,
                     const ResTIMG* timg4, const ResTLUT* tlut)
{
}

J2DWindow::J2DWindow(u32 tag, const JUTRect& bounds, const char* name1,
                     const char* name2, const char* name3, const char* name4,
                     const ResTLUT* tlut)
{
}

J2DWindow::J2DWindow(u32 tag, const JUTRect& bounds, const ResTIMG* timg,
                     J2DTextureBase base, const ResTLUT* tlut)
{
}

J2DWindow::J2DWindow(u32 tag, const JUTRect& bounds, const char* name,
                     J2DTextureBase base, const ResTLUT* tlut)
{
}

void J2DWindow::initiate(const ResTIMG* timg1, const ResTIMG* timg2,
                         const ResTIMG* timg3, const ResTIMG* timg4,
                         const ResTLUT* tlut, J2DWindowMirror mirror,
                         const JUTRect& bounds)
{
}

void J2DWindow::initiateColor() { }

J2DWindowMirror J2DConvertMirror(J2DTextureBase base)
{
	return J2DWindowMirror();
}

J2DWindow::~J2DWindow()
{
	if (mFrameTextureTopLeft)
		delete mFrameTextureTopLeft;
	if (mFrameTextureTopRight)
		delete mFrameTextureTopRight;
	if (mFrameTextureBottomLeft)
		delete mFrameTextureBottomLeft;
	if (mFrameTextureBottomRight)
		delete mFrameTextureBottomRight;
	if (mPalette)
		delete mPalette;
	if (mContentsTexture)
		delete mContentsTexture;
}

void J2DWindow::draw(const JUTRect& param_1) { }

void J2DWindow::draw_private(const JUTRect& param_1, const JUTRect& param_2,
                             Mtx* param_3)
{
	if (param_1.getWidth() >= mMinimumWidth
	    && param_1.getHeight() >= mMinimumHeight) {
#ifdef SMS_NATIVE_PLATFORM
		sb_native_window_submit(this, &param_1, &param_2, param_3);
#endif
		Mtx afStack_50;
		MTXConcat(*param_3, mGlobalMtx, afStack_50);
		GXLoadPosMtxImm(afStack_50, GX_PNMTX0);
		drawContents(param_2);
		GXClearVtxDesc();
		GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GXSetNumTexGens(1);
		if (mFrameTextureTopLeft && mFrameTextureTopRight
		    && mFrameTextureBottomLeft && mFrameTextureBottomRight) {
			int iVar6
			    = param_1.getWidth() - mFrameTextureBottomRight->getWidth();
			int iVar8
			    = param_1.getHeight() - mFrameTextureBottomRight->getHeight();
			u32 topLeftWidth  = mFrameTextureTopLeft->getWidth();
			u32 topLeftHeight = mFrameTextureTopLeft->getHeight();
			mFrameTextureTopLeft->draw(0, 0, !!(mMirrorFlags & 0x80),
			                           !!(mMirrorFlags & 0x40), mColorAlpha,
			                           mFrameBlack, mFrameWhite);
			mFrameTextureTopRight->draw(iVar6, 0, !!(mMirrorFlags & 0x20),
			                            !!(mMirrorFlags & 0x10), mColorAlpha,
			                            mFrameBlack, mFrameWhite);
			mFrameTextureBottomLeft->draw(0, iVar8, !!(mMirrorFlags & 8),
			                              !!(mMirrorFlags & 4), mColorAlpha,
			                              mFrameBlack, mFrameWhite);
			mFrameTextureBottomRight->draw(iVar6, iVar8, !!(mMirrorFlags & 2),
			                               !!(mMirrorFlags & 1), mColorAlpha,
			                               mFrameBlack, mFrameWhite);

			u16 a, b, c, d, e;

			b = a = (mMirrorFlags & 0x20) ? (u16)0x8000 : (u16)0;
			c     = (mMirrorFlags & 0x10) ? (u16)0 : (u16)0x8000;
			d     = c ^ 0x8000;
			mFrameTextureTopRight->draw(topLeftWidth, 0, iVar6 - topLeftWidth,
			                            mFrameTextureTopRight->getHeight(), b,
			                            c, a, d, mColorAlpha, mFrameBlack,
			                            mFrameWhite);

			d = a = (mMirrorFlags & 0x2) ? (u16)0x8000 : (u16)0;
			b     = (mMirrorFlags & 0x1) ? (u16)0 : (u16)0x8000;
			e     = b ^ 0x8000;
			mFrameTextureBottomRight->draw(
			    topLeftWidth, iVar8, iVar6 - topLeftWidth,
			    mFrameTextureBottomRight->getHeight(), d, b, a, e, mColorAlpha,
			    mFrameBlack, mFrameWhite);

			a = (mMirrorFlags & 0x8) ? (u16)0 : (u16)0x8000;
			b = a ^ 0x8000;
			d = c = (mMirrorFlags & 0x4) ? (u16)0x8000 : (u16)0;
			mFrameTextureBottomLeft->draw(
			    0, topLeftHeight, mFrameTextureBottomLeft->getWidth(),
			    iVar8 - topLeftHeight, a, d, b, c, mColorAlpha, mFrameBlack,
			    mFrameWhite);

			a = (mMirrorFlags & 0x2) ? (u16)0 : (u16)0x8000;
			b = a ^ 0x8000;
			d = c = (mMirrorFlags & 0x1) ? (u16)0x8000 : (u16)0;
			mFrameTextureBottomRight->draw(
			    iVar6, topLeftHeight, mFrameTextureBottomRight->getWidth(),
			    iVar8 - topLeftHeight, a, d, b, c, mColorAlpha, mFrameBlack,
			    mFrameWhite);
		}
		GXSetNumTexGens(0);
		GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
		              GX_COLOR0A0);
		GXSetVtxDesc(GX_VA_TEX0, GX_NONE);
	}
}

void J2DWindow::draw(const JUTRect& param_1, const JUTRect& param_2) { }

void J2DWindow::resize(int width, int height)
{
	int oldW = getWidth();
	int oldH = getHeight();
	J2DPane::resize(width, height);
	mContentsBounds.x2 += width - oldW;
	mContentsBounds.y2 += height - oldH;

	for (JSUTreeIterator<J2DPane> iter(getPaneTree()->getFirstChild());
	     iter != getPaneTree()->getEndChild(); iter++) {
		if (iter->getTag() == 0x13 && iter->isConnectParent()) {
			int nw = width - oldW + iter->getWidth();
			int nh = height - oldH + iter->getHeight();
			iter->J2DPane::resize(nw, nh);
		}
	}
}

void J2DWindow::setContentsColor(JUtility::TColor c1, JUtility::TColor c2,
                                 JUtility::TColor c3, JUtility::TColor c4)
{
}

void J2DWindow::drawSelf(int x, int y)
{
	Mtx id;
	MTXIdentity(id);
	drawSelf(x, y, &id);
}

void J2DWindow::drawSelf(int x, int y, Mtx* mtx)
{
	JUTRect tmp(mGlobalBounds.x1, mGlobalBounds.y1, mGlobalBounds.x2,
	            mGlobalBounds.y2);
	tmp.add(x, y);
	draw_private(tmp, mContentsBounds, mtx);
	clip(mContentsBounds);
}

void J2DWindow::drawContents(const JUTRect& rect)
{
	if (!rect.isEmpty()) {
		GXClearVtxDesc();
		GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetNumChans(1);
		GXSetNumTexGens(0);
		GXSetNumTevStages(1);
		GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
		              GX_COLOR0A0);
		GXSetChanCtrl(GX_COLOR0A0, 0, GX_SRC_REG, GX_SRC_VTX, 0, GX_DF_NONE,
		              GX_AF_NONE);
		if ((mContentsColorTopLeft & 0xff) == 0xff
		    && (mContentsColorTopRight & 0xff) == 0xff
		    && (mContentsColorBottomLeft & 0xff) == 0xff
		    && (mContentsColorBottomRight & 0xff) == 0xff
		    && mColorAlpha == 0xff) {
			GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);
		} else {
			GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
			               GX_LO_SET);
		}
		JUtility::TColor col1 = mContentsColorTopLeft;
		JUtility::TColor col2 = mContentsColorBottomLeft;
		JUtility::TColor col3 = mContentsColorTopRight;
		JUtility::TColor col4 = mContentsColorBottomRight;
		if (mColorAlpha != 0xff) {
			col1.a = (u8)((col1.a * mColorAlpha) / 0xff);
			col2.a = (u8)((col2.a * mColorAlpha) / 0xff);
			col3.a = (u8)((col3.a * mColorAlpha) / 0xff);
			col4.a = (u8)((col4.a * mColorAlpha) / 0xff);
		}
		GXBegin(GX_QUADS, GX_VTXFMT0, 4);
		GXPosition3s16(rect.x1, rect.y1, 0);
		GXColor1u32(col1);
		GXPosition3s16(rect.x2, rect.y1, 0);
		GXColor1u32(col3);
		GXPosition3s16(rect.x2, rect.y2, 0);
		GXColor1u32(col4);
		GXPosition3s16(rect.x1, rect.y2, 0);
		GXColor1u32(col2);
		GXEnd();
		if (mContentsTexture) {
			GXClearVtxDesc();
			GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
			GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
			GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
			GXSetNumTexGens(1);
			mContentsTexture->drawContentsTexture(
			    rect.x1, rect.y1, rect.x2 - rect.x1, rect.y2 - rect.y1,
			    mColorAlpha);
		}
	}
}

void J2DWindow::Texture::draw(int x, int y, int width, int height, u16 param_5,
                              u16 param_6, u16 param_7, u16 param_8, u8 param_9,
                              JUtility::TColor param_10,
                              JUtility::TColor param_11)
{
	int x2 = x + width;
	int y2 = y + height;

	load(GX_TEXMAP0);
	setTevMode(param_9, param_10, param_11);
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3s16(x, y, 0);
	GXParam1s32(-1);
	GXTexCoord2u16(param_7, param_8);
	GXPosition3s16(x2, y, 0);
	GXParam1s32(-1);
	GXTexCoord2u16(param_5, param_8);
	GXPosition3s16(x2, y2, 0);
	GXParam1s32(-1);
	GXTexCoord2u16(param_5, param_6);
	GXPosition3s16(x, y2, 0);
	GXParam1s32(-1);
	GXTexCoord2u16(param_7, param_6);
	GXEnd();
}

void J2DWindow::Texture::draw(int x, int y, bool param_3, bool param_4,
                              u8 param_5, JUtility::TColor param_6,
                              JUtility::TColor param_7)
{
	draw(x, y, getWidth(), getHeight(), param_3 ? 0 : 0x8000,
	     param_4 ? 0 : 0x8000, param_3 ? 0x8000 : 0, param_4 ? 0x8000 : 0,
	     param_5, param_6, param_7);
}

void J2DWindow::Texture::drawContentsTexture(int x, int y, int width,
                                             int height, u8 param_5)
{
	int x2 = x + width;
	int y2 = y + height;

	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_CLR_RGBA, GX_F32, 0);

	f32 w  = (f32)getWidth();
	f32 h  = (f32)getHeight();
	f32 u1 = -((f32)width / w - 1.0f) / 2.0f;
	f32 v1 = -((f32)height / h - 1.0f) / 2.0f;
	f32 u2 = u1 + (f32)width / w;
	f32 v2 = v1 + (f32)height / h;

	load(GX_TEXMAP0);
	setTevMode(param_5, 0, 0xffffffff);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3s16(x, y, 0);
	GXParam1s32(-1);
	GXTexCoord2f32(u1, v1);
	GXPosition3s16(x2, y, 0);
	GXParam1s32(-1);
	GXTexCoord2f32(u2, v1);
	GXPosition3s16(x2, y2, 0);
	GXParam1s32(-1);
	GXTexCoord2f32(u2, v2);
	GXPosition3s16(x, y2, 0);
	GXParam1s32(-1);
	GXTexCoord2f32(u1, v2);
	GXEnd();

	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_CLR_RGBA, GX_RGBX8, 15);
}

void J2DWindow::Texture::setTevMode(u8 opacity, JUtility::TColor black,
                                    JUtility::TColor white)
{
	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, 0x3c, 0, 0x7d);
	GXSetChanCtrl(GX_COLOR0A0, 0, GX_SRC_REG, GX_SRC_REG, 0, GX_DF_NONE,
	              GX_AF_NONE);
	GXSetChanMatColor(GX_COLOR0A0, (GXColor) { 0xff, 0xff, 0xff, opacity });
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	if (opacity == 0xff && black == 0 && white == 0xffffffff) {
		int alpha = getTransparency();
		GXSetNumTevStages(1);
		if (field_0x2c != nullptr) {
			alpha = field_0x2c->getTransparency();
		}

		if (alpha == 0) {
			GXSetTevColor(GX_TEVREG2, JUtility::TColor());
			GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
			                GX_CC_ZERO);
			GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A2, GX_CA_ZERO, GX_CA_ZERO,
			                GX_CA_ZERO);
			GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
			                1, GX_TEVPREV);
			GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
			                1, GX_TEVPREV);
			GXSetBlendMode(GX_BM_NONE, GX_BL_ONE, GX_BL_ZERO, GX_LO_SET);
		} else {
			GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
			GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
			               GX_LO_SET);
		}
	} else {
		u8 stage = GX_TEVSTAGE1;
		GXSetTevColor(GX_TEVREG2, JUtility::TColor());
		GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_ZERO,
		                GX_CC_ZERO);

		if (getTransparency() != 0)
			GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO,
			                GX_CA_ZERO);
		else
			GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A2, GX_CA_ZERO, GX_CA_ZERO,
			                GX_CA_ZERO);

		GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
		                GX_TEVPREV);
		GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, 1,
		                GX_TEVPREV);

		if (black != 0 || white != 0xffffffff) {
			GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
			              GX_COLOR_NULL);
			GXSetTevColor(GX_TEVREG0, black);
			GXSetTevColor(GX_TEVREG1, white);
			GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_C0, GX_CC_C1, GX_CC_CPREV,
			                GX_CC_ZERO);
			GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_A0, GX_CA_A1, GX_CA_APREV,
			                GX_CA_ZERO);
			GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
			                1, GX_TEVPREV);
			GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1,
			                1, GX_TEVPREV);
			stage = GX_TEVSTAGE2;
		}

		if (opacity != 0xff) {
			GXSetTevOrder((GXTevStageID)stage, GX_TEXCOORD_NULL, GX_TEXMAP_NULL,
			              GX_COLOR0A0);
			GXSetTevColorIn((GXTevStageID)stage, GX_CC_CPREV, GX_CC_ZERO,
			                GX_CC_ZERO, GX_CC_ZERO);
			GXSetTevAlphaIn((GXTevStageID)stage, GX_CA_ZERO, GX_CA_APREV,
			                GX_CA_RASA, GX_CA_ZERO);
			GXSetTevColorOp((GXTevStageID)stage, GX_TEV_ADD, GX_TB_ZERO,
			                GX_CS_SCALE_1, 1, GX_TEVPREV);
			GXSetTevAlphaOp((GXTevStageID)stage, GX_TEV_ADD, GX_TB_ZERO,
			                GX_CS_SCALE_1, 1, GX_TEVPREV);
			stage++;
		}

		GXSetNumTevStages(stage);
		GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA,
		               GX_LO_SET);
	}
}
