#pragma once


namespace adria
{
	class GfxCommandList;

	enum class GfxNsightPerfMode : Uint8
	{
		None,
		HTMLReport,
		HUD
	};

	class GfxNsightPerfManager
	{
	public:
		virtual ~GfxNsightPerfManager() = default;

		virtual void Update() = 0;
		virtual void BeginFrame() = 0;
		virtual void Render() = 0;
		virtual void EndFrame() = 0;
		virtual void PushRange(GfxCommandList*, Char const*) = 0;
		virtual void PopRange(GfxCommandList*) = 0;
		virtual void GenerateReport() = 0;
	};
}