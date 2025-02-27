#include "pch.h"
#include "Shared/Emulator.h"
#include "Shared/EmuSettings.h"
#include "Shared/MessageManager.h"
#include "Shared/Video/BaseVideoFilter.h"
#include "Shared/Video/RotateFilter.h"
#include "Shared/Video/ScaleFilter.h"
#include "Shared/Video/ScanlineFilter.h"
#include "Utilities/PNGHelper.h"
#include "Utilities/FolderUtilities.h"
#include "Utilities/NTSC/nes_ntsc.h"
#include "Utilities/NTSC/snes_ntsc.h"

const static double PI = 3.14159265358979323846;

BaseVideoFilter::BaseVideoFilter(Emulator* emu)
{
	_emu = emu;
	_overscan = _emu->GetSettings()->GetOverscan();
}

BaseVideoFilter::~BaseVideoFilter()
{
	auto lock = _frameLock.AcquireSafe();
	delete[] _outputBuffer;
}

void BaseVideoFilter::SetBaseFrameInfo(FrameInfo frameInfo)
{
	_baseFrameInfo = frameInfo;
}

FrameInfo BaseVideoFilter::GetFrameInfo()
{
	FrameInfo frameInfo = _baseFrameInfo;
	OverscanDimensions overscan = GetOverscan();
	frameInfo.Width -= overscan.Left + overscan.Right;
	frameInfo.Height -= overscan.Top + overscan.Bottom;
	return frameInfo;
}

void BaseVideoFilter::UpdateBufferSize()
{
	uint32_t newBufferSize = _frameInfo.Width*_frameInfo.Height;
	if(_bufferSize != newBufferSize) {
		_frameLock.Acquire();
		delete[] _outputBuffer;
		_bufferSize = newBufferSize;
		_outputBuffer = new uint32_t[newBufferSize];
		_frameLock.Release();
	}
}

OverscanDimensions BaseVideoFilter::GetOverscan()
{
	return _overscan;
}

void BaseVideoFilter::SetOverscan(OverscanDimensions overscan)
{
	_overscan = overscan;
}

void BaseVideoFilter::OnBeforeApplyFilter()
{
}

bool BaseVideoFilter::IsOddFrame()
{
	return _isOddFrame;
}

uint32_t BaseVideoFilter::GetBufferSize()
{
	return _bufferSize * sizeof(uint32_t);
}

FrameInfo BaseVideoFilter::SendFrame(uint16_t *ppuOutputBuffer, uint32_t frameNumber, void* frameData, bool enableOverscan)
{
	auto lock = _frameLock.AcquireSafe();
	_overscan = enableOverscan ? _emu->GetSettings()->GetOverscan() : OverscanDimensions{};
	_isOddFrame = frameNumber % 2;
	_frameData = frameData;
	FrameInfo frameInfo = GetFrameInfo();
	_frameInfo = frameInfo;
	UpdateBufferSize();
	OnBeforeApplyFilter();
	ApplyFilter(ppuOutputBuffer);
	return frameInfo;
}

uint32_t* BaseVideoFilter::GetOutputBuffer()
{
	return _outputBuffer;
}

void BaseVideoFilter::InitConversionMatrix(double hueShift, double saturationShift)
{
	double hue = hueShift * PI;
	double sat = saturationShift + 1;

	double baseValues[6] = { 0.956f, 0.621f, -0.272f, -0.647f, -1.105f, 1.702f };

	double s = sin(hue) * sat;
	double c = cos(hue) * sat;

	double* output = _yiqToRgbMatrix;
	double* input = baseValues;
	for(int n = 0; n < 3; n++) {
		double i = *input++;
		double q = *input++;
		*output++ = i * c - q * s;
		*output++ = i * s + q * c;
	}
}

void BaseVideoFilter::RgbToYiq(double r, double g, double b, double& y, double& i, double& q)
{
	y = r * 0.299f + g * 0.587f + b * 0.114f;
	i = r * 0.596f - g * 0.275f - b * 0.321f;
	q = r * 0.212f - g * 0.523f + b * 0.311f;
}

void BaseVideoFilter::YiqToRgb(double y, double i, double q, double& r, double& g, double& b)
{
	r = std::max(0.0, std::min(1.0, (y + _yiqToRgbMatrix[0] * i + _yiqToRgbMatrix[1] * q)));
	g = std::max(0.0, std::min(1.0, (y + _yiqToRgbMatrix[2] * i + _yiqToRgbMatrix[3] * q)));
	b = std::max(0.0, std::min(1.0, (y + _yiqToRgbMatrix[4] * i + _yiqToRgbMatrix[5] * q)));
}

