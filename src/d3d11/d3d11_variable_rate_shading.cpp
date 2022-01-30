#include "d3d11_variable_rate_shading.h"
#include "config.h"
#include "logging.h"
#include <Magick++.h>

//							  RGB
#define SHADING_RATE_4x4 0 // 000
#define SHADING_RATE_1x1 1 // 001
#define SHADING_RATE_1x2 2 // 010
#define SHADING_RATE_2x1 3 // 011
#define SHADING_RATE_2x2 4 // 100
#define SHADING_RATE_2x4 5 // 101
#define SHADING_RATE_4x2 6 // 110
//#define SHADING_RATE_4x4 7 // 111

namespace vrperfkit {
	uint8_t DistanceToVRSLevel(float distance) {
		if (distance < g_config.ffr.innerRadius) {
			return SHADING_RATE_1x1;
		}
		if (distance < g_config.ffr.midRadius) {
			return SHADING_RATE_1x2;
		}
		if (distance < g_config.ffr.outerRadius) {
			return SHADING_RATE_2x2;
		}
		return SHADING_RATE_4x4;
	}

	uint8_t ColorToVRSLevel(Magick::ColorRGB px) {
		uint8_t red = int(px.red()) *4;
		uint8_t green = int(px.green()) *2;
		uint8_t blue = int(px.blue());
		
		return red + green + blue;
	}
	

