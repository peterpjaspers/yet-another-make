#ifndef LANG_FUNCTIONS_H
#define LANG_FUNCTIONS_H

#include "Types.h"
#include "Descriptor.h"

namespace Language {

    Descriptor FunAdd( ThreadContext& context );
    Descriptor FunSub( ThreadContext& context );
    Descriptor FunMul( ThreadContext& context );
    Descriptor FunDiv( ThreadContext& context );
    Descriptor FunEq( ThreadContext& context );
    Descriptor FunNeq( ThreadContext& context );
    Descriptor FunLt( ThreadContext& context );
    Descriptor FunLteq( ThreadContext& context );
    Descriptor FunGt( ThreadContext& context );
    Descriptor FunGteq( ThreadContext& context );
    Descriptor FunCompare( ThreadContext& context );
    Descriptor FunNot( ThreadContext& context );
    Descriptor FunAnd( ThreadContext& context );
    Descriptor FunOr( ThreadContext& context );
    Descriptor FunShiftLeft( ThreadContext& context );
    Descriptor FunShiftRight( ThreadContext& context );
    Descriptor FunBitwiseOr( ThreadContext& context );
    Descriptor FunBitwiseXor( ThreadContext& context );
    Descriptor FunBitwiseAnd( ThreadContext& context );
    Descriptor FunBitwiseNegate( ThreadContext& context );
    Descriptor FunInvert( ThreadContext& context );
    Descriptor FunRemainder( ThreadContext& context );

} // namespace Language

#endif // LANG_FUNCTIONS_H