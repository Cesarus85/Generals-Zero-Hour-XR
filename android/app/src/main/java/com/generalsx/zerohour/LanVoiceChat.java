// GeneralsX @feature Claude 29/09/2026 PLAN-027 LAN voice chat with voice
// activation. Audio and networking stay entirely on the Android side: the
// native XR loop only publishes the other human players' LAN addresses and
// the user's Off/On/Muted choice. Voice uses its own UDP port and never
// touches lockstep messages, CRCs or the game simulation.
//
// Capture: AudioRecord with the VOICE_COMMUNICATION source (platform echo
// cancellation/noise suppression where available) at 16 kHz mono PCM16, in
// 20 ms frames. A simple adaptive-energy gate with hangover sends only while
// the player speaks. Uncompressed PCM is about 256 kbit/s per speaker, which
// is fine on a LAN (Opus can follow for internet play).
// Playback: one VOICE_COMMUNICATION AudioTrack; each peer has a small jitter
// queue (primed at 2 frames, capped at 10), mixed with clipping.
package com.generalsx.zerohour;

import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
import android.util.Log;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.util.ArrayDeque;
import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

public final class LanVoiceChat {
    private static final String TAG = "gx-voice";
    /** Lobby 8086 and match 8088 are the game's; voice stays clear of both. */
    static final int PORT = 8094;
    static final int RATE = 16000;
    static final int FRAME = 320; // 20 ms at 16 kHz
    private static final int HEADER = 12;
    private static final byte[] MAGIC = {'G', 'X', 'V', '1'};
    private static final int MAX_QUEUE = 10, PRIME = 2;

    public static final int MODE_OFF = 0, MODE_ON = 1, MODE_MUTED = 2;
    // status() bits
    public static final int STATUS_ACTIVE = 1, STATUS_LOCAL_SPEAKING = 2,
            STATUS_PEER_SPEAKING = 4, STATUS_MIC_MISSING = 8;

    private static final class Peer {
        final ArrayDeque<short[]> queue = new ArrayDeque<>();
        boolean playing;
        volatile long lastHeardMs;
    }

    private static final Object LOCK = new Object();
    private static final Map<String, Peer> sPeers = new HashMap<>();
    private static String[] sPeerList = new String[0];
    private static volatile int sMode = MODE_OFF;
    private static volatile boolean sMicMissing, sLocalSpeaking;
    private static volatile long sLocalSpeakingUntilMs;

    /** One network session; loops exit on their own when running drops. No joins. */
    private static final class Session {
        volatile boolean running = true;
        DatagramSocket socket;
    }
    /** One capture run; the thread exits within one 20 ms frame after on drops. */
    private static final class Capture {
        volatile boolean on = true;
    }
    private static volatile Session sSession;
    private static volatile Capture sCapture;

    private LanVoiceChat() {}

    /** Called periodically from the XR thread. Idempotent and non-blocking. */
    public static void configure(int mode, String[] peers, boolean micAllowed) {
        synchronized (LOCK) {
            if (peers == null) peers = new String[0];
            final boolean peersChanged = !Arrays.equals(peers, sPeerList);
            if (peersChanged) {
                sPeerList = peers.clone();
                sPeers.keySet().retainAll(Arrays.asList(sPeerList));
                for (String ip : sPeerList) if (!sPeers.containsKey(ip)) sPeers.put(ip, new Peer());
            }
            sMode = mode;
            final boolean active = mode != MODE_OFF && sPeerList.length > 0;
            if (active && sSession == null) start();
            else if (!active && sSession != null) stop();
            final boolean wantCapture = sSession != null && mode == MODE_ON && micAllowed;
            if (wantCapture && sCapture == null) startCapture();
            else if (!wantCapture && sCapture != null) stopCapture();
            sMicMissing = active && mode == MODE_ON && !micAllowed;
            if (peersChanged) Log.i(TAG, "peers=" + Arrays.toString(sPeerList) + " mode=" + mode);
        }
    }

    public static int status() {
        final long now = System.currentTimeMillis();
        int bits = sSession != null ? STATUS_ACTIVE : 0;
        if (sCapture != null && sLocalSpeaking && sMode == MODE_ON) bits |= STATUS_LOCAL_SPEAKING;
        synchronized (LOCK) {
            for (Peer p : sPeers.values()) if (now - p.lastHeardMs < 300) { bits |= STATUS_PEER_SPEAKING; break; }
        }
        if (sMicMissing) bits |= STATUS_MIC_MISSING;
        return bits;
    }

