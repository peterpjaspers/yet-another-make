#ifndef LANG_INSTRUCTION_H
#define LANG_INSTRUCTION_H

#include "Types.h"
#include "Descriptor.h"
#include "LogFile.h"

#include <filesystem>

namespace Language {

    // Virtual machine instruction op-codes:
    typedef uint8_t OpCode;
    static const OpCode OpPushNull          = OpCode(1U);
    static const OpCode OpPushInteger       = OpCode(2U);
    static const OpCode OpPushReal          = OpCode(3U);
    static const OpCode OpPop               = OpCode(4U);
    static const OpCode OpAdd               = OpCode(5U);
    static const OpCode OpSub               = OpCode(6U);
    static const OpCode OpMul               = OpCode(7U);
    static const OpCode OpDiv               = OpCode(8U);
    static const OpCode OpEq                = OpCode(9U);
    static const OpCode OpNeq               = OpCode(10U);
    static const OpCode OpLt                = OpCode(11U);
    static const OpCode OpLteq              = OpCode(12U);
    static const OpCode OpGt                = OpCode(13U);
    static const OpCode OpGteq              = OpCode(14U);
    static const OpCode OpCompare           = OpCode(15U);
    static const OpCode OpNot               = OpCode(16U);
    static const OpCode OpAnd               = OpCode(17U);
    static const OpCode OpOr                = OpCode(18U);
    static const OpCode OpJump              = OpCode(19U);
    static const OpCode OpConditionalJump   = OpCode(20U);
    static const OpCode OpArguments         = OpCode(21U);
    static const OpCode OpProcedureCall     = OpCode(22U);
    static const OpCode OpReturn            = OpCode(23U);
    static const OpCode OpLocals            = OpCode(24U);
    static const OpCode OpLoadArgument      = OpCode(25U);
    static const OpCode OpLoadLocal         = OpCode(26U);
    static const OpCode OpLoadGlobal        = OpCode(27U);
    static const OpCode OpStoreArgument     = OpCode(28U);
    static const OpCode OpStoreLocal        = OpCode(29U);
    static const OpCode OpStoreGlobal       = OpCode(30U);
    static const OpCode OpFail              = OpCode(31U);
    static const OpCode OpMark              = OpCode(32U);
    static const OpCode OpUnmark            = OpCode(33U);
    static const OpCode OpShiftLeft         = OpCode(34U);
    static const OpCode OpShiftRight        = OpCode(35U);
    static const OpCode OpBitOr             = OpCode(36U);
    static const OpCode OpBitXor            = OpCode(37U);
    static const OpCode OpBitAnd            = OpCode(38U);
    static const OpCode OpNegate            = OpCode(39U);
    static const OpCode OpInvert            = OpCode(40U);
    static const OpCode OpRem               = OpCode(41U);
    static const OpCode OpDup               = OpCode(42U);
    static const OpCode OpAssign            = OpCode(43U);
    static const OpCode OpPushDescriptor    = OpCode(44U);
    static const OpCode OpOutput            = OpCode(45U);
    static const OpCode OpExit              = OpCode(46U);

    struct OpCodeTableEntry { const char* name; int operand; };
    static const OpCodeTableEntry opCodeTable[] = {
        /* Undefined            */ { "?",                   0 },
        /* OpPushNull           */ { "PushNull",            0 },
        /* OpPushInteger        */ { "PushInteger",         1 },
        /* OpPushReal           */ { "PushReal   ",         1 },
        /* OpPop                */ { "Pop",                 0 },
        /* OpAdd                */ { "Add",                 0 },
        /* OpSub                */ { "Sub",                 0 },
        /* OpMul                */ { "Mul",                 0 },
        /* OpDiv                */ { "Div",                 0 },
        /* OpEq                 */ { "Eq",                  0 },
        /* OpNeq                */ { "Neq",                 0 },
        /* OpLt                 */ { "Lt",                  0 },
        /* OpLteq               */ { "Lteq",                0 },
        /* OpGt                 */ { "Gt",                  0 },
        /* OpGteq               */ { "Gteq",                0 },
        /* OpCompare            */ { "Compare",             0 },
        /* OpNot                */ { "Not",                 0 },
        /* OpAnd                */ { "And",                 0 },
        /* OpOr                 */ { "Or",                  0 },
        /* OpJump               */ { "Jump",                1 },
        /* OpConditionalJump    */ { "ConditionalJump",     1 },
        /* OpArguments          */ { "Arguments",           0 },
        /* OpProcedureCall      */ { "ProcedureCall",       0 },
        /* OpReturn             */ { "Return",              0 },
        /* OpLocals             */ { "Locals",              1 },
        /* OpLoadArgument       */ { "LoadArgument",        1 },
        /* OpLoadLocal          */ { "LoadLocal",           1 },
        /* OpLoadGlobal         */ { "LoadGlobal",          1 },
        /* OpStoreArgument      */ { "StoreArgument",       1 },
        /* OpStoreLocal         */ { "StoreLocal",          1 },
        /* OpStoreGlobal        */ { "StoreGlobal",         1 },
        /* OpFail               */ { "Fail",                0 },
        /* OpMark               */ { "Mark",                1 },
        /* OpUnmark             */ { "Unmark",              0 },
        /* OpShiftLeft          */ { "Shift left",          0 },
        /* OpShiftRight         */ { "Shift right",         0 },
        /* OpBitOr              */ { "Bitwise or",          0 },
        /* OpBitXor             */ { "Bitwise xor",         0 },
        /* OpBitAnd             */ { "Bitwise and",         0 },
        /* OpNegate             */ { "Negate",              0 },
        /* OpInvert             */ { "Bitwise invert",      0 },
        /* OpRem                */ { "Remainder",           0 },
        /* OpDup                */ { "Duplicate",           0 },
        /* OpAssign             */ { "Assign",              0 },
        /* OpPushDescriptor     */ { "Push",                2 },
        /* OpOutput             */ { "Output",              0 },
        /* OpExit               */ { "Exit",                0 },
    };

    // Execute (single) instruction
    bool executeInstruction();
    // Run a program starting at an address.
    int run( const Address start );
    // Push a descriptor-value on the stack
    void push( const Descriptor& descriptor );
    // Pop a descriptor-value from the stack
    Descriptor pop();
    // Store an instruction in Program memory
    void storeInstruction( const OpCode code );
    void storeInstruction( const OpCode code, const Word operand );
    void storeInstruction( const OpCode code, const Descriptor operand );
    // Convert op-code to human readble string
    std::string toReadable( const OpCode code );

    // Contents the contents of the program to a human readbale file.
    void printProgram( LogFile& stream );
    void printProgram( const std::filesystem::path file );

} // namespace Language

#endif // LANG_INSTRUCTION_H