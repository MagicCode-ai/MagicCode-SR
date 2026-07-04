package com.example.magiccamera;

import android.Manifest;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.util.Size;
import android.view.Gravity;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.camera.core.CameraSelector;
import androidx.camera.core.ImageAnalysis;
import androidx.camera.core.ImageProxy;
import androidx.camera.lifecycle.ProcessCameraProvider;
import androidx.core.content.ContextCompat;

import com.example.superresolution.natives.SuperResolutionLib;
import com.google.common.util.concurrent.ListenableFuture;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.atomic.AtomicBoolean;

public class MainActivity extends AppCompatActivity {
    private static final String TAG = "MagicMagnifierUI";
    private static final int INPUT_WIDTH = 1920;
    private static final int INPUT_HEIGHT = 1080;
    private static final int MODE_HIGH_SPEED = 0;
    private static final int MODE_SPEED = 1;
    private static final int BACKEND_OPENGLES = 5;
    private static final float MIN_SCALE = 1.0f;
    private static final float MAX_SCALE = 8.0f;
    private static final int SCALE_SEEK_MAX = 700;
    private static final long SCALE_APPLY_THROTTLE_MS = 80L;

    private final ExecutorService cameraExecutor = Executors.newSingleThreadExecutor();
    private final AtomicBoolean processingFrame = new AtomicBoolean(false);
    private final Object engineLock = new Object();
    private final Handler uiHandler = new Handler(Looper.getMainLooper());

    private ImageView outputView;
    private TextView statusText;
    private TextView modeHighBtn;
    private TextView modeSpeedBtn;
    private TextView scaleLabel;
    private SeekBar scaleSeekBar;

    private ProcessCameraProvider cameraProvider;
    private volatile boolean running;
    private long srHandle;
    private int selectedMode = MODE_HIGH_SPEED;
    private float selectedScale = 2.0f;
    private int engineInitWidth = -1;
    private int engineInitHeight = -1;
    private String currentModelPath = "";
    private byte[] rgbaInput;
    private byte[] rgbaOutput;

    private final Runnable scaleApplyRunnable = this::restartEngine;

    private final ActivityResultLauncher<String> cameraPermission =
            registerForActivityResult(new ActivityResultContracts.RequestPermission(), granted -> {
                if (granted) {
                    startCamera();
                } else {
                    stopWithError("camera permission denied");
                }
            });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        buildUi();
        try {
            ensureModelsInSandbox();
        } catch (RuntimeException e) {
            stopWithError(e.getMessage());
            return;
        }
        if (ContextCompat.checkSelfPermission(this, Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED) {
            startCamera();
        } else {
            cameraPermission.launch(Manifest.permission.CAMERA);
        }
    }

    @Override
    protected void onDestroy() {
        running = false;
        uiHandler.removeCallbacks(scaleApplyRunnable);
        stopCamera();
        synchronized (engineLock) {
            SuperResolutionLib.uninitSuperResolution();
            srHandle = 0;
        }
        cameraExecutor.shutdown();
        super.onDestroy();
    }

