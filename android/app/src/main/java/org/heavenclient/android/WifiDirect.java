package org.heavenclient.android;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.wifi.WifiManager;
import android.net.wifi.p2p.WifiP2pConfig;
import android.net.wifi.p2p.WifiP2pDevice;
import android.net.wifi.p2p.WifiP2pDeviceList;
import android.net.wifi.p2p.WifiP2pManager;
import android.provider.Settings;
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

    /**
     * What this device currently IS in Wi-Fi Direct terms, cached.
     *
     * 0 nothing, 1 the group owner, 2 a client in somebody's group.
     *
     * This matters more than it looks. A device that owns a group is being a
     * network; it cannot join another one. The app defaults to HOST at
     * launch, so it would create a group and then - on tapping JOIN - go
     * looking for a host while still being one. Two devices doing that sit
     * there being networks at each other forever, which is exactly what
     * happened.
     */
    private static volatile int role = 0;

    private WifiDirect() {
    }

    /**
     * Whether the wifi RADIO is on - which is not the same as being connected
     * to anything, and is the trap that stopped this working the first time
     * it was tried in earnest.
     *
     * Wi-Fi Direct needs no network, no router and no internet. It does need
     * the radio. Switching wifi off switches the peer-to-peer side off with
     * it: the p2p state machine drops into P2pDisabledState and every call
     * comes back BUSY, which reads exactly like a device that cannot do it.
     *
     * "Offline" means no network. It must not mean wifi off.
     */
    public static boolean isRadioOn(Context context) {
        if (context == null) {
            return false;
        }

        WifiManager wifi = (WifiManager) context.getApplicationContext()
                .getSystemService(Context.WIFI_SERVICE);

        return wifi != null && wifi.isWifiEnabled();
    }

    /**
     * Show Android's own wifi panel so it can be switched on.
     *
     * An app has not been allowed to enable wifi by itself since Android 10 -
     * `setWifiEnabled` does nothing for us - so the honest thing is to put
     * the switch in front of the person rather than fail quietly.
     */
    public static boolean openWifiSettings(Context context) {
        if (context == null) {
            return false;
        }

        try {
            Intent panel = new Intent(Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                    ? Settings.Panel.ACTION_WIFI
                    : Settings.ACTION_WIFI_SETTINGS);

            panel.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(panel);

            return true;
        } catch (Exception e) {
            Log.e(TAG, "could not open the wifi settings - " + e);
            return false;
        }
    }

    /** 0 nothing, 1 group owner, 2 client. Cached - see `role`. */
    public static int role() {
        return role;
    }

    /** Refresh what we are. Cheap, and the answer arrives on a callback. */
    public static void refreshRole(Context context) {
        if (manager == null || channel == null) {
            role = 0;
            return;
        }

        try {
            manager.requestConnectionInfo(channel, info -> {
                if (info == null || !info.groupFormed) {
                    role = 0;
                } else {
                    role = info.isGroupOwner ? 1 : 2;
                }
            });
        } catch (SecurityException ignored) {
            role = 0;
        }
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
                    role = 1;
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

    /**
     * Tear our own group down and give the wifi back.
     *
     * Called before looking for somebody to join, because a device that owns
     * a group cannot join another - it IS a network, and networks do not join
     * networks.
     */
    public static void removeGroup(Context context) {
        if (!connect(context)) {
            return;
        }

        try {
            manager.removeGroup(channel, new WifiP2pManager.ActionListener() {
                @Override
                public void onSuccess() {
                    Log.i(TAG, "wifi direct: stopped being a network");
                    role = 0;
                }

                @Override
                public void onFailure(int reason) {
                    // 2 = BUSY here usually means there was no group anyway.
                    role = 0;
                }
            });
        } catch (SecurityException ignored) {
        }
    }

    /**
     * Look for somebody hosting, and JOIN them when found.
     *
     * The first version of this only looked. It called discoverPeers, logged
     * that it was looking, and stopped - so even with everything else right
     * the two devices would find each other and sit there. Discovering a peer
     * is not connecting to one.
     *
     * The peer we want is the group owner: the host called createGroup, which
     * makes it a small access point advertising itself. Connecting to it puts
     * this device on its network at 192.168.49.x, and from there ordinary
     * discovery finds the game by name exactly as it does on any other wifi.
     *
     * Android puts a confirmation in front of the OTHER person. That is not
     * something to design around - joining somebody's network is their
     * decision as much as ours.
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
                    Log.i(TAG, "wifi direct: looking for a host");
                    askForPeers();
                }

                @Override
                public void onFailure(int reason) {
                    Log.e(TAG, "wifi direct: could not look, " + explain(reason));
                }
            });

            return true;
        } catch (SecurityException e) {
            Log.e(TAG, "wifi direct: refused - " + e);
            return false;
        }
    }

    /** Ask what discovery has turned up, and connect to a host if one is there. */
    private static void askForPeers() {
        try {
            manager.requestPeers(channel, new WifiP2pManager.PeerListListener() {
                @Override
                public void onPeersAvailable(WifiP2pDeviceList peers) {
                    for (WifiP2pDevice device : peers.getDeviceList()) {
                        // isGroupOwner is only set once a group exists, so
                        // prefer it and fall back to any available device -
                        // a host that has just started may not be flagged yet.
                        if (device.status == WifiP2pDevice.AVAILABLE
                                || device.isGroupOwner()) {
                            join(device);
                            return;
                        }
                    }
                }
            });
        } catch (SecurityException e) {
            Log.e(TAG, "wifi direct: refused - " + e);
        }
    }

    private static void join(WifiP2pDevice device) {
        WifiP2pConfig config = new WifiP2pConfig();
        config.deviceAddress = device.deviceAddress;

        // Let the host be the group owner. It is the one running the server,
        // so it should be the one at the fixed address.
        config.groupOwnerIntent = 0;

        try {
            manager.connect(channel, config, new WifiP2pManager.ActionListener() {
                @Override
                public void onSuccess() {
                    Log.i(TAG, "wifi direct: asked to join '" + device.deviceName + "'");
                }

                @Override
                public void onFailure(int reason) {
                    Log.e(TAG, "wifi direct: could not join, " + explain(reason));
                }
            });
        } catch (SecurityException e) {
            Log.e(TAG, "wifi direct: refused - " + e);
        }
    }

    /**
     * The reason codes are bare integers and BUSY covers a multitude - most
     * often, in practice, the wifi radio being switched off entirely.
     */
    private static String explain(int reason) {
        switch (reason) {
        case WifiP2pManager.P2P_UNSUPPORTED:
            return "this device cannot do Wi-Fi Direct";
        case WifiP2pManager.ERROR:
            return "the system refused";
        case WifiP2pManager.BUSY:
            return "busy - is the wifi radio switched off?";
        default:
            return "reason " + reason;
        }
    }
}
