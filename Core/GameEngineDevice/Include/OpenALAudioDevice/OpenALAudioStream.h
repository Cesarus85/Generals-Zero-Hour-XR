/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
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

// FILE: OpenALAudioStream.h //////////////////////////////////////////////////////////////////////////
// OpenALAudioStream implementation
// Author: Stephan Vedder, March 2025
#pragma once

#include "always.h"
#include <AL/al.h>
#include <stdint.h>
#include <functional>

#define AL_STREAM_BUFFER_COUNT 32

class OpenALAudioStream final
{
public:
    OpenALAudioStream();
    ~OpenALAudioStream();

    // GeneralsX @bugfix 14/06/2026 The data callback now returns FALSE when the source
    // stream has reached true end-of-file (no more data will ever come), so update()
    // can let a finished one-shot speech reach a stable AL_STOPPED instead of endlessly
    // restarting its drained source. Returns TRUE on a normal refill or transient error.
    void setRequireDataCallback(std::function<bool()> callback) { m_requireDataCallback = callback; }
    ALuint getSource() const { return m_source; }
    // GeneralsX @bugfix 14/06/2026 True once the source stream signalled real EOF.
    bool isAtEnd() const { return m_endOfData; }

    bool bufferData(uint8_t *data, size_t data_size, ALenum format, int samplerate);
    bool isPlaying();
    void update();
    void reset();

    // GeneralsX @bugfix Android port 08/09/2026 pause() has to LATCH, not just ask OpenAL
    // to pause. update() restarts any source it finds in AL_PAUSED with buffers queued (two
    // places, both deliberate -- they recover a stream whose queue ran dry). Nothing told
    // those two apart from a stream the GAME paused, so a paused stream un-paused itself on
    // the very next audio update, and TheAudio->UPDATE() keeps running while the game is
    // paused. Speech and music therefore never actually stayed paused.
    // GeneralsX @bugfix Android port 09/09/2026 Starting or stopping the source hands the
    // accounting back to OpenAL, so the "queued while stopped, never rendered" tally goes to
    // zero. alSourcePlay() always (re)starts a source at the FRONT of its queue, so anything
    // still queued is about to be played from the beginning; alSourceStop() abandons the queue.
    void play() { m_paused = false; m_unplayedWhileStopped = 0; alSourcePlay(m_source); }
    void pause() { m_paused = true; alSourcePause(m_source); }
    void stop() { m_paused = false; m_unplayedWhileStopped = 0; alSourceStop(m_source); }
    bool isPaused() const { return m_paused; }

    void setVolume(float vol) { alSourcef(m_source, AL_GAIN, vol); }

protected:
    // GeneralsX @bugfix Android port 09/09/2026 How many buffers OpenAL will claim are
    // "processed" that it has in fact never rendered a single sample of. See the long comment
    // on gxPlayedBuffers() in the .cpp: a source in AL_STOPPED reports its ENTIRE queue as
    // processed, including buffers queued after it stopped.
    ALint gxPlayedBuffers() const;

    std::function<bool()> m_requireDataCallback = nullptr;
    bool m_endOfData = false; ///< GeneralsX: source stream signalled true EOF; stop restarting it
    bool m_paused = false;    ///< GeneralsX: the GAME paused this stream; update() must not restart it
    int m_stalledProbes = 0;  ///< GeneralsX: consecutive EOF-probes that produced no new data
    unsigned long m_lastProbeMs = 0; ///< GeneralsX: when the previous EOF-probe ran, to tell a stuck decoder from a starved one
    ALuint m_source = 0;
    ALuint m_buffers[AL_STREAM_BUFFER_COUNT] = {};
    unsigned int m_current_buffer_idx = 0;
    ALint m_unplayedWhileStopped = 0; ///< GeneralsX: buffers queued onto an AL_STOPPED source, which OpenAL then misreports as already processed
};