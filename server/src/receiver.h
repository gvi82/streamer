#pragma once

#include "common/object.h"
#include "common/ptr.h"

namespace Server
{

struct IReceiverCallback : public virtual Common::IObject
{
    virtual void OnReceiverStarted() = 0;
    virtual void OnReceiverFailed() = 0;
};

struct IReceiver : public virtual Common::IObject
{
    virtual void Initialize() = 0;
    virtual void Uninitialize() = 0;
};

DECLARE_PTR_S(IReceiver)
DECLARE_PTR_S(IReceiverCallback)

IReceiverPtr CreateReceiver(const IReceiverCallbackPtr& callback, const std::string& sdp, int video_id);

}
