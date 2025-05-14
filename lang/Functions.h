#ifndef LANG_FUNCTIONS_H
#define LANG_FUNCTIONS_H

#include "Types.h"
#include "Descriptor.h"

namespace Language {

    Descriptor FunAdd();
    Descriptor FunSub();
    Descriptor FunMul();
    Descriptor FunDiv();
    Descriptor FunEq();
    Descriptor FunNeq();
    Descriptor FunLt();
    Descriptor FunLteq();
    Descriptor FunGt();
    Descriptor FunGteq();
    Descriptor FunCompare();
    Descriptor FunNot();
    Descriptor FunAnd();
    Descriptor FunOr();
    Descriptor FunShiftLeft();
    Descriptor FunShiftRight();
    Descriptor FunBitwiseOr();
    Descriptor FunBitwiseXor();
    Descriptor FunBitwiseAnd();
    Descriptor FunBitwiseNegate();
    Descriptor FunInvert();
    Descriptor FunRemainder();

} // namespace Language

#endif // LANG_FUNCTIONS_H