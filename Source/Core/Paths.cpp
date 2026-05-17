#include "Paths.h"

namespace adria
{

	std::string const paths::SourceDir = SOLUTION_DIR"/Source/";

	std::string const paths::ConfigDir = SourceDir;

	std::string const paths::AssetsDir = SOLUTION_DIR"/Assets/";

	std::string const paths::RuntimeDir = SOLUTION_DIR"/Runtime/";

	std::string const paths::ToolsDir = SourceDir + "Tools/";

	std::string const paths::FontsDir = AssetsDir + "Fonts/";
	
	std::string const paths::IconsDir = AssetsDir + "Icons/";

	std::string const paths::ShaderDir = AssetsDir + "Shaders/";

	std::string const paths::ModelsDir = AssetsDir + "Models/";

	std::string const paths::TexturesDir = AssetsDir + "Textures/";

	std::string const paths::MLDir = AssetsDir + "ML/";

	std::string const paths::ScreenshotsDir = RuntimeDir + "Screenshots/";

	std::string const paths::LogDir = RuntimeDir + "Log/";
	
	std::string const paths::RenderGraphDir = RuntimeDir + "RenderGraph/";

	std::string const paths::ShaderCacheDir = RuntimeDir + "ShaderCache/";

	std::string const paths::ShaderPDBDir = RuntimeDir + "ShaderPDB/";

	std::string const paths::IniDir = RuntimeDir + "Ini/";

	std::string const paths::ScenesDir = AssetsDir + "Scenes/";

	std::string const paths::AftermathDir = RuntimeDir + "Aftermath/";

	std::string const paths::NsightPerfReportDir = RuntimeDir + "NsightPerfReport/";

	std::string const paths::PixCapturesDir = RuntimeDir + "PixCaptures/";

	std::string const paths::RenderDocCapturesDir = RuntimeDir + "RenderDocCaptures/";
}

