#include "GfxScopedEvent.h"
#include "GfxCommandList.h"

namespace adria
{
	GfxScopedEvent::GfxScopedEvent(GfxCommandList* cmd_list, Char const* event_name) : cmd_list(cmd_list)
	{
		cmd_list->BeginEvent(event_name);
	}

	GfxScopedEvent::~GfxScopedEvent()
	{
		cmd_list->EndEvent();
	}

}

