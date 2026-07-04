# MagicSR: MagicCode AI Super-Resolution

MagicSR is MagicCode's AI super-resolution solution for mobile video and image enhancement.

MagicCode developed a fast convolution method called **PACA**, which accelerates convolution computation by dozens of times without sacrificing numerical accuracy. Based on this method, we designed MagicSR to provide strong visual quality with production-ready speed.

Official page: [https://www.magiccode-ai.com/products/video-super-resolution](https://www.magiccode-ai.com/products/video-super-resolution)

> Note: the comparison videos in the "MagicSR Overview" section are intentionally omitted here.

## MagicSR Performance

To evaluate MagicSR, we compared it with benchmark mobile SR solutions including Apple MetalFX and Qualcomm SGSR (2x super-resolution).

### iOS / iPadOS

- Image quality: `MagicSR-Speed > MagicSR-HighSpeed >= MetalFX-Temporal > MetalFX-Spatial`
- Processing speed: `MagicSR-HighSpeed > MagicSR-Speed > MetalFX-Spatial > MetalFX-Temporal`

### Android

- Image quality: `MagicSR-Speed > MagicSR-HighSpeed > SGSR2.0 > SGSR1.0`
- Processing speed: `MagicSR-HighSpeed ≈ SGSR1.0 > MagicSR-Speed > SGSR2.0`

## 2x Super-Resolution Speed Data

### iOS Super-Resolution Speed Comparison (A16 Pro, ms)

| Method | Time (ms) |
| --- | ---: |
| MagicSR HighSpeed | 3.47 |
| MagicSR Speed | 4.83 |
| MetalFX Spatial | 8.82 |
| MetalFX Temporal | 15.28 |

### Android Vulkan Super-Resolution Speed Comparison (Snapdragon 888, ms)

| Method | Time (ms) |
| --- | ---: |
| SGSR1.0 | 4.678 |
| MagicSR HighSpeed | 4.581 |
| MagicSR Speed | 9.91 |
| SGSR2.0 | 13.47 |

## Image Comparison Samples

### Comparison 1

![SGSR2.0 Comparison 1](https://www.magiccode-ai.com/images/sr-report/sgsr2.0-1.jpg)
![MagicSR HighSpeed Comparison 1](https://www.magiccode-ai.com/images/sr-report/quality-1-highspeed.jpg)
![MagicSR Speed Comparison 1](https://www.magiccode-ai.com/images/sr-report/quality-1-speed.jpg)
![MetalFX Temporal Comparison 1](https://www.magiccode-ai.com/images/sr-report/metalfx-tsr-1.jpg)
![MetalFX Spatial Comparison 1](https://www.magiccode-ai.com/images/sr-report/quality-1-metalfx.jpg)

### Comparison 2

![SGSR2.0 Comparison 2](https://www.magiccode-ai.com/images/sr-report/sgsr2.0-2.jpg)
![MagicSR HighSpeed Comparison 2](https://www.magiccode-ai.com/images/sr-report/quality-2-highspeed.jpg)
![MagicSR Speed Comparison 2](https://www.magiccode-ai.com/images/sr-report/quality-2-speed.jpg)
![MetalFX Temporal Comparison 2](https://www.magiccode-ai.com/images/sr-report/metalfx-tsr-2.jpg)
![MetalFX Spatial Comparison 2](https://www.magiccode-ai.com/images/sr-report/quality-2-metalfx.jpg)

### Comparison 3

![SGSR1.0 Comparison 3](https://www.magiccode-ai.com/images/sr-report/sgsr1.0-4.jpg)
![SGSR2.0 Comparison 3](https://www.magiccode-ai.com/images/sr-report/sgsr2.0-4.jpg)
![MagicSR HighSpeed Comparison 3](https://www.magiccode-ai.com/images/sr-report/highspeed-4.jpg)
![MagicSR Speed Comparison 3](https://www.magiccode-ai.com/images/sr-report/speed-4.jpg)
![MetalFX Spatial Comparison 3](https://www.magiccode-ai.com/images/sr-report/metalfx-4.jpg)
![MetalFX Temporal Comparison 3](https://www.magiccode-ai.com/images/sr-report/metalfx-tsr-4.jpg)

## Productization Support

MagicSR supports mainstream mobile GPU backends:

- Metal
- Vulkan
- OpenGLES

And provides integration support for:

- Unity plugin
- Unreal Engine plugin

## Contact

For customized AI super-resolution solutions and business cooperation:

- Website: [https://www.magiccode-ai.com/](https://www.magiccode-ai.com/)

---

© 2024 MagicCode Technology Limited. All rights reserved.
