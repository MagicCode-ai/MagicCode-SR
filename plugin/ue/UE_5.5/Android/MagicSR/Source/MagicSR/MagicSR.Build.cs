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
        return File.Exists(legacyAndroid) || File.Exists(legacyIOS) || File.Exists(releaseAndroid) || File.Exists(releaseIOS);
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

        throw new System.Exception("Unable to locate libmagic_sr.a from project root " + projectRoot);
    }

    private static string FindMagicProjectRoot(string startDirectory)
    {
        DirectoryInfo directory = new DirectoryInfo(startDirectory);
        while (directory != null)
        {
            string candidate = directory.FullName;
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
                "Engine"
            });

        string projectRoot = FindMagicProjectRoot(ModuleDirectory);
        string magicIncludeDir = Path.Combine(projectRoot, "src");
        string magicAndroidStaticLib = ResolveMagicLibrary(
            projectRoot,
            "../release/v1.1.0/lib/android/libmagic_sr.a",
            "build/android/build/libmagic_sr.a");
        string magicIOSStaticLib = ResolveMagicLibrary(
            projectRoot,
            "../release/v1.1.0/lib/ios/libmagic_sr.a",
            "build/ipad/magic_sr/Release-iphoneos/libmagic_sr.a");

        PublicIncludePaths.Add(magicIncludeDir);

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            PublicAdditionalLibraries.Add(magicAndroidStaticLib);
            PublicSystemLibraries.Add("log");
            PublicSystemLibraries.Add("android");
            PublicSystemLibraries.Add("GLESv3");
            PublicSystemLibraries.Add("EGL");
            PublicSystemLibraries.Add("vulkan");
            PublicDefinitions.Add("MAGIC_SR_ANDROID=1");
            PublicDefinitions.Add("MAGIC_SR_IOS=0");
        }
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
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
            PublicDefinitions.Add("MAGIC_SR_ANDROID=0");
            PublicDefinitions.Add("MAGIC_SR_IOS=1");
            PublicDefinitions.Add("SYS_IOS=1");
            PublicDefinitions.Add("HAVE_NEON=1");
        }
        else
        {
            PublicDefinitions.Add("MAGIC_SR_ANDROID=0");
            PublicDefinitions.Add("MAGIC_SR_IOS=0");
        }
    }
}
