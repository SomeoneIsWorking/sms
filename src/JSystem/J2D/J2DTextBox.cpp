#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/J2D/J2DPrint.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <JSystem/JUtility/JUTResource.hpp>
#include <JSystem/JSupport/JSURandomInputStream.hpp>
#include <dolphin/gx.h>
#ifdef SMS_NATIVE_PLATFORM
#include <stdio.h>
#include <stdlib.h>
#endif

J2DTextBox::J2DTextBox(const ResFONT* font, const char* str)
    : J2DPane()
    , mFont(nullptr)
    , mText(nullptr)
{
	initiate(font, str, HBIND_LEFT, VBIND_TOP);
}

J2DTextBox::J2DTextBox(J2DPane* parent, JSURandomInputStream* stream,
                       bool is_ex)
    : J2DPane(parent, stream, is_ex)
    , mFont(nullptr)
    , mText(nullptr)
{
	JUTResReference res;
	mInfoTag = 0x13;
	if (is_ex) {
		u8 fields = stream->readU8();

		if (ResFONT* font = (ResFONT*)res.getResource(stream, 'FONT', nullptr))
			mFont = new JUTResFont(font, nullptr);

		mCharColor.set(stream->readU32());
		mGradColor.set(stream->readU32());
		u8 bindings = stream->readU8();
		mHBinding   = (J2DTextBoxHBinding)(bindings >> 2 & 3);
		mVBinding   = (J2DTextBoxVBinding)(bindings & 3);
		mCharSpace  = stream->readS16();
		mLineSpace  = stream->readS16();
		mFontSizeX  = stream->readU16();
		mFontSizeY  = stream->readU16();

		s16 len = stream->readU16();
		mText   = new char[len + 1];
		stream->read(mText, len);
		mText[len] = 0;

		fields -= 10;

		if (fields != 0) {
			if (stream->readU8()) {
				setConnectParent(true);
			}
			--fields;
		}

		mBlack = 0;
		mWhite = 0xffffffff;

		if (fields != 0) {
			mBlack.set(stream->readU32());
			--fields;
		}
		if (fields != 0) {
			mWhite.set(stream->readU32());
		}
		unk104 = 0;
		unk108 = 0;
		stream->align(4);
	} else {
		if (ResFONT* font = (ResFONT*)res.getResource(stream, 'FONT', nullptr))
			mFont = new JUTResFont(font, nullptr);

		mCharColor.set(stream->readU32());
		mGradColor.set(stream->readU32());

		u8 flags  = stream->readU8();
		mHBinding = (J2DTextBoxHBinding)(flags & 0x7f);
		mVBinding = (J2DTextBoxVBinding)(stream->readU8());
		if (!mFont) {
			mCharSpace = 0;
			mLineSpace = 0;
			mFontSizeX = 0;
			mFontSizeY = 0;
		} else {
			if ((flags & 0x80) != 0) {
				mCharSpace = stream->readS16();
				mLineSpace = stream->readS16();
				mFontSizeX = stream->readU16();
				mFontSizeY = stream->readU16();
			} else {
				mCharSpace = 0;
				mLineSpace = mFont->getLeading();
				mFontSizeX = mFont->getWidth();
				mFontSizeY = mFont->getHeight();
			}
		}

		s16 len = stream->readS16();
		mText   = new char[len + 1];
		stream->read(mText, len);
		mText[len] = 0;

		mBlack = 0;
		mWhite = 0xffffffff;
		unk104 = 0;
		unk108 = 0;

		stream->align(4);
	}
	mTextFontOwned = true;
}

J2DTextBox::J2DTextBox(u32 tag, const JUTRect& bounds, const ResFONT* font,
                       const char* str, J2DTextBoxHBinding hbind,
                       J2DTextBoxVBinding vbind)
    : J2DPane(0x13, tag, bounds)
    , mFont(nullptr)
    , mText(nullptr)
{
	initiate(font, str, hbind, vbind);
}

void J2DTextBox::initiate(const ResFONT* font, const char* str,
                          J2DTextBoxHBinding hbind, J2DTextBoxVBinding vbind)
{
	if (font != nullptr)
		mFont = new JUTResFont(font, nullptr);

	mCharColor.set(0xFFFFFFFF);
	mGradColor.set(0xFFFFFFFF);
	mBlack      = 0;
	mWhite      = 0xFFFFFFFF;
	mHBinding   = hbind;
	mVBinding   = vbind;
#ifdef SMS_NATIVE_PLATFORM
	if (!str) // region-tolerant: a missing message (e.g. a US bmg lacking a JP/PAL
		str = ""; // index) yields null from SMSGetMessageData -> treat as empty text
#endif
	size_t temp = strlen(str);
	mText       = new char[temp + 1];
	strcpy(mText, str);
	unk104     = 0;
	unk108     = 0;
	mCharSpace = 0;
	if (mFont == nullptr) {
		mLineSpace = 0;
		mFontSizeX = 0;
		mFontSizeY = 0;
	} else {
		mLineSpace = mFont->getLeading();
		mFontSizeX = mFont->getWidth();
		mFontSizeY = mFont->getHeight();
	}
	mKind          = 'TBX1';
	mTextFontOwned = true;
}

