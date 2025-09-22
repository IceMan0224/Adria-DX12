#include <cstdarg> 
#include "Logging/Log.h"

namespace adria
{
	ADRIA_LOG_CHANNEL(FatalAssert);

	namespace details
	{
		ADRIA_NORETURN void TriggerFatalAssert(
			Char const* expression,
			Char const* file,
			Int line,
			Char const* msg_format, 
			...) 
		{
			ADRIA_LOG_SYNC(FATAL, "FATAL ASSERTION FAILED");
			va_list args;
			va_start(args, msg_format);
			ADRIA_LOG_SYNC(FATAL, msg_format, args);
			va_end(args);
			ADRIA_LOG_FLUSH();
			std::abort();
		}
	}
}

#define ADRIA_FATAL_ASSERT(expr, ...) \
do \
{ \
   if (!(!!(expr))) \
   { \
       ::adria::details::TriggerFatalAssert(#expr, __FILE__, __LINE__, __VA_ARGS__); \
   } \
} while(0)