    private void buildUi() {
        FrameLayout root = new FrameLayout(this);
        outputView = new ImageView(this);
        outputView.setScaleType(ImageView.ScaleType.FIT_CENTER);
        outputView.setBackgroundColor(0xff000000);
        root.addView(outputView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.VERTICAL);
        controls.setPadding(20, 16, 20, 16);
        controls.setBackgroundColor(0x99000000);

        LinearLayout modeRow = new LinearLayout(this);
        modeRow.setOrientation(LinearLayout.HORIZONTAL);
        modeRow.setGravity(Gravity.CENTER_VERTICAL);
        modeHighBtn = buildModeButton("highspeed", true, () -> selectMode(MODE_HIGH_SPEED));
        modeSpeedBtn = buildModeButton("speed", false, () -> selectMode(MODE_SPEED));
        modeRow.addView(modeHighBtn);
        LinearLayout.LayoutParams speedParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        speedParams.leftMargin = 12;
        modeRow.addView(modeSpeedBtn, speedParams);
        controls.addView(modeRow);

        LinearLayout scaleRow = new LinearLayout(this);
        scaleRow.setOrientation(LinearLayout.HORIZONTAL);
        scaleRow.setGravity(Gravity.CENTER_VERTICAL);
        scaleRow.setPadding(0, 10, 0, 0);
        scaleLabel = new TextView(this);
        scaleLabel.setTextColor(0xffffffff);
        scaleRow.addView(scaleLabel);
        scaleSeekBar = new SeekBar(this);
        scaleSeekBar.setMax(SCALE_SEEK_MAX);
        scaleSeekBar.setProgress(progressForScale(selectedScale));
        scaleSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                selectedScale = scaleForProgress(progress);
                updateControls();
                scheduleScaleApply();
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {
                scheduleScaleApply();
            }
        });
        LinearLayout.LayoutParams seekParams = new LinearLayout.LayoutParams(
                0, LinearLayout.LayoutParams.WRAP_CONTENT, 1.0f);
        seekParams.leftMargin = 12;
        scaleRow.addView(scaleSeekBar, seekParams);
        controls.addView(scaleRow);

        statusText = new TextView(this);
        statusText.setTextColor(0xffffffff);
        statusText.setPadding(0, 10, 0, 0);
        controls.addView(statusText);
        updateControls();

        FrameLayout.LayoutParams controlsParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP);
        root.addView(controls, controlsParams);
        setContentView(root);
    }

    private TextView buildModeButton(String text, boolean selected, Runnable onClick) {
        TextView btn = new TextView(this);
        btn.setText(text);
        btn.setPadding(24, 12, 24, 12);
        btn.setTextSize(14f);
        btn.setOnClickListener(v -> onClick.run());
        btn.setBackgroundColor(selected ? 0xffffffff : 0xff444444);
        btn.setTextColor(selected ? 0xff000000 : 0xffffffff);
        return btn;
    }

    private void selectMode(int mode) {
        if (selectedMode == mode) return;
        selectedMode = mode;
        updateControls();
        restartEngine();
    }

    private void updateControls() {
        if (scaleLabel != null) {
            scaleLabel.setText("scale=x" + formatScale(selectedScale));
        }
        if (modeHighBtn != null && modeSpeedBtn != null) {
            boolean high = selectedMode == MODE_HIGH_SPEED;
            modeHighBtn.setBackgroundColor(high ? 0xffffffff : 0xff444444);
            modeHighBtn.setTextColor(high ? 0xff000000 : 0xffffffff);
            modeSpeedBtn.setBackgroundColor(high ? 0xff444444 : 0xffffffff);
            modeSpeedBtn.setTextColor(high ? 0xffffffff : 0xff000000);
        }
    }

    private void scheduleScaleApply() {
        uiHandler.removeCallbacks(scaleApplyRunnable);
        uiHandler.postDelayed(scaleApplyRunnable, SCALE_APPLY_THROTTLE_MS);
    }

    private void startCamera() {
        running = true;
        ListenableFuture<ProcessCameraProvider> providerFuture = ProcessCameraProvider.getInstance(this);
        providerFuture.addListener(() -> {
            try {
                cameraProvider = providerFuture.get();
                bindAnalyzer();
            } catch (Exception e) {
                stopWithError("camera init failed: " + e.getMessage());
            }
        }, ContextCompat.getMainExecutor(this));
    }

    private void stopCamera() {
        running = false;
        if (cameraProvider != null) {
            cameraProvider.unbindAll();
        }
    }

    private void bindAnalyzer() {
        ImageAnalysis analysis = new ImageAnalysis.Builder()
                .setTargetResolution(new Size(INPUT_WIDTH, INPUT_HEIGHT))
                .setBackpressureStrategy(ImageAnalysis.STRATEGY_KEEP_ONLY_LATEST)
                .build();
        analysis.setAnalyzer(cameraExecutor, this::analyzeFrame);
        cameraProvider.unbindAll();
        cameraProvider.bindToLifecycle(this, CameraSelector.DEFAULT_BACK_CAMERA, analysis);
        statusText.setText("camera ready");
    }

    private void analyzeFrame(@NonNull ImageProxy image) {
        if (!running) {
            image.close();
            return;
        }
        if (!processingFrame.compareAndSet(false, true)) {
            image.close();
            return;
        }
        try {
            int inW = image.getWidth();
            int inH = image.getHeight();
            int outW = Math.max(1, Math.round(inW * selectedScale));
            int outH = Math.max(1, Math.round(inH * selectedScale));
            ensureBuffers(inW, inH, outW, outH);
            yuv420ToRgba(image, rgbaInput);
            ensureEngine(inW, inH);
            int ret = SuperResolutionLib.processImage(rgbaInput, rgbaOutput, 4, inW, inH, outW, outH);
            if (ret != 0) {
                throw new IllegalStateException("MC_Process failed: " + ret);
            }
            Bitmap bitmap = rgbaToBitmap(rgbaOutput, outW, outH);
            runOnUiThread(() -> {
                outputView.setImageBitmap(bitmap);
                statusText.setText("mode=" + modeName() + " scale=x" + formatScale(selectedScale)
                        + " in=" + inW + "x" + inH + " out=" + outW + "x" + outH);
            });
        } catch (Exception e) {
            runOnUiThread(() -> stopWithError(e.getMessage()));
        } finally {
            image.close();
            processingFrame.set(false);
        }
    }

    private void ensureBuffers(int inW, int inH, int outW, int outH) {
        int inBytes = inW * inH * 4;
        int outBytes = outW * outH * 4;
        if (rgbaInput == null || rgbaInput.length != inBytes) rgbaInput = new byte[inBytes];
        if (rgbaOutput == null || rgbaOutput.length != outBytes) rgbaOutput = new byte[outBytes];
    }

    private void ensureEngine(int width, int height) {
        synchronized (engineLock) {
            String modelPath = modelPathForCurrentMode();
            boolean stale = srHandle == 0
                    || engineInitWidth != width
                    || engineInitHeight != height
                    || !modelPath.equals(currentModelPath);
            if (!stale) return;
            SuperResolutionLib.uninitSuperResolution();
            srHandle = SuperResolutionLib.initSuperResolution(
                    width,
                    height,
                    selectedScale,
                    selectedMode,
                    1,
                    BACKEND_OPENGLES,
                    modelPath);
            if (srHandle == 0) {
                throw new IllegalStateException("MC_Init failed, model=" + modelPath);
            }
            engineInitWidth = width;
            engineInitHeight = height;
            currentModelPath = modelPath;
        }
    }

    private void restartEngine() {
        synchronized (engineLock) {
            SuperResolutionLib.uninitSuperResolution();
            srHandle = 0;
            engineInitWidth = -1;
            engineInitHeight = -1;
            currentModelPath = "";
        }
    }

    private void ensureModelsInSandbox() {
        File modelDir = new File(getFilesDir(), "models");
        if (!modelDir.exists() && !modelDir.mkdirs()) {
            throw new RuntimeException("cannot create model dir: " + modelDir.getAbsolutePath());
        }
        copyModelIfMissing(modelDir, "magic_gles_highspeed_gpu_params.bin");
        copyModelIfMissing(modelDir, "magic_gles_speed_gpu_params.bin");
    }

    private void copyModelIfMissing(File modelDir, String fileName) {
        File out = new File(modelDir, fileName);
        if (out.exists() && out.length() > 0) return;
        try (InputStream in = getAssets().open("model/" + fileName);
             OutputStream os = new FileOutputStream(out, false)) {
            byte[] buf = new byte[64 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) {
                os.write(buf, 0, n);
            }
            os.flush();
        } catch (Exception e) {
            throw new RuntimeException("copy model failed: " + fileName + " err=" + e.getMessage(), e);
        }
        if (!out.exists() || out.length() <= 0) {
            throw new RuntimeException("model invalid after copy: " + out.getAbsolutePath());
        }
    }

    private String modelPathForCurrentMode() {
        String name = selectedMode == MODE_SPEED
                ? "magic_gles_speed_gpu_params.bin"
                : "magic_gles_highspeed_gpu_params.bin";
        File model = new File(new File(getFilesDir(), "models"), name);
        if (!model.exists() || model.length() <= 0) {
            throw new IllegalStateException("model missing: " + model.getAbsolutePath());
        }
        return model.getAbsolutePath();
    }

    private String modeName() {
        return selectedMode == MODE_SPEED ? "speed" : "highspeed";
    }

    private static String formatScale(float scale) {
        return String.format(Locale.US, "%.2f", scale).replaceAll("0+$", "").replaceAll("\\.$", "");
    }

    private static int progressForScale(float scale) {
        float clamped = Math.max(MIN_SCALE, Math.min(MAX_SCALE, scale));
        float unit = (clamped - MIN_SCALE) / (MAX_SCALE - MIN_SCALE);
        return Math.round(unit * SCALE_SEEK_MAX);
    }

    private static float scaleForProgress(int progress) {
        int bounded = Math.max(0, Math.min(SCALE_SEEK_MAX, progress));
        float unit = (float) bounded / (float) SCALE_SEEK_MAX;
        return MIN_SCALE + unit * (MAX_SCALE - MIN_SCALE);
    }

    private void stopWithError(String message) {
        stopCamera();
        restartEngine();
        Log.e(TAG, "stopWithError: " + message);
        if (statusText != null) {
            statusText.setText("error: " + message);
        }
    }

    private static Bitmap rgbaToBitmap(byte[] rgba, int width, int height) {
        int[] pixels = new int[width * height];
        int p = 0;
        for (int i = 0; i < pixels.length; i++) {
            int r = rgba[p] & 0xff;
            int g = rgba[p + 1] & 0xff;
            int b = rgba[p + 2] & 0xff;
            int a = rgba[p + 3] & 0xff;
            pixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
            p += 4;
        }
        return Bitmap.createBitmap(pixels, width, height, Bitmap.Config.ARGB_8888);
    }

    private static void yuv420ToRgba(ImageProxy image, byte[] rgbaOut) {
        ImageProxy.PlaneProxy yPlane = image.getPlanes()[0];
        ImageProxy.PlaneProxy uPlane = image.getPlanes()[1];
        ImageProxy.PlaneProxy vPlane = image.getPlanes()[2];
        int width = image.getWidth();
        int height = image.getHeight();
        byte[] yData = new byte[yPlane.getBuffer().remaining()];
        byte[] uData = new byte[uPlane.getBuffer().remaining()];
        byte[] vData = new byte[vPlane.getBuffer().remaining()];
        yPlane.getBuffer().get(yData);
        uPlane.getBuffer().get(uData);
        vPlane.getBuffer().get(vData);
        int yRowStride = yPlane.getRowStride();
        int uRowStride = uPlane.getRowStride();
        int vRowStride = vPlane.getRowStride();
        int uPixelStride = uPlane.getPixelStride();
        int vPixelStride = vPlane.getPixelStride();
        int out = 0;
        for (int j = 0; j < height; j++) {
            int yRow = j * yRowStride;
            int uvRow = (j / 2);
            for (int i = 0; i < width; i++) {
                int y = yData[yRow + i] & 0xff;
                int uvCol = (i / 2);
                int u = uData[uvRow * uRowStride + uvCol * uPixelStride] & 0xff;
                int v = vData[uvRow * vRowStride + uvCol * vPixelStride] & 0xff;
                int c = y - 16;
                int d = u - 128;
                int e = v - 128;
                int r = (298 * c + 409 * e + 128) >> 8;
                int g = (298 * c - 100 * d - 208 * e + 128) >> 8;
                int b = (298 * c + 516 * d + 128) >> 8;
                rgbaOut[out++] = (byte) clamp255(r);
                rgbaOut[out++] = (byte) clamp255(g);
                rgbaOut[out++] = (byte) clamp255(b);
                rgbaOut[out++] = (byte) 255;
            }
        }
    }

    private static int clamp255(int value) {
        if (value < 0) return 0;
        return Math.min(value, 255);
    }
}
