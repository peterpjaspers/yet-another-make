#pragma once

#include "Node.h"

#include <map>
#include <filesystem>

namespace YAM
{
    class Node;

    class __declspec(dllexport) NodeMapStreamer
    {
    public:
        template<typename TNode>
        static void stream(
            IStreamer* streamer,
            std::map<std::filesystem::path,std::shared_ptr<TNode>>& map
        ) {
            uint32_t nItems;
            if (streamer->writing()) {
                const std::size_t length = map.size();
                if (length > UINT_MAX) throw std::exception("map too large");
                nItems = static_cast<uint32_t>(length);
            }
            streamer->stream(nItems);
            if (streamer->writing()) {
                for (auto const& pair : map) {
                    std::shared_ptr<TNode> node = pair.second;
                    streamer->stream(node);
                }
            } else {
                // Take care: when streaming nodes from persistent repository the
                // nodes are constructed but their members may not yet have been 
                // streamed. Use temporary names to build the map and update the 
                // map in restore();
                for (std::size_t i = 0; i < nItems; ++i) {
                    std::shared_ptr<TNode> node;
                    streamer->stream(node);
                    std::stringstream ss; ss << i;
                    map.insert({ std::filesystem::path(ss.str()), node });
                }
            }
        }

        template<typename TNode>
        static void restore(
            std::map<std::filesystem::path,std::shared_ptr<TNode>> & map
        ) {
            std::map<std::filesystem::path, std::shared_ptr<TNode>> restored;
            for (auto const& pair : map) restored.insert({ pair.second->name(), pair.second });
            map = restored;
        }
    };
}

