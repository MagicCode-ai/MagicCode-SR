#include "CoreTypes.h"
#include "Modules/ModuleManager.h"

class FMagicSRUESmokeModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		FModuleManager::Get().LoadModule(TEXT("MagicSR"));
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FMagicSRUESmokeModule, MagicSRUESmoke, "MagicSRUESmoke");
