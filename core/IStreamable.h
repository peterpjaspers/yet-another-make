#pragma once

#include <string>

namespace YAM
{ 
    class IStreamer;

    // Interface for serialization/deserialization of dynamically allocated
    // objects.
    //    
    class __declspec(dllexport) IStreamable
    {
    public:
        IStreamable() {}
        IStreamable(const IStreamable&) {}
        virtual ~IStreamable() {}

        // Implementors of IStreamable and IStreamableTypes can optionally
        // use this function to efficiently encode type of an IStreamable.
        // Background: each of the classes in the set of classes managed
        // by a IStreamableTypes implementation must return a type id that
        // is unique in a streaming session in whcih IStreamable objects
        // are streamed. Ensuring uniqueness is hard/impossible when this set
        // contains classes from independently developed libraries. In such
        // cases IStreamableTypes implementations better not use this function
        // to encode type.
        //
        virtual uint32_t typeId() const{ return 0; }
        
        // Implementors of IStreamable and IStreamableTypes can optionally 
        // use this function to encode type of an IStreamable as a string.
        // Similar to TypeId() each of the classes in the set of classes managed
        // by a IStreamableTypes implementation must return a unique type name.
        // This can be achieved more easily than for the TypeId() function by 
        // consistent use of namespaces.
        //
        virtual std::string typeName() const { return ""; }

        virtual void stream(IStreamer*) {}
    };
}