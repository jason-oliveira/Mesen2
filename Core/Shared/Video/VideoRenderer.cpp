#include "pch.h"
#include "Shared/Video/VideoRenderer.h"
#include "Shared/Video/VideoDecoder.h"
#include "Shared/Interfaces/IRenderingDevice.h"
#include "Shared/Emulator.h"
#include "Shared/EmuSettings.h"
#include "Shared/Video/DebugHud.h"
#include "Shared/Video/SystemHud.h"
#include "Shared/InputHud.h"
#include "Shared/MessageManager.h"
#include "Utilities/Video/IVideoRecorder.h"
#include "Utilities/Video/AviRecorder.h"
#include "Utilities/Video/GifRecorder.h"

VideoRenderer::VideoRenderer(Emulator* emu)
{
	_emu = emu;
	_stopFlag = false;

	_rendererHud.reset(new DebugHud());
	_systemHud.reset(new SystemHud(_emu, _rendererHud.get()));
	_inputHud.reset(new InputHud(emu, _rendererHud.get()));
}

VideoRenderer::~VideoRenderer()
{
	_stopFlag = true;
	StopThread();
}

FrameInfo VideoRenderer::GetRendererSize()
{
	FrameInfo frame = {};
	frame.Width = _rendererWidth;
	frame.Height = _rendererHeight;
	return frame;
}

void VideoRenderer::SetRendererSize(uint32_t width, uint32_t height)
{
	_rendererWidth = width;
	_rendererHeight = height;
}

void VideoRenderer::StartThread()
{
	if(!_renderThread) {
		auto lock = _stopStartLock.AcquireSafe();
		if(!_renderThread) {
			_stopFlag = false;
			_waitForRender.Reset();

			_renderThread.reset(new std::thread(&VideoRenderer::RenderThread, this));
		}
	}
}

void VideoRenderer::StopThread()
{
	_stopFlag = true;
	if(_renderThread) {
		auto lock = _stopStartLock.AcquireSafe();
		if(_renderThread) {
			_renderThread->join();
			_renderThread.reset();
		}
	}
}

void VideoRenderer::RenderThread()
{
	while(!_stopFlag.load()) {
		//Wait until a frame is ready, or until 32ms have passed (to allow HUD to update at ~30fps when paused)
		_waitForRender.Wait(32);
		if(_renderer) {
			FrameInfo size = _emu->GetVideoDecoder()->GetBaseFrameInfo(true);
			_scriptHudSurface.UpdateSize(size.Width * _scriptHudScale, size.Height * _scriptHudScale);

			//Adjust the system HUD's width to match the aspect ratio to allow text to be unstretched
			//(The Lua HUD is not adjusted to allow scripts that need to match positions on the game screen to work correctly.)
			double aspectRatio = _emu->GetSettings()->GetAspectRatio(_emu->GetRegion(), size);
			size.Width = (uint32_t)std::round(size.Height * aspectRatio);
			_emuHudSurface.UpdateSize(size.Width, size.Height);

			RenderedFrame frame;
			{
				_frameLock.AcquireSafe();
				frame = _lastFrame;
			}

			_emuHudSurface.Clear();
			_inputHud->DrawControllers(size, frame.InputData);
			_systemHud->Draw(size.Width, size.Height);
			_rendererHud->Draw(_emuHudSurface.Buffer, size, {}, 0, false);

			DrawScriptHud(frame);

			_renderer->Render(_emuHudSurface, _scriptHudSurface);
		}
	}
}

void VideoRenderer::DrawScriptHud(RenderedFrame& frame)
{
	if(_lastScriptHudFrameNumber != frame.FrameNumber) {
		//Clear+draw HUD for scripts
		//-Only when frame number changes (to prevent the HUD from disappearing when paused, etc.)
		//-Only when commands are queued, otherwise skip drawing/clearing to avoid wasting CPU time
		if(_needScriptHudClear) {
			_scriptHudSurface.Clear();
			_needScriptHudClear = false;
		}

		if(_emu->GetScriptHud()->HasCommands()) {
			auto [size, overscan] = GetScriptHudSize();
			_emu->GetScriptHud()->Draw(_scriptHudSurface.Buffer, size, overscan, frame.FrameNumber, false);
			_needScriptHudClear = true;
			_lastScriptHudFrameNumber = frame.FrameNumber;
		}
	}
}

std::pair<FrameInfo, OverscanDimensions> VideoRenderer::GetScriptHudSize()
{
	FrameInfo scriptHudSize = { _scriptHudSurface.Width, _scriptHudSurface.Height };
	OverscanDimensions overscan = _emu->GetSettings()->GetOverscan();
	overscan.Top *= _scriptHudScale;
	overscan.Bottom *= _scriptHudScale;
	overscan.Left *= _scriptHudScale;
	overscan.Right *= _scriptHudScale;
	return { scriptHudSize, overscan };
}

void VideoRenderer::UpdateFrame(RenderedFrame& frame)
{
	shared_ptr<IVideoRecorder> recorder = _recorder.lock();
	if(recorder) {
		recorder->AddFrame(frame.FrameBuffer, frame.Width, frame.Height, _emu->GetFps());
	}

	{
		_frameLock.AcquireSafe();
		_lastFrame = frame;
	}

	if(_renderer) {
		_renderer->UpdateFrame(frame);
		_waitForRender.Signal();
	}
}

void VideoRenderer::ClearFrame()
{
	if(_renderer) {
		_renderer->ClearFrame();
	}
}

void VideoRenderer::RegisterRenderingDevice(IRenderingDevice *renderer)
{
	_renderer = renderer;
	StartThread();
}

void VideoRenderer::UnregisterRenderingDevice(IRenderingDevice *renderer)
{
	if(_renderer == renderer) {
		StopThread();
		_renderer = nullptr;
	}
}

void VideoRenderer::StartRecording(string filename, VideoCodec codec, uint32_t compressionLevel)
{
	FrameInfo frameInfo = _emu->GetVideoDecoder()->GetFrameInfo();

	shared_ptr<IVideoRecorder> recorder;
	if(codec == VideoCodec::GIF) {
		recorder.reset(new GifRecorder());
	} else {
		recorder.reset(new AviRecorder(codec, compressionLevel));
	}

	if(recorder->StartRecording(filename, frameInfo.Width, frameInfo.Height, 4, _emu->GetSettings()->GetAudioConfig().SampleRate, _emu->GetFps())) {
		_recorder.reset(recorder);
		MessageManager::DisplayMessage("VideoRecorder", "VideoRecorderStarted", filename);
	}
}

void VideoRenderer::AddRecordingSound(int16_t* soundBuffer, uint32_t sampleCount, uint32_t sampleRate)
{
	shared_ptr<IVideoRecorder> recorder = _recorder.lock();
	if(recorder) {
		recorder->AddSound(soundBuffer, sampleCount, sampleRate);
	}
}

void VideoRenderer::StopRecording()
{
	shared_ptr<IVideoRecorder> recorder = _recorder.lock();
	if(recorder) {
		MessageManager::DisplayMessage("VideoRecorder", "VideoRecorderStopped", recorder->GetOutputFile());
	}
	_recorder.reset();
}

bool VideoRenderer::IsRecording()
{
	return _recorder != nullptr;
}