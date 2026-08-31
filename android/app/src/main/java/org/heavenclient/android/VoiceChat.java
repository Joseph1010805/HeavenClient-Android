package org.heavenclient.android;

import android.content.Context;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioRecord;
import android.media.AudioTrack;
import android.media.MediaRecorder;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
import android.net.wifi.WifiManager;
import android.util.Log;

import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.util.Arrays;

/**
 * PUSH-TO-TALK VOICE, ON THE LOCAL NETWORK ONLY.
 *
 * <p>Everyone on the wifi hears everyone else. There is no room, no channel and
 * no roster: the devices this is for sit on one table or in one house, and a
 * roster would be three screens of plumbing for a problem that does not exist
 * yet. A friend across the world needs a relay beside the Cosmic server - that
 * is the NEXT slice, and it plugs in where {@link #target()} decides where a
 * packet goes.
 *
 * <h3>Why raw PCM and no codec</h3>
 *
 * 16 kHz mono 16-bit is 256 kbit/s, which on a home wifi is nothing - a single
 * photo is several seconds of it. Opus would cut that by ten and cost a native
 * dependency, a build change and a class of bug that only shows up as garbled
 * audio. On a LAN the bandwidth is not the scarce thing, so the first version
 * spends it and stays debuggable: a packet is just samples, and anything that
 * can read a byte array can check them.
 *
 * <h3>Push to talk, deliberately</h3>
 *
 * Not open mic. These are handhelds in a living room with the speakers on, and
 * an open mic means every device rebroadcasting every other device's output
 * plus the game music. Holding a button also means the microphone is off
 * unless someone is holding it, which is the honest default for a thing with a
 * microphone in a house with children in it.
 *
 * <h3>The microphone is not shared</h3>
 *
 * Vosk holds an {@link AudioRecord} whenever speech-to-text is listening, and
 * Android will not reliably give a second one out. So the two arbitrate: this
 * refuses to start while {@link SpeechInput} is listening, and the button that
 * drives it reports the refusal rather than silently doing nothing.
 */
public final class VoiceChat {

    private static final String TAG = "VoiceChat";

    /** Matches Vosk's rate, so one capture could feed both later. */
    private static final int SAMPLE_RATE = 16000;

    /**
     * 20 ms of audio per packet - 320 samples, 640 bytes, plus the header.
     *
     * <p>Small enough that a lost packet is an unnoticeable gap rather than a
     * stutter, large enough that we are not paying UDP's overhead per syllable.
     */
    private static final int FRAME_SAMPLES = 320;
    private static final int FRAME_BYTES = FRAME_SAMPLES * 2;

    private static final int PORT = 8477;

    /**
     * A four-byte mark on the front of every packet.
     *
     * <p>Anything else on this port is not ours. Without it a stray broadcast
     * from another program on the network gets played as audio, which sounds
     * exactly like a fault in this code.
     */
    private static final byte[] MAGIC = {'L', 'S', 'V', '1'};
    private static final int HEADER = MAGIC.length + 4;   // magic + sender id

    private static DatagramSocket socket;
    private static Thread captureThread;
    private static Thread playThread;

    private static AudioRecord recorder;
    private static AudioTrack track;
    private static AcousticEchoCanceler canceller;
    private static NoiseSuppressor suppressor;

    private static WifiManager.MulticastLock multicastLock;

    private static volatile boolean running;
    private static volatile boolean transmitting;

    /** Random per-run, so a device does not play back its own broadcast. */
    private static int selfId;

    private static InetAddress broadcast;

    private VoiceChat() {
    }

