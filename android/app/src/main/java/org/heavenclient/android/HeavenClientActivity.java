package org.heavenclient.android;

import org.libsdl.app.SDLActivity;

/**
 * Entry activity.
 *
 * SDLActivity does all the real work - surface, input, lifecycle. The only
 * thing it needs from us is the list of native libraries to load, and the
 * order matters: dependencies first, the client last.
 */
public class HeavenClientActivity extends SDLActivity
{
    @Override
    protected String[] getLibraries()
    {
        return new String[] {
            "SDL2",
            "openal",
            "HeavenClient"
        };
    }
}
