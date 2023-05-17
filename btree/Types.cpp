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
    std::ostream & operator<<( std::ostream & stream, StreamKey const & range ) {
        stream << "[ " << range.index << " : " << range.sequence << " ]";
        return stream;
    };

} // namespace BTree
