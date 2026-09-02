package com.example.magiccamera;

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
 * Device smoke test for the v2.0.0 core Session JNI
 * ({@code MC_Init} / {@code MC_Process} / {@code MC_Uninit}).
 * Copies a model from APK assets so a clean install does not depend on MainActivity.
 */
public class MagicSrSessionInstrumentedTest {
    private static final String TAG = "MagicSRSessionITest";
    private static final int MODE_SPATIAL_SPEED = 0;
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

        long handle = 0L;
        int ret = -1;
        try {
            handle = SuperResolutionLib.initSuperResolution(
                    WIDTH, HEIGHT, SCALE, MODE_SPATIAL_SPEED, 1, BACKEND_OPENGLES,
                    modelFile.getAbsolutePath());
            ret = handle == 0 ? -1 : SuperResolutionLib.processImage(
                    input, output, 4, WIDTH, HEIGHT, outW, outH);
            Log.i(TAG, "handle=" + handle + " ret=" + ret + " model=" + modelFile);
            assertTrue("MC_Init/MC_Process failed handle=" + handle + " ret=" + ret,
                    handle != 0 && ret == 0);
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
