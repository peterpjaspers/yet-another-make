#pragma once

#include "IStreamable.h"

#include <cstdint>

namespace YAM
{
    class __declspec(dllexport) IPersistable : public IStreamable
    {
    public:
        // Set/get whether the object is modified. Only modified objects
        // will be stored by the persistent repository. The persistent 
        // repository resets the modified flag when storage has completed.
        virtual void modified(bool value) = 0;
        virtual bool modified() const = 0;

        // Restore the persistable.
        // Called by the persistent repository after it has deserialized the
        // complete graph of which this object is the root. 
        // Example: a class can have a redundant member field, i.e. a field
        // whose value is derived from other member fields. Redundant fields 
        // should not be streamed and instead recomputed during restore.
        virtual void restore(void* context) = 0;
    };
}