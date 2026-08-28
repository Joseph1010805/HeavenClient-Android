package org.heavenclient.android;

import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

/**
 * Entry activity.
 *
 * SDLActivity does all the real work - surface, input, lifecycle. The only
 * thing it needs from us is the list of native libraries to load, and the
 * order matters: dependencies first, the client last.
 *
 * On a handheld with a second display - the AYN Thor has one below the main
 * panel - a Presentation is put on it as well. That is strictly additive:
 * where there is no second display, none of this runs.
 */
public class HeavenClientActivity extends SDLActivity
{
    private static final String TAG = "HeavenClient";

    private SecondScreen secondscreen;

    @Override
    protected String[] getLibraries()
    {
        return new String[] {
            "SDL2",
            "openal",
            "HeavenClient"
        };
    }

    @Override
    protected void onCreate(Bundle state)
    {
        super.onCreate(state);

        // Do not let the screen sleep while the game is up.
        //
        // Standing still in town is a normal way to spend time, and it used to
        // end the session: the screen times out, SDL halts the main loop with
        // the surface, the client stops answering the server's pings, and
        // Cosmic hangs up on its own after fifteen seconds of no pong. Nothing
        // in the game reported anything - the player came back to a dead
        // connection and no reason for it.
        //
        // A handheld running a game is expected to stay lit; this is the same
        // flag every other game on the device uses, and it is dropped
        // automatically when the activity goes away.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);

        listInputDevices();

        Display display = findSecondDisplay();

        if (display == null)
        {
            Log.i(TAG, "no second display");
            return;
        }

        Log.i(TAG, "second display: " + display.getName() + " id " + display.getDisplayId());

        secondscreen = new SecondScreen(this, display);

        // If it does go away for any reason - the display sleeping, the system
        // taking it - put it back rather than leaving Android showing where
        // the game's lower half should be.
        secondscreen.setOnDismissListener(dialog ->
        {
            if (secondscreen != null)
                secondscreen.show();
        });

        secondscreen.show();
    }

    @Override
    protected void onDestroy()
    {
        if (secondscreen != null)
        {
            secondscreen.dismiss();
            secondscreen = null;
        }

        super.onDestroy();
    }

    /**
     * The first display that is not the one the game is already on.
     *
     * PRESENTATION is the category Android uses for a display an app may put a
     * second window on, which is exactly what the Thor's lower panel reports
     * itself as.
     */
    // DOES ANDROID GIVE US ANYTHING FROM THE QUEST CONTROLLERS?
    //
    // SDL enumerates only two devices on this headset - a controller entry
    // marked Enabled: false, and the accelerometer. The live one, which
    // dumpsys shows as INPUT_DEVICE_CLASS_VR_PERIPHERAL and which only exists
    // while the controllers are held, is never offered to it.
    //
    // But SDL is not the only way in. Every key and motion event the system
    // routes to this app passes through the Activity first, whatever SDL
    // makes of the device afterwards - so if anything at all arrives from
    // those controllers, it arrives HERE, and can be forwarded.
    //
    // Logging only, for now. There is no point writing the forwarding until
    // it is known whether there is anything to forward.
    @Override
    public boolean dispatchKeyEvent(android.view.KeyEvent event)
    {
        // Left in, quiet, because what the controller buttons produce is
        // still an open question - they arrive as clicks rather than as
        // distinct keys, so telling A from B needs more than this.
        if (event.getSource() != android.view.InputDevice.SOURCE_MOUSE)
            Log.i(TAG, "KEY " + event.getKeyCode()
                    + " action=" + event.getAction()
                    + " src=0x" + Integer.toHexString(event.getSource())
                    + " dev=" + event.getDeviceId());

        return super.dispatchKeyEvent(event);
    }

    // THE THUMBSTICK, ARRIVING AS SCROLL.
    //
    // The Quest never gives a 2D app the controller as a joystick - the sticks
    // belong to the VR runtime and are read through OpenXR, not through
    // Android input. Every layer says no: SDL enumerates only a disabled
    // duplicate, and the Activity sees the controllers purely as touchscreen
    // pointers.
    //
    // But Meta had to choose SOME flat-screen behaviour for the stick, and
    // what they chose is SCROLL. That is an ordinary Android motion axis, and
    // it does reach us:
    //
    //     MOTION src=0x2 dev=-1 vscroll=0.173 hscroll=0.139
    //
    // Roughly -0.27 to +0.27 on both axes. So the stick is readable after all,
    // just not as the thing it obviously is.
    //
    // Turned into d-pad keys because that route is already proven end to end:
    // `input keyevent 21` produced "key 263 -> game" and walked the character.
    // Nothing downstream needs to know where these came from.
    private static final float PRESS = 0.06f;
    private static final float RELEASE = 0.03f;

