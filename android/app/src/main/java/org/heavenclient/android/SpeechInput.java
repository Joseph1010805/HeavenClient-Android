package org.heavenclient.android;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.media.audiofx.AcousticEchoCanceler;
import android.media.audiofx.NoiseSuppressor;
import android.util.Log;

import org.json.JSONObject;
import org.vosk.Model;
import org.vosk.Recognizer;
import org.vosk.android.RecognitionListener;
import org.vosk.android.SpeechService;

import java.io.File;

/**
 * Speech to text, entirely on the device.
 *
 * <p>None of these machines has a keyboard. The Thor and the RP5 have a d-pad
 * and an on-screen key grid; the Quest has a laser pointer and nothing else.
 * Saying a sentence beats spelling it out with a thumbstick by a mile.
 *
 * <p><b>Nothing leaves the house.</b> This is Vosk with a small English model
 * on local storage - no speech API, no account, no network. It works with the
 * router unplugged, which is the promise the rest of this build makes.
 *
 * <p>Android's own {@code SpeechRecognizer} was the obvious alternative and is
 * not usable here: the Thor answers {@code cmd package query-services
 * android.speech.RecognitionService} with "No services found", so there is no
 * engine installed to talk to. It would also want the network for anything
 * good.
 *
 * <p>The model is <b>not in the apk</b> - it is ~40MB and not ours to put on a
 * release page. {@code tools/deploy_data.sh} pushes it beside the game data.
 * When it is absent this reports itself unavailable and the microphone buttons
 * stay quiet, rather than appearing to work and doing nothing.
 */
public final class SpeechInput
{
    private static final String TAG = "HeavenClient";

    /** Beside the .nx files, which is the one place large data already goes. */
    private static final String MODEL_DIR = "vosk-model";

    /** What Vosk's small English models are trained at. Not negotiable. */
    private static final float SAMPLE_RATE = 16000.0f;

    private static Model model;
    private static SpeechService service;

    // The platform DSP, held so it can be released with the session.
    private static NoiseSuppressor noise;
    private static AcousticEchoCanceler echo;

    private SpeechInput()
    {
    }

    private static File modelPath(Context context)
    {
        File dir = context.getExternalFilesDir(null);

        if (dir == null)
            return null;

        return new File(new File(dir, "HeavenClient"), MODEL_DIR);
    }

    /**
     * Whether speech input can be offered at all - which means a MODEL on
     * disk, and nothing else.
     *
     * <p>The microphone permission is deliberately NOT checked here. It used to
     * be, and that made the feature impossible to ever turn on: the button is
     * hidden while speech is unavailable, permission is requested when the
     * button is pressed, so the permission could never be granted and the
     * button could never appear. A perfect circle, and entirely self-inflicted.
     *
     * <p>What this asks is "could this ever work on this device" - is the model
     * deployed. Whether it can work RIGHT NOW is start()'s business, and that
     * is where permission is asked for, at the moment the player has just shown
     * they want it.
     */
    public static boolean isAvailable(Context context)
    {
        File path = modelPath(context);

        return path != null && path.isDirectory();
    }

    public static boolean start(Context context)
    {
        if (service != null)
            return true;

        File path = modelPath(context);

        if (path == null || !path.isDirectory())
        {
            Log.i(TAG, "speech: no model at " + path + " - run tools/deploy_data.sh");
            return false;
        }

        if (context.checkSelfPermission(Manifest.permission.RECORD_AUDIO)
                != PackageManager.PERMISSION_GRANTED)
        {
            // Asked for here rather than at launch, because a game that wants
            // the microphone before it has said why is a game people refuse.
            // The press that triggers this is the explanation.
            if (context instanceof HeavenClientActivity)
                ((HeavenClientActivity) context).requestPermissions(
                        new String[] { Manifest.permission.RECORD_AUDIO }, 1);

            Log.i(TAG, "speech: asking for the microphone; press again once granted");
            return false;
        }

        try
        {
            // Loading the model takes a second or so and is kept afterwards -
            // doing it per sentence would put that pause in front of every
            // thing anybody wanted to say.
            if (model == null)
                model = new Model(path.getAbsolutePath());

            Recognizer recognizer = new Recognizer(model, SAMPLE_RATE);

            service = new SpeechService(recognizer, SAMPLE_RATE);
            service.startListening(LISTENER);

            attachEffects();

            Log.i(TAG, "speech: listening");

            return true;
        }
        catch (Exception e)
        {
            Log.i(TAG, "speech: could not start - " + e);

            shutdown();

            return false;
        }
    }

