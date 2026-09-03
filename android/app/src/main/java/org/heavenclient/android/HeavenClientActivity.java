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

    /**
     * Take the whole screen, including the strip the status bar reserves.
     *
     * <p>Without this the game is not actually fullscreen. SurfaceFlinger was
     * asked directly and said so:
     *
     * <pre>
     *   geomBufferSize = [0 0 1920 1080]
     *   geomLayerTransform (TRANSLATE)  ... 55.0 ...
     *   visibleRegion  [0, 55, 1920, 1080]
     *   source crop    0 0 1025 1920
     * </pre>
     *
     * <p>The client renders a full 1920x1080 frame, and the SurfaceView holding
     * it is pushed 55 pixels DOWN into a slot only 1025 tall - so the top 55
     * rows are the status bar's reserved strip and the bottom 55 rows of the
     * picture fall off the screen. That is the HP bar and the button row going
     * missing, and it is why the "cut off screen" survived a rotation fix, a
     * stale-drawable fix, a letterbox and three different resolutions: none of
     * them was ever the cause.
     *
     * <p>SDL does set these flags, but only when an app asks it for fullscreen
     * through SDL_SetWindowFullscreen - see COMMAND_CHANGE_WINDOW_STYLE in
     * SDLActivity. This client never does, so they were never applied.
     *
     * <p>Reapplied on focus as well as at startup because the bars come back on
     * their own: IMMERSIVE_STICKY only hides them again after a swipe, and
     * anything that takes focus - a permission dialog, the notification shade -
     * hands the layout back the way it found it.
     */
    private void takeWholeScreen()
    {
        getWindow().getDecorView().setSystemUiVisibility(
                android.view.View.SYSTEM_UI_FLAG_FULLSCREEN
                        | android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                        | android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE);

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);

        // Draw into the notch as well, on the machines that have one.
        if (android.os.Build.VERSION.SDK_INT >= 28)
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus)
    {
        super.onWindowFocusChanged(hasFocus);

        if (hasFocus)
            takeWholeScreen();
    }

    @Override
    protected void onCreate(Bundle state)
    {
        super.onCreate(state);

        // Alive only so that swiping the game away reaches onTaskRemoved and
        // the local server can be stopped. It does no work otherwise.
        startService(new android.content.Intent(this, ShutdownWatcher.class));

        takeWholeScreen();

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

        // TAKE THE SERVER DOWN WITH THE GAME.
        //
        // Hosting used to have a start and no stop, so closing the game left
        // Cosmic running in Termux indefinitely: draining a handheld's
        // battery, holding the database open, and - worst - still answering
        // on the network, so somebody else could carry on playing in a world
        // whose host had put their device in a pocket. Nothing on either
        // screen said so.
        //
        // onDestroy, not onPause: tabbing out of the game must not throw
        // everyone off. On a device that is not hosting, the kill matches no
        // process and does nothing.
        //
        // This covers BACKING OUT of the game. It does not cover swiping it
        // off the recents screen, where Android may kill the process without
        // calling this at all - ShutdownWatcher exists for that.
        if (isFinishing())
        {
            LocalServer.stop(this);
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
    // ============================================================
    // QUEST CONTROLS
    // ============================================================
    //
    // A 2D app on Horizon OS is never given the controllers. vrshell reads
    // them through OpenXR and INJECTS synthetic touchscreen events into our
    // window - device ids in Android's virtual range, 0x100000 upward -
    // applying its own policy on the way in:
    //
    //     A / X / either trigger   ->  BUTTON_PRIMARY  (the laser's click)
    //     B / Y                    ->  BUTTON_BACK, plus a KEYCODE_BACK
    //     either stick click       ->  BUTTON_TERTIARY
    //     either grip, Menu        ->  discarded entirely
    //     either thumbstick        ->  SCROLL, on one merged device
    //
    // So A and the trigger cannot be told apart HERE because they are already
    // the same event by the time we see it, and the two sticks likewise. This
    // is not a gap to keep probing at: the real controllers sit on
    // /dev/input/event4 as a complete gamepad with every one of those
    // separated - BTN_GAMEPAD/EAST/NORTH/WEST, both grips, Menu, and an ANALOG
    // trigger reporting 1024 steps - and Android marks that device
    // Enabled:false behind a signature-level permission. The setting named for
    // exactly this, secure/controller_emulation_mode, was tried at 1 and 2
    // across reboots: it adds enabled-but-silent sibling nodes and leaves the
    // loaded one switched off. docs_QUEST.md carries the whole measurement.
    //
    // That leaves six usable signals, and BUTTON_PRIMARY has to stay the mouse
    // - it is how every window, NPC and inventory in the game is worked. The
    // four that are free carry the gameplay:
    //
    //     B  (right thumb)      attack
    //     right stick click     pick up
    //     Y  (left thumb)       jump
    //     left stick click      sit
    //
    // Sent as the LETTERS A/S/D/G rather than as actions, because Keytable
    // indices 30/31/32/34 are exactly those letters, and those four are the
    // keymap defaults KeymapHandler fills in. Going through the game's own key
    // mapping means a player who rebinds attack in the hotkey window gets the
    // Quest button following it, without this file knowing anything about it.
    //
    // Set PROBE to true to get the raw event dump back. Note the lesson in it:
    // the first version of this probe filtered out SOURCE_MOUSE, which is
    // precisely where a controller button arrives, and so reported the absence
    // it had created. Log everything and filter when READING.
    private static final boolean PROBE = false;

    // Every axis worth asking about, with its name. Iterated rather than
    // hand-picked, because the useful one (HSCROLL) was not one anybody would
    // have guessed at.
    private static final int[] PROBE_AXES = {
        android.view.MotionEvent.AXIS_X,
        android.view.MotionEvent.AXIS_Y,
        android.view.MotionEvent.AXIS_Z,
        android.view.MotionEvent.AXIS_RX,
        android.view.MotionEvent.AXIS_RY,
        android.view.MotionEvent.AXIS_RZ,
        android.view.MotionEvent.AXIS_HAT_X,
        android.view.MotionEvent.AXIS_HAT_Y,
        android.view.MotionEvent.AXIS_LTRIGGER,
        android.view.MotionEvent.AXIS_RTRIGGER,
        android.view.MotionEvent.AXIS_THROTTLE,
        android.view.MotionEvent.AXIS_BRAKE,
        android.view.MotionEvent.AXIS_GAS,
        android.view.MotionEvent.AXIS_RUDDER,
        android.view.MotionEvent.AXIS_WHEEL,
        android.view.MotionEvent.AXIS_VSCROLL,
        android.view.MotionEvent.AXIS_HSCROLL,
        android.view.MotionEvent.AXIS_PRESSURE,
        android.view.MotionEvent.AXIS_SIZE,
        android.view.MotionEvent.AXIS_TOUCH_MAJOR,
        android.view.MotionEvent.AXIS_ORIENTATION,
        android.view.MotionEvent.AXIS_DISTANCE,
        android.view.MotionEvent.AXIS_TILT,
        android.view.MotionEvent.AXIS_GENERIC_1,
        android.view.MotionEvent.AXIS_GENERIC_2,
        android.view.MotionEvent.AXIS_GENERIC_3,
        android.view.MotionEvent.AXIS_GENERIC_4,
    };

    private static String axisDump(android.view.MotionEvent e)
    {
        StringBuilder out = new StringBuilder();

        for (int axis : PROBE_AXES)
        {
            float v = e.getAxisValue(axis);

            if (v == 0.0f)
                continue;

            out.append(' ')
               .append(android.view.MotionEvent.axisToString(axis))
               .append('=')
               .append(v);
        }

        return out.toString();
    }

    private static String sourceName(int src)
    {
        StringBuilder out = new StringBuilder();

        if ((src & android.view.InputDevice.SOURCE_KEYBOARD) == android.view.InputDevice.SOURCE_KEYBOARD) out.append("KEYBOARD|");
        if ((src & android.view.InputDevice.SOURCE_DPAD) == android.view.InputDevice.SOURCE_DPAD) out.append("DPAD|");
        if ((src & android.view.InputDevice.SOURCE_GAMEPAD) == android.view.InputDevice.SOURCE_GAMEPAD) out.append("GAMEPAD|");
        if ((src & android.view.InputDevice.SOURCE_TOUCHSCREEN) == android.view.InputDevice.SOURCE_TOUCHSCREEN) out.append("TOUCHSCREEN|");
        if ((src & android.view.InputDevice.SOURCE_MOUSE) == android.view.InputDevice.SOURCE_MOUSE) out.append("MOUSE|");
        if ((src & android.view.InputDevice.SOURCE_STYLUS) == android.view.InputDevice.SOURCE_STYLUS) out.append("STYLUS|");
        if ((src & android.view.InputDevice.SOURCE_TRACKBALL) == android.view.InputDevice.SOURCE_TRACKBALL) out.append("TRACKBALL|");
        if ((src & android.view.InputDevice.SOURCE_JOYSTICK) == android.view.InputDevice.SOURCE_JOYSTICK) out.append("JOYSTICK|");
        if ((src & android.view.InputDevice.SOURCE_TOUCHPAD) == android.view.InputDevice.SOURCE_TOUCHPAD) out.append("TOUCHPAD|");

        out.append("0x").append(Integer.toHexString(src));

        return out.toString();
    }

    private static String buttonName(int b)
    {
        if (b == 0)
            return "none";

        StringBuilder out = new StringBuilder();

        if ((b & android.view.MotionEvent.BUTTON_PRIMARY) != 0) out.append("PRIMARY|");
        if ((b & android.view.MotionEvent.BUTTON_SECONDARY) != 0) out.append("SECONDARY|");
        if ((b & android.view.MotionEvent.BUTTON_TERTIARY) != 0) out.append("TERTIARY|");
        if ((b & android.view.MotionEvent.BUTTON_BACK) != 0) out.append("BACK|");
        if ((b & android.view.MotionEvent.BUTTON_FORWARD) != 0) out.append("FORWARD|");

        out.append("0x").append(Integer.toHexString(b));

        return out.toString();
    }

    private void probeMotion(String where, android.view.MotionEvent e)
    {
        if (!PROBE)
            return;

        int action = e.getActionMasked();

        boolean interesting =
                action != android.view.MotionEvent.ACTION_HOVER_MOVE
                && action != android.view.MotionEvent.ACTION_MOVE;

        if (!interesting && e.getButtonState() == 0
                && e.getAxisValue(android.view.MotionEvent.AXIS_VSCROLL) == 0.0f
                && e.getAxisValue(android.view.MotionEvent.AXIS_HSCROLL) == 0.0f)
            return;

        Log.i(TAG, where
                + " action=" + android.view.MotionEvent.actionToString(action)
                + " buttons=" + buttonName(e.getButtonState())
                + " src=" + sourceName(e.getSource())
                + " dev=" + e.getDeviceId()
                + " tool=" + e.getToolType(0)
                + " pointers=" + e.getPointerCount()
                + axisDump(e));
    }

    // The letters, not the actions - see the note at the top of this section.
    private static final int KEY_ATTACK = android.view.KeyEvent.KEYCODE_A;
    private static final int KEY_PICKUP = android.view.KeyEvent.KEYCODE_S;
    private static final int KEY_JUMP   = android.view.KeyEvent.KEYCODE_D;
    private static final int KEY_SIT    = android.view.KeyEvent.KEYCODE_G;

    // Anything at or above this is injected rather than physical hardware,
    // which is also what keeps this whole section inert on the Thor and the
    // RP5 - their real touchscreens get small device ids and never come near
    // it. Nothing here needs an "am I a Quest" test.
    private static final int VIRTUAL_DEVICE_BASE = 0x100000;

    private int handLeft = -1;
    private int handRight = -1;
    private boolean handsLocked = false;
    private boolean swapHands = false;
    private boolean questSeen = false;

    private final android.util.SparseIntArray lastButtons = new android.util.SparseIntArray();
    private final android.util.SparseLongArray lastMotionAt = new android.util.SparseLongArray();
    private final java.util.HashSet<Integer> heldKeys = new java.util.HashSet<Integer>();

    /**
     * Works out which injected device is which hand.
     *
     * Nothing in the event says left or right, so it is taken from the device
     * id: the controllers arrive as a stable pair and the LOWER id is the left
     * hand. That is measured, not assumed - A and B came from 1048578, X and Y
     * from 1048577.
     *
     * Locked as soon as two distinct ids have been seen, because a third has
     * been observed appearing briefly and must not be allowed to shift the
     * mapping halfway through a fight. Until the second one turns up the only
     * known device is treated as the right hand, so attack and pick up work
     * from the first press rather than waiting.
     *
     * If it ever comes out backwards, creating a file called `swap_hands`
     * beside the game data flips it without a rebuild:
     *
     *     adb shell touch /sdcard/Android/data/org.heavenclient.android/files/swap_hands
     */
    private void noteController(int deviceId)
    {
        questSeen = true;

        if (handsLocked)
            return;

        if (handLeft == -1)
        {
            handLeft = deviceId;
            handRight = deviceId;
            return;
        }

        if (deviceId == handLeft || deviceId == handRight)
            return;

        int lo = Math.min(handLeft, deviceId);
        int hi = Math.max(handLeft, deviceId);

        java.io.File dir = getExternalFilesDir(null);
        swapHands = dir != null && new java.io.File(dir, "swap_hands").exists();

        handLeft = swapHands ? hi : lo;
        handRight = swapHands ? lo : hi;
        handsLocked = true;

        Log.i(TAG, "quest hands: left=" + handLeft + " right=" + handRight
                + (swapHands ? "  (swapped by request)" : ""));
    }

    /** Presses or releases a key, and never sends the same edge twice. */
    private void hold(int keycode, boolean down)
    {
        if (down)
        {
            if (!heldKeys.add(keycode))
                return;

            SDLActivity.onNativeKeyDown(keycode);

            stuckWatch.removeCallbacks(releaseStuck);
            stuckWatch.postDelayed(releaseStuck, STUCK_POLL_MS);
        }
        else if (heldKeys.remove(keycode))
        {
            SDLActivity.onNativeKeyUp(keycode);
        }
    }

    /**
     * A button held on a controller that has stopped reporting.
     *
     * While a controller is tracked it produces hover events at a steady ~11ms
     * whether or not it is being moved, so the ordinary release edge always
     * arrives and this never fires in play. What it covers is the controller
     * being put down, losing tracking or running flat while a button is held -
     * where there is no further event to lift the key, and the character would
     * attack forever. 300ms of total silence cannot be reached by a hand.
     */
    private static final long CONTROLLER_SILENT_MS = 300;
    private static final long STUCK_POLL_MS = 250;

    private final android.os.Handler stuckWatch =
            new android.os.Handler(android.os.Looper.getMainLooper());

    private final Runnable releaseStuck = new Runnable()
    {
        @Override
        public void run()
        {
            long now = android.os.SystemClock.uptimeMillis();

            for (int i = 0; i < lastButtons.size(); i++)
            {
                int dev = lastButtons.keyAt(i);

                if (lastButtons.valueAt(i) == 0)
                    continue;

                if (now - lastMotionAt.get(dev, 0L) < CONTROLLER_SILENT_MS)
                    continue;

                Log.i(TAG, "quest: controller " + dev
                        + " went silent still holding a button - releasing");

                lastButtons.put(dev, 0);

                boolean right = (dev == handRight);

                hold(right ? KEY_ATTACK : KEY_JUMP, false);
                hold(right ? KEY_PICKUP : KEY_SIT, false);
            }

            if (!heldKeys.isEmpty())
                stuckWatch.postDelayed(this, STUCK_POLL_MS);
        }
    };

    /**
     * The four gameplay buttons, read as button-state edges per controller.
     *
     * BUTTON_PRIMARY is deliberately untouched - that is the laser's click,
     * and taking it would cost the pointer that operates every window in the
     * game.
     */
    private void questButtons(android.view.MotionEvent e)
    {
        int dev = e.getDeviceId();

        if (dev < VIRTUAL_DEVICE_BASE)
            return;

        noteController(dev);

        lastMotionAt.put(dev, android.os.SystemClock.uptimeMillis());

        int now = e.getButtonState();
        int was = lastButtons.get(dev, 0);

        if (now == was)
            return;

        lastButtons.put(dev, now);

        int changed = now ^ was;
        boolean right = (dev == handRight);

        if ((changed & android.view.MotionEvent.BUTTON_BACK) != 0)
            hold(right ? KEY_ATTACK : KEY_JUMP,
                    (now & android.view.MotionEvent.BUTTON_BACK) != 0);

        if ((changed & android.view.MotionEvent.BUTTON_TERTIARY) != 0)
            hold(right ? KEY_PICKUP : KEY_SIT,
                    (now & android.view.MotionEvent.BUTTON_TERTIARY) != 0);
    }

    @Override
    public boolean dispatchKeyEvent(android.view.KeyEvent event)
    {
        if (PROBE)
        {
            Log.i(TAG, "KEY " + android.view.KeyEvent.keyCodeToString(event.getKeyCode())
                    + " (" + event.getKeyCode() + ")"
                    + (event.getAction() == android.view.KeyEvent.ACTION_DOWN ? " DOWN" : " UP")
                    + " src=" + sourceName(event.getSource())
                    + " dev=" + event.getDeviceId()
                    + " scan=" + event.getScanCode()
                    + " repeat=" + event.getRepeatCount()
                    + " meta=0x" + Integer.toHexString(event.getMetaState()));
        }

        // B and Y arrive twice - as the BUTTON_BACK bit, and as a plain
        // KEYCODE_BACK carrying no hint of which hand sent it. The bit is what
        // we act on, so the key is a duplicate; left alone it would also reach
        // the client as an escape and shut a window every time the player
        // attacked. Only swallowed once an injected controller has been seen,
        // so the Thor and the RP5 keep their real back button.
        if (questSeen && event.getKeyCode() == android.view.KeyEvent.KEYCODE_BACK)
            return true;

        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean dispatchTouchEvent(android.view.MotionEvent event)
    {
        probeMotion("TOUCH", event);
        questButtons(event);

        return super.dispatchTouchEvent(event);
    }

    // THE THUMBSTICK, ARRIVING AS SCROLL.
    //
    // Meta had to pick some flat-screen behaviour for the stick, and what they
    // chose is scroll - an ordinary Android motion axis that reaches this
    // method perfectly well, roughly -0.27..+0.27 on each of VSCROLL and
    // HSCROLL.
    //
    // Both sticks scroll and BOTH ARRIVE ON dev=-1, a single merged virtual
    // device, so they cannot be told apart. Only their clicks can. That is why
    // walking is simply "a stick", not "the left stick".
    //
    // Converted to KEYCODE_DPAD_* because that route was proven first -
    // `adb shell input keyevent 21` walks the character.
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
        // Two thresholds, so a stick held near the edge does not chatter.
        stickEdge(0, stickDown[0] ? h < -RELEASE : h < -PRESS,
                android.view.KeyEvent.KEYCODE_DPAD_LEFT);
        stickEdge(1, stickDown[1] ? h > RELEASE : h > PRESS,
                android.view.KeyEvent.KEYCODE_DPAD_RIGHT);
        stickEdge(2, stickDown[2] ? v > RELEASE : v > PRESS,
                android.view.KeyEvent.KEYCODE_DPAD_UP);
        stickEdge(3, stickDown[3] ? v < -RELEASE : v < -PRESS,
                android.view.KeyEvent.KEYCODE_DPAD_DOWN);
    }

    // LETTING GO SENDS NOTHING AT ALL.
    //
    // Scroll is a stream of deltas, not a position: an axis reports its way
    // back to zero, scroll simply STOPS. Nothing says "no longer scrolling",
    // so the key went down with no event left to lift it and the character
    // kept walking. 120ms is far longer than the ~11ms between events while
    // the stick is held, so it cannot fire mid-movement.
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
        probeMotion("MOTION", event);
        questButtons(event);

        float h = event.getAxisValue(android.view.MotionEvent.AXIS_HSCROLL);
        float v = event.getAxisValue(android.view.MotionEvent.AXIS_VSCROLL);

        if (h != 0.0f || v != 0.0f)
        {
            stickAxis(h, v);

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
