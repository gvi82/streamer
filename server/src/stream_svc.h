#pragma once

#include "common/object.h"
#include "common/ptr.h"

namespace Server
{

struct IStreamService : public virtual Common::IObject
{
    virtual void Initialize() = 0;
    virtual void Uninitialize() = 0;
};

DECLARE_PTR_S(IStreamService)
DECLARE_PTR_S(IServerApp)

IStreamServicePtr CreateStreamService(const IServerAppPtr& app);

}


