/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 TheSuperHackers
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

////////////////////////////////////////////////////////////////////////////////
//																																						//
//  (c) 2001-2003 Electronic Arts Inc.																				//
//																																						//
////////////////////////////////////////////////////////////////////////////////

//////// FFmpegVideoPlayer.cpp ///////////////////////////
// Stephan Vedder, April 2025
/////////////////////////////////////////////////

//----------------------------------------------------------------------------
//         Includes
//----------------------------------------------------------------------------

#include <cstdio>
#include "GXTrace.h"

#include "Lib/BaseType.h"
#include "VideoDevice/FFmpeg/FFmpegVideoPlayer.h"
#include "Common/AudioAffect.h"
#include "Common/GameAudio.h"
#include "Common/GameMemory.h"
#include "Common/GlobalData.h"
#include "Common/Registry.h"
#include "Common/FileSystem.h"

#include "VideoDevice/FFmpeg/FFmpegFile.h"

extern "C" {
	#include <libavcodec/avcodec.h>
	#include <libswscale/swscale.h>
}

#ifdef SAGE_USE_OPENAL
#include "OpenALAudioDevice/OpenALAudioManager.h"
#include "OpenALAudioDevice/OpenALAudioStream.h"
#endif

#include <chrono>

//----------------------------------------------------------------------------
//         Externals
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Defines
//----------------------------------------------------------------------------
#define VIDEO_LANG_PATH_FORMAT "Data/%s/Movies/%s.%s"
#define VIDEO_PATH	"Data\\Movies"
#define VIDEO_EXT		"bik"



//----------------------------------------------------------------------------
//         Private Types
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Data
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Prototypes
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Private Functions
//----------------------------------------------------------------------------



//----------------------------------------------------------------------------
//         Public Functions
//----------------------------------------------------------------------------


//============================================================================
// FFmpegVideoPlayer::FFmpegVideoPlayer
//============================================================================

FFmpegVideoPlayer::FFmpegVideoPlayer()
{

}

//============================================================================
// FFmpegVideoPlayer::~FFmpegVideoPlayer
//============================================================================

FFmpegVideoPlayer::~FFmpegVideoPlayer()
{
	deinit();
}

//============================================================================
// FFmpegVideoPlayer::init
//============================================================================

void	FFmpegVideoPlayer::init()
{
	// Need to load the stuff from the ini file.
	VideoPlayer::init();

	initializeBinkWithMiles();
}

//============================================================================
// FFmpegVideoPlayer::deinit
//============================================================================

void FFmpegVideoPlayer::deinit()
{
	TheAudio->releaseHandleForBink();
	VideoPlayer::deinit();
}

//============================================================================
// FFmpegVideoPlayer::reset
//============================================================================

void	FFmpegVideoPlayer::reset()
{
	VideoPlayer::reset();
}

//============================================================================
// FFmpegVideoPlayer::update
//============================================================================

void	FFmpegVideoPlayer::update()
{
	VideoPlayer::update();

}

//============================================================================
// FFmpegVideoPlayer::loseFocus
//============================================================================

void	FFmpegVideoPlayer::loseFocus()
{
	VideoPlayer::loseFocus();
}

//============================================================================
// FFmpegVideoPlayer::regainFocus
//============================================================================

void	FFmpegVideoPlayer::regainFocus()
{
	VideoPlayer::regainFocus();
}

//============================================================================
// FFmpegVideoPlayer::createStream
//============================================================================

VideoStreamInterface* FFmpegVideoPlayer::createStream( File* file )
{

	if ( file == nullptr )
	{
		return nullptr;
	}

	FFmpegFile* ffmpegHandle = NEW FFmpegFile();
	if(!ffmpegHandle->open(file))
	{
		delete ffmpegHandle;
		return nullptr;
	}

	FFmpegVideoStream *stream = NEW FFmpegVideoStream(ffmpegHandle);

	if ( stream )
	{

		stream->m_next = m_firstStream;
		stream->m_player = this;
		m_firstStream = stream;

		// never let volume go to 0, as Bink will interpret that as "play at full volume".
		Int mod = (Int) ((TheAudio->getVolume(AudioAffect_Speech) * 0.8f) * 100) + 1;
		[[maybe_unused]]  Int volume = (32768 * mod) / 100;
		DEBUG_LOG(("FFmpegVideoPlayer::createStream() - About to set volume (%g -> %d -> %d",
			TheAudio->getVolume(AudioAffect_Speech), mod, volume));
		//BinkSetVolume( stream->m_handle,0, volume);
		DEBUG_LOG(("FFmpegVideoPlayer::createStream() - set volume"));
	}

	return stream;
}

