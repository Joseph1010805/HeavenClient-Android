package org.heavenclient.android;

import android.hardware.display.DisplayManager;
import android.os.Bundle;
import android.util.Log;
import android.view.Display;

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
