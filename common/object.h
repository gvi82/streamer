#ifndef OBJECT_H
#define OBJECT_H

#include "ptr.h"
#include <atomic>

//uncomment for diagnostic
//#include "common.h"
//#define LOG_COUNTER() LOG(__FUNCTION__ << typeid(T).name() << " " << ref_count)
#define LOG_COUNTER()

namespace Common {

struct IObject
{
    virtual ~IObject() {}
};

DECLARE_PTR_S(IObject)

typedef std::atomic<unsigned> RefCountType;

template <typename T>
class ObjectCounter
{
    static RefCountType& ref_count;
protected:
    ObjectCounter()
    {
        ++ref_count;
        LOG_COUNTER();
    }

    ObjectCounter(const ObjectCounter& )
    {
        ++ref_count;
        LOG_COUNTER();
    }

    ObjectCounter(ObjectCounter&& )
    {
        ++ref_count;
        LOG_COUNTER();
    }

    ~ObjectCounter()
    {
        LOG_COUNTER();
        --ref_count;
        LOG_COUNTER();
    }
};

RefCountType& register_counter(const char* tname);

template <typename T>
RefCountType& ObjectCounter<T>::ref_count = register_counter(typeid(T).name());


}


#endif // OBJECT_H
