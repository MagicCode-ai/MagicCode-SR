using System.IO;
using UnrealBuildTool;

public class MagicSR : ModuleRules
{
    private static bool HasAnyMagicLibrary(string candidateRoot)
    {
        string legacyAndroid = Path.Combine(candidateRoot, "build/android/build/libmagic_sr.a");
        string legacyIOS = Path.Combine(candidateRoot, "build/ipad/magic_sr/Release-iphoneos/libmagic_sr.a");
        string releaseAndroid = Path.GetFullPath(Path.Combine(candidateRoot, "../release/v1.1.0/lib/android/libmagic_sr.a"));
        string releaseIOS = Path.GetFullPath(Path.Combine(candidateRoot, "../release/v1.1.0/lib/ios/libmagic_sr.a"));
        string releaseMac = Path.GetFullPath(Path.Combine(candidateRoot, "../release/v1.1.0/lib/mac_arm/libmagic_sr.a"));
        string releaseWin = Path.GetFullPath(Path.Combine(candidateRoot, "../release/v1.1.0/lib/windows/libmagic_sr.lib"));
        return File.Exists(legacyAndroid) || File.Exists(legacyIOS) || File.Exists(releaseAndroid) ||
               File.Exists(releaseIOS) || File.Exists(releaseMac) || File.Exists(releaseWin);
    }

    private static string ResolveMagicLibrary(string projectRoot, params string[] relativeCandidates)
    {
        foreach (string relativePath in relativeCandidates)
        {
            string path = Path.GetFullPath(Path.Combine(projectRoot, relativePath));
            if (File.Exists(path))
            {
                return path;
            }
        }

        throw new System.Exception("Unable to locate MagicSR native library from project root " + projectRoot);
    }

    private static string FindMagicProjectRoot(string startDirectory)
    {
        DirectoryInfo directory = new DirectoryInfo(startDirectory);
        while (directory != null)
        {
            string candidate = directory.FullName;
            if (File.Exists(Path.Combine(candidate, "interface/mc_interface.h")) &&
                HasAnyMagicLibrary(candidate))
            {
                return candidate;
            }
            if (File.Exists(Path.Combine(candidate, "src/mc_interface.h")) &&
                HasAnyMagicLibrary(candidate))
            {
                return candidate;
            }

            directory = directory.Parent;
        }

        throw new System.Exception("Unable to locate MagicSR project root from " + startDirectory);
    }

    public MagicSR(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "RHI",
                "RenderCore",
                "Renderer"
            });

        string projectRoot = FindMagicProjectRoot(ModuleDirectory);
        string magicInterfaceDir = Path.Combine(projectRoot, "interface");
        string magicIncludeDir = Path.Combine(projectRoot, "src");

        if (Directory.Exists(magicInterfaceDir))
        {
            PublicIncludePaths.Add(magicInterfaceDir);
        }
        PublicIncludePaths.Add(magicIncludeDir);

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            string magicAndroidStaticLib = ResolveMagicLibrary(
                projectRoot,
                "build/android/build/libmagic_sr.a",
                "../release/v1.1.0/lib/android/libmagic_sr.a");
            PublicAdditionalLibraries.Add(magicAndroidStaticLib);
            PublicSystemLibraries.Add("log");
            PublicSystemLibraries.Add("android");
            PublicSystemLibraries.Add("GLESv3");
            PublicSystemLibraries.Add("EGL");
            PublicSystemLibraries.Add("vulkan");
            PublicSystemLibraries.Add("z");
            PublicDefinitions.Add("MAGIC_SR_ANDROID=1");
            PublicDefinitions.Add("MAGIC_SR_IOS=0");
            PublicDefinitions.Add("MAGIC_SR_MAC=0");
            PublicDefinitions.Add("MAGIC_SR_WINDOWS=0");
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            string magicIOSStaticLib = ResolveMagicLibrary(
                projectRoot,
                "build/ipad/magic_sr/Release-iphoneos/libmagic_sr.a",
                "../release/v1.1.0/lib/ios/libmagic_sr.a");
            PublicAdditionalLibraries.Add(magicIOSStaticLib);
            PublicFrameworks.AddRange(
                new[]
                {
                    "Metal",
                    "MetalKit",
                    "MetalPerformanceShaders",
                    "QuartzCore",
                    "CoreVideo",
                    "Foundation"
                });
            PublicSystemLibraries.Add("z");
            PublicDefinitions.Add("MAGIC_SR_ANDROID=0");
            PublicDefinitions.Add("MAGIC_SR_IOS=1");
            PublicDefinitions.Add("MAGIC_SR_MAC=0");
            PublicDefinitions.Add("MAGIC_SR_WINDOWS=0");
            PublicDefinitions.Add("SYS_IOS=1");
            PublicDefinitions.Add("HAVE_NEON=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string magicMacStaticLib = ResolveMagicLibrary(
                projectRoot,
                "../release/v1.1.0/lib/mac_arm/libmagic_sr.a",
                "build/Apple_M/magic_sr_static/Release/libmagic_sr_static.a");
            PublicAdditionalLibraries.Add(magicMacStaticLib);
            PublicFrameworks.AddRange(
                new[]
                {
                    "Metal",
                    "MetalKit",
                    "MetalPerformanceShaders",
                    "QuartzCore",
                    "CoreVideo",
                    "Foundation"
                });
            PublicDefinitions.Add("MAGIC_SR_ANDROID=0");
            PublicDefinitions.Add("MAGIC_SR_IOS=0");
            PublicDefinitions.Add("MAGIC_SR_MAC=1");
            PublicDefinitions.Add("MAGIC_SR_WINDOWS=0");
            PublicDefinitions.Add("SYS_MACOSX=1");
        }
        else if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string magicWinStaticLib = ResolveMagicLibrary(
                projectRoot,
                "../release/v1.1.0/lib/windows/libmagic_sr.lib",
                "build/windows/build/Release/magic_sr.lib",
                "build/windows/build/magic_sr.lib");
            PublicAdditionalLibraries.Add(magicWinStaticLib);
            PublicSystemLibraries.Add("opengl32");
            PublicSystemLibraries.Add("user32");
            PublicSystemLibraries.Add("gdi32");
            PublicDefinitions.Add("MAGIC_SR_ANDROID=0");
            PublicDefinitions.Add("MAGIC_SR_IOS=0");
            PublicDefinitions.Add("MAGIC_SR_MAC=0");
            PublicDefinitions.Add("MAGIC_SR_WINDOWS=1");
        }
        else
        {
            PublicDefinitions.Add("MAGIC_SR_ANDROID=0");
            PublicDefinitions.Add("MAGIC_SR_IOS=0");
            PublicDefinitions.Add("MAGIC_SR_MAC=0");
            PublicDefinitions.Add("MAGIC_SR_WINDOWS=0");
        }
    }
}