	std::vector<uint8_t> CreateCombinedFixedFoveatedVRSPatternFromRadii( int width, int height, float leftProjX, float leftProjY, float rightProjX, float rightProjY ) {
		std::vector<uint8_t> data (width * height);
		int halfWidth = width / 2;

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < halfWidth; ++x) {
				float fx = float(x) / halfWidth;
				float fy = float(y) / height;
				float distance = 2 * sqrtf((fx - leftProjX) * (fx - leftProjX) + (fy - leftProjY) * (fy - leftProjY));
				data[y * width + x] = DistanceToVRSLevel(distance);
			}
			for (int x = halfWidth; x < width; ++x) {
				float fx = float(x - halfWidth) / halfWidth;
				float fy = float(y) / height;
				float distance = 2 * sqrtf((fx - rightProjX) * (fx - rightProjX) + (fy - rightProjY) * (fy - rightProjY));
				data[y * width + x] = DistanceToVRSLevel(distance);
			}
		}
		return data;
	}
	
	// Open from image file supported by imagemagick library
	std::vector<uint8_t> CreateCombinedFixedFoveatedVRSPatternFromFile(int width, int height, float leftProjX, float leftProjY, float rightProjX, float rightProjY) {
		std::vector<uint8_t> data (width * height);

		LOG_INFO << "Custom VRS: Open image file using ImageMagick: " << g_config.ffr.patternFilePath;

		try {
			// Loading color palette
			Magick::Image palette;
			palette.read("pal.gif");
			// loading pattern image
			Magick::Image imgPattern;
			imgPattern.read(g_config.ffr.patternFilePath);
			imgPattern.modifyImage();

			// scale the pattern image
			Magick::Geometry to_size(100 * g_config.ffr.patternScale, 100 * g_config.ffr.patternScale);
			to_size.percent(true);
			imgPattern.scale( to_size );
			imgPattern.map(palette); // only use colors defined in palette
			int imgWidth = imgPattern.columns();
			int imgHeight = imgPattern.rows();
			LOG_INFO << "Custom VRS: pattern image scaled size " << imgWidth << "x" << imgHeight;

			// Create background image;
			Magick::Image VRSPattern(Magick::Geometry(width, height), "white"); 
			VRSPattern.modifyImage();

			// Calculate Left Eye Offset (Top Left corner)
			int xOffset = int( 0 + leftProjX * width * 0.5 - 0.5*imgWidth);
			int yOffset = int( leftProjY * height - 0.5*imgWidth );
			Magick::Geometry leftOffset(0, 0, xOffset, yOffset);
			LOG_INFO << "Custom VRS: LeftEye Offset: " << xOffset << "x" << yOffset;

			// Calculate Right Eye Offset (Top Left corner)
			xOffset = int(width*0.5 + rightProjX * width * 0.5 - 0.5*imgWidth);
			Magick::Geometry rightOffset(0, 0, xOffset, yOffset);
			LOG_INFO << "Custom VRS: RightEye Offset: " << xOffset << "x" << yOffset;

			// Compose final image. Right eye image is flipped horizontal (flop)
			VRSPattern.composite(imgPattern, leftOffset, MagickCore::OverCompositeOp);
			imgPattern.flop();
			VRSPattern.composite(imgPattern, rightOffset, MagickCore::OverCompositeOp);
			
			// Write final pattern image for debugging
			VRSPattern.write("out.png");

			imgWidth = VRSPattern.columns();
			imgHeight = VRSPattern.rows();
			LOG_INFO << "Custom VRS: image size " << imgWidth << "x" << imgHeight;

			for (int row = 0; row < imgHeight; row++) {
				for (int column = 0; column < imgWidth; column++) {
					data[row * width + column] = ColorToVRSLevel(VRSPattern.pixelColor(column, row));
					LOG_INFO << int(data[row * width + column]) << ",";
				}
			}
		}
		catch (Magick::Exception &error) {
			LOG_ERROR << "Caught Imagemagick error: " << error.what();
		}
		LOG_INFO << "Created VRS Pattern from File " << g_config.ffr.patternFilePath << " with length " << data.size();
		return data;
	}
	std::vector<uint8_t> CreateCombinedFixedFoveatedVRSPattern(int width, int height, float leftProjX, float leftProjY, float rightProjX, float rightProjY) {
		if (g_config.ffr.pattern) {
			return CreateCombinedFixedFoveatedVRSPatternFromFile(width, height, leftProjX, leftProjY, rightProjX, rightProjY);
		}
		else {
			return CreateCombinedFixedFoveatedVRSPatternFromRadii(width, height, leftProjX, leftProjY, rightProjX, rightProjY);
		}
	}
	std::vector<uint8_t> CreateSingleEyeFixedFoveatedVRSPattern( int width, int height, float projX, float projY ) {
		std::vector<uint8_t> data (width * height);

		for (int y = 0; y < height; ++y) {
			for (int x = 0; x < width; ++x) {
				float fx = float(x) / width;
				float fy = float(y) / height;
				float distance = 2 * sqrtf((fx - projX) * (fx - projX) + (fy - projY) * (fy - projY));
				data[y * width + x] = DistanceToVRSLevel(distance);
			}
		}

		return data;
	}

	D3D11VariableRateShading::D3D11VariableRateShading(ComPtr<ID3D11Device> device) {
		active = false;
		LOG_INFO << "Trying to load NVAPI...";

		if (!nvapiLoaded) {
			NvAPI_Status result = NvAPI_Initialize();
			if (result != NVAPI_OK) {
				return;
			}
			nvapiLoaded = true;
		}

		NV_D3D1x_GRAPHICS_CAPS caps;
		memset(&caps, 0, sizeof(NV_D3D1x_GRAPHICS_CAPS));
		NvAPI_Status status = NvAPI_D3D1x_GetGraphicsCapabilities(device.Get(), NV_D3D1x_GRAPHICS_CAPS_VER, &caps);
		if (status != NVAPI_OK || !caps.bVariablePixelRateShadingSupported) {
			LOG_INFO << "Variable rate shading is not available.";
			return;
		}

		this->device = device;
		device->GetImmediateContext(context.GetAddressOf());
		active = true;
		LOG_INFO << "Successfully initialized NVAPI; Variable Rate Shading is available.";
	}

	void D3D11VariableRateShading::UpdateTargetInformation(int targetWidth, int targetHeight, TextureMode mode, float leftProjX, float leftProjY, float rightProjX, float rightProjY) {
		this->targetWidth = targetWidth;
		this->targetHeight = targetHeight;
		this->targetMode = mode;
		proj[0][0] = leftProjX;
		proj[0][1] = leftProjY;
		proj[1][0] = rightProjX;
		proj[1][1] = rightProjY;
	}

	void D3D11VariableRateShading::PostOMSetRenderTargets(UINT numViews, ID3D11RenderTargetView * const *renderTargetViews,
			ID3D11DepthStencilView *depthStencilView) {
		if (!active || numViews == 0 || renderTargetViews == nullptr || renderTargetViews[0] == nullptr || !g_config.ffr.enabled) {
			DisableVRS();
			return;
		}

		ComPtr<ID3D11Resource> resource;
		renderTargetViews[0]->GetResource( resource.GetAddressOf() );
		D3D11_RENDER_TARGET_VIEW_DESC rtd;
		renderTargetViews[0]->GetDesc( &rtd );
		if (rtd.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D && rtd.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DARRAY
				&& rtd.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMS && rtd.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY) {
			DisableVRS();
			return;
		}
		ID3D11Texture2D *tex = (ID3D11Texture2D*)resource.Get();
		D3D11_TEXTURE2D_DESC td;
		tex->GetDesc( &td );

		if (td.Width == td.Height) {
			// probably a shadow map or similar extra resources
			DisableVRS();
			return;
		}

		if (targetMode == TextureMode::SINGLE && td.Width >= 2 * targetWidth && td.Height >= targetHeight) {
			ApplyCombinedVRS(td.Width, td.Height);
		}
		else if (targetMode == TextureMode::COMBINED && td.Width >= targetWidth && td.Height >= targetHeight) {
			ApplyCombinedVRS(td.Width, td.Height);
		}
		else if (targetMode != TextureMode::COMBINED && td.ArraySize == 2 && td.Width >= targetWidth && td.Height >= targetHeight) {
			ApplyArrayVRS(td.Width, td.Height);
		}
		else if (targetMode == TextureMode::SINGLE && td.ArraySize == 1 && td.Width >= targetWidth && td.Height >= targetHeight) {
			// FIXME: how to guess the current eye?
			LOG_DEBUG << "VRS: Single eye target, don't know which eye";
			DisableVRS();
		}
		else {
			DisableVRS();
		}
	}

	void D3D11VariableRateShading::ApplyCombinedVRS(int width, int height) {
		if (!active)
			return;

		SetupCombinedVRS(width, height, proj[0][0], proj[0][1], proj[1][0], proj[1][1] );
		NvAPI_Status status = NvAPI_D3D11_RSSetShadingRateResourceView( context.Get(), combinedVRSView.Get() );
		if (status != NVAPI_OK) {
			LOG_ERROR << "Error while setting shading rate resource view: " << status;
			Shutdown();
			return;
		}

		EnableVRS();
	}

	void D3D11VariableRateShading::ApplyArrayVRS(int width, int height) {
		if (!active)
			return;

		SetupArrayVRS( width, height, proj[0][0], proj[0][1], proj[1][0], proj[1][1] );
		NvAPI_Status status = NvAPI_D3D11_RSSetShadingRateResourceView( context.Get(), arrayVRSView.Get() );
		if (status != NVAPI_OK) {
			LOG_ERROR << "Error while setting shading rate resource view: " << status;
			Shutdown();
			return;
		}

		EnableVRS();
	}

	void D3D11VariableRateShading::ApplySingleEyeVRS(int eye, int width, int height) {
		if (!active)
			return;

		SetupSingleEyeVRS( eye, width, height, proj[eye][0], proj[eye][1] );
		NvAPI_Status status = NvAPI_D3D11_RSSetShadingRateResourceView( context.Get(), singleEyeVRSView[eye].Get() );
		if (status != NVAPI_OK) {
			LOG_ERROR << "Error while setting shading rate resource view: " << status;
			Shutdown();
			return;
		}

		EnableVRS();
	}

	void D3D11VariableRateShading::DisableVRS() {
		if (!active)
			return;

		NV_D3D11_VIEWPORT_SHADING_RATE_DESC vsrd[2];
		vsrd[0].enableVariablePixelShadingRate = false;
		vsrd[1].enableVariablePixelShadingRate = false;
		memset(vsrd[0].shadingRateTable, 0, sizeof(vsrd[0].shadingRateTable));
		memset(vsrd[1].shadingRateTable, 0, sizeof(vsrd[1].shadingRateTable));
		NV_D3D11_VIEWPORTS_SHADING_RATE_DESC srd;
		srd.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
		srd.numViewports = 2;
		srd.pViewports = vsrd;
		NvAPI_Status status = NvAPI_D3D11_RSSetViewportsPixelShadingRates( context.Get(), &srd );
		if (status != NVAPI_OK) {
			LOG_ERROR << "Error while setting shading rates: " << status;
			Shutdown();
		}
	}

	void D3D11VariableRateShading::Shutdown() {
		DisableVRS();

		if (nvapiLoaded) {
			NvAPI_Unload();
		}
		nvapiLoaded = false;
		active = false;
		for (int i = 0; i < 2; ++i) {
			singleEyeVRSTex[i].Reset();
			singleEyeVRSView[i].Reset();
		}
		combinedVRSTex.Reset();
		combinedVRSView.Reset();
		arrayVRSTex.Reset();
		arrayVRSView.Reset();
		device.Reset();
		context.Reset();
	}

	void D3D11VariableRateShading::EnableVRS() {
		NV_D3D11_VIEWPORT_SHADING_RATE_DESC vsrd[2];
		for (int i = 0; i < 2; ++i) {
			vsrd[i].enableVariablePixelShadingRate = true;
			memset(vsrd[i].shadingRateTable, 5, sizeof(vsrd[i].shadingRateTable));//RGB
			vsrd[i].shadingRateTable[0] = NV_PIXEL_X1_PER_4X4_RASTER_PIXELS;	//  000
			vsrd[i].shadingRateTable[1] = NV_PIXEL_X1_PER_RASTER_PIXEL;			//  001
			vsrd[i].shadingRateTable[2] = NV_PIXEL_X1_PER_1X2_RASTER_PIXELS;	//  010
			vsrd[i].shadingRateTable[3] = NV_PIXEL_X1_PER_2X1_RASTER_PIXELS;	//  011
			vsrd[i].shadingRateTable[4] = NV_PIXEL_X1_PER_2X2_RASTER_PIXELS;	//  100
			vsrd[i].shadingRateTable[5] = NV_PIXEL_X1_PER_2X4_RASTER_PIXELS;	//  101
			vsrd[i].shadingRateTable[6] = NV_PIXEL_X1_PER_4X2_RASTER_PIXELS;	//  110
			vsrd[i].shadingRateTable[7] = NV_PIXEL_X1_PER_4X4_RASTER_PIXELS;	//  111 
		}
		NV_D3D11_VIEWPORTS_SHADING_RATE_DESC srd;
		srd.version = NV_D3D11_VIEWPORTS_SHADING_RATE_DESC_VER;
		srd.numViewports = 2;
		srd.pViewports = vsrd;
		NvAPI_Status status = NvAPI_D3D11_RSSetViewportsPixelShadingRates( context.Get(), &srd );
		if (status != NVAPI_OK) {
			LOG_ERROR << "Error while setting shading rates: " << status;
			Shutdown();
		}
	}

	void D3D11VariableRateShading::SetupSingleEyeVRS( int eye, int width, int height, float projX, float projY ) {
		if (!active || (singleEyeVRSTex[eye] && width == singleWidth[eye] && height == singleHeight[eye])) {
			return;
		}
		singleEyeVRSTex[eye].Reset();
		singleEyeVRSView[eye].Reset();

		singleWidth[eye] = width;
		singleHeight[eye] = height;
		int vrsWidth = width / NV_VARIABLE_PIXEL_SHADING_TILE_WIDTH;
		int vrsHeight = height / NV_VARIABLE_PIXEL_SHADING_TILE_HEIGHT;

		LOG_INFO << "Creating VRS pattern texture for eye " << eye << " of size " << vrsWidth << "x" << vrsHeight;

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = vrsWidth;
		td.Height = vrsHeight;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8_UINT;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;
		td.MiscFlags= 0;
		td.MipLevels = 1;
		auto data = CreateSingleEyeFixedFoveatedVRSPattern(vrsWidth, vrsHeight, projX, projY);
		D3D11_SUBRESOURCE_DATA srd;
		srd.pSysMem = data.data();
		srd.SysMemPitch = vrsWidth;
		srd.SysMemSlicePitch = 0;
		HRESULT result = device->CreateTexture2D( &td, &srd, singleEyeVRSTex[eye].GetAddressOf() );
		if (FAILED(result)) {
			Shutdown();
			LOG_ERROR << "Failed to create VRS pattern texture for eye " << eye << ": " << std::hex << result << std::dec;
			return;
		}

		LOG_INFO << "Creating shading rate resource view for eye " << eye;
		NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC vd = {};
		vd.version = NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;
		vd.Format = td.Format;
		vd.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2D;
		vd.Texture2D.MipSlice = 0;
		NvAPI_Status status = NvAPI_D3D11_CreateShadingRateResourceView( device.Get(), singleEyeVRSTex[eye].Get(), &vd, singleEyeVRSView[eye].GetAddressOf() );
		if (status != NVAPI_OK) {
			Shutdown();
			LOG_ERROR << "Failed to create VRS pattern view for eye " << eye << ": " << status;
			return;
		}
	}

	void D3D11VariableRateShading::SetupCombinedVRS( int width, int height, float leftProjX, float leftProjY, float rightProjX, float rightProjY ) {
		if (!active || (combinedVRSTex && width == combinedWidth && height == combinedHeight)) {
			return;
		}
		combinedVRSTex.Reset();
		combinedVRSView.Reset();

		combinedWidth = width;
		combinedHeight = height;
		int vrsWidth = width / NV_VARIABLE_PIXEL_SHADING_TILE_WIDTH;
		int vrsHeight = height / NV_VARIABLE_PIXEL_SHADING_TILE_HEIGHT;

		LOG_INFO << "Creating combined VRS pattern texture of size " << vrsWidth << "x" << vrsHeight;

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = vrsWidth;
		td.Height = vrsHeight;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R8_UINT;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;
		td.MiscFlags= 0;
		td.MipLevels = 1;
		auto data = CreateCombinedFixedFoveatedVRSPattern(vrsWidth, vrsHeight, leftProjX, leftProjY, rightProjX, rightProjY);
		D3D11_SUBRESOURCE_DATA srd;
		srd.pSysMem = data.data();
		srd.SysMemPitch = vrsWidth;
		srd.SysMemSlicePitch = 0;
		HRESULT result = device->CreateTexture2D( &td, &srd, combinedVRSTex.GetAddressOf() );
		if (FAILED(result)) {
			Shutdown();
			LOG_ERROR << "Failed to create combined VRS pattern texture: " << std::hex << result << std::dec;
			return;
		}

		LOG_INFO << "Creating combined shading rate resource view";
		NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC vd = {};
		vd.version = NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;
		vd.Format = td.Format;
		vd.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2D;
		vd.Texture2D.MipSlice = 0;
		NvAPI_Status status = NvAPI_D3D11_CreateShadingRateResourceView( device.Get(), combinedVRSTex.Get(), &vd, combinedVRSView.GetAddressOf() );
		if (status != NVAPI_OK) {
			Shutdown();
			LOG_ERROR << "Failed to create combined VRS pattern view: " << status;
			return;
		}
	}

	void D3D11VariableRateShading::SetupArrayVRS( int width, int height, float leftProjX, float leftProjY, float rightProjX, float rightProjY ) {
		if (!active || (arrayVRSTex && width == arrayWidth && height == arrayHeight)) {
			return;
		}
		arrayVRSTex.Reset();
		arrayVRSView.Reset();

		arrayWidth = width;
		arrayHeight = height;
		int vrsWidth = width / NV_VARIABLE_PIXEL_SHADING_TILE_WIDTH;
		int vrsHeight = height / NV_VARIABLE_PIXEL_SHADING_TILE_HEIGHT;

		LOG_INFO << "Creating array VRS pattern texture of size " << vrsWidth << "x" << vrsHeight;

		D3D11_TEXTURE2D_DESC td = {};
		td.Width = vrsWidth;
		td.Height = vrsHeight;
		td.ArraySize = 2;
		td.Format = DXGI_FORMAT_R8_UINT;
		td.SampleDesc.Count = 1;
		td.SampleDesc.Quality = 0;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		td.CPUAccessFlags = 0;
		td.MiscFlags= 0;
		td.MipLevels = 1;
		HRESULT result = device->CreateTexture2D( &td, nullptr, arrayVRSTex.GetAddressOf() );
		if (FAILED(result)) {
			Shutdown();
			LOG_ERROR << "Failed to create array VRS pattern texture: " << std::hex << result << std::dec;
			return;
		}

		// array rendering is most likely a new Unity engine game, which for some reason renders upside down.
		// so we invert the y projection center coordinate to match the upside down render.
		auto data = CreateSingleEyeFixedFoveatedVRSPattern( vrsWidth, vrsHeight, leftProjX, 1.f - leftProjY );
		context->UpdateSubresource( arrayVRSTex.Get(), D3D11CalcSubresource( 0, 0, 1 ), nullptr, data.data(), vrsWidth, 0 );
		data = CreateSingleEyeFixedFoveatedVRSPattern( vrsWidth, vrsHeight, rightProjX, 1.f - rightProjY );
		context->UpdateSubresource( arrayVRSTex.Get(), D3D11CalcSubresource( 0, 1, 1 ), nullptr, data.data(), vrsWidth, 0 );

		LOG_INFO << "Creating array shading rate resource view";
		NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC vd = {};
		vd.version = NV_D3D11_SHADING_RATE_RESOURCE_VIEW_DESC_VER;
		vd.Format = td.Format;
		vd.ViewDimension = NV_SRRV_DIMENSION_TEXTURE2DARRAY;
		vd.Texture2DArray.MipSlice = 0;
		vd.Texture2DArray.ArraySize = 2;
		vd.Texture2DArray.FirstArraySlice = 0;
		NvAPI_Status status = NvAPI_D3D11_CreateShadingRateResourceView( device.Get(), arrayVRSTex.Get(), &vd, arrayVRSView.GetAddressOf() );
		if (status != NVAPI_OK) {
			Shutdown();
			LOG_ERROR << "Failed to create array VRS pattern view: " << status;
			return;
		}
	}

}
