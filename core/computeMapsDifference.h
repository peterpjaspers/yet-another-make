#pragma once

#include <map>
#include <filesystem>

namespace YAM
{
    template<class TNode>
    void computeMapsDifference(
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& in1,
        std::map<std::filesystem::path, std::shared_ptr<TNode>> const& in2,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& inBoth,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& onlyIn1,
        std::map<std::filesystem::path, std::shared_ptr<TNode>>& onlyIn2
    ) {
        for (auto i1 : in1) {
            if (in2.find(i1.first) != in2.end()) inBoth.insert(i1);
            else onlyIn1.insert(i1);
        }
        for (auto i2 : in2) {
            if (in1.find(i2.first) == in1.end()) onlyIn2.insert(i2);
        }
    }
}