template<typename T>
bool BaseVideoFilter::NtscFilterOptionsChanged(T& ntscSetup)
{
	VideoConfig& cfg = _emu->GetSettings()->GetVideoConfig();

	return (
		ntscSetup.hue != cfg.Hue ||
		ntscSetup.saturation != cfg.Saturation ||
		ntscSetup.brightness != cfg.Brightness ||
		ntscSetup.contrast != cfg.Contrast ||
		ntscSetup.artifacts != cfg.NtscArtifacts ||
		ntscSetup.bleed != cfg.NtscBleed ||
		ntscSetup.fringing != cfg.NtscFringing ||
		ntscSetup.gamma != cfg.NtscGamma ||
		(ntscSetup.merge_fields == 1) != cfg.NtscMergeFields ||
		ntscSetup.resolution != cfg.NtscResolution ||
		ntscSetup.sharpness != cfg.NtscSharpness
	);
}

template<typename T>
void BaseVideoFilter::InitNtscFilter(T& ntscSetup)
{
	VideoConfig& cfg = _emu->GetSettings()->GetVideoConfig();
	ntscSetup.hue = cfg.Hue;
	ntscSetup.saturation = cfg.Saturation;
	ntscSetup.brightness = cfg.Brightness;
	ntscSetup.contrast = cfg.Contrast;

	ntscSetup.artifacts = cfg.NtscArtifacts;
	ntscSetup.bleed = cfg.NtscBleed;
	ntscSetup.fringing = cfg.NtscFringing;
	ntscSetup.gamma = cfg.NtscGamma;
	ntscSetup.merge_fields = (int)cfg.NtscMergeFields;
	ntscSetup.resolution = cfg.NtscResolution;
	ntscSetup.sharpness = cfg.NtscSharpness;
}

void BaseVideoFilter::TakeScreenshot(VideoFilterType filterType, string filename, std::stringstream *stream)
{
	uint32_t* pngBuffer;
	FrameInfo frameInfo;
	uint32_t* frameBuffer = nullptr;
	{
		auto lock = _frameLock.AcquireSafe();
		if(_bufferSize == 0 || !GetOutputBuffer()) {
			return;
		}

		frameBuffer = new uint32_t[_bufferSize];
		memcpy(frameBuffer, GetOutputBuffer(), _bufferSize * sizeof(frameBuffer[0]));
		frameInfo = _frameInfo;
	}

	pngBuffer = frameBuffer;
	
	uint8_t scale = 1;

	uint32_t screenRotation = _emu->GetSettings()->GetVideoConfig().ScreenRotation;
	unique_ptr<RotateFilter> rotateFilter(new RotateFilter(screenRotation));
	if(screenRotation != 0) {
		pngBuffer = rotateFilter->ApplyFilter(pngBuffer, frameInfo.Width, frameInfo.Height);
		frameInfo = rotateFilter->GetFrameInfo(frameInfo);
	}

	unique_ptr<ScaleFilter> scaleFilter = ScaleFilter::GetScaleFilter(filterType);
	if(scaleFilter) {
		pngBuffer = scaleFilter->ApplyFilter(pngBuffer, frameInfo.Width, frameInfo.Height);
		frameInfo = scaleFilter->GetFrameInfo(frameInfo);
		scale = scaleFilter->GetScale();
	}

	ScanlineFilter::ApplyFilter(pngBuffer, frameInfo.Width, frameInfo.Height, _emu->GetSettings()->GetVideoConfig().ScanlineIntensity, scale);
	
	if(!filename.empty()) {
		PNGHelper::WritePNG(filename, pngBuffer, frameInfo.Width, frameInfo.Height);
	} else {
		PNGHelper::WritePNG(*stream, pngBuffer, frameInfo.Width, frameInfo.Height);
	}

	delete[] frameBuffer;
}

void BaseVideoFilter::TakeScreenshot(string romName, VideoFilterType filterType)
{
	string romFilename = FolderUtilities::GetFilename(romName, false);

	int counter = 0;
	string baseFilename = FolderUtilities::CombinePath(FolderUtilities::GetScreenshotFolder(), romFilename);
	string ssFilename;
	while(true) {
		string counterStr = std::to_string(counter);
		while(counterStr.length() < 3) {
			counterStr = "0" + counterStr;
		}
		ssFilename = baseFilename + "_" + counterStr + ".png";
		ifstream file(ssFilename, ios::in);
		if(file) {
			file.close();
		} else {
			break;
		}
		counter++;
	}

	TakeScreenshot(filterType, ssFilename);

	MessageManager::DisplayMessage("ScreenshotSaved", FolderUtilities::GetFilename(ssFilename, true));
}

template bool BaseVideoFilter::NtscFilterOptionsChanged<nes_ntsc_setup_t>(nes_ntsc_setup_t& ntscSetup);
template bool BaseVideoFilter::NtscFilterOptionsChanged<snes_ntsc_setup_t>(snes_ntsc_setup_t& ntscSetup);
template void BaseVideoFilter::InitNtscFilter<nes_ntsc_setup_t>(nes_ntsc_setup_t& ntscSetup);
template void BaseVideoFilter::InitNtscFilter<snes_ntsc_setup_t>(snes_ntsc_setup_t& ntscSetup);