    public static void stop()
    {
        if (service != null)
        {
            // stop() rather than cancel(), so a sentence cut short is still
            // delivered - it is usually still what the player meant to say.
            service.stop();
        }
    }

    /**
     * Switches on the platform's own noise suppression and echo cancellation
     * for Vosk's recorder.
     *
     * <p>Vosk already opens the microphone as {@code VOICE_RECOGNITION} - the
     * right source, confirmed by disassembling its {@code SpeechService}. But
     * choosing that source only asks for a clean signal; it does not turn on
     * the DSP. {@link NoiseSuppressor} and {@link AcousticEchoCanceler} are
     * separate effects, attached to the recorder's audio SESSION, and on most
     * devices they are off unless somebody asks.
     *
     * <p>The echo canceller is the interesting one here. The game's own sound
     * is muted while the microphone is open, but the speaker still carries the
     * other handheld in the room, and a Thor's speaker is an inch from its
     * microphone.
     *
     * <p>The session id comes out by reflection, because {@code SpeechService}
     * keeps its {@code AudioRecord} private and exposes nothing. That is worth
     * doing for a well-known library on a pinned version, and it fails softly:
     * if the field is ever renamed this logs and carries on with the
     * recognition it already had.
     */
    private static void attachEffects()
    {
        try
        {
            java.lang.reflect.Field field = SpeechService.class.getDeclaredField("recorder");
            field.setAccessible(true);

            android.media.AudioRecord recorder =
                    (android.media.AudioRecord) field.get(service);

            if (recorder == null)
                return;

            int session = recorder.getAudioSessionId();

            if (NoiseSuppressor.isAvailable())
            {
                noise = NoiseSuppressor.create(session);

                if (noise != null)
                    noise.setEnabled(true);
            }

            if (AcousticEchoCanceler.isAvailable())
            {
                echo = AcousticEchoCanceler.create(session);

                if (echo != null)
                    echo.setEnabled(true);
            }

            Log.i(TAG, "speech: noise suppressor " + (noise != null ? "on" : "unavailable")
                    + ", echo canceller " + (echo != null ? "on" : "unavailable"));
        }
        catch (Exception e)
        {
            // Not fatal. Recognition still works, just without the help.
            Log.i(TAG, "speech: could not attach audio effects - " + e);
        }
    }

    private static void releaseEffects()
    {
        if (noise != null)
        {
            noise.release();
            noise = null;
        }

        if (echo != null)
        {
            echo.release();
            echo = null;
        }
    }

    private static void shutdown()
    {
        // Effects first - they hold a session that is about to go away.
        releaseEffects();

        if (service != null)
        {
            service.shutdown();
            service = null;
        }
    }

    /** Pulls the words out of Vosk's {"text" : "..."} result. */
    private static String textOf(String hypothesis)
    {
        if (hypothesis == null)
            return "";

        try
        {
            return new JSONObject(hypothesis).optString("text", "").trim();
        }
        catch (Exception e)
        {
            return "";
        }
    }

    private static final RecognitionListener LISTENER = new RecognitionListener()
    {
        @Override
        public void onPartialResult(String hypothesis)
        {
            // Passed on, so the bubble over the player's head fills in as they
            // talk.
            //
            // These were ignored at first, on the grounds that partials rewrite
            // themselves as the recogniser changes its mind and watching a chat
            // BOX flicker through wrong guesses is worse than waiting. That is
            // still true of the chat box - which is why the box only ever gets
            // a final result. Over a character's head it is not a defect at
            // all: it reads as somebody thinking aloud, and it is the only
            // thing that tells you the microphone is actually hearing you.
            //
            // Vosk uses "partial" here rather than "text".
            String text = "";

            try
            {
                text = new JSONObject(hypothesis).optString("partial", "").trim();
            }
            catch (Exception e)
            {
                return;
            }

            if (!text.isEmpty())
                nativePartial(text);
        }

        @Override
        public void onResult(String hypothesis)
        {
            deliver(textOf(hypothesis));
        }

        @Override
        public void onFinalResult(String hypothesis)
        {
            deliver(textOf(hypothesis));
            shutdown();
        }

        @Override
        public void onError(Exception e)
        {
            Log.i(TAG, "speech: " + e);
            shutdown();
            nativeStopped();
        }

        @Override
        public void onTimeout()
        {
            shutdown();
            nativeStopped();
        }

        private void deliver(String text)
        {
            if (text.isEmpty())
                return;

            Log.i(TAG, "speech: heard \"" + text + "\"");

            nativePhrase(text);
        }
    };

    private static native void nativePhrase(String text);

    private static native void nativePartial(String text);

    private static native void nativeStopped();
}
