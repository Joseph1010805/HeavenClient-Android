package org.heavenclient.android;

import android.app.Presentation;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.Display;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;

/**
 * The second screen.
 *
 * SDL's Android backend owns exactly one window, so a second display cannot be
 * asked of it. This puts a plain SurfaceView on the other display instead and
 * hands the bare Surface down to the client, which draws to it with the GL
 * context it already has - the same context, not a shared one, so the sprite
 * atlas and every texture in it are usable from both screens with nothing
 * duplicated.
 *
 * Nothing here is required. A device with one display never constructs this,
 * and the client renders exactly as it did before.
 */
public class SecondScreen extends Presentation
{
    private SurfaceView view;

    public SecondScreen(Context outer, Display display)
    {
        super(outer, display);
    }

    @Override
    public void onBackPressed()
    {
        // A Presentation is a Dialog, and a Dialog closes on Back. On a device
        // with gesture navigation a swipe near the edge IS Back, so swiping
        // between pages was dismissing the whole panel and leaving Android
        // showing through. The panel is not something to be dismissed.
    }

    @Override
    protected void onCreate(Bundle state)
    {
        super.onCreate(state);

        // Same reason - nothing should be able to cancel this out from under
        // the game.
        setCancelable(false);

        view = new SurfaceView(getContext());
        view.getHolder().addCallback(new SurfaceHolder.Callback()
        {
            @Override
            public void surfaceCreated(SurfaceHolder holder)
            {
            }

            @Override
            public void surfaceChanged(SurfaceHolder holder, int format, int width, int height)
            {
                // The size arrives here rather than from the Display, because
                // this is the surface actually being drawn to.
                nativeSurfaceChanged(holder.getSurface(), width, height);
            }

            @Override
            public void surfaceDestroyed(SurfaceHolder holder)
            {
                nativeSurfaceDestroyed();
            }
        });

        setContentView(view, new ViewGroup.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.MATCH_PARENT));

