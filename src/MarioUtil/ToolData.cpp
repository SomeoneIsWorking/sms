#include <MarioUtil/ToolData.hpp>
#include <macros.h>
#ifdef SMS_NATIVE_PLATFORM
#include <dolphin/os.h>
#include "bcsv_swap.h"
#endif

namespace Koga {
ToolData::ToolData() { mData = nullptr; }
ToolData::~ToolData() { }

BOOL ToolData::Attach(const void* jmapData)
{
	if (!jmapData)
		return FALSE;
#ifdef SMS_NATIVE_PLATFORM
	// Koga::ToolData reads this BCSV/JMap blob directly (no separate parsed
	// copy — GetValue casts straight through mData for the object's whole
	// lifetime), so swap it to host endianness in place here, once, at the
	// single Attach() seam every consumer goes through. See bcsv_swap.h.
	smsport::assets::BcsvSwapResult swapResult
	    = smsport::assets::bcsv_swap_to_host(jmapData);
	if (!swapResult.ok) {
		OSPanic(__FILE__, __LINE__,
		        "Koga::ToolData::Attach: corrupt BCSV field table (field %u, "
		        "JMap type %u out of range 0-7)",
		        swapResult.bad_field_index, (unsigned)swapResult.bad_field_type);
	}
#endif
	this->mData = (JMapData*)jmapData;
	return TRUE;
}

BOOL ToolData::GetValue(int entryIndex, const char* key, s32& pValueOut) const
{
	s32 itemIndex = searchItemInfo(key);
	if (itemIndex < 0) {
		return FALSE;
	}
	return getValue(entryIndex, itemIndex, &pValueOut);
}

BOOL ToolData::GetValue(int entryIndex, const char* key,
                        const char*& pValueOut) const
{
	s32 itemIndex = searchItemInfo(key);
	if (itemIndex < 0) {
		return FALSE;
	}
	return getValue(entryIndex, itemIndex, &pValueOut);
}
} // namespace Koga
