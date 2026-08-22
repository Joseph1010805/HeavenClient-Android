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

    @Override
    public boolean onTouchEvent(MotionEvent event)
    {
        // Touches on this display are delivered here, not to SDL, so they are
        // forwarded by hand and tagged as belonging to the second screen.
        int action = event.getActionMasked();
        int index = event.getActionIndex();

        if (event.getPointerCount() > 1)
            android.util.Log.i("HeavenClient",
                "[cursor] " + event.getPointerCount() + " contacts, following id "
                + activePointer);

        switch (action)
        {
        case MotionEvent.ACTION_DOWN:
            activePointer = event.getPointerId(index);
            nativeTouch(event.getX(index), event.getY(index), true, false);
            return true;

        case MotionEvent.ACTION_MOVE:
        {
            // Where OUR contact is now, whatever slot it has ended up in.
            int at = event.findPointerIndex(activePointer);

            if (at >= 0)
                nativeTouch(event.getX(at), event.getY(at), false, false);

            return true;
        }

        case MotionEvent.ACTION_UP:
        case MotionEvent.ACTION_CANCEL:
        {
            int at = event.findPointerIndex(activePointer);

            if (at >= 0)
                nativeTouch(event.getX(at), event.getY(at), false, true);

            activePointer = -1;
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
