#pragma once

#include "common/object.h"
#include "common/ptr.h"

namespace Client
{

DECLARE_PTR_S(ISender)

struct ISenderEvents : public virtual Common::IObject
{
    virtual void OnSenderStopped(ISenderPtr sender) = 0;
};

struct ISender : public virtual Common::IObject
{
    virtual void Initialize() = 0;
    virtual void Uninitialize() = 0;
};

DECLARE_PTR_S(IClientApp)
DECLARE_PTR_S(ISenderEvents)

struct ClientParams;

ISenderPtr CreateSender(const ClientParams& params, const ISenderEventsPtr& handler);

}
