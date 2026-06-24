#include <System/Params.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#include <cstring>
const char* g_sb_prm_dbg_name = nullptr; // set by load(filename) so load(stream) can name entries
#endif

static const char SceneParamsDir[] = "/map/params";

JKRFileLoader* TParams::mArc      = nullptr;
JKRFileLoader* TParams::mSceneArc = nullptr;

void TParams::load(JSUMemoryInputStream& stream)
{
	if (mHead != nullptr) {
		s32 length = stream.readS32();
		for (int i = 0; i < length; i++) {
			u16 keyCode = stream.read16b();
			char buffer[0x50];
			stream.readString(buffer, 0x50);

			TBaseParam* param;
			for (param = mHead; param != nullptr; param = param->next) {
				if (keyCode == param->keyCode && !strcmp(buffer, param->name)) {
#ifdef SMS_NATIVE_PLATFORM
					if (g_sb_prm_dbg_name && getenv("SB_PRM_DBG")
					    && strstr(g_sb_prm_dbg_name, "Option")) {
						s32 pos = stream.getPosition();
						stream.skip(4);
						u32 bits = stream.read32b();
						f32 v;
						__builtin_memcpy(&v, &bits, 4);
						stream.seekPos(pos, JSUStreamSeekFrom_SET);
						fprintf(stderr, "[prm] %s: '%s' key=0x%x val=%.2f (0x%08x)\n",
						        g_sb_prm_dbg_name, buffer, keyCode, v, bits);
					}
#endif
					param->load(stream);
					break;
				}
			}
			if (param == nullptr) {
				s32 end = stream.read32b();
				stream.skip(end);
			}
		}
	}
}

void TParams::init()
{
	mArc      = JKRFileLoader::getVolume("params");
	mSceneArc = JKRFileLoader::getVolume("scene");
}

void TParams::finalize()
{
	mArc      = nullptr;
	mSceneArc = nullptr;
}

bool TParams::load(const char* filename)
{
	bool found = false;
	if (filename[0] == '/') {
		filename++;
	}
#ifdef SMS_NATIVE_PLATFORM
	g_sb_prm_dbg_name = filename;
#endif

	void* resource = nullptr;
	if (mSceneArc != nullptr && mSceneArc->becomeCurrent(SceneParamsDir)) {
		resource = mSceneArc->getResource(filename);
	}

	if (resource != nullptr) {
		s32 size = mSceneArc->getResSize(resource);
		JSUMemoryInputStream stream(resource, size);
		load(stream);
		found = true;
	} else {
		if (mArc != nullptr) {
			mArc->becomeCurrent("/");
			void* resource2 = mArc->getResource(filename);
			if (resource2 != nullptr) {
				s32 size = mArc->getResSize(resource2);
				JSUMemoryInputStream stream(resource2, size);
				load(stream);
				found = true;
			}
		}
	}

	return found;
}
