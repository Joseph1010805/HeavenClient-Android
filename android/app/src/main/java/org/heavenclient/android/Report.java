package org.heavenclient.android;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;

import androidx.core.content.FileProvider;

import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.OutputStream;
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

    /**
     * PUT A COPY WHERE A PERSON CAN ACTUALLY FIND IT.
     *
     * The report is written next to the game data, under
     * Android/data/<package>/files. Since Android 11 that directory is
     * invisible: no file manager shows it and it does not appear over USB.
     * So the share sheet was the only way out, and a handheld with no mail
     * app installed offers nothing useful in it - the report exists, is
     * correct, and cannot be handed to anybody.
     *
     * This copies it to the public Downloads folder, which every file
     * manager shows and which appears over a cable. The share sheet is still
     * offered; this is the floor under it.
     *
     * @return where it went, in words fit to show a player, or null.
     */
    public static String saveToDownloads(Activity activity, String picture) {
        if (activity == null) {
            return null;
        }

        try {
            File dir = activity.getExternalFilesDir(null);

            if (dir == null) {
                return null;
            }

            File report = new File(dir, "report.txt");

            if (!report.exists() || report.length() == 0) {
                return null;
            }

            String stamp = picture != null && picture.startsWith("report-")
                    ? picture.substring("report-".length()).replace(".png", "")
                    : Long.toString(System.currentTimeMillis() / 1000L);

            String name = "report-" + stamp + ".txt";

            if (copy(activity, report, name, "text/plain") == null) {
                return null;
            }

            // The picture too, when there is one - a screenshot is half of
            // what makes a report readable.
            if (picture != null && !picture.isEmpty()) {
                File shot = new File(dir, picture);

                if (shot.exists() && shot.length() > 0) {
                    copy(activity, shot, picture, "image/png");
                }
            }

            return "Download/" + FOLDER + "/" + name;
        } catch (Exception e) {
            Log.e(TAG, "could not copy the report to Downloads", e);
            return null;
        }
    }

    private static final String FOLDER = "BugsNBeans";

    /**
     * One file into public Downloads, by whichever route this Android allows.
     *
     * MediaStore on 29 and up - a direct write to Downloads is refused there
     * and fails with nothing in the log worth reading. Below that there is no
     * MediaStore Downloads collection, so the plain path is correct.
     */
    private static Uri copy(Activity activity, File src, String name,
            String mime) throws Exception {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            ContentResolver res = activity.getContentResolver();

            ContentValues v = new ContentValues();
            v.put(MediaStore.Downloads.DISPLAY_NAME, name);
            v.put(MediaStore.Downloads.MIME_TYPE, mime);
            v.put(MediaStore.Downloads.RELATIVE_PATH,
                    Environment.DIRECTORY_DOWNLOADS + "/" + FOLDER);

            Uri out = res.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, v);

            if (out == null) {
                return null;
            }

            InputStream in = new FileInputStream(src);
            OutputStream os = res.openOutputStream(out);

            try {
                pipe(in, os);
            } finally {
                close(in);
                close(os);
            }

            return out;
        }

        File downloads = Environment.getExternalStoragePublicDirectory(
                Environment.DIRECTORY_DOWNLOADS);
        File folder = new File(downloads, FOLDER);

        if (!folder.exists() && !folder.mkdirs()) {
            return null;
        }

        File dst = new File(folder, name);

        InputStream in = new FileInputStream(src);
        OutputStream os = new java.io.FileOutputStream(dst);

        try {
            pipe(in, os);
        } finally {
            close(in);
            close(os);
        }

        return Uri.fromFile(dst);
    }

    private static void pipe(InputStream in, OutputStream out)
            throws Exception {
        if (in == null || out == null) {
            throw new IllegalStateException("no stream");
        }

        byte[] buf = new byte[65536];
        int n;

        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
        }

        out.flush();
    }

    private static void close(java.io.Closeable c) {
        if (c == null) {
            return;
        }

        try {
            c.close();
        } catch (Exception ignored) {
            // Closing is not worth failing a bug report over.
        }
    }
}
