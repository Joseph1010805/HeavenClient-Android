package org.heavenclient.android;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

import androidx.core.content.FileProvider;

import java.io.File;
import java.util.ArrayList;

/**
 * Hands a bug report to whatever the player already has installed.
 *
 * The point of this class is that the player does nothing. No file manager, no
 * cable, no account, no typing a description - they press one button in the
 * game and pick an app they already use. Anything more than that and reports
 * stop arriving, which is the same as having no reports at all.
 *
 * A FileProvider is required rather than a plain file:// URI: since Android 7
 * handing another app a raw path throws FileUriExposedException and the share
 * dies instantly. The provider grants read access to just these files and only
 * to the app the player chose.
 */
public final class Report {
    private static final String TAG = "HeavenClient";

    private Report() {
    }

    /**
     * Offer the report, the play log and the screenshot as one share.
     *
     * Called from native code once the renderer has actually written the
     * picture - sharing it any earlier attaches a file that does not exist yet.
     * Missing pieces are skipped rather than failing: a report without a
     * picture is still worth reading.
     */
    public static void share(final Activity activity, final String picture) {
        if (activity == null) {
            return;
        }

        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                try {
                    offer(activity, picture);
                } catch (Exception e) {
                    // Never take the game down over a bug report.
                    Log.e(TAG, "could not share the report", e);
                }
            }
        });
    }

    private static void offer(Activity activity, String picture) {
        File dir = activity.getExternalFilesDir(null);

        if (dir == null) {
            Log.e(TAG, "no external files dir - nothing to share");
            return;
        }

        ArrayList<Uri> files = new ArrayList<>();

        add(activity, files, new File(dir, "report.txt"));
        add(activity, files, new File(dir, "HeavenClient/playlog.txt"));

        if (picture != null && !picture.isEmpty()) {
            add(activity, files, new File(dir, picture));
        }

        if (files.isEmpty()) {
            Log.e(TAG, "report share: nothing on disk to send");
            return;
        }

        Intent intent = new Intent(files.size() > 1
                ? Intent.ACTION_SEND_MULTIPLE
                : Intent.ACTION_SEND);

        // Mixed text and image, so the general type - a share sheet narrowed
        // to image/* would hide the mail apps, which are the useful ones.
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_SUBJECT, "Bugs 'n Beans bug report");

        // The player types nothing. This is the whole message.
        intent.putExtra(Intent.EXTRA_TEXT,
                "A bug report from the game. The text file says what was "
                        + "happening, the log says what the game was telling "
                        + "itself, and the picture is what it looked like.");

        if (files.size() > 1) {
            intent.putParcelableArrayListExtra(Intent.EXTRA_STREAM, files);
        } else {
            intent.putExtra(Intent.EXTRA_STREAM, files.get(0));
        }

        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        activity.startActivity(Intent.createChooser(intent, "Send the report"));
    }

    private static void add(Activity activity, ArrayList<Uri> into, File file) {
        if (!file.exists() || file.length() == 0) {
            return;
        }

        into.add(FileProvider.getUriForFile(
                activity, activity.getPackageName() + ".reports", file));
    }
}
