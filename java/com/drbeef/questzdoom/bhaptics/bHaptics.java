package com.drbeef.questzdoom.bhaptics;

import android.Manifest;
import android.app.Activity;
import android.content.Context;
import android.support.v4.app.ActivityCompat;
import android.util.Log;

import com.bhaptics.bhapticsmanger.BhapticsManager;
import com.bhaptics.bhapticsmanger.BhapticsManagerCallback;
import com.bhaptics.bhapticsmanger.BhapticsModule;
import com.bhaptics.bhapticsmanger.HapticPlayer;
import com.bhaptics.commons.PermissionUtils;
import com.bhaptics.commons.model.BhapticsDevice;
import com.bhaptics.commons.model.DotPoint;
import com.bhaptics.commons.model.PositionType;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.Reader;
import java.nio.charset.Charset;
import java.nio.charset.StandardCharsets;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import java.util.UUID;
import java.util.Vector;



public class bHaptics {
    
    public static class Haptic
    {
        Haptic(PositionType type, String key, String altKey, float intensity, float duration) {
            this.type = type;
            this.key = key;
            this.altKey = altKey;
            this.intensity = intensity;
            this.duration = duration;
        }

        public final String key;
        public final String altKey;
        public final float intensity;
        public final float duration;
        public final PositionType type;
    };
    
    private static final String TAG = "Doom3Quest.bHaptics";

    private static Random rand = new Random();

    private static boolean hasPairedDevice = false;

    private static boolean enabled = false;
    private static boolean requestingPermission = false;
    private static boolean initialised = false;

    private static HapticPlayer player;

    private static Context context;

    private static Map<String, Vector<Haptic>> eventToEffectKeyMap = new HashMap<>();

    public static void initialise()
    {
        if (initialised)
        {
            //Already initialised, but might need to rescan
            scanIfNeeded();
            return;
        }

        BhapticsModule.initialize(context);

        scanIfNeeded();

        player = BhapticsModule.getHapticPlayer();

        /*
            DAMAGE
        */
        registerFromAsset(context, "bHaptics/Damage/Body_Heartbeat.tact", PositionType.Vest, "heartbeat", 1.0f, 1.2f);

        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Melee1.tact", "melee_left");
        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Melee2.tact", "melee_right");
        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Fireball.tact", "fireball");
        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Bullet.tact", "bullet");
        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Shotgun.tact", "shotgun");
        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Electric.tact", "electric");
        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Fire.tact", "fire");
        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Fire.tact", "poison"); // reuse
        registerFromAsset(context, "bHaptics/Damage/Body_DMG_Falling.tact", "fall");
        registerFromAsset(context, "bHaptics/Damage/Body_Shield_Break.tact", "slime");

        /*
            INTERACTIONS
         */
        registerFromAsset(context, "bHaptics/Interaction/Vest/Body_Healstation.tact", "healstation");
        registerFromAsset(context, "bHaptics/Interaction/Arms/Healthstation_L.tact", PositionType.ForearmL, "healstation");
        registerFromAsset(context, "bHaptics/Interaction/Arms/Healthstation_R.tact", PositionType.ForearmR, "healstation");

        registerFromAsset(context, "bHaptics/Interaction/Arms/Ammo_L.tact", PositionType.ForearmL, "pickup");
        registerFromAsset(context, "bHaptics/Interaction/Arms/Ammo_R.tact", PositionType.ForearmR, "pickup");

        registerFromAsset(context, "bHaptics/Interaction/Vest/Body_Shield_Get.tact", "pickup_weapon");
        registerFromAsset(context, "bHaptics/Interaction/Arms/Pickup_L.tact", PositionType.ForearmL, "pickup_weapon");
        registerFromAsset(context, "bHaptics/Interaction/Arms/Pickup_R.tact", PositionType.ForearmR, "pickup_weapon");

        registerFromAsset(context, "bHaptics/Weapon/Vest/Body_Pistol.tact", "fire_weapon");
        registerFromAsset(context, "bHaptics/Weapon/Arms/ShootDefault_L.tact", PositionType.ForearmL, "fire_weapon");
        registerFromAsset(context, "bHaptics/Weapon/Arms/ShootDefault_R.tact", PositionType.ForearmR, "fire_weapon");

        initialised = true;
    }

