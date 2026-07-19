using System;
using UnityEngine;

namespace MagicSR.UnityPlugin
{
    /// <summary>
    /// Public entry: generic GPU texture Enable / Disable (any source).
    /// Camera / URP / HDRP callers should obtain a Texture/RT and call Enable the same way.
    /// </summary>
    public static class MagicSR
    {
        static readonly MagicSRSession Shared = new MagicSRSession();

        /// <summary>
        /// Optional: set model directory for MC_Enable (contains magic_veryfast_*_params.bin).
        /// </summary>
        public static void SetModelDir(string modelDir)
        {
            MagicSRSession.SetModelDir(modelDir);
        }

        /// <summary>
        /// Optional size hint before Enable* (needed for Android Vulkan).
        /// </summary>
        public static void SetInputSizeHint(int width, int height)
        {
            MagicSRSession.SetInputSizeHint(width, height);
        }

        /// <summary>
        /// Process one SR frame from a native GPU texture (Metal / GLES / Vulkan handle).
        /// Wraps <c>MC_Enable</c>: session is created lazily and reused when scale/size/mode/backend are unchanged.
        /// Input must be RGBA8Unorm (Metal/GLES) or RGB8Unorm (Vulkan).
        /// Returns a borrowed output texture pointer owned until <see cref="Disable"/>.
        /// Do not FreeHGlobal / CFRelease / glDelete the returned pointer.
        /// scale in [1,8]; &lt;=0 =&gt; 2.0f.
        /// </summary>
        public static IntPtr Enable(IntPtr inputNativeTexture, float scale)
        {
            if (inputNativeTexture == IntPtr.Zero)
            {
                Debug.LogError("[MagicSR] Enable failed: inputNativeTexture is null.");
                return IntPtr.Zero;
            }

            return Shared.Enable(inputNativeTexture, scale);
        }

        public static IntPtr Enable_3params(IntPtr inputNativeTexture, float scale, MagicSRAlgMode mode)
        {
            if (inputNativeTexture == IntPtr.Zero)
            {
                Debug.LogError("[MagicSR] Enable_3params failed: inputNativeTexture is null.");
                return IntPtr.Zero;
            }

            return Shared.Enable_3params(inputNativeTexture, scale, mode);
        }

        public static IntPtr Enable_4params(IntPtr inputNativeTexture, float scale, MagicSRAlgMode mode, MagicSRBackend backend)
        {
            if (inputNativeTexture == IntPtr.Zero)
            {
                Debug.LogError("[MagicSR] Enable_4params failed: inputNativeTexture is null.");
                return IntPtr.Zero;
            }

            return Shared.Enable_4params(inputNativeTexture, scale, mode, backend);
        }

        /// <summary>
        /// Process one SR frame from a Unity Texture / RenderTexture.
        /// Equivalent to Enable(input.GetNativeTexturePtr(), scale).
        /// </summary>
        public static IntPtr Enable(Texture inputTexture, float scale)
        {
            if (inputTexture == null)
            {
                Debug.LogError("[MagicSR] Enable failed: inputTexture is null.");
                return IntPtr.Zero;
            }

            SetInputSizeHint(inputTexture.width, inputTexture.height);
            return Enable(inputTexture.GetNativeTexturePtr(), scale);
        }

        public static IntPtr Enable_3params(Texture inputTexture, float scale, MagicSRAlgMode mode)
        {
            if (inputTexture == null)
            {
                Debug.LogError("[MagicSR] Enable_3params failed: inputTexture is null.");
                return IntPtr.Zero;
            }

            SetInputSizeHint(inputTexture.width, inputTexture.height);
            return Enable_3params(inputTexture.GetNativeTexturePtr(), scale, mode);
        }

        public static IntPtr Enable_4params(Texture inputTexture, float scale, MagicSRAlgMode mode, MagicSRBackend backend)
        {
            if (inputTexture == null)
            {
                Debug.LogError("[MagicSR] Enable_4params failed: inputTexture is null.");
                return IntPtr.Zero;
            }

            SetInputSizeHint(inputTexture.width, inputTexture.height);
            return Enable_4params(inputTexture.GetNativeTexturePtr(), scale, mode, backend);
        }

        /// <summary>
        /// Tear down the MC_Enable singleton and its output texture.
        /// Only valid release for pointers returned by Enable*.
        /// </summary>
        public static void Disable()
        {
            Shared.Disable();
        }
    }
}