    public static void shutdown() {
        synchronized (LOCK) { if (sSession != null) stop(); }
    }

    // ---------------------------------------------------------------- session
    private static void start() {
        final Session session = new Session();
        try {
            session.socket = new DatagramSocket(null);
            session.socket.setReuseAddress(true);
            session.socket.bind(new InetSocketAddress(PORT));
        } catch (Exception e) {
            Log.w(TAG, "voice socket unavailable", e);
            if (session.socket != null) session.socket.close();
            return;
        }
        sSession = session;
        final Thread rx = new Thread(() -> receiveLoop(session), "gx-voice-rx");
        final Thread play = new Thread(() -> playLoop(session), "gx-voice-play");
        rx.setDaemon(true); play.setDaemon(true);
        rx.start(); play.start();
        Log.i(TAG, "voice session started on UDP " + PORT);
    }

    private static void stop() {
        stopCapture();
        final Session session = sSession;
        sSession = null;
        if (session != null) { session.running = false; session.socket.close(); }
        for (Peer p : sPeers.values()) { p.queue.clear(); p.playing = false; }
        Log.i(TAG, "voice session stopped");
    }

    private static void startCapture() {
        final Capture capture = new Capture();
        sCapture = capture;
        final Thread t = new Thread(() -> captureLoop(capture), "gx-voice-tx");
        t.setDaemon(true);
        t.start();
    }

    private static void stopCapture() {
        final Capture capture = sCapture;
        sCapture = null;
        if (capture != null) capture.on = false;
        sLocalSpeaking = false;
    }

    // ---------------------------------------------------------------- capture
    private static void captureLoop(Capture capture) {
        AudioRecord record = null;
        AcousticEchoCanceler aec = null;
        NoiseSuppressor ns = null;
        try {
            final int min = AudioRecord.getMinBufferSize(RATE, AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);
            record = new AudioRecord(MediaRecorder.AudioSource.VOICE_COMMUNICATION, RATE,
                    AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT, Math.max(min, FRAME * 2 * 8));
            if (record.getState() != AudioRecord.STATE_INITIALIZED) throw new IllegalStateException("AudioRecord not initialized");
            if (AcousticEchoCanceler.isAvailable()) { aec = AcousticEchoCanceler.create(record.getAudioSessionId()); if (aec != null) aec.setEnabled(true); }
            if (NoiseSuppressor.isAvailable()) { ns = NoiseSuppressor.create(record.getAudioSessionId()); if (ns != null) ns.setEnabled(true); }
            Log.i(TAG, "capture started aec=" + (aec != null) + " ns=" + (ns != null));
            record.startRecording();
            final short[] pcm = new short[FRAME];
            final byte[] packet = new byte[HEADER + FRAME * 2];
            System.arraycopy(MAGIC, 0, packet, 0, 4);
            double noise = 300;
            int seq = 0, hangover = 0;
            while (capture.on) {
                int got = 0;
                while (got < FRAME && capture.on) {
                    final int n = record.read(pcm, got, FRAME - got);
                    if (n <= 0) break;
                    got += n;
                }
                if (got < FRAME) continue;
                double sum = 0;
                for (short s : pcm) sum += (double) s * s;
                final double rms = Math.sqrt(sum / FRAME);
                // Adaptive gate: speak when clearly above the tracked noise floor.
                final boolean loud = rms > Math.max(noise * 3.0, 450.0);
                if (loud) hangover = 15; // keep sending 300 ms after the last loud frame
                else noise = noise * 0.98 + rms * 0.02;
                final boolean speaking = hangover > 0;
                if (hangover > 0) --hangover;
                sLocalSpeaking = speaking;
                if (!speaking || sMode != MODE_ON) continue;
                packet[4] = (byte) (seq >>> 24); packet[5] = (byte) (seq >>> 16);
                packet[6] = (byte) (seq >>> 8); packet[7] = (byte) seq;
                packet[8] = (byte) (FRAME >>> 8); packet[9] = (byte) FRAME;
                packet[10] = 0; packet[11] = 0;
                for (int i = 0; i < FRAME; ++i) { packet[HEADER + 2 * i] = (byte) pcm[i]; packet[HEADER + 2 * i + 1] = (byte) (pcm[i] >>> 8); }
                ++seq;
                final String[] peers;
                synchronized (LOCK) { peers = sPeerList; }
                final Session session = sSession;
                if (session == null) continue;
                final DatagramSocket socket = session.socket;
                for (String ip : peers) {
                    try { socket.send(new DatagramPacket(packet, packet.length, InetAddress.getByName(ip), PORT)); }
                    catch (Exception e) { /* a peer that left is ignored until the next configure */ }
                }
            }
        } catch (SecurityException e) {
            sMicMissing = true;
            Log.w(TAG, "microphone permission missing", e);
        } catch (Exception e) {
            Log.w(TAG, "capture failed", e);
        } finally {
            if (aec != null) aec.release();
            if (ns != null) ns.release();
            if (record != null) { try { record.stop(); } catch (Exception ignored) {} record.release(); }
            if (sCapture == null || sCapture == capture) sLocalSpeaking = false;
            Log.i(TAG, "capture stopped");
        }
    }

