#include <cstdarg> 

namespace adria
{

	namespace details
	{
		ADRIA_NORETURN void TriggerFatalAssert(
			Char const* expression,
			Char const* file,
			Int line,
			Char const* msg_format, 
			...);
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
