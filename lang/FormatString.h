#ifndef LANG_FORMAT_STRING_H
#define LANG_FORMAT_STRING_H

#include "Types.h"
#include "ThreadContext.h"

namespace Language {

    bool isFormatString( const ThreadContext& ctx, const Descriptor string );
    Descriptor evaluateFormatString( ThreadContext& ctx, const Descriptor source );

}

#endif // LANG_FORMAT_STRING_H
