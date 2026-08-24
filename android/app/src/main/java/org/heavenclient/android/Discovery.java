package org.heavenclient.android;

import android.content.Context;
import android.net.nsd.NsdManager;
import android.net.nsd.NsdServiceInfo;
import android.os.Build;
import android.util.Log;

import java.net.InetAddress;
import java.util.LinkedHashMap;
import java.util.Map;

/**
 * Finding games to join, by NAME rather than by address.
 *
 * Nobody should ever type an IP to play with their brother. A device hosting
 * a game shouts its name onto the local network; every other device listens
 * and shows a list. Tap "Joey's Thor" and you are in.
 *
 * This is plain mDNS service discovery - the same mechanism a printer uses to
 * appear in a print dialog. It matters that it is NOT tied to Wi-Fi Direct:
 * it works over anything the devices share, whether that is a phone's
 * hotspot, a travel router, the house wifi, or a Wi-Fi Direct group. Getting
 * a network to exist is a separate problem, solved in WifiDirect.java. This
 * only answers "who is hosting on the network I am already on".
 */
public final class Discovery {
    private static final String TAG = "HeavenClient";

    /** Our own corner of mDNS. The trailing dot-less form is what Android wants. */
    private static final String SERVICE_TYPE = "_maplestory._tcp.";

    private static NsdManager manager;
    private static NsdManager.RegistrationListener registration;
    private static NsdManager.DiscoveryListener discovery;

    /** Found games, newest name wins. Keyed by name so a re-announce replaces. */
    private static final Map<String, String> found = new LinkedHashMap<>();

    private Discovery() {
    }

    private static NsdManager manager(Context context) {
        if (manager == null && context != null) {
            manager = (NsdManager) context.getSystemService(Context.NSD_SERVICE);
        }

        return manager;
    }

    /**
     * Shout that a game is running here. `name` is what other people will see
     * in the list, so it should be something a child recognises.
     */
    public static boolean host(Context context, String name, int port) {
        NsdManager nsd = manager(context);

        if (nsd == null) {
            return false;
        }

        stopHosting(context);

        NsdServiceInfo info = new NsdServiceInfo();
        info.setServiceName(name);
        info.setServiceType(SERVICE_TYPE);
        info.setPort(port);

        registration = new NsdManager.RegistrationListener() {
            @Override
            public void onServiceRegistered(NsdServiceInfo info) {
                Log.i(TAG, "hosting as '" + info.getServiceName() + "'");
            }

            @Override
            public void onRegistrationFailed(NsdServiceInfo info, int code) {
                Log.e(TAG, "could not announce the game: " + code);
            }

            @Override
            public void onServiceUnregistered(NsdServiceInfo info) {
            }

            @Override
            public void onUnregistrationFailed(NsdServiceInfo info, int code) {
            }
        };

        try {
            nsd.registerService(info, NsdManager.PROTOCOL_DNS_SD, registration);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "could not announce the game - " + e);
            registration = null;
            return false;
        }
    }

    public static void stopHosting(Context context) {
        NsdManager nsd = manager(context);

        if (nsd == null || registration == null) {
            return;
        }

        try {
            nsd.unregisterService(registration);
        } catch (Exception ignored) {
            // Already gone. Nothing to do and nothing worth saying.
        }

        registration = null;
    }

    /** Start listening for games. Safe to call repeatedly. */
    public static boolean browse(Context context) {
        NsdManager nsd = manager(context);

        if (nsd == null) {
            return false;
        }

        if (discovery != null) {
            return true;
        }

        synchronized (found) {
            found.clear();
        }

        discovery = new NsdManager.DiscoveryListener() {
            @Override
            public void onDiscoveryStarted(String type) {
            }

            @Override
            public void onServiceFound(NsdServiceInfo info) {
                // A find is only a NAME. The address needs a second step.
                resolve(info);
            }

            @Override
            public void onServiceLost(NsdServiceInfo info) {
                synchronized (found) {
                    found.remove(info.getServiceName());
                }
            }

            @Override
            public void onDiscoveryStopped(String type) {
            }

            @Override
            public void onStartDiscoveryFailed(String type, int code) {
                Log.e(TAG, "could not look for games: " + code);
                discovery = null;
            }

            @Override
            public void onStopDiscoveryFailed(String type, int code) {
                discovery = null;
            }
        };

        try {
            nsd.discoverServices(SERVICE_TYPE, NsdManager.PROTOCOL_DNS_SD, discovery);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "could not look for games - " + e);
            discovery = null;
            return false;
        }
    }

    @SuppressWarnings("deprecation")
    private static void resolve(NsdServiceInfo info) {
        // resolveService is deprecated in Android 14 for a callback-based
        // API, but the replacement does not exist below it and these devices
        // are on 13. Kept deliberately, with the warning silenced rather than
        // left to rot in the build output.
        manager.resolveService(info, new NsdManager.ResolveListener() {
            @Override
            public void onResolveFailed(NsdServiceInfo info, int code) {
                // Common and harmless - another resolve is usually in flight.
            }

            @Override
            public void onServiceResolved(NsdServiceInfo info) {
                InetAddress host = info.getHost();

                if (host == null) {
                    return;
                }

                synchronized (found) {
                    found.put(info.getServiceName(), host.getHostAddress());
                }

                Log.i(TAG, "found game '" + info.getServiceName()
                        + "' at " + host.getHostAddress());
            }
        });
    }

    public static void stopBrowsing(Context context) {
        NsdManager nsd = manager(context);

        if (nsd == null || discovery == null) {
            return;
        }

        try {
            nsd.stopServiceDiscovery(discovery);
        } catch (Exception ignored) {
        }

        discovery = null;
    }

    /**
     * What has been found so far, as "name\taddress" strings.
     *
     * Polled by the client rather than pushed, because the answer changes on
     * NSD's own threads and the game reads it once a frame from its own. A
     * flat array of strings is the cheapest thing to hand across JNI.
     */
    public static String[] list() {
        synchronized (found) {
            String[] out = new String[found.size()];
            int i = 0;

            for (Map.Entry<String, String> entry : found.entrySet()) {
                out[i++] = entry.getKey() + "\t" + entry.getValue();
            }

            return out;
        }
    }

    /** A name for this device that a child would recognise. */
    public static String suggestedName() {
        String model = Build.MODEL;

        return (model == null || model.isEmpty()) ? "Maple Server" : model;
    }
}
