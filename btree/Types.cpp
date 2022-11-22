#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include "Types.h"

namespace BTree {

    std::ostream & operator<<( std::ostream & stream, PageLink const & link ) {
        const int StreamSize = 10;
        char string[ StreamSize + 1 ];
        if (link.null()) {
            snprintf( string, StreamSize, "%s", "< null >" );
        } else {
            snprintf( string, StreamSize, "< %d >", link.index );
        }
        stream << string;
        return stream;
    };

} // namespace BTree
