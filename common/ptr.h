#ifndef PTR_H
#define PTR_H

#include <memory>

#define DECLARE_PTR_(cname, type) type cname;\
    typedef std::shared_ptr<cname> cname ## Ptr;\
    typedef std::weak_ptr<cname> cname ## WPtr; \
    typedef std::unique_ptr<cname> cname ## UPtr;

#define DECLARE_PTR(cname) DECLARE_PTR_(cname,class)
#define DECLARE_PTR_S(sname) DECLARE_PTR_(sname,struct)

#endif // PTR_H
