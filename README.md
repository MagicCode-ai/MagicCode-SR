# MagicSR: MagicCode AI Super-Resolution

MagicSR is MagicCode's AI super-resolution solution for video/image enhancement on mobile platforms.

MagicCode developed a fast convolution method called **PACA**, which accelerates convolution computation by dozens of times without sacrificing numerical accuracy. Based on this method, we designed MagicSR to provide both strong visual quality and production-ready performance.

## MagicSR Performance

To evaluate MagicSR, we compared it with benchmark mobile super-resolution solutions including Apple MetalFX and Qualcomm SGSR (2x SR).

### iOS / iPadOS

- Image quality: `MagicSR-Speed > MagicSR-HighSpeed >= MetalFX-Temporal > MetalFX-Spatial`
- Processing speed: `MagicSR-HighSpeed > MagicSR-Speed > MetalFX-Spatial > MetalFX-Temporal`

### Android

- Image quality: `MagicSR-Speed > MagicSR-HighSpeed > SGSR2.0 > SGSR1.0`
- Processing speed: `MagicSR-HighSpeed ≈ SGSR1.0 > MagicSR-Speed > SGSR2.0`

## 2x Super-Resolution Benchmark Focus

- iOS speed comparison (A16 Pro platform, metric in ms)
- Android Vulkan speed comparison (Snapdragon 888 platform, metric in ms)
- Cross-method image quality comparison to inspect edge clarity, texture detail, and artifacts

## Test Conclusions

### iOS Conclusion

On A16 Pro, compared with Apple MetalFX, MagicSR is significantly faster while delivering comparable or better output image quality.

- Speed mode is about `1.82x` faster than MetalFX-Spatial
- Speed mode is about `3.19x` faster than MetalFX-Temporal

### Android Conclusion

On Snapdragon 888, compared with Qualcomm SGSR:

- MagicSR HighSpeed (`~4.581 ms`) is roughly comparable to SGSR1.0 (`~4.678 ms`) in speed
- MagicSR HighSpeed and MagicSR Speed provide better image quality than SGSR1.0 and SGSR2.0

## Productization Support

MagicSR is product-ready and supports mainstream mobile GPU backends:

- Metal
- Vulkan
- OpenGLES

It also provides integration support for:

- Unity plugin
- Unreal Engine plugin

## Product Download

Download the AI Super-Resolution SDK to evaluate visual quality enhancement and runtime performance in your product pipeline.

Before downloading and using the SDK, please review the Terms of Service and Privacy Policy on the official website.

## Contact

For customized AI super-resolution solutions and business cooperation, please contact MagicCode:

- Website: <http://localhost:5173/products/video-super-resolution>

---

© 2024 MagicCode Technology Limited. All rights reserved.
