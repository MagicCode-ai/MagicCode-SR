using UnrealBuildTool;

public class MagicSRUESmokeEditorTarget : TargetRules
{
	public MagicSRUESmokeEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		ExtraModuleNames.Add("MagicSRUESmoke");
	}
}
