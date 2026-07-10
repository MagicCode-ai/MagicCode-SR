using UnrealBuildTool;

public class MagicSRUESmoke : ModuleRules
{
	public MagicSRUESmoke(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PrivateDependencyModuleNames.AddRange(new[] { "Core", "MagicSR" });
	}
}
