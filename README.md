# MagicCode AI Super-Resolution: Near 4K@60fps Speed on iPhone 14 Pro, 8K Perfection

We are pleased to announce the release of MagicCode SR v1.0.0. Leveraging the proprietary ECBOG algorithm independently developed by MagicCode, our AI super-resolution technology has achieved a breakthrough in the speed-to-quality ratio—its processing efficiency ranks among the global top tier. Specifically, the VeryFast mode supports a processing speed of nearly 4K@60fps on iPhone 14 Pro, establishing itself as the fastest AI super-resolution solutions available today.

We invite you to download and try it out, or visit our official website:  
[www.magiccode-ai.com](https://www.magiccode-ai.com)

Before using the SDK, please read our Terms of Service(doc/MagicCode Super-Resolution Software End User License Agreement (EULA).pdf) and Privacy Policy(MagicCode Super-Resolution Software Privacy Policy.pdf).

## Repository Structure

### `demo`
- Purpose: Provides a complete example of integrating and using the MagicCode super-resolution library
- Contains: Sample code, test assets, and step-by-step usage instructions

### `doc` (Documentation)
- `MagicCode Super-Resolution Software End User License Agreement (EULA).pdf`: Legal terms for software use
- `MagicCode Super-Resolution Software Error Code Manual.pdf`: Detailed explanation of error codes and troubleshooting steps
- `MagicCode Super-Resolution Software Privacy Policy.pdf`: Data processing and privacy protection rules
- `MagicCode Super-Resolution Software Specification.pdf`: Technical details (e.g., algorithm principles, performance indicators)

### `header` (Header Files)
- `mc_interface.h`: Core header file of the library, including API declarations, data structure definitions, and function prototypes for integration

### `lib` (Platform-Specific Libraries)
- `android`: Precompiled super-resolution library for Android (supports ARM64 architectures)
- `iOS`: Precompiled super-resolution library for iOS (supports iPhone/iPad devices)
- `macos_arm`: Precompiled super-resolution library for Mac with Apple Silicon (M1/M2/M3/M4 chips)
- `macos_x86`: Precompiled super-resolution library for x86_64-based Mac systems
- `windows`: Precompiled super-resolution library for x86_64-based Windows systems (Windows 10/11)

### `model` (Model Parameters)
- Purpose: Stores pre-trained super-resolution model files
- Feature: Optimized for edge devices (low latency, small memory footprint)

