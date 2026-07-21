using System;
using System.Runtime.InteropServices;

namespace MagicSR.UnityPlugin
{
    public enum MagicSRAlgMode
    {
        HighSpeed = 0,
        Speed = 1,
        UltraSpeed = HighSpeed
    }

    public enum MagicSRInputType
    {
        Buffer = 0,
        TextureRgb8Unorm = 1,
        TextureR8Unorm = 2
    }

    public enum MagicSRBackend
    {
        Default = 0,
        X86 = 1,
        Neon = 2,
        Metal = 3,
        OpenGL = 4,
        OpenGLES = 5,
        Vulkan = 6
    }

    public enum MagicSRControlCommand
    {
        SetParam = 0,
        QueryStatus = 1
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct MagicSRStatus
    {
        public uint Width;
        public uint Height;
        public uint OutputWidth;
        public uint OutputHeight;
        public float ScalerFactor;
        public uint AlgMode;
        public uint InputType;
        public uint Backend;
        public uint NumThreads;
        public double GpuTime;
        public uint ErrorCode;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    public struct MagicSRControlParam
    {
        public uint Width;
        public uint Height;
        public float ScalerFactor;
        public uint AlgMode;

        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 256)]
        public string ModelPath;
    }

    public sealed class MagicSRSession : IDisposable
    {
#if UNITY_IOS && !UNITY_EDITOR
        private const string NativeLibraryName = "__Internal";
#else
        private const string NativeLibraryName = "libmagic_sr_unity.so";
#endif
        private IntPtr _handle = IntPtr.Zero;

        public bool IsCreated => _handle != IntPtr.Zero;

        [DllImport(NativeLibraryName)]
        private static extern IntPtr MagicSR_Create(
            string modelPath,
            int width,
            int height,
            float scalerFactor,
            int algMode,
            int numThreads);

        [DllImport(NativeLibraryName)]
        private static extern IntPtr MagicSR_CreateEx(
            string modelPath,
            int width,
            int height,
            float scalerFactor,
            int algMode,
            int numThreads,
            int inputType,
            int backend);

        [DllImport(NativeLibraryName)]
        private static extern void MagicSR_SetModelDir(string modelDir);

        [DllImport(NativeLibraryName)]
        private static extern void MagicSR_SetModelPath(string modelPath);

        [DllImport(NativeLibraryName)]
        private static extern void MagicSR_SetInputSizeHint(uint width, uint height);

        [DllImport(NativeLibraryName)]
        private static extern IntPtr MagicSR_Enable(IntPtr inputTexture, float scale);

        [DllImport(NativeLibraryName)]
        private static extern IntPtr MagicSR_Enable_3params(IntPtr inputTexture, float scale, int algMode);

        [DllImport(NativeLibraryName)]
        private static extern IntPtr MagicSR_Enable_4params(IntPtr inputTexture, float scale, int algMode, int backend);

        [DllImport(NativeLibraryName)]
        private static extern int MagicSR_Disable(IntPtr handle);

        [DllImport(NativeLibraryName)]
        private static extern int MagicSR_Process(
            IntPtr handle,
            byte[] inputY,
            int inputSize,
            byte[] outputY,
            int outputSize);

        [DllImport(NativeLibraryName)]
        private static extern int MagicSR_ProcessTexture(
            IntPtr handle,
            IntPtr inputTexture,
            IntPtr outputTexture);

        [DllImport(NativeLibraryName)]
        private static extern int MagicSR_SetParam(
            IntPtr handle,
            int cmd,
            ref MagicSRControlParam param,
            IntPtr outStatus);

        [DllImport(NativeLibraryName, EntryPoint = "MagicSR_SetParam")]
        private static extern int MagicSR_GetControlStatus(
            IntPtr handle,
            int cmd,
            IntPtr param,
            out MagicSRStatus status);

        [DllImport(NativeLibraryName)]
        private static extern int MagicSR_Destroy(IntPtr handle);

        [DllImport(NativeLibraryName)]
        private static extern IntPtr MagicSR_GetVersion();

        public static string GetVersion()
        {
            IntPtr ptr = MagicSR_GetVersion();
            return ptr == IntPtr.Zero ? "unknown" : Marshal.PtrToStringAnsi(ptr);
        }

        public bool Create(string modelPath, int width, int height, float scale, MagicSRAlgMode mode, int numThreads = 1)
        {
            Destroy();
            _handle = MagicSR_Create(modelPath, width, height, scale, (int)mode, numThreads);
            return _handle != IntPtr.Zero;
        }

