package com.example.superresolution.natives;

public final class SuperResolutionLib {
    static {
        System.loadLibrary("superresolution");
    }

    private SuperResolutionLib() {}

    public static native long initSuperResolution(int width, int height, float scalerFactor,
                                                  int srMode, int srOnGpu, int backend,
                                                  String modelPath);

    public static native int processImage(byte[] input, byte[] output, int channelNum,
                                          int inputWidth, int inputHeight,
                                          int outputWidth, int outputHeight);

    public static native void uninitSuperResolution();

    public static native String getVersion();
}
