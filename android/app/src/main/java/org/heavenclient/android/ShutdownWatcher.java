package org.heavenclient.android;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;
import android.util.Log;

/**
 * NOTICES THAT THE GAME WAS SWIPED AWAY, so the server does not outlive it.
 *
 * <p>Stopping the server from {@code Activity.onDestroy} covers backing out of
 * the game, and nothing else. Android does NOT reliably call onDestroy when a
 * task is swiped off the recents screen - it can simply kill the process, and
 * then no code of ours runs at all. On a handheld, swiping away IS how people
 * close a game, so that was the common case and not the rare one.
 *
 * <p>{@code onTaskRemoved} is the one callback that does arrive then, and it
 * only exists on a Service. That is the whole reason this class exists: it has
 * no other job, holds nothing, and does no work while the game is running.
 *
 * <p>It must be declared with {@code android:stopWithTask="false"}, otherwise
 * the service is torn down with the task and the callback never comes - which
 * would look exactly like this class not working.
 */
public class ShutdownWatcher extends Service {
    private static final String TAG = "HeavenClient";

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        // Nothing to do while the game runs. START_NOT_STICKY: there is no
        // state worth recreating this service for after the process dies -
        // by then the thing it exists to notice has already happened.
        return START_NOT_STICKY;
    }

    @Override
    public void onTaskRemoved(Intent rootIntent) {
        Log.i(TAG, "game was swiped away - stopping the local server");

        LocalServer.stop(this);

        stopSelf();

        super.onTaskRemoved(rootIntent);
    }
}
