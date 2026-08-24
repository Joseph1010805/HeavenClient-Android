package org.heavenclient.android;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.net.wifi.p2p.WifiP2pManager;
import android.os.Build;
import android.os.Looper;
import android.util.Log;

/**
 * Making a network exist where there is none - a car, mostly.
 *
 * Wi-Fi Direct is two devices agreeing to be a wifi network between
 * themselves, with no router and no internet. One becomes the "group owner",
 * which is a small access point, and always takes the address 192.168.49.1.
 * Everyone else joins it.
 *
 * WHAT THIS IS NOT. It does not find games and it does not know their names -
 * that is Discovery.java, which works over any network including this one.
 * The two are deliberately separate, because a phone's hotspot or a travel
 * router solves the same problem with no code at all, and on those days none
 * of this needs to run.
 *
 * THE ADDRESS IS FIXED, WHICH IS THE USEFUL PART. A group owner is always
 * 192.168.49.1, so a client that has joined a group knows where the server is
 * without being told. Discovery still runs on top, so the list shows a name
 * rather than a number, but if discovery fails the address is not a mystery.
 */
public final class WifiDirect {
    private static final String TAG = "HeavenClient";

    /** Every Wi-Fi Direct group owner is this. Not a guess - it is fixed. */
    public static final String GROUP_OWNER = "192.168.49.1";

    private static WifiP2pManager manager;
    private static WifiP2pManager.Channel channel;

    private WifiDirect() {
    }

    /** Whether this device has the hardware at all. The Quest is the doubt. */
    public static boolean isSupported(Context context) {
        if (context == null) {
            return false;
        }

        return context.getPackageManager()
                .hasSystemFeature(PackageManager.FEATURE_WIFI_DIRECT);
    }

    /**
     * Android 13 renamed the permission this needs. Below 13 it was filed
     * under location, of all things, because a list of nearby wifi devices
     * says roughly where you are.
     */
    private static String permission() {
        return Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU
                ? Manifest.permission.NEARBY_WIFI_DEVICES
                : Manifest.permission.ACCESS_FINE_LOCATION;
    }

    public static boolean hasPermission(Context context) {
        return context != null
                && context.checkSelfPermission(permission())
                        == PackageManager.PERMISSION_GRANTED;
    }

    /** Ask for it. The answer arrives long after this returns. */
    public static void requestPermission(Context context) {
        if (context instanceof Activity) {
            ((Activity) context).requestPermissions(
                    new String[] { permission() }, 4712);
        }
    }

    private static boolean connect(Context context) {
        if (manager != null && channel != null) {
            return true;
        }

        manager = (WifiP2pManager) context.getSystemService(Context.WIFI_P2P_SERVICE);

        if (manager == null) {
            Log.e(TAG, "wifi direct: no p2p service on this device");
            return false;
        }

        channel = manager.initialize(context, Looper.getMainLooper(), new WifiP2pManager.ChannelListener() {
            @Override
            public void onChannelDisconnected() {
                Log.i(TAG, "wifi direct: channel lost");
                channel = null;
            }
        });

        return channel != null;
    }

    /**
     * Become the network. This device turns into a small access point at
     * 192.168.49.1 and other devices can join it from their own wifi
     * settings, or from the game.
     */
    public static boolean createGroup(Context context) {
        if (!isSupported(context)) {
            Log.i(TAG, "wifi direct: not supported here");
            return false;
        }

        if (!hasPermission(context)) {
            requestPermission(context);
            return false;
        }

        if (!connect(context)) {
            return false;
        }

        try {
            manager.createGroup(channel, new WifiP2pManager.ActionListener() {
                @Override
                public void onSuccess() {
                    Log.i(TAG, "wifi direct: group created, we are " + GROUP_OWNER);
                }

                @Override
                public void onFailure(int reason) {
                    // 2 = BUSY, which usually means a group already exists.
                    Log.e(TAG, "wifi direct: could not create a group, reason " + reason);
                }
            });

            return true;
        } catch (SecurityException e) {
            Log.e(TAG, "wifi direct: refused - " + e);
            return false;
        }
    }

    /** Tear the group down and give the wifi back. */
    public static void removeGroup(Context context) {
        if (manager == null || channel == null) {
            return;
        }

        try {
            manager.removeGroup(channel, null);
        } catch (SecurityException ignored) {
        }
    }

    /**
     * Look for a group to join. Android shows its own picker and its own
     * confirmation on the other device, which is why this cannot simply
     * connect: joining somebody's network is their decision as well as ours.
     */
    public static boolean discoverPeers(Context context) {
        if (!isSupported(context)) {
            return false;
        }

        if (!hasPermission(context)) {
            requestPermission(context);
            return false;
        }

        if (!connect(context)) {
            return false;
        }

        try {
            manager.discoverPeers(channel, new WifiP2pManager.ActionListener() {
                @Override
                public void onSuccess() {
                    Log.i(TAG, "wifi direct: looking for groups");
                }

                @Override
                public void onFailure(int reason) {
                    Log.e(TAG, "wifi direct: could not look, reason " + reason);
                }
            });

            return true;
        } catch (SecurityException e) {
            Log.e(TAG, "wifi direct: refused - " + e);
            return false;
        }
    }
}
