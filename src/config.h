#pragma once
#include "types.h"

#include <filesystem>

namespace vrperfkit {
	struct UpscaleConfig {
		bool enabled = false;
		UpscaleMethod method = UpscaleMethod::FSR;
		float renderScale = 1.0f;
		float sharpness = 0.7f;
		float radius = 0.6f;
		bool applyMipBias = true;
	};

	struct DxvkConfig {
		bool enabled = false;
		std::string dxgiDllPath = "dxvk\\dxgi.dll";
		std::string d3d11DllPath = "dxvk\\d3d11.dll";

		// not actually a config option, but a real-time toggle hack...
		bool shouldUseDxvk = true;
	};

	struct FixedFoveatedConfig {
		bool enabled = false;
		bool pattern = true;
		float patternScale = 1.0;
		FixedFoveatedMethod method = FixedFoveatedMethod::VRS;
		float innerRadius = 0.6f;
		float midRadius = 0.8f;
		float outerRadius = 1.0f;
		std::string patternFilePath = "singlePattern.png";
	};

	struct Config {
		UpscaleConfig upscaling;
		DxvkConfig dxvk;
		FixedFoveatedConfig ffr;
		bool debugMode = false;
		std::string dllLoadPath = "";

		// not a config option, but a signal to take a capture of the final rendering output
		bool captureOutput = false;
	};

	extern Config g_config;

	void LoadConfig(const std::filesystem::path &configPath);
	void PrintCurrentConfig();
}
