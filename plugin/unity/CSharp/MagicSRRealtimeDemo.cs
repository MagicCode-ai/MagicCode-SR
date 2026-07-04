using System;
using UnityEngine;
using UnityEngine.UI;

namespace MagicSR.UnityPlugin
{
    public class MagicSRRealtimeDemo : MonoBehaviour
    {
        [Header("Input")]
        public Texture2D sourceTexture;
        public string modelPath;
        [Range(1, 8)] public int scale = 2;
        public MagicSRAlgMode mode = MagicSRAlgMode.HighSpeed;
        public int numThreads = 1;

        [Header("Output")]
        public RawImage outputView;
        public bool runEveryFrame = true;

        private MagicSRSession _session;
        private Texture2D _outputTexture;
        private byte[] _inputY;
        private byte[] _outputY;
        private int _outputWidth;
        private int _outputHeight;

        private void Start()
        {
            if (sourceTexture == null || string.IsNullOrWhiteSpace(modelPath))
            {
                Debug.LogError("[MagicSR] sourceTexture/modelPath is missing.");
                enabled = false;
                return;
            }

            _session = new MagicSRSession();
            if (!_session.Create(modelPath, sourceTexture.width, sourceTexture.height, scale, mode, numThreads))
            {
                Debug.LogError("[MagicSR] Create failed.");
                enabled = false;
                return;
            }

            if (!QueryStatus(out MagicSRStatus status))
            {
                Debug.LogError("[MagicSR] status query failed after Create.");
                enabled = false;
                return;
            }

            _outputWidth = (int)status.OutputWidth;
            _outputHeight = (int)status.OutputHeight;
            _inputY = new byte[sourceTexture.width * sourceTexture.height];
            _outputY = new byte[_outputWidth * _outputHeight];
            _outputTexture = new Texture2D(_outputWidth, _outputHeight, TextureFormat.Alpha8, false);

            if (outputView != null)
            {
                outputView.texture = _outputTexture;
            }

            Debug.Log($"[MagicSR] Version={MagicSRSession.GetVersion()}, output={_outputWidth}x{_outputHeight}");
            RunOneFrame();
        }

        private void Update()
        {
            if (runEveryFrame)
            {
                RunOneFrame();
            }
        }

        private void RunOneFrame()
        {
            FillInputLuma(sourceTexture, _inputY);
            int ret = _session.Process(_inputY, _outputY);
            if (ret != 0)
            {
                Debug.LogError($"[MagicSR] Process failed: {ret}");
                return;
            }

            _outputTexture.LoadRawTextureData(_outputY);
            _outputTexture.Apply(false);

            if (QueryStatus(out MagicSRStatus status))
            {
                Debug.Log($"[MagicSR] gpu_time={status.GpuTime:F3}ms err=0x{status.ErrorCode:X8}");
            }
        }

        private bool QueryStatus(out MagicSRStatus status)
        {
            MagicSRControlParam ignored = default;
            return _session.SetParam(MagicSRControlCommand.QueryStatus, ref ignored, out status) == 0;
        }

        private static void FillInputLuma(Texture2D texture, byte[] luma)
        {
            Color32[] pixels = texture.GetPixels32();
            for (int i = 0; i < pixels.Length; i++)
            {
                Color32 c = pixels[i];
                luma[i] = (byte)((c.r * 77 + c.g * 150 + c.b * 29) >> 8);
            }
        }

        private void OnDestroy()
        {
            _session?.Destroy();
            _session = null;
        }
    }
}
