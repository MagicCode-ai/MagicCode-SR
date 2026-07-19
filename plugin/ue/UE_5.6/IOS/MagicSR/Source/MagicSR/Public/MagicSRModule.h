#pragma once

#include "Modules/ModuleManager.h"

class FMagicSRModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
