using UnrealBuildTool;

public class MagicSRUESmokeTarget : TargetRules
{
	public MagicSRUESmokeTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.Add("MagicSRUESmoke");
	}
}
