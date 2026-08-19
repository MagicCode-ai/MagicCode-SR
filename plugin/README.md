# MagicSR Engine Plugins

- `unity/`: Unity plugin (native + C#)
- `ue/UE_<version>/<Platform>/MagicSR/`: Unreal Engine plugin (copy the matching engine version)

**User manual:** [`../doc/User_Guide.md`](../doc/User_Guide.md) (English) · [`../doc/用户使用说明书.md`](../doc/用户使用说明书.md) (中文)

Core dependencies:
- `interface/mc_interface.h` (`MC_Init` / `MC_Process` / …)
- `interface/mc_enable.h` (`MC_Enable` / `MC_Disable`)
- `lib/android/libmagic_sr_enable.a` and `lib/ios/libmagic_sr_enable.a`

Per-engine English guides:
- `unity/USAGE_GUIDE.md`
- `ue/USAGE_GUIDE.md`