//============================================================================
// FFmpegVideoPlayer::open
//============================================================================

VideoStreamInterface*	FFmpegVideoPlayer::open( AsciiString movieTitle )
{
	VideoStreamInterface*	stream = nullptr;

	const Video* pVideo = getVideo(movieTitle);
	if (pVideo) {
		DEBUG_LOG(("FFmpegVideoPlayer::createStream() - About to open bink file"));

		if (TheGlobalData->m_modDir.isNotEmpty())
		{
			char filePath[ _MAX_PATH ];
			snprintf( filePath, ARRAY_SIZE(filePath), "%s%s\\%s.%s", TheGlobalData->m_modDir.str(), VIDEO_PATH, pVideo->m_filename.str(), VIDEO_EXT );
			File* file =  TheFileSystem->openFile(filePath);
			DEBUG_ASSERTLOG(!file, ("opened bink file %s", filePath));
			if (file)
			{
				return createStream( file );
			}
		}

		char localizedFilePath[ _MAX_PATH ];
		snprintf( localizedFilePath, ARRAY_SIZE(localizedFilePath), VIDEO_LANG_PATH_FORMAT, GetRegistryLanguage().str(), pVideo->m_filename.str(), VIDEO_EXT );
		File* file =  TheFileSystem->openFile(localizedFilePath);
		DEBUG_ASSERTLOG(!file, ("opened localized bink file %s", localizedFilePath));
		if (!file)
		{
			char filePath[ _MAX_PATH ];
			snprintf( filePath, ARRAY_SIZE(filePath), "%s\\%s.%s", VIDEO_PATH, pVideo->m_filename.str(), VIDEO_EXT );
			file = TheFileSystem->openFile(filePath);
			DEBUG_ASSERTLOG(!file, ("opened bink file %s", filePath));
		}

		DEBUG_LOG(("FFmpegVideoPlayer::createStream() - About to create stream"));
		stream = createStream( file );
	}

	return stream;
}

//============================================================================
// FFmpegVideoPlayer::load
//============================================================================

VideoStreamInterface*	FFmpegVideoPlayer::load( AsciiString movieTitle )
{
	return open(movieTitle); // load() used to have the same body as open(), so I'm combining them.  Munkee.
}

//============================================================================
//============================================================================
void FFmpegVideoPlayer::notifyVideoPlayerOfNewProvider( Bool nowHasValid )
{
	if (!nowHasValid) {
		TheAudio->releaseHandleForBink();
		//BinkSetSoundTrack(0, 0);
	} else {
		initializeBinkWithMiles();
	}
}

//============================================================================
//============================================================================
void FFmpegVideoPlayer::initializeBinkWithMiles()
{
	Int retVal = 0;
	void *driver = TheAudio->getHandleForBink();

	if ( driver )
	{
		//retVal = BinkSoundUseDirectSound(driver);
	}
	if( !driver || retVal == 0)
	{
		//BinkSetSoundTrack ( 0,0 );
	}
}

//============================================================================
// FFmpegVideoStream::FFmpegVideoStream
//============================================================================

