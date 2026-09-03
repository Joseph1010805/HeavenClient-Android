package org.heavenclient.android;

import android.util.Log;

import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.charset.StandardCharsets;

/**
 * THE ONE LEG OF MESSAGING THAT LEAVES THE HOUSE.
 *
 * <p>Everything on the local network is plain HTTP and is done in C++ with
 * asio (Util/Http). This is not: a relay lives on the internet and is
 * therefore https, and <b>this build has no TLS at all</b> - mbedtls is
 * compiled in for the Switch and removed for Android, which uses asio
 * (CMakeLists.txt:597).
 *
 * <p>Rather than add a TLS library and a certificate store to the native
 * side - and get certificate checking subtly wrong, which is the usual
 * outcome - the leg that needs it runs here. Android's own stack already
 * validates against the platform trust store, is patched by the system, and
 * costs nothing to use.
 *
 * <p>⚠ NOTHING IN THE GAME REQUIRES THIS. With no relay configured, which is
 * the default, messages still reach anybody on the same world through the
 * server's own post box, with no internet whatsoever. That is the case that
 * has to survive a power cut, and this is only how a message ALSO reaches
 * somebody in another state.
 *
 * <p>Called from Util/Messages.cpp through JNI, the same way LocalServer is.
 */
public final class Relay {
    private static final String TAG = "HeavenClient";

    /** A handheld on a weak connection, not a datacentre. */
    private static final int CONNECT_MS = 8000;
    private static final int READ_MS = 8000;

    /** A message, not a file transfer. Refused rather than truncated. */
    private static final int MAX_REPLY = 256 * 1024;

    private Relay() {
    }

    /**
     * One exchange.
     *
     * @param body null for a GET, the text to send for a POST.
     * @return the reply body on success, or null. NULL MEANS FAILED - an
     *         empty string is a perfectly good answer meaning "nothing is
     *         waiting for you", and the two must not be confused, or a device
     *         with no post would look like a device that could not connect.
     */
    public static String exchange(String url, String body) {
        HttpURLConnection connection = null;

        try {
            connection = (HttpURLConnection) new URL(url).openConnection();

            connection.setConnectTimeout(CONNECT_MS);
            connection.setReadTimeout(READ_MS);
            connection.setRequestProperty("Content-Type",
                    "text/plain; charset=utf-8");

            if (body != null) {
                connection.setRequestMethod("POST");
                connection.setDoOutput(true);

                byte[] out = body.getBytes(StandardCharsets.UTF_8);

                connection.setFixedLengthStreamingMode(out.length);

                try (OutputStream stream = connection.getOutputStream()) {
                    stream.write(out);
                }
            }

            int code = connection.getResponseCode();

            if (code != 200) {
                // The reason, not just the number. The thing at the other end
                // is a handheld with no console.
                Log.w(TAG, "relay " + url + " answered " + code + ": "
                        + read(connection.getErrorStream()));

                return null;
            }

            return read(connection.getInputStream());
        } catch (Exception e) {
            // NOT an error worth shouting about. No signal in a car is the
            // normal state of this feature, and the message simply waits.
            Log.i(TAG, "relay unreachable (" + url + "): " + e);

            return null;
        } finally {
            if (connection != null) {
                connection.disconnect();
            }
        }
    }

    private static String read(InputStream in) throws Exception {
        if (in == null) {
            return "";
        }

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        byte[] chunk = new byte[8192];
        int got;
        int total = 0;

        while ((got = in.read(chunk)) > 0) {
            total += got;

            if (total > MAX_REPLY) {
                throw new IllegalStateException("reply too large");
            }

            out.write(chunk, 0, got);
        }

        in.close();

        return out.toString("UTF-8");
    }
}
