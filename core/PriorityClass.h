#pragma once

namespace YAM
{
    enum PriorityClass {
        VeryHigh, // nPriorities
        High,     // nPriorities * 0.75
        Medium,   // nPriorities * 0.5
        Low,      // nPriorities * 0.25
        VeryLow   // 0
    };
}