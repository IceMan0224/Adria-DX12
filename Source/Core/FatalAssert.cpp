#include "FatalAssert.h"
#include "Logging/Log.h"

namespace adria
{
	ADRIA_LOG_CHANNEL(FatalAssert);

	ADRIA_NORETURN void details::TriggerFatalAssert(Char const* expression, Char const* file, Int line, Char const* msg_format, ...)
	{
		ADRIA_LOG_SYNC(FATAL, "FATAL ASSERTION FAILED");

		va_list args;
		va_start(args, msg_format);
		va_list args_copy;
		va_copy(args_copy, args);
		Int const size = vsnprintf(nullptr, 0, msg_format, args_copy);
		va_end(args_copy);
		std::unique_ptr<Char[]> buf = std::make_unique<Char[]>(size + 1);
		vsnprintf(buf.get(), size + 1, msg_format, args);
		va_end(args);

		ADRIA_LOG_SYNC(FATAL, "%s", buf.get());
		ADRIA_LOG_FLUSH();
		std::abort();
	}
}