FFmpegVideoStream::FFmpegVideoStream(FFmpegFile* file)
: m_ffmpegFile(file)
{
	m_ffmpegFile->setFrameCallback(onFrame);
	m_ffmpegFile->setUserData(this);

#ifdef SAGE_USE_OPENAL
	// Release the audio handle if it's already in use
	OpenALAudioStream* audioStream = (OpenALAudioStream*)TheAudio->getHandleForBink();
	// GeneralsX @feature Android port 09/09/2026 Measure the tail the previous movie lost.
	// reset() is alSourceStop() + unqueue-everything, so whatever the last movie still had
	// buffered ahead of the speaker is discarded here. The movie's audio queue runs a decode
	// burst ahead of playback, so this is how much of the previous movie was never heard.
	ALint gxLeftOver = 0, gxPrevState = 0;
	alGetSourcei(audioStream->getSource(), AL_BUFFERS_QUEUED, &gxLeftOver);
	alGetSourcei(audioStream->getSource(), AL_SOURCE_STATE, &gxPrevState);
	GX_AUDIO_TRACE("new movie: dropping %d queued buffer(s) from the previous stream (state=%s)\n",
			(int)gxLeftOver,
			gxPrevState == AL_PLAYING ? "PLAYING" :
			gxPrevState == AL_STOPPED ? "STOPPED" :
			gxPrevState == AL_PAUSED  ? "PAUSED"  : "INITIAL");
	audioStream->reset();
#endif

	// Decode until we have our first video frame
	while (m_good && m_gotFrame == false)
		m_good = m_ffmpegFile->decodePacket();

#ifdef SAGE_USE_OPENAL
	// Start audio playback
	// GeneralsX @bugfix fbraz3 23/04/2026 Ensure video stream starts with audible gain even after prior source reuse.
	// Issue: https://github.com/fbraz3/GeneralsX/issues/38
	audioStream->setVolume(1.0f);
	// GeneralsX @bugfix Android port 09/09/2026 Do NOT restart a source that is already playing.
	// alSourcePlay() on an AL_PLAYING source rewinds it to the FRONT of its queue (openal-soft
	// StartSources: "A source that's already playing is restarted from the beginning"), so this
	// replayed every buffer the decode loop above had just queued. update() already starts the
	// source the moment the first buffer lands, which is why it is playing by the time we get
	// here; all this call has to do is cover the case where it is not.
	if (!audioStream->isPlaying())
	    audioStream->play();
#endif

	m_startTime = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

//============================================================================
// FFmpegVideoStream::~FFmpegVideoStream
//============================================================================

FFmpegVideoStream::~FFmpegVideoStream()
{
	// GeneralsX @bugfix fbraz3 20/04/2026 Stop and clear queued OpenAL buffers so video audio
	// does not bleed into gameplay after the loading screen closes the stream.
	// Issue: https://github.com/fbraz3/GeneralsX/issues/38
#ifdef SAGE_USE_OPENAL
	if (TheAudio)
	{
		OpenALAudioStream* audioStream = (OpenALAudioStream*)TheAudio->getHandleForBink();
		if (audioStream)
			audioStream->reset();
	}
#endif
	av_freep(&m_audioBuffer);
	av_frame_free(&m_frame);
	sws_freeContext(m_swsContext);
	delete m_ffmpegFile;
}

void FFmpegVideoStream::onFrame(AVFrame *frame, int stream_idx, int stream_type, void *user_data)
{
	FFmpegVideoStream *videoStream = static_cast<FFmpegVideoStream *>(user_data);
	if (stream_type == AVMEDIA_TYPE_VIDEO) {
		av_frame_free(&videoStream->m_frame);
		videoStream->m_frame = av_frame_clone(frame);
		videoStream->m_gotFrame = true;
	}
#ifdef SAGE_USE_OPENAL
	else if (stream_type == AVMEDIA_TYPE_AUDIO) {
		OpenALAudioStream* audioStream = (OpenALAudioStream*)TheAudio->getHandleForBink();
		AVSampleFormat sampleFmt = static_cast<AVSampleFormat>(frame->format);
		const int bytesPerSample = av_get_bytes_per_sample(sampleFmt);
		const int frameSize = av_samples_get_buffer_size(nullptr, frame->ch_layout.nb_channels, frame->nb_samples, sampleFmt, 1);
		if (frameSize <= 0 || bytesPerSample <= 0 || frame->ch_layout.nb_channels <= 0 || frame->nb_samples <= 0)
			return;

		int outputBitsPerSample = bytesPerSample * 8;
		int outputFrameSize = frameSize;
		uint8_t* frameData = frame->data[0];

		// GeneralsX @bugfix fbraz3 23/04/2026 Convert float32 FFmpeg output to PCM16 for robust OpenAL compatibility.
		// Issue: https://github.com/fbraz3/GeneralsX/issues/38
		if (sampleFmt == AV_SAMPLE_FMT_FLT || sampleFmt == AV_SAMPLE_FMT_FLTP)
		{
			outputBitsPerSample = 16;
			outputFrameSize = frame->nb_samples * frame->ch_layout.nb_channels * (int)sizeof(int16_t);
			videoStream->m_audioBuffer = static_cast<uint8_t*>(av_realloc(videoStream->m_audioBuffer, outputFrameSize));
			if (videoStream->m_audioBuffer == nullptr)
			{
				DEBUG_LOG(("Failed to allocate converted audio buffer"));
				return;
			}
			int16_t *dst = reinterpret_cast<int16_t *>(videoStream->m_audioBuffer);
			if (sampleFmt == AV_SAMPLE_FMT_FLTP)
			{
				for (int sample_idx = 0; sample_idx < frame->nb_samples; ++sample_idx)
				{
					for (int channel_idx = 0; channel_idx < frame->ch_layout.nb_channels; ++channel_idx)
					{
						const float sample = reinterpret_cast<const float *>(frame->data[channel_idx])[sample_idx];
						const float clamped = (sample < -1.0f) ? -1.0f : ((sample > 1.0f) ? 1.0f : sample);
						*dst++ = static_cast<int16_t>(clamped * 32767.0f);
					}
				}
			}
			else
			{
				const float *src = reinterpret_cast<const float *>(frame->data[0]);
				const int totalSamples = frame->nb_samples * frame->ch_layout.nb_channels;
				for (int i = 0; i < totalSamples; ++i)
				{
					const float clamped = (src[i] < -1.0f) ? -1.0f : ((src[i] > 1.0f) ? 1.0f : src[i]);
					*dst++ = static_cast<int16_t>(clamped * 32767.0f);
				}
			}
			frameData = videoStream->m_audioBuffer;
		}
		// The format is planar - convert it to interleaved
		else if (av_sample_fmt_is_planar(sampleFmt))
		{
			videoStream->m_audioBuffer = static_cast<uint8_t*>(av_realloc(videoStream->m_audioBuffer, outputFrameSize));
			if (videoStream->m_audioBuffer == nullptr)
			{
				DEBUG_LOG(("Failed to allocate audio buffer"));
				return;
			}
			// Write the samples into our audio buffer
			for (int sample_idx = 0; sample_idx < frame->nb_samples; sample_idx++)
			{
				int byte_offset = sample_idx * bytesPerSample;
				for (int channel_idx = 0; channel_idx < frame->ch_layout.nb_channels; channel_idx++)
				{
					uint8_t* dst = &videoStream->m_audioBuffer[byte_offset * frame->ch_layout.nb_channels + channel_idx * bytesPerSample];
					uint8_t* src = &frame->data[channel_idx][byte_offset];
					memcpy(dst, src, bytesPerSample);
				}
			}
			frameData = videoStream->m_audioBuffer;
		}

		ALenum format = OpenALAudioManager::getALFormat(frame->ch_layout.nb_channels, outputBitsPerSample);

		// GeneralsX @bugfix Android port 08/09/2026 Give the movie's audio queue a cushion of
		// silence ahead of its first real buffer. Movie audio is produced here, one AVFrame per
		// decoded packet, and the decode loop only runs when the player wants the next VIDEO
		// frame -- so a single ~40ms buffer is queued, played out in 40ms, and the source drains
		// and stops before the next video frame arrives. On Android that made the intro movies
		// completely inaudible. Decoding ahead is not an option (it would drop video frames),
		// but padding the FRONT of the queue costs only a fixed lead-in and keeps the source
		// AL_PLAYING across frame boundaries. Kept in sync with the GeneralsMD copy of this file,
		// which is the one that actually links (see FFmpegFile.cpp for that trap).
		if (!videoStream->m_audioPrimed) {
			videoStream->m_audioPrimed = true;
			const int gxPrimeBuffers = 6;   // 6 * ~40ms == ~240ms of lead-in
			uint8_t* gxSilence = static_cast<uint8_t*>(av_mallocz(outputFrameSize));
			if (gxSilence != nullptr) {
				for (int i = 0; i < gxPrimeBuffers; ++i) {
					if (!audioStream->bufferData(gxSilence, outputFrameSize, format, frame->sample_rate))
						break;
				}
				av_freep(&gxSilence);
			}
		}

		// GeneralsX @feature Android port 09/09/2026 Peak amplitude of the PCM we are about to
		// queue. This is the one thing the device logs could never answer: whether the silence
		// is in the pipeline or in the file. peak==0 across a whole movie means the decoder is
		// handing us digital silence; a healthy track peaks in the thousands.
		static int gxPeakSinceTrace = 0;
		if (outputBitsPerSample == 16) {
			const int16_t* gxPcm = reinterpret_cast<const int16_t*>(frameData);
			const int gxCount = outputFrameSize / (int)sizeof(int16_t);
			for (int i = 0; i < gxCount; ++i) {
				const int v = gxPcm[i] < 0 ? -(int)gxPcm[i] : (int)gxPcm[i];
				if (v > gxPeakSinceTrace)
					gxPeakSinceTrace = v;
			}
		}

		if (!audioStream->bufferData(frameData, outputFrameSize, format, frame->sample_rate)) {
			GX_AUDIO_TRACE("movie audio frame: bufferData() FAILED (format=0x%x rate=%d size=%d)\n",
					(unsigned)format, frame->sample_rate, outputFrameSize);
		}
		audioStream->update();

		static int gxAudioFrameCount = 0;
		if ((gxAudioFrameCount++ % 30) == 0) {
			ALint gxQueued = 0, gxProcessed = 0, gxState = 0;
			alGetSourcei(audioStream->getSource(), AL_BUFFERS_QUEUED, &gxQueued);
			alGetSourcei(audioStream->getSource(), AL_BUFFERS_PROCESSED, &gxProcessed);
			alGetSourcei(audioStream->getSource(), AL_SOURCE_STATE, &gxState);
			GX_AUDIO_TRACE("movie audio frame #%d: ch=%d samples=%d fmt=%d rate=%d peak=%d queued=%d processed=%d state=%s\n",
					gxAudioFrameCount, frame->ch_layout.nb_channels, frame->nb_samples,
					(int)frame->format, frame->sample_rate, gxPeakSinceTrace,
					(int)gxQueued, (int)gxProcessed,
					gxState == AL_PLAYING ? "PLAYING" :
					gxState == AL_STOPPED ? "STOPPED" :
					gxState == AL_PAUSED  ? "PAUSED"  : "INITIAL");
			gxPeakSinceTrace = 0;
		}
	}
#endif
}


//============================================================================
// FFmpegVideoStream::update
//============================================================================

void FFmpegVideoStream::update()
{
#ifdef SAGE_USE_OPENAL
	// Resume audio playback only if not already playing.
	// Calling alSourcePlay() on an AL_PLAYING source restarts it from the
	// first unprocessed buffer (OpenAL 1.1 spec §4.3.9), which causes
	// effective silence when invoked every game frame.
	OpenALAudioStream* audioStream = (OpenALAudioStream*)TheAudio->getHandleForBink();

	// GeneralsX @bugfix Android port 09/09/2026 Keep the movie's audio fed at REAL TIME,
	// independently of how fast the video is managing to decode. Packets are only pulled when
	// the player wants the next VIDEO frame, so on a device that cannot decode and software-
	// scale the movie at its own frame rate the audio advances in slow motion and its source
	// drains between every pair of frames. Topping the queue up here is self-limiting:
	// decoding ahead advances frameIndex(), isFrameReady() compares that against the wall
	// clock, and the player stops asking for frames until time catches up -- so video frames
	// are dropped on a device too slow to show them all, with audio as the master clock.
	// m_good is deliberately not assigned here, so end-of-stream is still detected in
	// frameNext(). Kept in sync with the GeneralsMD copy, which is the one that links.
	if (m_good && m_ffmpegFile != nullptr && m_ffmpegFile->hasAudio())
	{
		ALint gxQueued = 0;
		alGetSourcei(audioStream->getSource(), AL_BUFFERS_QUEUED, &gxQueued);
		int gxBudget = 24;
		// Never let the look-ahead run into the tail: Display::update() ends a movie by
		// testing frameIndex() against frameCount()-1 for EQUALITY, so overshooting it hangs
		// the movie on its last frame with no way to skip. Stop a few frames short and let
		// Display::update() walk onto the last frame itself.
		const int gxLastFrame = m_ffmpegFile->getNumFrames() - 1;
		while (gxQueued < AL_STREAM_BUFFER_COUNT / 2 && gxBudget-- > 0
		       && m_ffmpegFile->getCurrentFrame() + 4 < gxLastFrame)
		{
			if (!m_ffmpegFile->decodePacket())
				break;
			alGetSourcei(audioStream->getSource(), AL_BUFFERS_QUEUED, &gxQueued);
		}
	}

	if (!audioStream->isPlaying()) {
		audioStream->play();
	}
#endif
	//BinkWait( m_handle );
}

//============================================================================
// FFmpegVideoStream::isFrameReady
//============================================================================

Bool FFmpegVideoStream::isFrameReady()
{
	uint64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
	bool ready = (time - m_startTime) >= m_ffmpegFile->getFrameTime() * frameIndex();
	return ready;

	//return !BinkWait( m_handle );
}

//============================================================================
// FFmpegVideoStream::frameDecompress
//============================================================================

void FFmpegVideoStream::frameDecompress()
{
	//BinkDoFrame( m_handle );
}

//============================================================================
// FFmpegVideoStream::frameRender
//============================================================================

void FFmpegVideoStream::frameRender( VideoBuffer *buffer )
{
	if (buffer == nullptr) {
		return;
	}

	if (m_frame == nullptr) {
		return;
	}

	if (m_frame->data == nullptr) {
		return;
	}

	AVPixelFormat dst_pix_fmt;

	switch (buffer->format()) {
		case VideoBuffer::TYPE_R8G8B8:
			dst_pix_fmt = AV_PIX_FMT_RGB24;
			break;
		case VideoBuffer::TYPE_X8R8G8B8:
			dst_pix_fmt = AV_PIX_FMT_BGR0;
			break;
		case VideoBuffer::TYPE_R5G6B5:
			dst_pix_fmt = AV_PIX_FMT_RGB565;
			break;
		case VideoBuffer::TYPE_X1R5G5B5:
			dst_pix_fmt = AV_PIX_FMT_RGB555;
			break;
		default:
			return;
	}

	// GeneralsX @bugfix Android port 06/08/2026 vcpkg's own ffmpeg port
	// (ports/ffmpeg/build.sh.in: "Disable asm and x86asm on all android
	// targets because they trigger build failures") passes
	// --disable-asm --disable-x86asm for every Android arch, arm64
	// included -- confirmed via this build's own ffbuild/config.log
	// (ARCH_AARCH64 0, HAVE_NEON 0). That leaves libswscale with no SIMD/
	// NEON path at all on Android, so every sws_scale() call here already
	// runs the slow portable-C fallback (visible as libswscale's own
	// "No accelerated colorspace conversion found" warning, once per video
	// frame). SWS_BICUBIC on top of that made a real device report the
	// background/cutscene video crawl to ~1fps while audio (decoupled from
	// this path) stayed real-time. There's no accelerated path to lose by
	// switching filters here, so use the cheapest one instead of the
	// highest-quality one -- SWS_FAST_BILINEAR trades a barely-perceptible
	// amount of smoothing for a large constant-factor speedup, still on
	// pure C. Desktop platforms keep SWS_BICUBIC since their swscale build
	// does have SIMD acceleration and the cost is negligible there.
#if defined(__ANDROID__)
	Int swsFlags = SWS_FAST_BILINEAR;
#else
	Int swsFlags = SWS_BICUBIC;
#endif
	m_swsContext = sws_getCachedContext(m_swsContext,
		width(),
		height(),
		static_cast<AVPixelFormat>(m_frame->format),
		buffer->width(),
		buffer->height(),
		dst_pix_fmt,
		swsFlags,
		nullptr,
		nullptr,
		nullptr);

	uint8_t *buffer_data = static_cast<uint8_t *>(buffer->lock());
	if (buffer_data == nullptr) {
		// Was a DEBUG_LOG (compiled out in release), which made this failure
		// invisible in device logs -- and it is precisely what fired on the
		// Mali-G57 right before its driver-side crash (issue #9, build 176:
		// the sws context line printed but the frame-geometry line right
		// after this lock never did). Keep it loud and rate-limited.
		static int lockFailLogs = 0;
		if (lockFailLogs < 8) {
			++lockFailLogs;
			fprintf(stderr, "[GX-VIDBUF] frameRender: buffer->lock() FAILED (dst=%ux%u)\n",
				buffer->width(), buffer->height());
			fflush(stderr);
		}
		return;
	}

	int dst_strides[] = { (int)buffer->pitch() };
	uint8_t *dst_data[] = { buffer_data };

	// GeneralsX @bugfix Android port 18/07/2026 issue #9 follow-up: log the
	// exact geometry of every video-frame blit into the locked surface. A
	// Mali device that now gets past DXVK device creation reaches this code
	// path (loading-screen video) and then hard-faults inside the vendor
	// GLES driver a few lines later -- if dst_strides[0] is ever smaller
	// than one destination row actually needs, sws_scale would write past
	// the row into the next one and eventually off the end of the surface,
	// corrupting whatever GPU-mapped memory follows it (which would only
	// visibly crash later, inside the driver, matching what we're seeing).
	static bool loggedOnce = false;
	if (!loggedOnce) {
		loggedOnce = true;
		int bytesPerPixel = 4;
		switch (dst_pix_fmt) {
			case AV_PIX_FMT_RGB24: bytesPerPixel = 3; break;
			case AV_PIX_FMT_BGR0:  bytesPerPixel = 4; break;
			case AV_PIX_FMT_RGB565: bytesPerPixel = 2; break;
			case AV_PIX_FMT_RGB555: bytesPerPixel = 2; break;
			default: break;
		}
		int minStride = (int)buffer->width() * bytesPerPixel;
		fprintf(stderr, "[GX-VIDBUF] frameRender dst=%ux%u pitch=%d bpp=%d min_required_stride=%d %s\n",
			buffer->width(), buffer->height(), dst_strides[0], bytesPerPixel, minStride,
			dst_strides[0] < minStride ? "!!! STRIDE TOO SMALL !!!" : "ok");
		fflush(stderr);
	}

	[[maybe_unused]] int result =
		sws_scale(m_swsContext, m_frame->data, m_frame->linesize, 0, height(), dst_data, dst_strides);
	DEBUG_ASSERTLOG(result >= 0, ("Failed to scale frame"));
	buffer->unlock();
}

//============================================================================
// FFmpegVideoStream::frameNext
//============================================================================

void FFmpegVideoStream::frameNext()
{
	m_gotFrame = false;
	// Decode until we have our next video frame
	while (m_good && m_gotFrame == false)
		m_good = m_ffmpegFile->decodePacket();
}

//============================================================================
// FFmpegVideoStream::frameIndex
//============================================================================

Int FFmpegVideoStream::frameIndex()
{
	return m_ffmpegFile->getCurrentFrame();
}

//============================================================================
// FFmpegVideoStream::totalFrames
//============================================================================

Int	FFmpegVideoStream::frameCount()
{
	return m_ffmpegFile->getNumFrames();
}

//============================================================================
// FFmpegVideoStream::frameGoto
//============================================================================

void FFmpegVideoStream::frameGoto( Int index )
{
	m_ffmpegFile->seekFrame(index);
}

//============================================================================
// VideoStream::height
//============================================================================

Int		FFmpegVideoStream::height()
{
	return m_ffmpegFile->getHeight();
}

//============================================================================
// VideoStream::width
//============================================================================

Int		FFmpegVideoStream::width()
{
	return m_ffmpegFile->getWidth();
}


