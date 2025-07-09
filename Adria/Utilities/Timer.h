#pragma once

namespace adria
{
	template<typename Duration = std::chrono::microseconds, typename Clock = std::chrono::steady_clock>
	class Timer
	{
		using tp = typename Clock::time_point;

	public:
		Timer() : t0{ Clock::now() } { t1 = t0; }

		typename Duration::rep Mark()
		{
			tp t2 = Clock::now();
			auto dt = std::chrono::duration_cast<Duration>(t2 - t1).count();
			t1 = t2;
			return dt;
		}

		typename Duration::rep Peek() const
		{
			tp t2 = Clock::now();
			auto dt = std::chrono::duration_cast<Duration>(t2 - t1).count();
			return dt;
		}

		typename Duration::rep Elapsed() const
		{
			tp t2 = Clock::now();
			auto elapsed = std::chrono::duration_cast<Duration>(t2 - t0).count();
			return elapsed;
		}

		Float ElapsedInSeconds() const
		{
			return Elapsed() / DurationSecondRatio;
		}

		Float PeekInSeconds() const
		{
			return Peek() / DurationSecondRatio;
		}

		Float MarkInSeconds()
		{
			return Mark() / DurationSecondRatio;
		}

	private:
		const tp t0; 
		tp t1;  

	private:
		static constexpr Float DurationSecondRatio = std::chrono::seconds(1) * 1.0f / Duration(1);
	};

};
