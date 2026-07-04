using System.IO;
using UnrealBuildTool;

public class MagicSR : ModuleRules
{
    private static string FindMagicProjectRoot(string startDirectory)
    {
        DirectoryInfo directory = new DirectoryInfo(startDirectory);
        while (directory != null)
        {
            string candidate = directory.FullName;
            if (File.Exists(Path.Combine(candidate, "src/mc_interface.h")) &&
                (File.Exists(Path.Combine(candidate, "build/android/build/libmagic_sr.a")) ||
                 File.Exists(Path.Combine(candidate, "build/ipad/magic_sr/Release-iphoneos/libmagic_sr.a"))))
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
        string magicAndroidStaticLib = Path.Combine(projectRoot, "build/android/build/libmagic_sr.a");
        string magicIOSStaticLib = Path.Combine(projectRoot, "build/ipad/magic_sr/Release-iphoneos/libmagic_sr.a");

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
