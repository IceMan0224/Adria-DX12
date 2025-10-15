#include "D3D12NsightPerfManager.h"
#if defined(GFX_ENABLE_NV_PERF)
#include "D3D12Device.h"
#include "D3D12CommandList.h"
#include "D3D12CommandQueue.h"
#include "Core/Paths.h"
#include "Core/CommandLineOptions.h"
#include "Core/ConsoleManager.h"
#include "nvperf_host_impl.h"
#include "NvPerfUtility/include/NvPerfPeriodicSamplerD3D12.h"
#include "NvPerfUtility/include/NvPerfMetricConfigurationsHAL.h"
#include "NvPerfUtility/include/NvPerfHudDataModel.h"
#include "NvPerfUtility/include/NvPerfHudImPlotRenderer.h"
#include "NvPerfUtility/include/NvPerfReportGeneratorD3D12.h"
#define RYML_SINGLE_HDR_DEFINE_NOW
#include "NvPerfUtility/imports/rapidyaml-0.4.0/ryml_all.hpp"
#endif

namespace adria
{
	ADRIA_LOG_CHANNEL(NSight);

#if defined(GFX_ENABLE_NV_PERF)
	class D3D12NsightPerfReporter
	{
	public:
		explicit D3D12NsightPerfReporter(GfxDevice* gfx, Bool active) : gfx(gfx), active(active),
			generate_report_command("nsight.perf.report", "Generate Nsight Perf HTML report", ConsoleCommandDelegate::CreateMember(&D3D12NsightPerfReporter::GenerateReport, *this))
		{
			ID3D12Device* d3d12_device = (ID3D12Device*)gfx->GetNative();
			if (active)
			{
				if (!report_generator.InitializeReportGenerator(d3d12_device))
				{
					ADRIA_WARNING("NsightPerf Report Generator Initalization failed, check the VS Output View for NVPERF logs");
					return;
				}
				report_generator.SetFrameLevelRangeName("Frame");
				report_generator.SetNumNestingLevels(3);
				report_generator.SetMaxNumRanges(128);
				report_generator.outputOptions.directoryName = paths::NsightPerfReportDir;
				std::filesystem::create_directory(paths::NsightPerfReportDir);

				clock_info = nv::perf::D3D12GetDeviceClockState(d3d12_device);
				nv::perf::D3D12SetDeviceClockState(d3d12_device, NVPW_DEVICE_CLOCK_SETTING_DEFAULT);
			}
		}
		~D3D12NsightPerfReporter()
		{
			if (active)
			{
				report_generator.Reset();
			}
		}

		void Update()
		{
			if (active && generate_report)
			{
				if (!report_generator.StartCollectionOnNextFrame())
				{
					ADRIA_WARNING("Nsight Perf: Report Generator Start Collection failed. Please check the logs.");
				}
				generate_report = false;
			}
		}
		void BeginFrame()
		{
			if (active)
			{
				GfxCommandQueue* cmd_queue = gfx->GetCommandQueue(GfxCommandListType::Graphics);
				report_generator.OnFrameStart((ID3D12CommandQueue*)cmd_queue->GetHandle());
			}
		}
		void EndFrame()
		{
			if (active)
			{
				report_generator.OnFrameEnd();
				if (report_generator.IsCollectingReport())
				{
					ADRIA_INFO("Nsight Perf: Currently profiling the frame. HTML Report will be written to: %s", paths::NsightPerfReportDir.c_str());
				}
				if (report_generator.GetInitStatus() != nv::perf::profiler::ReportGeneratorInitStatus::Succeeded)
				{
					ADRIA_WARNING("Nsight Perf: Initialization failed. Please check the logs.");
				}
			}
		}
		void GenerateReport()
		{
			generate_report = true;
		}
		void PushRange(GfxCommandList* cmd_list, Char const* name)
		{
			if (!active)
			{
				return;
			}
			report_generator.rangeCommands.PushRange((ID3D12GraphicsCommandList*)cmd_list->GetNative(), name);
		}
		void PopRange(GfxCommandList* cmd_list)
		{
			if (!active)
			{
				return;
			}
			report_generator.rangeCommands.PopRange((ID3D12GraphicsCommandList*)cmd_list->GetNative());
		}

	private:
		GfxDevice* gfx;
		Bool active;
		nv::perf::profiler::ReportGeneratorD3D12 report_generator;
		nv::perf::ClockInfo clock_info;
		Bool generate_report = false;
		AutoConsoleCommand generate_report_command;
	};

