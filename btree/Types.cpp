#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "Types.h"

namespace BTree {

    std::ostream & operator<<( std::ostream & stream, PageLink const & link ) {
        const int StreamSize = 20;
        char string[ StreamSize + 1 ];
        if (link.null()) {
            snprintf( string, StreamSize, "%s", "< null >" );
        } else {
            snprintf( string, StreamSize, "< %d >", link.index );
        }
        stream << string;
        return stream;
    };

    BTreeStatistics::BTreeStatistics() { clear(); }
    BTreeStatistics* BTreeStatistics::clear() {
        insertions = 0;
        retrievals = 0;
        replacements = 0;
        removals = 0;
        finds = 0;
        grows = 0;
        pageAllocations = 0;
        pageFrees = 0;
        mergeAttempts = 0;
        pageMerges = 0;
        rootUpdates = 0;
        splitUpdates = 0;
        commits = 0;
        recovers = 0;
        pageWrites = 0;
        pageReads = 0;
        return this;
    }
    BTreeStatistics* BTreeStatistics::operator=( const BTreeStatistics& stats ) {
        insertions = stats.insertions;
        retrievals = stats.retrievals;
        replacements = stats.replacements;
        removals = stats.removals;
        finds = stats.finds;
        grows = stats.grows;
        pageAllocations = stats.pageAllocations;
        pageFrees = stats.pageFrees;
        mergeAttempts = stats.mergeAttempts;
        pageMerges = stats.pageMerges;
        rootUpdates = stats.rootUpdates;
        splitUpdates = stats.splitUpdates;
        commits = stats.commits;
        recovers = stats.recovers;
        pageWrites = stats.pageWrites;
        pageReads = stats.pageReads;
        return this;
    }
    BTreeStatistics* BTreeStatistics::operator+( const BTreeStatistics& stats ) {
        insertions += stats.insertions;
        retrievals += stats.retrievals;
        replacements += stats.replacements;
        removals += stats.removals;
        finds += stats.finds;
        grows += stats.grows;
        pageAllocations += stats.pageAllocations;
        pageFrees += stats.pageFrees;
        mergeAttempts += stats.mergeAttempts;
        pageMerges += stats.pageMerges;
        rootUpdates += stats.rootUpdates;
        splitUpdates += stats.splitUpdates;
        commits += stats.commits;
        recovers += stats.recovers;
        pageWrites += stats.pageWrites;
        pageReads += stats.pageReads;
        return this;
    }

} // namespace BTree