J2DTextBox::~J2DTextBox()
{
	if (mTextFontOwned && mFont)
		delete mFont;

	if (mText)
		delete[] mText;
}

void J2DTextBox::setFont(JUTFont* font)
{
	if (!font)
		return;

	if (mTextFontOwned)
		delete mFont;

	mFont          = font;
	mTextFontOwned = false;
}

void J2DTextBox::draw(int x, int y)
{
	if (!mVisible)
		return;
	J2DPrint print(mFont, mCharSpace, mLineSpace, mCharColor, mGradColor);
	print.setFontSize(mFontSizeX, mFontSizeY);
	print.setSomeColors(mBlack, mWhite);
	print.initiate();
	makeMatrix(x, y);
	GXLoadPosMtxImm(mPositionMtx, GX_PNMTX0);
	GXSetCurrentMtx(GX_PNMTX0);
	print.print(0, 0, mColorAlpha, "%s", mText);
	Mtx mtx;
	MTXIdentity(mtx);
	GXLoadPosMtxImm(mtx, GX_PNMTX0);
}

char* J2DTextBox::getStringPtr() const { return mText; }

size_t J2DTextBox::setString(const char* str, ...)
{
	va_list args;
	va_start(args, str);

	// They... they didn't implement the va args printf-ing...

	if (mText)
		delete[] mText;

#ifdef SMS_NATIVE_PLATFORM
	if (!str) // region-tolerant: null message (missing US bmg index) -> empty text
		str = "";
#endif
	size_t sz = strlen(str);
	mText     = new char[sz + 1];
	strcpy(mText, str);

	va_end(args);
	return sz;
}

bool J2DTextBox::setConnectParent(bool connected)
{
	if (getPaneTree()->getParent() == nullptr)
		return false;
	if (getPaneTree()->getParent()->getObject()->mInfoTag != 17)
		return false;
	mConnected = connected;
	return connected;
}

void J2DTextBox::drawSelf(int x, int y)
{
	Mtx id;
	MTXIdentity(id);
	drawSelf(x, y, &id);
}

void J2DTextBox::drawSelf(int x, int y, Mtx* mtx)
{
#ifdef SMS_NATIVE_PLATFORM
	// SB_TBX_DBG: trace textbox glyph-draw path for the file-select banner. Filtered to
	// non-empty strings so the per-frame spam stays readable; tells us whether drawSelf is
	// even reached, mFont is valid, and the size/color/visibility are sane for the banner.
	if (getenv("SB_TBX_DBG") && mText && mText[0]) {
		static int once = 0;
		if (once < 24) {
			fprintf(stderr, "[tbx] drawSelf x=%d y=%d vis=%d font=%p text=\"%.20s\" "
			        "szX=%d szY=%d charCol=0x%08x alpha=%d bnd=(%d,%d %dx%d)\n",
			        x, y, (int)mVisible, (void*)mFont, mText, mFontSizeX, mFontSizeY,
			        (unsigned)mCharColor, (int)mColorAlpha, mBounds.x1, mBounds.y1,
			        mBounds.getWidth(), mBounds.getHeight());
			fprintf(stderr, "[tbx]   mGlobalMtx [%.3f %.3f %.3f %.3f][%.3f %.3f %.3f %.3f]"
			        "[%.3f %.3f %.3f %.3f]\n",
			        mGlobalMtx[0][0], mGlobalMtx[0][1], mGlobalMtx[0][2], mGlobalMtx[0][3],
			        mGlobalMtx[1][0], mGlobalMtx[1][1], mGlobalMtx[1][2], mGlobalMtx[1][3],
			        mGlobalMtx[2][0], mGlobalMtx[2][1], mGlobalMtx[2][2], mGlobalMtx[2][3]);
			++once;
		}
	}
#endif
	J2DPrint print(mFont, mCharSpace, mLineSpace, mCharColor, mGradColor);
	print.setFontSize(mFontSizeX, mFontSizeY);
	print.setSomeColors(mBlack, mWhite);
	print.initiate();
	Mtx transform;
	MTXConcat(*mtx, mGlobalMtx, transform);
	GXLoadPosMtxImm(transform, GX_PNMTX0);
	print.locate(x, y);
	print.printReturn(mText, mBounds.getWidth(), mBounds.getHeight(), mHBinding,
	                  mVBinding, unk104, unk108, mColorAlpha);
}

void J2DTextBox::resize(int w, int h)
{
	if (mConnected && getPaneTree() != nullptr
	    && getPaneTree()->getParent() != nullptr) {
		J2DPane* pane = getPaneTree()->getParent()->getObject();

		if (pane->mInfoTag == 0x11) {
			int newW = pane->getWidth() + (w - getWidth());
			int newH = pane->getHeight() + (h - getHeight());
			pane->resize(newW, newH);
			return;
		}
	}

	J2DPane::resize(w, h);
}