    // ---------------------------------------------------------------- network
    private static void receiveLoop(Session session) {
        final byte[] buf = new byte[HEADER + FRAME * 2 + 64];
        final DatagramPacket packet = new DatagramPacket(buf, buf.length);
        while (session.running) {
            try {
                packet.setLength(buf.length);
                session.socket.receive(packet);
                if (packet.getLength() != HEADER + FRAME * 2) continue;
                if (buf[0] != MAGIC[0] || buf[1] != MAGIC[1] || buf[2] != MAGIC[2] || buf[3] != MAGIC[3]) continue;
                final int samples = ((buf[8] & 0xff) << 8) | (buf[9] & 0xff);
                if (samples != FRAME) continue;
                final String ip = packet.getAddress().getHostAddress();
                final short[] pcm = new short[FRAME];
                for (int i = 0; i < FRAME; ++i) pcm[i] = (short) ((buf[HEADER + 2 * i] & 0xff) | (buf[HEADER + 2 * i + 1] << 8));
                synchronized (LOCK) {
                    final Peer peer = sPeers.get(ip);
                    if (peer == null) continue; // only players of the current LAN game
                    if (peer.queue.size() >= MAX_QUEUE) peer.queue.pollFirst();
                    peer.queue.addLast(pcm);
                    peer.lastHeardMs = System.currentTimeMillis();
                }
            } catch (Exception e) {
                if (session.running) Log.w(TAG, "receive failed", e);
            }
        }
    }

    // ---------------------------------------------------------------- playback
    private static void playLoop(Session session) {
        AudioTrack track = null;
        try {
            final int min = AudioTrack.getMinBufferSize(RATE, AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT);
            track = new AudioTrack.Builder()
                    .setAudioAttributes(new AudioAttributes.Builder()
                            .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                            .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH).build())
                    .setAudioFormat(new AudioFormat.Builder().setSampleRate(RATE)
                            .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                            .setEncoding(AudioFormat.ENCODING_PCM_16BIT).build())
                    .setTransferMode(AudioTrack.MODE_STREAM)
                    .setBufferSizeInBytes(Math.max(min, FRAME * 2 * 4)).build();
            track.play();
            final int[] mix = new int[FRAME];
            final short[] out = new short[FRAME];
            while (session.running) {
                Arrays.fill(mix, 0);
                synchronized (LOCK) {
                    for (Peer p : sPeers.values()) {
                        if (!p.playing && p.queue.size() >= PRIME) p.playing = true;
                        if (!p.playing) continue;
                        final short[] frame = p.queue.pollFirst();
                        if (frame == null) { p.playing = false; continue; }
                        for (int i = 0; i < FRAME; ++i) mix[i] += frame[i];
                    }
                }
                for (int i = 0; i < FRAME; ++i) out[i] = (short) Math.max(-32768, Math.min(32767, mix[i]));
                track.write(out, 0, FRAME); // blocks: paces the loop at 20 ms
            }
        } catch (Exception e) {
            Log.w(TAG, "playback failed", e);
        } finally {
            if (track != null) { try { track.stop(); } catch (Exception ignored) {} track.release(); }
        }
    }
}
