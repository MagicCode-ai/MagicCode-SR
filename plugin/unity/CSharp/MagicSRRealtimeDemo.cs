using System;
using UnityEngine;
using UnityEngine.Experimental.Rendering;
using UnityEngine.UI;

namespace MagicSR.UnityPlugin
{
    /// <summary>
    /// Preferred integration path: GPU Texture in/out via ProcessTexture.
    /// Buffer/Y8 remains available as an explicit fallback for CPU backends.
    /// </summary>
    public class MagicSRRealtimeDemo : MonoBehaviour
    {
        public enum DemoPath
        {
            TexturePreferred = 0,
            BufferY8Fallback = 1
        }

        [Header("Input")]
        public Texture sourceTexture;
        public string modelPath;
        [Range(1f, 8f)] public float scale = 2f;
        public MagicSRAlgMode mode = MagicSRAlgMode.HighSpeed;
        public int numThreads = 1;
        public DemoPath path = DemoPath.TexturePreferred;

        [Header("Texture Path Options")]
        [Tooltip("Android: Vulkan uses RGB8 textures; OpenGLES uses R8. iOS Metal uses R8.")]
        public MagicSRBackend androidBackend = MagicSRBackend.Vulkan;

        [Header("Output")]
        public RawImage outputView;
        public bool runEveryFrame = true;

        private MagicSRSession _session;
        private Texture _gpuInputTexture;
        private RenderTexture _outputTexture;
        private Texture2D _bufferOutputTexture;
        private byte[] _inputY;
        private byte[] _outputY;
        private int _outputWidth;
        private int _outputHeight;
        private bool _ownsGpuInputTexture;