    /**
     * Opens the socket and starts listening. Does NOT open the microphone -
     * that happens only while the button is held.
     *
     * @return false if the socket could not be opened, or if speech-to-text
     *         currently owns the microphone.
     */
    public static synchronized boolean start(Context context) {
        if (running) {
            return true;
        }

        if (SpeechInput.isListening()) {
            Log.w(TAG, "not starting - speech to text is holding the microphone");
            return false;
        }

        try {
            selfId = (int) (System.nanoTime() ^ android.os.Process.myPid());

            socket = new DatagramSocket(PORT);
            socket.setBroadcast(true);
            socket.setReuseAddress(true);

            broadcast = target(context);

            // Broadcast packets are dropped by wifi power saving unless a lock
            // is held. Without this the app hears nothing and everything else
            // about the setup looks correct, which is the worst kind of bug.
            WifiManager wifi =
                    (WifiManager) context.getApplicationContext()
                            .getSystemService(Context.WIFI_SERVICE);

            if (wifi != null) {
                multicastLock = wifi.createMulticastLock("localstory-voice");
                multicastLock.setReferenceCounted(true);
                multicastLock.acquire();
            }

            openPlayback();

            running = true;

            playThread = new Thread(VoiceChat::receiveLoop, "voice-rx");
            playThread.start();

            captureThread = new Thread(() -> captureLoop(context), "voice-tx");
            captureThread.start();

            Log.i(TAG, "listening on " + PORT + " as " + selfId);

            return true;
        } catch (Exception e) {
            Log.e(TAG, "could not start", e);
            stop();

            return false;
        }
    }

    public static synchronized void stop() {
        running = false;
        transmitting = false;

        // The socket is what both loops block on, so closing it is how they
        // are woken; interrupt alone does not break a blocking receive.
        if (socket != null) {
            socket.close();
            socket = null;
        }

        join(captureThread);
        join(playThread);

        captureThread = null;
        playThread = null;

        closeCapture();
        closePlayback();

        if (multicastLock != null && multicastLock.isHeld()) {
            multicastLock.release();
        }

        multicastLock = null;
    }

    /** True while the socket is open and the receive loop is running. */
    public static boolean isRunning() {
        return running;
    }

    /** True while the button is held and samples are going out. */
    public static boolean isTransmitting() {
        return transmitting;
    }

    /**
     * Hold or release the button.
     *
     * <p>The capture loop watches this rather than being started and stopped,
     * because opening an {@link AudioRecord} takes long enough to clip the
     * first word off every sentence.
     */
    public static void setTransmitting(boolean on) {
        transmitting = on;
    }

    // -----------------------------------------------------------------------

    /**
     * Where a packet goes. The subnet's broadcast address, so every device on
     * the wifi gets it without anyone maintaining a list of who is playing.
     *
     * <p>This is the one place that changes when a remote relay is added: the
     * packet gets sent to the relay's address instead, and the relay fans it
     * out. Nothing else in this class knows the difference.
     */
    private static InetAddress target(Context context) throws Exception {
        return InetAddress.getByName("255.255.255.255");
    }

    private static void openPlayback() {
        int min = AudioTrack.getMinBufferSize(SAMPLE_RATE,
                AudioFormat.CHANNEL_OUT_MONO, AudioFormat.ENCODING_PCM_16BIT);

        track = new AudioTrack.Builder()
                .setAudioAttributes(new AudioAttributes.Builder()
                        // VOICE_COMMUNICATION, not MEDIA: it routes to the
                        // earpiece/headset the way a call does and lets the
                        // echo canceller below actually have something to
                        // cancel against.
                        .setUsage(AudioAttributes.USAGE_VOICE_COMMUNICATION)
                        .setContentType(AudioAttributes.CONTENT_TYPE_SPEECH)
                        .build())
                .setAudioFormat(new AudioFormat.Builder()
                        .setSampleRate(SAMPLE_RATE)
                        .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                        .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                        .build())
                .setBufferSizeInBytes(Math.max(min, FRAME_BYTES * 8))
                .setTransferMode(AudioTrack.MODE_STREAM)
                .build();

        track.play();
    }

    private static void closePlayback() {
        if (track != null) {
            try {
                track.stop();
            } catch (Exception ignored) {
            }

            track.release();
            track = null;
        }
    }

    private static void openCapture() {
        int min = AudioRecord.getMinBufferSize(SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_MONO, AudioFormat.ENCODING_PCM_16BIT);

        recorder = new AudioRecord(
                // VOICE_COMMUNICATION gives us the platform's own echo
                // cancellation and gain control. MIC would be rawer and would
                // howl the moment two devices sit on the same table.
                MediaRecorder.AudioSource.VOICE_COMMUNICATION,
                SAMPLE_RATE,
                AudioFormat.CHANNEL_IN_MONO,
                AudioFormat.ENCODING_PCM_16BIT,
                Math.max(min, FRAME_BYTES * 8));

        int session = recorder.getAudioSessionId();

        if (AcousticEchoCanceler.isAvailable()) {
            canceller = AcousticEchoCanceler.create(session);

            if (canceller != null) {
                canceller.setEnabled(true);
            }
        }

        if (NoiseSuppressor.isAvailable()) {
            suppressor = NoiseSuppressor.create(session);

            if (suppressor != null) {
                suppressor.setEnabled(true);
            }
        }

        recorder.startRecording();
    }

