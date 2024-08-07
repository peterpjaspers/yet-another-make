#pragma once

#include "IStreamable.h"

#include <unordered_set>
#include <cstdint>
#include <sstream>

namespace YAM
{
    class __declspec(dllexport) IPersistable : public IStreamable
    {
    public:
        friend class PersistentBuildState;

        // Set/get whether the object was modified since previous storage.
        // Note: newly created objects must be in modified() state to get
        // stored. At next storage request modified objects will be 
        // inserted/replaced in persistent storage.
        virtual void modified(bool modified) = 0;
        virtual bool modified() const = 0;

        // Return whether the object is to-be-deleted from persistent store. 
        // deleted() objects can still be referenced by other persistent
        // objects. Deleting the object from persistent store is deleted until
        // it is no longer referenced.
        virtual bool deleted() const = 0;
        // Post: !deleted()
        virtual void undelete() = 0;

        virtual std::string describeName() const = 0;
        virtual std::string describeType() const = 0;

        // Prepare the object for deserialization.
        // To be called when the object is about to be deserialized.
        // Example: a command node has input nodes that reference back to the
        // command node. These back-references are set by the command when an
        // input node is added to its set of inputs. The input nodes do not 
        // stream these back-references (because redundant).
        // During prepareDeserialize the command node clears the back-references
        // on its input nodes. 
        virtual void prepareDeserialize() = 0;

        // Initialize member variables that were not deserialized.
        // To be called after deserialization of the complete graph in which
        // this object is contained. 
        // Example: a class can have a member field whose value is derived from
        // other member fields. Redundant fields should not be streamed and 
        // instead re-computed during restore.
        // Example: during restore a command node sets the back-references on its
        // input nodes.
        // Avoids duplicate restore by keeping track of restored objects
        // in 'restored'.
        // Post: restored.contains(this)
        // Return whether this object was added to restored, i.e was not already 
        // contained in restored.
        virtual bool restore(
            void* context,
            std::unordered_set<IPersistable const*>& restored
        ) = 0;
    };
}