        public bool CreateEx(
            string modelPath,
            int width,
            int height,
            float scale,
            MagicSRAlgMode mode,
            MagicSRInputType inputType,
            MagicSRBackend backend,
            int numThreads = 1)
        {
            Destroy();
            _handle = MagicSR_CreateEx(
                modelPath,
                width,
                height,
                scale,
                (int)mode,
                numThreads,
                (int)inputType,
                (int)backend);
            return _handle != IntPtr.Zero;
        }

        /// <summary>
        /// Tell MC_Enable the directory containing model .bin files.
        /// Call before Enable when models live under StreamingAssets / persistentDataPath.
        /// </summary>
        public static void SetModelDir(string modelDir)
        {
            MagicSR_SetModelDir(modelDir ?? string.Empty);
        }

        /// <summary>
        /// Tell MC_Enable the full path to one model .bin file.
        /// </summary>
        public static void SetModelPath(string modelPath)
        {
            MagicSR_SetModelPath(modelPath ?? string.Empty);
        }

        /// <summary>
        /// Optional size hint before Enable* (required for Android Vulkan size query).
        /// </summary>
        public static void SetInputSizeHint(int width, int height)
        {
            MagicSR_SetInputSizeHint((uint)Math.Max(0, width), (uint)Math.Max(0, height));
        }

        /// <summary>
        /// Process one SR frame via native MC_Enable (session reused when scale/size/mode/backend unchanged).
        /// Returns a borrowed output GPU texture pointer owned by the native library until Disable().
        /// Do not FreeHGlobal / free / CFRelease / glDelete this pointer — release only via Disable().
        /// </summary>
        public IntPtr Enable(IntPtr inputTexture, float scale)
        {
            return MagicSR_Enable(inputTexture, scale);
        }

        public IntPtr Enable_3params(IntPtr inputTexture, float scale, MagicSRAlgMode mode)
        {
            return MagicSR_Enable_3params(inputTexture, scale, (int)mode);
        }

        public IntPtr Enable_4params(IntPtr inputTexture, float scale, MagicSRAlgMode mode, MagicSRBackend backend)
        {
            return MagicSR_Enable_4params(inputTexture, scale, (int)mode, (int)backend);
        }

        /// <summary>
        /// Tear down the MC_Enable singleton session and its output texture.
        /// This is the only valid way to release the pointer returned by Enable*.
        /// </summary>
        public void Disable()
        {
            MagicSR_Disable(IntPtr.Zero);
            _handle = IntPtr.Zero;
        }

        public int Process(byte[] inputY, byte[] outputY)
        {
            if (_handle == IntPtr.Zero)
            {
                return -2001;
            }
            return MagicSR_Process(_handle, inputY, inputY.Length, outputY, outputY.Length);
        }

        public int ProcessTexture(IntPtr inputTexture, IntPtr outputTexture)
        {
            if (_handle == IntPtr.Zero)
            {
                return -2001;
            }
            return MagicSR_ProcessTexture(_handle, inputTexture, outputTexture);
        }

        public int SetParam(string modelPath, int width, int height, float scale, MagicSRAlgMode mode)
        {
            if (_handle == IntPtr.Zero)
            {
                return -2001;
            }

            var param = new MagicSRControlParam
            {
                Width = (uint)width,
                Height = (uint)height,
                ScalerFactor = scale < 1f ? 1f : (scale > 8f ? 8f : scale),
                AlgMode = (uint)mode,
                ModelPath = modelPath ?? string.Empty
            };
            MagicSRStatus ignored;
            return SetParam(MagicSRControlCommand.SetParam, ref param, out ignored);
        }

        public int SetParam(MagicSRControlCommand cmd, ref MagicSRControlParam param, out MagicSRStatus status)
        {
            status = default;
            if (_handle == IntPtr.Zero)
            {
                return -2001;
            }

            if (cmd == MagicSRControlCommand.QueryStatus)
            {
                return MagicSR_GetControlStatus(_handle, (int)cmd, IntPtr.Zero, out status);
            }

            return MagicSR_SetParam(_handle, (int)cmd, ref param, IntPtr.Zero);
        }

        public void Destroy()
        {
            if (_handle != IntPtr.Zero)
            {
                MagicSR_Destroy(_handle);
                _handle = IntPtr.Zero;
            }
        }

        public void Dispose()
        {
            Destroy();
            GC.SuppressFinalize(this);
        }

        ~MagicSRSession()
        {
            Destroy();
        }
    }
}