    private static void closeCapture() {
        if (canceller != null) {
            canceller.release();
            canceller = null;
        }

        if (suppressor != null) {
            suppressor.release();
            suppressor = null;
        }

        if (recorder != null) {
            try {
                recorder.stop();
            } catch (Exception ignored) {
            }

            recorder.release();
            recorder = null;
        }
    }

    private static void captureLoop(Context context) {
        byte[] packet = new byte[HEADER + FRAME_BYTES];

        System.arraycopy(MAGIC, 0, packet, 0, MAGIC.length);

        packet[MAGIC.length] = (byte) (selfId >> 24);
        packet[MAGIC.length + 1] = (byte) (selfId >> 16);
        packet[MAGIC.length + 2] = (byte) (selfId >> 8);
        packet[MAGIC.length + 3] = (byte) selfId;

        boolean open = false;

        while (running) {
            if (!transmitting) {
                if (open) {
                    // The microphone goes off the moment the button does. It
                    // is not left open "ready" - a live microphone nobody
                    // asked for is exactly what this design is avoiding.
                    closeCapture();
                    open = false;
                }

                try {
                    Thread.sleep(20);
                } catch (InterruptedException e) {
                    return;
                }

                continue;
            }

            if (!open) {
                try {
                    openCapture();
                    open = true;
                } catch (Exception e) {
                    Log.e(TAG, "microphone would not open", e);
                    transmitting = false;

                    continue;
                }
            }

            int got = recorder.read(packet, HEADER, FRAME_BYTES);

            if (got <= 0) {
                continue;
            }

            try {
                DatagramSocket s = socket;

                if (s != null) {
                    s.send(new DatagramPacket(packet, HEADER + got, broadcast, PORT));
                }
            } catch (Exception e) {
                // A send failing is normal when the wifi drops; it is not a
                // reason to tear the whole thing down.
                Log.w(TAG, "send failed: " + e.getMessage());
            }
        }

        if (open) {
            closeCapture();
        }
    }

    private static void receiveLoop() {
        byte[] buffer = new byte[HEADER + FRAME_BYTES * 2];
        DatagramPacket in = new DatagramPacket(buffer, buffer.length);

        while (running) {
            try {
                DatagramSocket s = socket;

                if (s == null) {
                    return;
                }

                s.receive(in);

                int length = in.getLength();

                if (length <= HEADER) {
                    continue;
                }

                // Not ours - some other program's broadcast.
                if (buffer[0] != MAGIC[0] || buffer[1] != MAGIC[1]
                        || buffer[2] != MAGIC[2] || buffer[3] != MAGIC[3]) {
                    continue;
                }

                int from = ((buffer[MAGIC.length] & 0xff) << 24)
                        | ((buffer[MAGIC.length + 1] & 0xff) << 16)
                        | ((buffer[MAGIC.length + 2] & 0xff) << 8)
                        | (buffer[MAGIC.length + 3] & 0xff);

                // Our own broadcast, come back to us. Playing it is a howl.
                if (from == selfId) {
                    continue;
                }

                AudioTrack t = track;

                if (t != null) {
                    // Straight out, no mixing. Two people talking at once will
                    // interleave rather than blend, which sounds like a bad
                    // radio - correct enough for a first version, and the fix
                    // is a per-sender buffer summed before write.
                    t.write(buffer, HEADER, length - HEADER);
                }
            } catch (Exception e) {
                if (running) {
                    Log.w(TAG, "receive stopped: " + e.getMessage());
                }

                return;
            }
        }
    }

    private static void join(Thread thread) {
        if (thread == null) {
            return;
        }

        try {
            thread.join(500);
        } catch (InterruptedException ignored) {
            Thread.currentThread().interrupt();
        }
    }
}
