#include <JSystem/JDrama/JDRNameRef.hpp>
#include <JSystem/JDrama/JDRNameRefGen.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstring>
#include <cstdlib>
#include <dolphin/os.h>
#endif

using namespace JDrama;

u16 TNameRef::calcKeyCode(const char* name)
{
	u32 result = 0;
	while ((s32)(*(u8*)name) != 0) {
		u32 tmp = result * 3;
		result  = *(u8*)name++ + tmp;
	}
	return result;
}

const char* TNameRef::getType(JSUMemoryInputStream& param_1,
                              JSUMemoryInputStream& param_2)
{
	u32 x = param_1.readU32();

	// TODO: wtf is happening here
	param_2.setBuffer((u8*)param_1.mBuffer + param_1.mPosition, x - 4);
	param_1.skip(x - 4);

	u32 len = param_2.readU16();
	return param_2.readString();
}

TNameRef* TNameRef::genObject(JSUMemoryInputStream& param_1,
                              JSUMemoryInputStream& param_2)
{
	const char* type = getType(param_1, param_2);
	TNameRef* obj    = TNameRefGen::getInstance()->getNameRef(type);
#ifdef SMS_NATIVE_PLATFORM
	// FAIL FAST: getNameRef returning null means the scene asset references a
	// type no getNameRef case creates. The list loaders silently skip the null
	// child (JDRNameRefPtrList.hpp:29), so the object never enters the tree and a
	// later setup search() returns null -> null deref three frames away (e.g.
	// setup2's TCardLoad "データロード"). Crash here naming the missing type.
	if (obj == nullptr && type != nullptr && type[0] != '\0') {
		// Known-unimplemented types: classes not yet decompiled (TODO cases in
		// the getNameRef tables). The object is skipped; tolerated rather than
		// panicked so boot proceeds, but logged so it's never invisible. Any
		// OTHER null type is a real bug (missing getNameRef case) -> fail fast.
		static const char* const kUnimplemented[] = { "MapObjFlagManager" };
		bool known = false;
		for (unsigned i = 0; i < sizeof(kUnimplemented) / sizeof(*kUnimplemented);
		     ++i) {
			if (strcmp(type, kUnimplemented[i]) == 0) {
				known = true;
				break;
			}
		}
		// Survey mode (SB_GENOBJ_SKIP_ALL=1): skip+log EVERY unknown type instead
		// of panicking, so one boot enumerates the whole missing-type set for a new
		// scene (e.g. the plaza) rather than rediscovering them one panic per run.
		// Diagnostic only — leave the default fail-fast on for normal runs.
		static int skip_all = -1;
		if (skip_all < 0) {
			const char* e = getenv("SB_GENOBJ_SKIP_ALL");
			skip_all = (e && e[0] && e[0] != '0') ? 1 : 0;
		}
		if (known) {
			OSReport("[genObject] skipping unimplemented type \"%s\"\n", type);
		} else if (skip_all) {
			OSReport("[genObject-survey] MISSING type \"%s\"\n", type);
		} else {
			OSPanic(__FILE__, __LINE__,
			        "TNameRef::genObject: no getNameRef case for type \"%s\"",
			        type);
		}
	}
#endif
	return obj;
}

TNameRef::~TNameRef() { }

int TNameRef::getType() const { return 0; }

void TNameRef::load(JSUMemoryInputStream& stream)
{
	mKeyCode = stream.readU16();
	mName    = stream.readString();
}

void TNameRef::save(JSUMemoryOutputStream&) { }

void TNameRef::loadAfter() { }

TNameRef* TNameRef::searchF(u16 key, const char* name)
{
	bool match = false;
	if (mKeyCode == key && strcmp(mName, name) == 0)
		match = true;

	if (match)
		return this;
	else
		return nullptr;
}
