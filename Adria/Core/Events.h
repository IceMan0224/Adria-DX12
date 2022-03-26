#pragma once
#include "../Events/IEvent.h"
#include "../Core/Definitions.h"

namespace adria
{

	struct ResizeEvent : IEvent
	{
		uint32 width;
		uint32 height;

		ResizeEvent(uint32 width, uint32 height) : width(width), height(height)
		{}

		virtual EventTypeID GetEventTypeID() const override
		{
			return EventTypeIdGenerator::type<ResizeEvent>;
		}
	};

	struct ScrollEvent : IEvent
	{
		int32 scroll;

		ScrollEvent(int32 scroll) : scroll(scroll)
		{}

		virtual EventTypeID GetEventTypeID() const override
		{
			return EventTypeIdGenerator::type<ScrollEvent>;
		}
	};

	struct RightMouseClickedEvent : IEvent
	{
		int32 mx, my;

		RightMouseClickedEvent(int32 mx, int32 my) : mx(mx), my(my)
		{}

		virtual EventTypeID GetEventTypeID() const override
		{
			return EventTypeIdGenerator::type<RightMouseClickedEvent>;
		}
	};
}