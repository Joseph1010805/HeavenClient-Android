package org.heavenclient.android;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

/**
 * Starts the game server on this device, so it can be played with nothing to
 * connect to.
 *
 * The server is Cosmic - a Java program with its own database - which cannot
 * live inside this app: it needs a real JVM and a MariaDB daemon, neither of
 * which Android gives an ordinary app. It runs in Termux instead, and this is
 * how we ask Termux to start it.
 *
 * WHY IT IS ASKED RATHER THAN LAUNCHED: Termux only accepts commands from
 * another app when `allow-external-apps=true` is set in
 * ~/.termux/termux.properties, which lives in Termux's own private storage.
 * Nothing outside Termux can write that file - not this app, not adb. So the
 * one-off setup script writes it, and until it has been run this whole class
 * politely does nothing. That is deliberate: an app that could silently drive
 * a terminal on your phone would be a bad thing to have.
 */
public final class LocalServer {
    private static final String TAG = "HeavenClient";

    private static final String TERMUX = "com.termux";
    private static final String RUN_SERVICE = "com.termux.app.RunCommandService";
    private static final String RUN_ACTION = "com.termux.RUN_COMMAND";

    private static final String BIN = "/data/data/com.termux/files/usr/bin/bash";
    private static final String SCRIPT = "/data/data/com.termux/files/home/cosmic/run.sh";

    private LocalServer() {
    }

    /** Whether Termux is even installed. */
    public static boolean isAvailable(Context context) {
        if (context == null) {
            return false;
        }

        try {
            context.getPackageManager().getPackageInfo(TERMUX, 0);
            return true;
        } catch (PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    /**
     * Asks Termux to run the server, in the background, with a notification of
     * its own so it is not killed the moment the game takes the foreground.
     *
     * Returns false if Termux is missing or refused. It cannot report whether
     * the server actually came up - that takes a minute and shows itself when
     * the client connects.
     */
    public static boolean start(Context context) {
        if (!isAvailable(context)) {
            Log.i(TAG, "local server: Termux is not installed");
            return false;
        }

        Intent intent = new Intent(RUN_ACTION);
        intent.setComponent(new ComponentName(TERMUX, RUN_SERVICE));
        intent.putExtra("com.termux.RUN_COMMAND_PATH", BIN);
        intent.putExtra("com.termux.RUN_COMMAND_ARGUMENTS", new String[] { SCRIPT });
        intent.putExtra("com.termux.RUN_COMMAND_WORKDIR",
                "/data/data/com.termux/files/home/cosmic");

        // Background, but with its own notification. A foreground service is
        // what keeps Android from killing it while the game is on screen -
        // which is the whole point, since the game is the thing connecting.
        intent.putExtra("com.termux.RUN_COMMAND_BACKGROUND", true);
        intent.putExtra("com.termux.RUN_COMMAND_SESSION_ACTION", "0");

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent);
            } else {
                context.startService(intent);
            }

            Log.i(TAG, "local server: asked Termux to start it");
            return true;
        } catch (Exception e) {
            // The usual cause is `allow-external-apps` not being set, and
            // Termux refuses without saying much.
            Log.e(TAG, "local server: Termux refused - " + e);
            return false;
        }
    }
}
