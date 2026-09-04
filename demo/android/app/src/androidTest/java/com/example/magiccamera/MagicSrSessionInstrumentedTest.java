package com.example.magiccamera;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import androidx.test.platform.app.InstrumentationRegistry;

import com.example.superresolution.natives.SuperResolutionLib;

import org.junit.Test;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;

/**
 * Device smoke test for the v2.1.0 core Session JNI
 * ({@code MC_Process} / {@code MC_Uninit}).
 * Copies a model from APK assets so a clean install does not depend on MainActivity.
 */
public class MagicSrSessionInstrumentedTest {
    private static final String TAG = "MagicSRSessionITest";
    private static final int MODE_SPATIAL_SPEED = 0;
    private static final int MODE_SPATIAL_BALANCED = 1;
    private static final int BACKEND_OPENGLES = 5;
    private static final float SCALE = 2.0f;
    private static final int WIDTH = 64;
    private static final int HEIGHT = 64;
    private static final String COMBINED_GPU_MODEL = "magic_sr_gpu_params.bin";

    @Test
    public void magicOpenGlesSessionPathAcceptsHandleOnDevice() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        File modelFile = copyAssetModel(context);
        assertTrue("copied model empty: " + modelFile, modelFile.length() > 0);

        int outW = MainActivity.scaledDimension(WIDTH, SCALE);
        int outH = MainActivity.scaledDimension(HEIGHT, SCALE);
        byte[] input = new byte[WIDTH * HEIGHT * 4];
        byte[] output = new byte[outW * outH * 4];
        for (int i = 0; i < input.length; i += 4) {
            input[i] = (byte) 64;
            input[i + 1] = (byte) 128;
            input[i + 2] = (byte) 192;
            input[i + 3] = (byte) 255;
        }

        String version = SuperResolutionLib.getVersion();
        Log.i(TAG, "MagicSR version: " + version);
        assertEquals("v2.1.0", version);

        long handle = 0L;
        int ret = -1;
        try {
            handle = SuperResolutionLib.initSuperResolution(
                    WIDTH, HEIGHT, SCALE, MODE_SPATIAL_SPEED, 1, BACKEND_OPENGLES,
                    modelFile.getAbsolutePath());
            ret = handle == 0 ? -1 : SuperResolutionLib.processImage(
                    input, output, 4, WIDTH, HEIGHT, outW, outH);
            Log.i(TAG, "handle=" + handle + " ret=" + ret + " model=" + modelFile);
            assertTrue("MC_Process failed handle=" + handle + " ret=" + ret,
                    handle != 0 && ret == 0);

            // Verify output has content
            boolean hasNonZero = false;
            for (byte b : output) {
                if (b != 0) {
                    hasNonZero = true;
                    break;
                }
            }
            assertTrue("output image should not be all zero", hasNonZero);
        } finally {
            SuperResolutionLib.uninitSuperResolution();
        }
    }

    @Test
    public void magicBalancedModeOnDevice() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        File modelFile = copyAssetModel(context);

        int outW = MainActivity.scaledDimension(WIDTH, SCALE);
        int outH = MainActivity.scaledDimension(HEIGHT, SCALE);
        byte[] input = new byte[WIDTH * HEIGHT * 4];
        byte[] output = new byte[outW * outH * 4];
        for (int i = 0; i < input.length; i += 4) {
            input[i] = (byte) 100;
            input[i + 1] = (byte) 150;
            input[i + 2] = (byte) 200;
            input[i + 3] = (byte) 255;
        }

        long handle = 0L;
        int ret = -1;
        try {
            handle = SuperResolutionLib.initSuperResolution(
                    WIDTH, HEIGHT, SCALE, MODE_SPATIAL_BALANCED, 1, BACKEND_OPENGLES,
                    modelFile.getAbsolutePath());
            ret = handle == 0 ? -1 : SuperResolutionLib.processImage(
                    input, output, 4, WIDTH, HEIGHT, outW, outH);
            Log.i(TAG, "Balanced handle=" + handle + " ret=" + ret);
            assertTrue("Balanced mode failed handle=" + handle + " ret=" + ret,
                    handle != 0 && ret == 0);
        } finally {
            SuperResolutionLib.uninitSuperResolution();
        }
    }

    @Test
    public void magicConsecutiveFramesAndDynamicScaleChange() throws Exception {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        File modelFile = copyAssetModel(context);

        try {
            // First run 1.5x scale
            float scale1 = 1.5f;
            int outW1 = MainActivity.scaledDimension(WIDTH, scale1);
            int outH1 = MainActivity.scaledDimension(HEIGHT, scale1);
            byte[] input = new byte[WIDTH * HEIGHT * 4];
            byte[] output1 = new byte[outW1 * outH1 * 4];
            for (int i = 0; i < input.length; i += 4) {
                input[i] = (byte) (i % 256);
                input[i + 1] = (byte) ((i * 3) % 256);
                input[i + 2] = (byte) ((i * 7) % 256);
                input[i + 3] = (byte) 255;
            }

            long handle1 = SuperResolutionLib.initSuperResolution(
                    WIDTH, HEIGHT, scale1, MODE_SPATIAL_SPEED, 1, BACKEND_OPENGLES,
                    modelFile.getAbsolutePath());
            assertTrue("scale 1.5 init failed", handle1 != 0);

            // Run 5 consecutive frames
            for (int f = 0; f < 5; f++) {
                int ret = SuperResolutionLib.processImage(
                        input, output1, 4, WIDTH, HEIGHT, outW1, outH1);
                assertEquals("frame " + f + " process failed", 0, ret);
            }

            // Switch to 2.0x scale (triggering dynamic reinit inside session)
            float scale2 = 2.0f;
            int outW2 = MainActivity.scaledDimension(WIDTH, scale2);
            int outH2 = MainActivity.scaledDimension(HEIGHT, scale2);
            byte[] output2 = new byte[outW2 * outH2 * 4];

            long handle2 = SuperResolutionLib.initSuperResolution(
                    WIDTH, HEIGHT, scale2, MODE_SPATIAL_SPEED, 1, BACKEND_OPENGLES,
                    modelFile.getAbsolutePath());
            assertTrue("scale 2.0 reinit failed", handle2 != 0);

            for (int f = 0; f < 5; f++) {
                int ret = SuperResolutionLib.processImage(
                        input, output2, 4, WIDTH, HEIGHT, outW2, outH2);
                assertEquals("reinit frame " + f + " process failed", 0, ret);
            }
            Log.i(TAG, "Consecutive frames & dynamic scale change succeeded!");
        } finally {
            SuperResolutionLib.uninitSuperResolution();
        }
    }

    private static File copyAssetModel(Context context) throws IOException {
        AssetManager am = context.getAssets();
        String assetName = firstExistingAsset(am, new String[] {
                "model/" + COMBINED_GPU_MODEL,
                "model/magic_gles_speed_gpu_params.bin",
                "model/magic_gles_highspeed_gpu_params.bin"
        });
        assertTrue("No spatial GPU model in APK assets/model/ (need " + COMBINED_GPU_MODEL + ")",
                assetName != null);

        File out = new File(context.getCacheDir(), "mc_session_itest_" + new File(assetName).getName());
        try (InputStream in = am.open(assetName);
             OutputStream os = new FileOutputStream(out, false)) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                os.write(buf, 0, n);
            }
            os.flush();
        }
        return out;
    }

    private static String firstExistingAsset(AssetManager am, String[] names) {
        for (String name : names) {
            try (InputStream in = am.open(name)) {
                if (in != null) return name;
            } catch (IOException ignored) {
                // try next
            }
        }
        return null;
    }
}
