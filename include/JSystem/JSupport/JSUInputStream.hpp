#ifndef JSU_INPUT_STREAM_H
#define JSU_INPUT_STREAM_H

#include <types.h>
#include <JSystem/JSupport/JSUIosBase.hpp>

// JSU streams read serialized GC asset data, which is big-endian. read()/readData()
// copy raw bytes, so the multi-byte scalar readers must byteswap to host endian on a
// little-endian host (else counts/offsets/pointers in e.g. the JDrama NameRef tree in
// stageArc.bin come back byteswapped -> wild pointers). u8/raw reads are unaffected.
#ifdef SMS_NATIVE_PLATFORM
#define JSU_BE16(x) __builtin_bswap16(x)
#define JSU_BE32(x) __builtin_bswap32(x)
#define JSU_BE64(x) __builtin_bswap64(x)
#else
#define JSU_BE16(x) (x)
#define JSU_BE32(x) (x)
#define JSU_BE64(x) (x)
#endif

class JSUInputStream : public JSUIosBase {
public:
	virtual ~JSUInputStream();
	virtual int getAvailable() const = 0;
	virtual int skip(s32 amount);
	virtual int readData(void* buf, s32 size) = 0;

	u32 read(void* buf, s32 size);
	char* read(char* buf);
	char* readString();
	char* readString(char* buf, u16 len);

	u32 read(s8& p) { return read(&p, sizeof(s8)); }
	u32 read(u8& p) { return read(&p, sizeof(u8)); }
	u32 read(bool& p) { return read(&p, sizeof(bool)); }
	u32 read(s16& p) { return read(&p, sizeof(s16)); }
	u32 read(u16& p) { return read(&p, sizeof(u16)); }
	u32 read(s32& p) { return read(&p, sizeof(s32)); }
	u32 read(u32& p) { return read(&p, sizeof(u32)); }
	u32 read(s64& p) { return read(&p, sizeof(s64)); }
	u32 read(u64& p) { return read(&p, sizeof(u64)); }

	u8 read8b()
	{
		u8 b;
		read(&b, sizeof(u8));
		return b;
	}

	u16 read16b()
	{
		u16 s;
		read(&s, sizeof(u16));
		return JSU_BE16(s);
	}

	u32 read32b()
	{
		u32 i;
		read(&i, sizeof(u32));
		return JSU_BE32(i);
	}

	/* @fabricated */
	s8 readS8()
	{
		s8 b;
		read(&b, sizeof(s8));
		return b;
	}

	u8 readU8()
	{
		u8 b;
		read(&b, sizeof(u8));
		return b;
	}

	s16 readS16()
	{
		s16 s;
		read(&s, sizeof(s16));
		return (s16)JSU_BE16((u16)s);
	}

	u16 readU16()
	{
		u16 s;
		read(&s, sizeof(u16));
		return JSU_BE16(s);
	}

	s32 readS32()
	{
		s32 i;
		read(&i, sizeof(s32));
		return (s32)JSU_BE32((u32)i);
	}

	u32 readU32()
	{
		u32 i;
		read(&i, sizeof(u32));
		return JSU_BE32(i);
	}

	u64 readU64()
	{
		u64 i;
		read(&i, sizeof(u64));
		return JSU_BE64(i);
	}

	/* @fabricated */
	f32 readF32()
	{
		f32 f;
		read(&f, sizeof(f32));
		return f;
	}

	JSUInputStream& operator>>(bool& p)
	{
		read(&p, sizeof(bool));
		return *this;
	}

	JSUInputStream& operator>>(s8& p)
	{
		read(&p, sizeof(s8));
		return *this;
	}

	JSUInputStream& operator>>(u8& p)
	{
		read(&p, sizeof(u8));
		return *this;
	}

	// NB: like read16b/read32b above, the multi-byte operator>> overloads read
	// raw BE asset bytes and must byteswap to host endian on a little-endian host
	// (JSU_BE* is identity on BE). On the GameCube these were plain raw reads; on
	// our LE port a missing swap returns byteswapped scalars (e.g. TCameraMapTool
	// mMode 22 -> 0x16000000 -> OOB mSaveKindParam[] fetch).
	JSUInputStream& operator>>(s16& p)
	{
		read(&p, sizeof(s16));
		p = (s16)JSU_BE16((u16)p);
		return *this;
	}

	JSUInputStream& operator>>(u16& p)
	{
		read(&p, sizeof(u16));
		p = JSU_BE16(p);
		return *this;
	}

	JSUInputStream& operator>>(s32& p)
	{
		read(&p, sizeof(s32));
		p = (s32)JSU_BE32((u32)p);
		return *this;
	}

	JSUInputStream& operator>>(u32& p)
	{
		read(&p, sizeof(u32));
		p = JSU_BE32(p);
		return *this;
	}

	JSUInputStream& operator>>(f32& p)
	{
		read(&p, sizeof(f32));
#ifdef SMS_NATIVE_PLATFORM
		u32 bits;
		__builtin_memcpy(&bits, &p, sizeof(bits));
		bits = __builtin_bswap32(bits);
		__builtin_memcpy(&p, &bits, sizeof(bits));
#endif
		return *this;
	}
};

#endif
