//https://github.com/biocomp/hw_break modified a bit

#pragma once
#pragma warning(push)
#include <Windows.h>
#include <cstddef>
#include <algorithm>
#include <array>
#include <cassert>
#include <bitset>
#pragma warning(pop)

namespace adria::hwbp
{
    enum class Result
    {
        Success,
        CantGetThreadContext,
        CantSetThreadContext,
        NoAvailableRegisters,
        BadWhen, // Unsupported value of When passed
        BadSize  // Size can only be 1, 2, 4, 8
    };

    enum class When
    {
        ReadOrWritten,
        Written,
        Executed
    };

    struct Breakpoint
    {
        static constexpr Breakpoint MakeFailed(Result result)
        {
            return 
            {
                0,
                result
            };
        }

        Uint8 register_index;
        Result error;
    };

    namespace Detail
    {
        template <typename ActionT, typename FailureT>
        auto UpdateThreadContext(ActionT action, FailureT failure)
        {
            CONTEXT ctx{0};
            ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
            if (::GetThreadContext(::GetCurrentThread(), &ctx) == FALSE)
            {
                return failure(Result::CantGetThreadContext);
            }

            std::array<Bool, 4> busy_debug_register{ false, false, false, false };
            auto CheckBusyRegister = [&](Uint64 index, DWORD64 mask)
            {
                if (ctx.Dr7 & mask)
                    busy_debug_register[index] = true;
            };

            CheckBusyRegister(0, 1);
            CheckBusyRegister(1, 4);
            CheckBusyRegister(2, 16);
            CheckBusyRegister(3, 64);

            const auto action_result = action(ctx, busy_debug_register);

            if (::SetThreadContext(::GetCurrentThread(), &ctx) == FALSE)
            {
                return failure(Result::CantSetThreadContext);
            }

            return action_result;
        }
    }

    static Breakpoint Set(const void* onPointer, Uint8 size, When when)
    {
        return Detail::UpdateThreadContext(
            [&](CONTEXT& ctx, const std::array<Bool, 4>& busyDebugRegister) -> Breakpoint
            {
                const auto found = std::find(begin(busyDebugRegister), end(busyDebugRegister), false);
                if (found == end(busyDebugRegister))
                {
                    return Breakpoint::MakeFailed(Result::NoAvailableRegisters);
                }
                const auto register_index = static_cast<std::uint16_t>(std::distance(begin(busyDebugRegister), found));
                switch (register_index)
                {
                case 0:
                    ctx.Dr0 = reinterpret_cast<DWORD_PTR>(const_cast<void*>(onPointer));
                    break;
                case 1:
                    ctx.Dr1 = reinterpret_cast<DWORD_PTR>(const_cast<void*>(onPointer));
                    break;
                case 2:
                    ctx.Dr2 = reinterpret_cast<DWORD_PTR>(const_cast<void*>(onPointer));
                    break;
                case 3:
                    ctx.Dr3 = reinterpret_cast<DWORD_PTR>(const_cast<void*>(onPointer));
                    break;
                default:
                    ADRIA_ASSERT(!"Impossible happened - searching in array of 4 got index < 0 or > 3");
                    std::exit(EXIT_FAILURE);
                }
                std::bitset<sizeof(ctx.Dr7) * 8> dr7;
                memcpy(&dr7, &ctx.Dr7, sizeof(ctx.Dr7));
                dr7.set(register_index * 2); 
                switch (when)
                {
                case When::ReadOrWritten:
                    dr7.set(16 + register_index * 4 + 1, true);
                    dr7.set(16 + register_index * 4, true);
                    break;

                case When::Written:
                    dr7.set(16 + register_index * 4 + 1, false);
                    dr7.set(16 + register_index * 4, true);
                    break;

                case When::Executed:
                    dr7.set(16 + register_index * 4 + 1, false);
                    dr7.set(16 + register_index * 4, false);
                    break;

                default:
                    return Breakpoint::MakeFailed(Result::BadWhen);
                }

                switch (size)
                {
                case 1:
                    dr7.set(16 + register_index * 4 + 3, false);
                    dr7.set(16 + register_index * 4 + 2, false);
                    break;

                case 2:
                    dr7.set(16 + register_index * 4 + 3, false);
                    dr7.set(16 + register_index * 4 + 2, true);
                    break;

                case 8:
                    dr7.set(16 + register_index * 4 + 3, true);
                    dr7.set(16 + register_index * 4 + 2, false);
                    break;

                case 4:
                    dr7.set(16 + register_index * 4 + 3, true);
                    dr7.set(16 + register_index * 4 + 2, true);
                    break;

                default:
                    return Breakpoint::MakeFailed(Result::BadSize);
                }
                memcpy(&ctx.Dr7, &dr7, sizeof(ctx.Dr7));
                return Breakpoint{ static_cast<Uint8>(register_index), Result::Success };
            },
            [](auto failureCode)
            {
                return Breakpoint::MakeFailed(failureCode);
            }
        );
    }

    static void Remove(Breakpoint const& bp)
    {
        if (bp.error != Result::Success)
        {
            return;
        }

        Detail::UpdateThreadContext(
            [&](CONTEXT& ctx, std::array<Bool, 4> const&) -> Breakpoint
            {
                std::bitset<sizeof(ctx.Dr7) * 8> dr7;
                memcpy(&dr7, &ctx.Dr7, sizeof(ctx.Dr7));
                dr7.set(bp.register_index * 2, false);
                memcpy(&ctx.Dr7, &dr7, sizeof(ctx.Dr7));
                return Breakpoint{};
            },
            [](auto failureCode)
            {
                return Breakpoint::MakeFailed(failureCode);
            }
        );
    }
}