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

        // Add object to the trail iff it does not cause a cycle.
        // Return whether the object was added, i.e. return whether
        // the trail is without cycle.
        bool add(T object) {
            if (_visited.contains(object)) return false;
            auto it = _trail.insert(_trail.end(), object);
            _visited.insert({ object, it });
            return true;
        }

        // Remove the object from the trail.
        void remove(T object) {
            auto it = _visited.find(object);
            if (it == _visited.end()) {
                throw std::runtime_error("trail does not contain object");
            }
            _trail.erase(it->second);
            _visited.erase(it);
        }

        // Return the trail. Iterating the list returns the objects
        // in order of addition.
        std::list<T>const& trail() { return _trail; }

        bool empty() const { return _visited.empty(); }

        void clear() {
            _visited.clear();
            _trail.clear();
        }

    private:
        std::unordered_map<T, typename std::list<T>::iterator> _visited;
        std::list<T> _trail;
    };
}