    public static void registerFromAsset(Context context, String filename, PositionType type, String key, float intensity, float duration)
    {
        String content = read(context, filename);
        if (content != null) {

            String hapticKey = key + "_" + type.name();
            player.registerProject(hapticKey, content);

            UUID uuid = UUID.randomUUID();
            Haptic haptic = new Haptic(type, hapticKey, uuid.toString(), intensity, duration);

            Vector<Haptic> haptics;
            if (!eventToEffectKeyMap.containsKey(key))
            {
                haptics = new Vector<>();
                haptics.add(haptic);
                eventToEffectKeyMap.put(key, haptics);
            }
            else
            {
                haptics = eventToEffectKeyMap.get(key);
                haptics.add(haptic);
            }
        }
    }

    public static void registerFromAsset(Context context, String filename, String key)
    {
        registerFromAsset(context, filename, PositionType.Vest, key, 1.0f, 1.0f);
    }

    public static void registerFromAsset(Context context, String filename, PositionType type, String key)
    {
        registerFromAsset(context, filename, type, key, 1.0f, 1.0f);
    }

    public static void destroy()
    {
        if (initialised) {
            BhapticsModule.destroy();
        }
    }

    private static boolean hasPermissions() {
        boolean blePermission = PermissionUtils.hasBluetoothPermission(context);
        boolean filePermission = PermissionUtils.hasFilePermissions(context);
        return blePermission && filePermission;
    }

    private static void requestPermissions() {
        if (!requestingPermission) {
            requestingPermission = true;
            ActivityCompat.requestPermissions((Activity) context,
                    new String[]{Manifest.permission.ACCESS_FINE_LOCATION},
                    2);
        }
    }

    private static boolean checkPermissionsAndInitialize() {
        if (hasPermissions()) {
            // Permissions have already been granted.
            return true;
        }
        else
        {
            requestPermissions();
        }

        return false;
    }

    public static void enable(Context ctx)
    {
        context = ctx;

        if (!enabled)
        {
            if (checkPermissionsAndInitialize()) {
                initialise();

                enabled = true;
            }
        }
    }

    public static void disable()
    {
        enabled = false;
        requestingPermission = false;
    }

    public static void playHaptic(String event, int position, float intensity, float angle, float yHeight)
    {

        if (enabled && hasPairedDevice) {
            String key = getHapticEventKey(event);

            //Log.v(TAG, event);

            //Special rumble effect that changes intensity per frame
            if (key.contains("rumble"))
            {
                {
                    float highDuration = angle;

                    List<DotPoint> vector = new Vector<>();
                    int flipflop = 0;
                    for (int d = 0; d < 20; d += 4) // Only select every other dot
                    {
                        vector.add(new DotPoint(d + flipflop, (int) intensity));
                        vector.add(new DotPoint(d + 2 + flipflop, (int) intensity));
                        flipflop = 1 - flipflop;
                    }

                    if (key.contains("front")) {
                        player.submitDot("rumble_front", PositionType.VestFront, vector, (int) highDuration);
                    }
                    else {
                        player.submitDot("rumble_back", PositionType.VestBack, vector, (int) highDuration);
                    }
                }
            }
            else if (eventToEffectKeyMap.containsKey(key)) {
                Vector<Haptic> haptics = eventToEffectKeyMap.get(key);

                for (Haptic haptic : haptics) {

                    //Don't allow heartbeat haptic to interrupt itself if it is already playing
                    if (haptic.key.contains("heartbeat") &&
                            player.isPlaying(haptic.altKey))
                    {
                        continue;
                    }

                    //The following groups play at full intensity
                    if (haptic.altKey.compareTo("environment") == 0) {
                        intensity = 100;
                    }

                    if (position > 0)
                    {
                        //If playing left position and haptic type is right, don;t play that one
                        if (position == 1 && haptic.type == PositionType.ForearmR)
                        {
                            continue;
                        }

                        //If playing right position and haptic type is left, don;t play that one
                        if (position == 2 && haptic.type == PositionType.ForearmL)
                        {
                            continue;
                        }
                    }

                    if (haptic != null) {
                        float flIntensity = ((intensity / 100.0F) * haptic.intensity);
                        float duration = haptic.duration;

                        //Special hack for heartbeat
                        if (haptic.key.contains("heartbeat"))
                        {
                            //The worse condition we are in, the faster the heart beats!
                            float health = intensity;
                            duration = 1.0f - (0.6f * ((25 - health) / 25));
                            flIntensity = ((25 - health) / 25);
                        }

                        if (flIntensity > 0)
                        {
                            player.submitRegistered(haptic.key, haptic.altKey, flIntensity, duration, angle, yHeight);
                        }
                    }
                }
            }
            else
            {
                Log.i(TAG, "Unknown event: " + event);
            }
        }
    }

