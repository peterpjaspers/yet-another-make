#ifndef LANG_INTRINSICS_H
#define LANG_INTRINSICS_H

#include "Descriptor.h"

#include <string>

namespace Language {

    typedef Descriptor(IntrinsicFunction)( int arc, Descriptor* argv );

    IntrinsicFunction* intrinsic( const Descriptor& descriptor );

    void defineIntrinsics();

}

#endif // LANG_INTRINSICS_H
