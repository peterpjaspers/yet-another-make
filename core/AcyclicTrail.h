#pragma once

#include <unordered_map>
#include <list>
#include <stdexcept>

namespace YAM
{
    class Node;

    // A AcyclicTrail keeps track of a trail (ordered list) of objects in which
    // each object is unique. It will refuse adding a duplicate object because this
    // would introduce a cycle in the trail.
    // E.g. adding 1 to the trail of integers 1,2,3 would introduce a cycle.
    //
    template<typename T>
    class __declspec(dllexport) AcyclicTrail
    {
    public:
        AcyclicTrail() {}

        // Add the node to the trail iff it does not cause a cycle.
        // Return whether the node was added, i.e. return whether
        // the trail is without cycle.
        bool add(T object) {
            if (_trail.contains(object)) return false;
            auto it = _orderedTrail.insert(_orderedTrail.end(), object);
            _trail.insert({ object, it });
            return true;
        }

        // Remove the node from the trail.
        void remove(T object) {
            auto it = _trail.find(object);
            if (it == _trail.end()) {
                throw std::runtime_error("trail does not contain object");
            }
            _orderedTrail.erase(it->second);
            _trail.erase(it);
        }

        // Return the trail. Iterating the list returns the objects
        // in order of addition.
        std::list<T>const& trail() { return _orderedTrail; }

        bool empty() const { return _trail.empty(); }

    private:
        std::unordered_map<T, typename std::list<T>::iterator> _trail;
        std::list<T> _orderedTrail;
    };
}