    private static String getHapticEventKey(String e) {
        String event = e.toLowerCase();
        String key = event;
        if (event.contains("melee") ||
                event.contains("strike") ||
                event.contains("rip") ||
                event.contains("tear") ||
                event.contains("slice") ||
                event.contains("claw")) {
            if (event.contains("right"))
            {
                key = "melee_right";
            }
            else if (event.contains("left"))
            {
                key = "melee_left";
            }
            else {
                key = rand.nextInt(2) == 0 ? "melee_right" : "melee_left";
            }
        } else if (event.contains("bullet")) {
            key = "bullet";
        } else if (event.contains("burn") ||
                event.contains("flame")) {
            key = "fireball";
        } else if (event.contains("shotgun")) {
            key = "shotgun";
        } else if (event.contains("fall")) {
            key = "fall";
        }

        return key;
    }

    public static void stopAll() {

        if (hasPairedDevice) {
            player.turnOffAll();
        }
    }

    public static void stopHaptic(String event) {

        if (hasPairedDevice) {

            String key = getHapticEventKey(event);
            {
                player.turnOff(key);
            }
        }
    }


    public static String read(Context context, String fileName) {
        try {
            InputStream is = context.getAssets().open(fileName);
            StringBuilder textBuilder = new StringBuilder();
            try (Reader reader = new BufferedReader(new InputStreamReader
                    (is, Charset.forName(StandardCharsets.UTF_8.name())))) {
                int c = 0;
                while ((c = reader.read()) != -1) {
                    textBuilder.append((char) c);
                }
            }

            return textBuilder.toString();
        } catch (IOException e) {
            Log.e(TAG, "read: ", e);
        }
        return null;
    }

    public static void scanIfNeeded() {
        BhapticsManager manager = BhapticsModule.getBhapticsManager();

        List<BhapticsDevice> deviceList = manager.getDeviceList();
        for (BhapticsDevice device : deviceList) {
            if (device.isPaired()) {
                hasPairedDevice = true;
                break;
            }
        }

        if (hasPairedDevice) {
            manager.scan();

            manager.addBhapticsManageCallback(new BhapticsManagerCallback() {
                @Override
                public void onDeviceUpdate(List<BhapticsDevice> list) {

                }

                @Override
                public void onScanStatusChange(boolean b) {

                }

                @Override
                public void onChangeResponse() {

                }

                @Override
                public void onConnect(String s) {
                    Thread t = new Thread(() -> {
                        try {
                            Thread.sleep(1000);
                            manager.ping(s);
                        }
                        catch (Throwable e) {
                        }
                    });
                    t.start();
                }

                @Override
                public void onDisconnect(String s) {

                }
            });

        }
    }
}
