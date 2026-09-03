package org.heavenclient.android;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.Build;
import android.util.Log;

import java.io.IOException;
import java.net.InetSocketAddress;
import java.net.Socket;

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

    /**
     * Termux protects RUN_COMMAND at `dangerous` level, so declaring it in the
     * manifest is NOT enough - Android will not grant it until it is asked for
     * at runtime and the person says yes. Declared-but-not-granted looks
     * exactly like working right up to the moment the service refuses.
     */
    private static final String RUN_PERMISSION = "com.termux.permission.RUN_COMMAND";
    private static final int RUN_PERMISSION_REQUEST = 4711;

    private static final String BIN = "/data/data/com.termux/files/usr/bin/bash";

    /**
     * WHAT HOST ACTUALLY RUNS.
     *
     * <p>Not {@code run.sh} directly. {@code bootstrap.sh} installs whatever
     * tools/stage_server.sh has left in /sdcard/Download/cosmic and then execs
     * run.sh, so pressing HOST always starts the newest build that has been
     * sent to the device.
     *
     * <p>This is not a convenience. Staged builds were silently never
     * installed for days - the push said "ok", the file was on the device, the
     * server restarted cleanly, and it restarted the jar from the original
     * setup every time. Making the start path do the install is the only
     * arrangement where that cannot happen again.
     *
     * <p>It lives on /sdcard rather than in Termux's home for the same reason:
     * a script in Termux's home can only be updated from inside Termux, which
     * is exactly the manual step this removes. Termux can read /sdcard once
     * termux-setup-storage has been run, which the setup does.
     */
    private static final String SCRIPT = "/sdcard/Download/cosmic/bootstrap.sh";

    /** Where the old, non-updating start script lives, as a fallback. */
    private static final String LEGACY_SCRIPT =
            "/data/data/com.termux/files/home/cosmic/run.sh";

    /**
     * Whether the server is answering, cached.
     *
     * The obvious test - does the setup script's marker file exist - does not
     * work. The app cannot look inside Termux's storage, and on Android 13 it
     * cannot read /sdcard/Download either: READ_EXTERNAL_STORAGE is no longer
     * granted and a plain file is not "media". The app can only read its own
     * directory, and the setup script cannot write there. There is nowhere
     * both sides can meet.
     *
     * So ask the SERVER instead. A connection to the login port needs no
     * permission of any kind, and answers a better question than the marker
     * did: not "was this installed once" but "is it working right now".
     */
    private static final int LOGIN_PORT = 8484;

    private static boolean serverUp = false;

    /** Whether THIS app asked for the server that is running. */
    private static boolean startedHere = false;
    private static long lastProbe = 0;
    private static Thread probing = null;

    /** What is needed before this device can host, as a set of flags. */
    public static final int HAS_TERMUX     = 1;
    public static final int HAS_PERMISSION = 2;
    /** The server is answering on this device right now. */
    public static final int HAS_SERVER     = 4;
    public static final int HAS_WIFI_DIRECT = 8;

    private LocalServer() {
    }

    /**
     * Everything that has to be true before HOST will work, in one call, so
     * the screen can show a list of ticks and crosses instead of failing
     * later with one vague message.
     */
    public static int readiness(Context context) {
        int flags = 0;

        if (isAvailable(context)) {
            flags |= HAS_TERMUX;
        }

        if (hasPermission(context)) {
            flags |= HAS_PERMISSION;
        }

        probeServer();

        if (serverUp) {
            flags |= HAS_SERVER;
        }

        if (WifiDirect.isSupported(context)) {
            flags |= HAS_WIFI_DIRECT;
        }

        return flags;
    }

    /**
     * STOP THE SERVER THIS DEVICE STARTED.
     *
     * <p>Hosting had a start and no stop. Closing the game left Cosmic
     * running in Termux for ever - burning battery on a handheld, holding the
     * database open, and still answering on the network, so another device
     * could sit playing on a world whose host had walked away. The person
     * hosting has no way to tell: the game is gone from their screen and
     * nothing says the server is still up.
     *
     * <p>Only ever kills OUR server. {@code pkill -f Cosmic.jar} matches the
     * jar this project starts and nothing else Termux might be running.
     *
     * <p>This is deliberately not called when the game merely goes to the
     * background. Tabbing out to look something up must not throw everybody
     * off; only actually closing the game does.
     *
     * @return true if Termux was asked. It cannot know whether the server
     *         had already stopped, and does not pretend to.
     */
    public static boolean stop(Context context) {
        if (!isAvailable(context) || !hasPermission(context)) {
            return false;
        }

        // NO GUARD. This used to refuse unless `startedHere` or `serverUp`,
        // and that is exactly why closing the game on the host left the
        // server running and another handheld still playing on it:
        //
        //   * `startedHere` is false whenever the server was started by an
        //     EARLIER run of the app - which is the normal case, because
        //     reinstalling the game does not restart the server.
        //   * `serverUp` is only refreshed by readiness(), which the login
        //     screen calls. Once somebody is in the world nothing probes
        //     again, so by the time the game closes the flag is stale.
        //
        // Both false, so stop() returned without doing anything, silently.
        // The guard was protecting against a cost that does not exist:
        // `pkill -f Cosmic.jar` on a device that is not hosting matches
        // nothing and does nothing. Attempting it always is both simpler and
        // the only version that cannot leave a server running behind you.

        Intent intent = new Intent(RUN_ACTION);
        intent.setComponent(new ComponentName(TERMUX, RUN_SERVICE));
        intent.putExtra("com.termux.RUN_COMMAND_PATH", BIN);
        intent.putExtra("com.termux.RUN_COMMAND_ARGUMENTS",
                new String[] { "-c", "pkill -f Cosmic.jar" });
        intent.putExtra("com.termux.RUN_COMMAND_BACKGROUND", true);
        intent.putExtra("com.termux.RUN_COMMAND_SESSION_ACTION", "0");

        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                context.startForegroundService(intent);
            } else {
                context.startService(intent);
            }

            Log.i(TAG, "local server: asked Termux to stop it");
            startedHere = false;
            serverUp = false;
            lastProbe = 0;

            return true;
        } catch (Exception e) {
            Log.e(TAG, "local server: could not ask Termux to stop - " + e);
            return false;
        }
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
     * Knock on the login port, off the calling thread.
     *
     * A blocking connect belongs nowhere near the frame loop - even a failed
     * one against loopback is fast, but "fast" is not "bounded". The answer is
     * cached and refreshed at most every two seconds, which is far quicker
     * than anybody can read a line of text changing.
     */
    private static void probeServer() {
        long now = System.currentTimeMillis();

        if (now - lastProbe < 2000 || (probing != null && probing.isAlive())) {
            return;
        }

        lastProbe = now;

        probing = new Thread(() -> {
            try (Socket socket = new Socket()) {
                socket.connect(new InetSocketAddress("127.0.0.1", LOGIN_PORT), 400);
                serverUp = true;
            } catch (IOException e) {
                serverUp = false;
            }
        });

        probing.setDaemon(true);
        probing.start();
    }

    /** Whether this app may drive Termux yet. */
    public static boolean hasPermission(Context context) {
        if (context == null) {
            return false;
        }

        return context.checkSelfPermission(RUN_PERMISSION)
                == PackageManager.PERMISSION_GRANTED;
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

        if (!hasPermission(context)) {
            // Ask, and give up for now. The person has to answer a dialog, and
            // the answer arrives long after this call has returned - so the
            // honest thing is to report failure and let them tap again.
            if (context instanceof Activity) {
                Log.i(TAG, "local server: asking for permission to drive Termux");

                ((Activity) context).requestPermissions(
                        new String[] { RUN_PERMISSION }, RUN_PERMISSION_REQUEST);
            }

            return false;
        }

        Intent intent = new Intent(RUN_ACTION);
        intent.setComponent(new ComponentName(TERMUX, RUN_SERVICE));
        intent.putExtra("com.termux.RUN_COMMAND_PATH", BIN);
        // The bootstrap if it has been staged, the old start script if not.
        // A device that has never had stage_server.sh run against it since
        // this change should still be able to start its server.
        String script = new java.io.File(SCRIPT).exists() ? SCRIPT : LEGACY_SCRIPT;

        Log.i(TAG, "starting the server with " + script);

        intent.putExtra("com.termux.RUN_COMMAND_ARGUMENTS", new String[] { script });
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
            startedHere = true;

            return true;
        } catch (Exception e) {
            // The usual cause is `allow-external-apps` not being set, and
            // Termux refuses without saying much.
            Log.e(TAG, "local server: Termux refused - " + e);
            return false;
        }
    }
}
