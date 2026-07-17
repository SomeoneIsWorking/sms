#ifdef SMS_NATIVE_PLATFORM
#include <cstdio>
#include <cstdlib>
#endif
#include <JSystem/JAudio/JASystem/JASCmdStack.hpp>
#include <JSystem/JAudio/JASystem/JASCallback.hpp>
#include <dolphin/os.h>
#include <types.h>

namespace JASystem {

namespace Kernel {

	JASystem::Kernel::TPortHead cmd_once;
	JASystem::Kernel::TPortHead cmd_stay;

	static s32 portCmdMain(void* data);
	static TPortCmd* getPortCmd(TPortHead* head);

	TPortCmd::TPortCmd()
	{
		unk0 = nullptr;
		unk4 = nullptr;
		unk8 = nullptr;
		unkC = nullptr;
	}

	BOOL TPortCmd::addPortCmdOnce() { return addPortCmd(&cmd_once); }

	BOOL TPortCmd::addPortCmdStay() { return addPortCmd(&cmd_stay); }

	BOOL TPortCmd::setPortCmd(PortCallback func, TPortArgs* args)
	{
#ifdef SMS_NATIVE_PLATFORM
		// If this command is STILL queued, dequeue it first. The decomp just clears unk0
		// (the queued-flag) below without unlinking; when the sequence-port setup re-arms a
		// command whose cmd_once entry hasn't drained yet (timing differs on the native DSP
		// driver), addPortCmd would re-link an already-linked node, corrupting the list into a
		// wild entry -> portCmdMain derefs freed memory in setSePortParameter (2026-07-17 UAF,
		// fault at an unmapped `args`). cancelPortCmd (previously a no-op stub) unlinks it.
		if (unk0 != nullptr)
			cancelPortCmd(unk0);
#endif
		unk8 = func;
		unkC = args;
		unk0 = nullptr;
		return true;
	}

	BOOL TPortCmd::addPortCmd(TPortHead* head)
	{
		BOOL enable = OSDisableInterrupts();
		if (unk0) {
			OSRestoreInterrupts(enable);
			return false;
		}

		if (head->unk4)
			head->unk4->unk4 = this;
		else
			head->unk0 = this;

		head->unk4 = this;
		unk4       = nullptr;
		unk0       = head;
		OSRestoreInterrupts(enable);
		return true;
	}

	// Unlink `this` from `head`'s singly-linked queue (head->unk0 = first, chained by unk4;
	// head->unk4 = last). Was an empty no-op stub — nothing ever removed a queued command, so a
	// re-armed command double-linked and corrupted the list (see setPortCmd). unk0 != nullptr
	// means "currently queued in that head".
	void TPortCmd::cancelPortCmd(TPortHead* head)
	{
		BOOL enable = OSDisableInterrupts();
		if (unk0 == nullptr) { // not queued
			OSRestoreInterrupts(enable);
			return;
		}
		if (head->unk0 == this) {
			head->unk0 = unk4;
			if (head->unk4 == this)
				head->unk4 = nullptr;
		} else {
			TPortCmd* prev = head->unk0;
			while (prev != nullptr && prev->unk4 != this)
				prev = prev->unk4;
			if (prev != nullptr) {
				prev->unk4 = unk4;
				if (head->unk4 == this)
					head->unk4 = prev;
			}
		}
		unk4 = nullptr;
		unk0 = nullptr;
		OSRestoreInterrupts(enable);
	}

	void TPortCmd::cancelPortCmdStay() { cancelPortCmd(&cmd_stay); }

	void portCmdProcOnce(TPortHead* head)
	{
		for (;;) {
			TPortCmd* cmd = getPortCmd(head);
			if (!cmd)
				break;
#ifdef SMS_NATIVE_PLATFORM
			if (std::getenv("SB_DBG_AUDIO"))
				std::fprintf(stderr, "[audio] portCmdOnce run: cmd=%p func=%p args=%p\n",
				             (void*)cmd, (void*)cmd->unk8, (void*)cmd->unkC);
#endif
			cmd->unk8(cmd->unkC);
		}
	}

	void portCmdProcStay(TPortHead* head)
	{
		TPortCmd* cmd = head->unk0;
		for (;;) {
			if (!cmd)
				break;

			cmd->unk8(cmd->unkC);

			cmd = cmd->unk4;
		}
	}

	void portHeadInit(TPortHead* head)
	{
		head->unk0 = nullptr;
		head->unk4 = nullptr;
	}

	void portCmdInit()
	{
		portHeadInit(&cmd_once);
		portHeadInit(&cmd_stay);
		registerAiCallback(portCmdMain, nullptr);
	}

	static TPortCmd* getPortCmd(TPortHead* head)
	{
		TPortCmd* r30 = nullptr;

		if (head->unk0) {
			TPortCmd* r31 = head->unk0;
			r30           = r31;
			head->unk0    = r31->unk4;
			if (!head->unk0)
				head->unk4 = nullptr;

			r31->unk0 = nullptr;
		}
		return r30;
	}

	static s32 portCmdMain(void* data)
	{
		// TODO: inlines inside of portCmdProc*
		// (but does it matter?)
		char trash[0x30];

		portCmdProcOnce(&cmd_once);
		portCmdProcStay(&cmd_stay);
		return 0;
	}

} // namespace Kernel

} // namespace JASystem