    private final boolean[] stickDown = new boolean[4];   // L, R, U, D

    private void stickEdge(int slot, boolean want, int keycode)
    {
        if (want == stickDown[slot])
            return;

        stickDown[slot] = want;

        if (want)
            SDLActivity.onNativeKeyDown(keycode);
        else
            SDLActivity.onNativeKeyUp(keycode);
    }

    private void stickAxis(float h, float v)
    {
        // Two thresholds, so a stick held near the edge does not chatter -
        // releasing and re-pressing many times a second reads as the character
        // stuttering on the spot.
        stickEdge(0, stickDown[0] ? h < -RELEASE : h < -PRESS,
                android.view.KeyEvent.KEYCODE_DPAD_LEFT);
        stickEdge(1, stickDown[1] ? h > RELEASE : h > PRESS,
                android.view.KeyEvent.KEYCODE_DPAD_RIGHT);

        // Scroll up is positive; on a stick, up is up.
        stickEdge(2, stickDown[2] ? v > RELEASE : v > PRESS,
                android.view.KeyEvent.KEYCODE_DPAD_UP);
        stickEdge(3, stickDown[3] ? v < -RELEASE : v < -PRESS,
                android.view.KeyEvent.KEYCODE_DPAD_DOWN);
    }

    // LETTING GO SENDS NOTHING AT ALL.
    //
    // A joystick axis reports its way back to zero, so a release is just
    // another event. Scroll does not: it is a stream of deltas, and when the
    // stick returns to centre the events simply STOP. Nothing ever says "no
    // longer scrolling".
    //
    // So the character kept walking after the stick was released - the key had
    // gone down and there was no event left to take it up again.
    //
    // A short idle timeout stands in for the release. 120ms is comfortably
    // longer than the ~11ms gap between events while the stick is held, so it
    // cannot fire mid-movement, and short enough that stopping feels immediate.
    private static final long STICK_IDLE_MS = 120;

    private final android.os.Handler stickIdle =
            new android.os.Handler(android.os.Looper.getMainLooper());

    private final Runnable releaseStick = new Runnable()
    {
        @Override
        public void run()
        {
            stickAxis(0.0f, 0.0f);
        }
    };

    @Override
    public boolean dispatchGenericMotionEvent(android.view.MotionEvent event)
    {
        float h = event.getAxisValue(android.view.MotionEvent.AXIS_HSCROLL);
        float v = event.getAxisValue(android.view.MotionEvent.AXIS_VSCROLL);

        if (h != 0.0f || v != 0.0f)
        {
            stickAxis(h, v);

            // Restart the clock. While the stick is held this is pushed back
            // on every event and never fires.
            stickIdle.removeCallbacks(releaseStick);
            stickIdle.postDelayed(releaseStick, STICK_IDLE_MS);
        }

        return super.dispatchGenericMotionEvent(event);
    }

    // What the system says it has, asked the way an app asks rather than the
    // way dumpsys does - these are the devices SDL is choosing from.
    private void listInputDevices()
    {
        for (int id : android.view.InputDevice.getDeviceIds())
        {
            android.view.InputDevice d = android.view.InputDevice.getDevice(id);

            if (d == null)
                continue;

            Log.i(TAG, "input device " + id + ": '" + d.getName()
                    + "' sources=0x" + Integer.toHexString(d.getSources())
                    + " joystick=" + ((d.getSources() & android.view.InputDevice.SOURCE_JOYSTICK)
                            == android.view.InputDevice.SOURCE_JOYSTICK)
                    + " gamepad=" + ((d.getSources() & android.view.InputDevice.SOURCE_GAMEPAD)
                            == android.view.InputDevice.SOURCE_GAMEPAD)
                    + " virtual=" + d.isVirtual());
        }
    }

    private Display findSecondDisplay()
    {
        DisplayManager manager = (DisplayManager) getSystemService(DISPLAY_SERVICE);

        if (manager == null)
            return null;

        Display[] displays = manager.getDisplays(DisplayManager.DISPLAY_CATEGORY_PRESENTATION);

        for (Display display : displays)
            if (display.getDisplayId() != Display.DEFAULT_DISPLAY)
                return display;

        return null;
    }
}
