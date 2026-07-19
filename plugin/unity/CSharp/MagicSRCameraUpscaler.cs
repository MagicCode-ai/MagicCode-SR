using System;
using UnityEngine;

namespace MagicSR.UnityPlugin
{
    /// <summary>
    /// Built-in pipeline sample: camera color → generic <see cref="MagicSR.Enable"/>.
    /// URP/HDRP: blit camera color to a RenderTexture, then call the same Enable API.
    /// </summary>
    [DisallowMultipleComponent]
    [RequireComponent(typeof(Camera))]
    public class MagicSRCameraUpscaler : MonoBehaviour
    {
        [Range(1f, 8f)] public float scale = 2f;

        Texture2D _externalOutput;

        void OnRenderImage(RenderTexture src, RenderTexture dest)
        {
            if (src == null || dest == null)
            {
                return;
            }

            IntPtr nativeOut = MagicSR.Enable(src, scale);
            if (nativeOut == IntPtr.Zero)
            {
                Graphics.Blit(src, dest);
                return;
            }

            float resolved = scale <= 0f ? 2f : Mathf.Clamp(scale, 1f, 8f);
            int outW = Mathf.Max(1, Mathf.RoundToInt(src.width * resolved));
            int outH = Mathf.Max(1, Mathf.RoundToInt(src.height * resolved));

            if (_externalOutput == null || _externalOutput.width != outW || _externalOutput.height != outH)
            {
                if (_externalOutput != null)
                {
                    Destroy(_externalOutput);
                }

                _externalOutput = Texture2D.CreateExternalTexture(
                    outW,
                    outH,
                    TextureFormat.RGBA32,
                    false,
                    false,
                    nativeOut);
            }
            else
            {
                _externalOutput.UpdateExternalTexture(nativeOut);
            }

            Graphics.Blit(_externalOutput, dest);
        }

        void OnDisable()
        {
            MagicSR.Disable();
            if (_externalOutput != null)
            {
                Destroy(_externalOutput);
                _externalOutput = null;
            }
        }
    }
}