        excludeSystemGestures();
    }

    /**
     * Ask the system to leave this surface's edges alone.
     *
     * Without this, a swipe that starts near the left or right edge is taken
     * as a navigation gesture before the app ever sees it - so a page swipe
     * would sometimes turn a page and sometimes go Back. The system caps how
     * much of an edge an app may claim, so this reduces the problem rather
     * than removing it, and is why Back is refused above as well.
     */
    private void excludeSystemGestures()
    {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q)
            return;

        view.addOnLayoutChangeListener((v, left, top, right, bottom,
                                        oldLeft, oldTop, oldRight, oldBottom) ->
        {
            java.util.List<Rect> rects = new java.util.ArrayList<>();
            rects.add(new Rect(0, 0, right - left, bottom - top));

            v.setSystemGestureExclusionRects(rects);
        });
    }

    /**
     * The contact being followed, by pointer ID rather than by index.
     *
     * getX() with no argument means pointer INDEX 0, and an index is not an
     * identity: it is a slot in whatever set of contacts the screen currently
     * reports. Let a second contact appear for a single frame - a knuckle, a
     * rejected palm sample, a digitizer ghost - and index 0 can become the
     * other one, so the reported position jumps to it and back again. That is
     * the pointer wandering about; nothing needs to be pressed on purpose for
     * it to happen.
     *
     * An ID belongs to one contact for as long as it lasts, so following the
     * ID follows the finger.
     */
    private int activePointer = -1;

    /**
     * The last three raw positions, and the median of them is what gets used.
     *
     * The screen emits the occasional wild sample in the middle of a stroke -
     * measured at 258 pixels in 8 milliseconds, which no finger does. One bad
     * sample, then straight back to where the finger really is. That is the
     * pointer jumping about, and it comes from the digitizer: the log shows it
     * arriving that way from Android, with a single contact reported the whole
     * time.
     *
     * A median throws away outliers completely while passing real movement
     * through untouched - unlike an average, which would drag the pointer part
     * of the way towards every bad sample.
     *
     * Five samples, not three. Three removes a SINGLE bad sample, and measuring
     * the stream showed the bad ones are not always alone: steps of 128, 97, 70
     * and 70 pixels came through in one stroke. Five survives two bad samples
     * in a row. The cost is two samples of lag, about 16ms.
     */
    private static final int WINDOW = 5;

    private final float[] recentX = new float[WINDOW];
    private final float[] recentY = new float[WINDOW];
    private final float[] scratch = new float[WINDOW];
    private int recentCount = 0;

    private void remember(float x, float y)
    {
        for (int i = WINDOW - 1; i > 0; i--)
        {
            recentX[i] = recentX[i - 1];
            recentY[i] = recentY[i - 1];
        }

        recentX[0] = x;
        recentY[0] = y;

        if (recentCount < WINDOW)
            recentCount++;
    }

    private float middle(float[] from)
    {
        int n = recentCount;

        System.arraycopy(from, 0, scratch, 0, n);
        java.util.Arrays.sort(scratch, 0, n);

        return scratch[n / 2];
    }

    private float steadyX()
    {
        return recentCount == 0 ? 0.0f : middle(recentX);
    }

    private float steadyY()
    {
        return recentCount == 0 ? 0.0f : middle(recentY);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
        // Touches on this display are delivered here, not to SDL, so they are
        // forwarded by hand and tagged as belonging to the second screen.
        int action = event.getActionMasked();
        int index = event.getActionIndex();

        // Temporary. The converted coordinates were seen jumping about, and
        // reading the raw ones back from them has been guesswork twice over -
        // once wrongly. So this is what Android actually handed us, before
        // anything here touches it: the event, the raw position, how many
        // contacts there are, and the size of the view those coordinates are
        // measured against. If the raw stream is smooth then the fault is
        // downstream of this line; if it jumps, it is not ours at all.
        android.util.Log.i("HeavenClient",
            "[raw] a=" + action + " id=" + activePointer
            + " n=" + event.getPointerCount()
            + " xy=" + (int) event.getX() + "," + (int) event.getY()
            + " view=" + (view == null ? 0 : view.getWidth())
            + "x" + (view == null ? 0 : view.getHeight())
            + " src=" + event.getSource() + " tool=" + event.getToolType(0)
            + " dev=" + event.getDeviceId());

        switch (action)
        {
        case MotionEvent.ACTION_DOWN:
            activePointer = event.getPointerId(index);

            // A press is taken exactly where it landed. There is no history to
            // judge it against, and delaying a tap to be sure of it would be
            // worse than the occasional bad one.
            recentCount = 0;
            remember(event.getX(index), event.getY(index));

            nativeTouch(event.getX(index), event.getY(index), true, false);
            return true;

        case MotionEvent.ACTION_MOVE:
        {
            // Where OUR contact is now, whatever slot it has ended up in.
            int at = event.findPointerIndex(activePointer);

            if (at >= 0)
            {
                remember(event.getX(at), event.getY(at));

                nativeTouch(steadyX(), steadyY(), false, false);
            }

            return true;
        }

        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_CANCEL:
        {
            int at = event.findPointerIndex(activePointer);

            if (at >= 0)
            {
                // Where the finger steadily WAS, not where the last sample
                // claimed - a bad one on the way up would move the click.
                nativeTouch(steadyX(), steadyY(), false, true);
            }

            activePointer = -1;
            recentCount = 0;
            return true;
        }

        case MotionEvent.ACTION_POINTER_DOWN:
            // Someone else's finger. Not a press, and not ours to follow.
            return true;

        case MotionEvent.ACTION_POINTER_UP:
            // Only if the one that left is the one we were following.
            if (event.getPointerId(index) == activePointer)
            {
                nativeTouch(event.getX(index), event.getY(index), false, true);
                activePointer = -1;
            }

            return true;
        }

        return true;
    }

    private static native void nativeSurfaceChanged(Surface surface, int width, int height);
    private static native void nativeSurfaceDestroyed();
    private static native void nativeTouch(float x, float y, boolean down, boolean up);
}
