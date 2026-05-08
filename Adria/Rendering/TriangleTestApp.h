#pragma once

namespace adria
{
	class Window;

	class TriangleTestApp
	{
	public:
		TriangleTestApp(Window* window);
		~TriangleTestApp();

		void Run();

	private:
		struct Impl;
		std::unique_ptr<Impl> pimpl;
	};
}
