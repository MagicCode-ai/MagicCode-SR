#include "MagicSRModule.h"

#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FMagicSRModule, MagicSR)

void FMagicSRModule::StartupModule()
{
    UE_LOG(LogTemp,
           Display,
           TEXT("[MagicSR] StartupModule project=%s commandline=%s"),
           FApp::GetProjectName(),
           FCommandLine::Get());
}

void FMagicSRModule::ShutdownModule() {}