	class D3D12NsightPerfHUD
	{
#if defined(_DEBUG) || defined(_PROFILE)
		static constexpr Uint32 SamplingFrequency = 30;
#else
		static constexpr Uint32 SamplingFrequency = 60;
#endif
	public:
		D3D12NsightPerfHUD(GfxDevice* gfx, Bool active) : active(active)
		{
			if (active)
			{
				if (!periodic_sampler.Initialize((ID3D12Device*)gfx->GetNative()))
				{
					ADRIA_WARNING("NsightPerf Periodic Sampler Initalization failed, check the VS Output View for NVPERF logs");
					return;
				}
				const nv::perf::DeviceIdentifiers device_identifiers = periodic_sampler.GetGpuDeviceIdentifiers();
				static constexpr Uint32 SamplingIntervalInNanoSeconds = 1000 * 1000 * 1000 / SamplingFrequency;
				static constexpr Uint32 MaxDecodeLatencyInNanoSeconds = 1000 * 1000 * 1000;
				ID3D12CommandQueue* d3d12_cmd_queue = (ID3D12CommandQueue*)gfx->GetCommandQueue(GfxCommandListType::Graphics)->GetHandle();
				if (!periodic_sampler.BeginSession(d3d12_cmd_queue, SamplingIntervalInNanoSeconds, MaxDecodeLatencyInNanoSeconds, GFX_BACKBUFFER_COUNT))
				{
					ADRIA_WARNING("NsightPerf Periodic Sampler BeginSession failed, check the VS Output View for NVPERF logs");
					return;
				}

				nv::perf::hud::HudPresets hud_presets;
				hud_presets.Initialize(device_identifiers.pChipName);
				static constexpr Float PlotTimeWidthInSeconds = 4.0;
				hud_data_model.Load(hud_presets.GetPreset("Graphics General Triage"));
				std::string metric_config_name;
				nv::perf::MetricConfigObject metric_config_object;
				if (nv::perf::MetricConfigurations::GetMetricConfigNameBasedOnHudConfigurationName(metric_config_name, device_identifiers.pChipName, "Graphics General Triage"))
				{
					nv::perf::MetricConfigurations::LoadMetricConfigObject(metric_config_object, device_identifiers.pChipName, metric_config_name);
				}
				hud_data_model.Initialize(1.0 / (Float64)SamplingFrequency, PlotTimeWidthInSeconds, metric_config_object);
				periodic_sampler.SetConfig(&hud_data_model.GetCounterConfiguration());
				hud_data_model.PrepareSampleProcessing(periodic_sampler.GetCounterData());
				hud_renderer.Initialize(hud_data_model);
			}
		}
		~D3D12NsightPerfHUD()
		{
			if (active)
			{
				periodic_sampler.Reset();
			}
		}

		void Update()
		{
			if (active)
			{
				periodic_sampler.DecodeCounters();
				periodic_sampler.ConsumeSamples([&](Uint8 const* pCounterDataImage, Uint64 counterDataImageSize, Uint32 rangeIndex, Bool& stop)
					{
						stop = false;
						return hud_data_model.AddSample(pCounterDataImage, counterDataImageSize, rangeIndex);
					});

				for (auto& frame_delimeter : periodic_sampler.GetFrameDelimiters())
				{
					hud_data_model.AddFrameDelimiter(frame_delimeter.frameEndTime);
				}
			}
		}
		void BeginFrame()
		{
		}

		void Render()
		{
			if (active)
			{
				hud_renderer.Render();
			}
		}
		void EndFrame()
		{
			if (active)
			{
				periodic_sampler.OnFrameEnd();
			}
		}

	private:
		Bool active = false;
		nv::perf::sampler::PeriodicSamplerTimeHistoryD3D12 periodic_sampler;
		nv::perf::hud::HudDataModel hud_data_model;
		nv::perf::hud::HudImPlotRenderer hud_renderer;
	};

	D3D12NsightPerfManager::D3D12NsightPerfManager(GfxDevice* gfx, GfxNsightPerfMode perf_mode)
	{
		perf_reporter = std::make_unique<D3D12NsightPerfReporter>(gfx, perf_mode == GfxNsightPerfMode::HTMLReport);
		perf_hud = std::make_unique<D3D12NsightPerfHUD>(gfx, perf_mode == GfxNsightPerfMode::HUD);
	}
	D3D12NsightPerfManager::~D3D12NsightPerfManager() = default;

	void D3D12NsightPerfManager::Update()
	{
		perf_hud->Update();
		perf_reporter->Update();
	}
	void D3D12NsightPerfManager::BeginFrame()
	{
		perf_hud->BeginFrame();
		perf_reporter->BeginFrame();
	}
	void D3D12NsightPerfManager::Render()
	{
		perf_hud->Render();
	}
	void D3D12NsightPerfManager::EndFrame()
	{
		perf_hud->EndFrame();
		perf_reporter->EndFrame();
	}
	void D3D12NsightPerfManager::PushRange(GfxCommandList* cmd_list, Char const* name)
	{
		perf_reporter->PushRange(cmd_list, name);
	}
	void D3D12NsightPerfManager::PopRange(GfxCommandList* cmd_list)
	{
		perf_reporter->PopRange(cmd_list);
	}
	void D3D12NsightPerfManager::GenerateReport()
	{
		perf_reporter->GenerateReport();
	}
#else
	D3D12NsightPerfManager::D3D12NsightPerfManager(GfxDevice* gfx, GfxNsightPerfMode perf_mode) {}
	D3D12NsightPerfManager::~D3D12NsightPerfManager() {}
	void D3D12NsightPerfManager::Update() {}
	void D3D12NsightPerfManager::BeginFrame() {}
	void D3D12NsightPerfManager::Render() {}
	void D3D12NsightPerfManager::EndFrame() {}
	void D3D12NsightPerfManager::PushRange(GfxCommandList*, Char const*) {}
	void D3D12NsightPerfManager::PopRange(GfxCommandList* cmd_list) {}
	void D3D12NsightPerfManager::GenerateReport() {}
#endif
}