        private void Start()
        {
            if (sourceTexture == null || string.IsNullOrWhiteSpace(modelPath))
            {
                Debug.LogError("[MagicSR] sourceTexture/modelPath is missing.");
                enabled = false;
                return;
            }

            _session = new MagicSRSession();
            bool created = path == DemoPath.TexturePreferred
                ? CreateTextureSession()
                : CreateBufferSession();

            if (!created)
            {
                Debug.LogError("[MagicSR] Create failed. Check model/backend/inputType match.");
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

            if (path == DemoPath.TexturePreferred)
            {
                _outputTexture = CreateOutputRenderTexture(_outputWidth, _outputHeight, status.InputType);
                if (_outputTexture == null)
                {
                    Debug.LogError("[MagicSR] Failed to create output RenderTexture.");
                    enabled = false;
                    return;
                }

                if (outputView != null)
                {
                    outputView.texture = _outputTexture;
                }
            }
            else
            {
                _inputY = new byte[sourceTexture.width * sourceTexture.height];
                _outputY = new byte[_outputWidth * _outputHeight];
                _bufferOutputTexture = new Texture2D(_outputWidth, _outputHeight, TextureFormat.Alpha8, false);
                if (outputView != null)
                {
                    outputView.texture = _bufferOutputTexture;
                }
            }

            Debug.Log(
                $"[MagicSR] Version={MagicSRSession.GetVersion()}, path={path}, " +
                $"output={_outputWidth}x{_outputHeight}, backend={status.Backend}, inputType={status.InputType}");
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
            if (path == DemoPath.TexturePreferred)
            {
                RunTextureFrame();
            }
            else
            {
                RunBufferFrame();
            }
        }

        private void RunTextureFrame()
        {
            if (_gpuInputTexture == null || _outputTexture == null)
            {
                return;
            }

            // Keep GPU input in sync when source is a dynamic texture (e.g. camera/RT).
            if (sourceTexture is RenderTexture || _ownsGpuInputTexture == false)
            {
                // Source already GPU-native; ProcessTexture reads it directly.
            }
            else if (sourceTexture is Texture2D)
            {
                // Static Texture2D was uploaded once in CreateTextureSession.
            }

            int ret = _session.ProcessTexture(
                _gpuInputTexture.GetNativeTexturePtr(),
                _outputTexture.GetNativeTexturePtr());
            if (ret != 0)
            {
                Debug.LogError($"[MagicSR] ProcessTexture failed: {ret}");
                return;
            }

            if (QueryStatus(out MagicSRStatus status))
            {
                Debug.Log($"[MagicSR] ProcessTexture gpu_time={status.GpuTime:F3}ms err=0x{status.ErrorCode:X8}");
            }
        }

        private void RunBufferFrame()
        {
            if (!(sourceTexture is Texture2D tex2D))
            {
                Debug.LogError("[MagicSR] BufferY8Fallback requires Texture2D sourceTexture.");
                return;
            }

            FillInputLuma(tex2D, _inputY);
            int ret = _session.Process(_inputY, _outputY);
            if (ret != 0)
            {
                Debug.LogError($"[MagicSR] Process(Y8) failed: {ret}");
                return;
            }

            _bufferOutputTexture.LoadRawTextureData(_outputY);
            _bufferOutputTexture.Apply(false);

            if (QueryStatus(out MagicSRStatus status))
            {
                Debug.Log($"[MagicSR] Process(Y8) gpu_time={status.GpuTime:F3}ms err=0x{status.ErrorCode:X8}");
            }
        }

        private bool CreateTextureSession()
        {
            ResolveTexturePath(out MagicSRInputType inputType, out MagicSRBackend backend, out TextureFormat uploadFormat);

            if (!PrepareGpuInputTexture(inputType, uploadFormat))
            {
                return false;
            }

            return _session.CreateEx(
                modelPath,
                sourceTexture.width,
                sourceTexture.height,
                scale,
                mode,
                inputType,
                backend,
                numThreads);
        }

        private bool CreateBufferSession()
        {
            return _session.Create(modelPath, sourceTexture.width, sourceTexture.height, scale, mode, numThreads);
        }

        private void ResolveTexturePath(out MagicSRInputType inputType, out MagicSRBackend backend, out TextureFormat uploadFormat)
        {
#if UNITY_IOS && !UNITY_EDITOR
            inputType = MagicSRInputType.TextureR8Unorm;
            backend = MagicSRBackend.Metal;
            uploadFormat = TextureFormat.R8;
#elif UNITY_ANDROID && !UNITY_EDITOR
            if (androidBackend == MagicSRBackend.OpenGLES)
            {
                inputType = MagicSRInputType.TextureR8Unorm;
                backend = MagicSRBackend.OpenGLES;
                uploadFormat = TextureFormat.R8;
            }
            else
            {
                inputType = MagicSRInputType.TextureRgb8Unorm;
                backend = MagicSRBackend.Vulkan;
                uploadFormat = TextureFormat.RGBA32;
            }
#else
            // Editor / desktop preview: keep texture API shape, but creation may fail without mobile libs.
            inputType = MagicSRInputType.TextureRgb8Unorm;
            backend = MagicSRBackend.Default;
            uploadFormat = TextureFormat.RGBA32;
#endif
        }

        private bool PrepareGpuInputTexture(MagicSRInputType inputType, TextureFormat uploadFormat)
        {
            if (sourceTexture is RenderTexture)
            {
                _gpuInputTexture = sourceTexture;
                _ownsGpuInputTexture = false;
                return true;
            }

            if (sourceTexture is Texture2D src2D)
            {
                if (inputType == MagicSRInputType.TextureR8Unorm)
                {
                    var r8 = new Texture2D(src2D.width, src2D.height, TextureFormat.R8, false, true);
                    var luma = new byte[src2D.width * src2D.height];
                    FillInputLuma(src2D, luma);
                    r8.LoadRawTextureData(luma);
                    r8.Apply(false, false);
                    _gpuInputTexture = r8;
                    _ownsGpuInputTexture = true;
                    return true;
                }

                // RGB8Unorm path: reuse source Texture2D directly when possible.
                if (uploadFormat == TextureFormat.RGBA32)
                {
                    _gpuInputTexture = src2D;
                    _ownsGpuInputTexture = false;
                    return true;
                }
            }

            Debug.LogError("[MagicSR] Unsupported sourceTexture type for Texture path.");
            return false;
        }

        private static RenderTexture CreateOutputRenderTexture(int width, int height, uint inputType)
        {
            bool useR8 = inputType == (uint)MagicSRInputType.TextureR8Unorm;
            var desc = new RenderTextureDescriptor(width, height)
            {
                graphicsFormat = useR8 ? GraphicsFormat.R8_UNorm : GraphicsFormat.R8G8B8A8_UNorm,
                depthBufferBits = 0,
                msaaSamples = 1,
                dimension = UnityEngine.Rendering.TextureDimension.Tex2D,
                enableRandomWrite = true,
                useMipMap = false,
                autoGenerateMips = false,
                sRGB = false
            };

            var rt = new RenderTexture(desc)
            {
                name = "MagicSR_Output"
            };
            return rt.Create() ? rt : null;
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

            if (_ownsGpuInputTexture && _gpuInputTexture != null)
            {
                Destroy(_gpuInputTexture);
                _gpuInputTexture = null;
            }

            if (_outputTexture != null)
            {
                _outputTexture.Release();
                Destroy(_outputTexture);
                _outputTexture = null;
            }

            if (_bufferOutputTexture != null)
            {
                Destroy(_bufferOutputTexture);
                _bufferOutputTexture = null;
            }
        }
    }
}
