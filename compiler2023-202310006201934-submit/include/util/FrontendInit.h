//
// Created by XuRuiyuan on 2023/8/1.
//

#ifndef FASTER_MEOW_FRONTENDINIT_H
#define FASTER_MEOW_FRONTENDINIT_H

#include "util/util.h"
#include "File.h"
#include "Lexer.h"

FILE *FileDealer::fpInput;
FILE *FileDealer::fpOutput;
FILE *FileDealer::fpArm;
char *FileDealer::fileStr;

bool Lexer::detect_float = false;

namespace CenterControl {
    bool _O2 = false;
    bool _GLOBAL_BSS = true;
    bool _FAST_REG_ALLOCATE = true;
    bool _IMM_TRY = false;
    bool STABLE = true;
    // bool _BACKEND_COMMENT_OUTPUT = false;
    bool _ONLY_FRONTEND = false;
    bool _cutLiveNessShortest = false;
    bool _FixStackWithPeepHole = true;
    bool _GPRMustToStack = true;
    bool _FPRMustToStack = true;
    bool _GPR_NOT_RESPILL = true;
    bool _FPR_NOT_RESPILL = true;
    bool _AGGRESSIVE_ADD_MOD_OPT = false;
    bool _OPEN_PARALLEL = true;
    // bool _OPEN_PARALLEL_BACKEND = true;
    bool AlreadyBackend = false;

    std::string _TAG = "1";
    bool _FUNC_PARALLEL = false;
    // bool _initAll = false;


    //O2-control
    bool _CLOSE_ALL_FLOAT_OPT = false;
    bool _OPEN_CONST_TRANS_FOLD = _CLOSE_ALL_FLOAT_OPT ? false : true;
    bool _OPEN_FLOAT_INSTR_COMB = _CLOSE_ALL_FLOAT_OPT ? false : true;
    bool _OPEN_FLOAT_LOOP_STRENGTH_REDUCTION = _CLOSE_ALL_FLOAT_OPT ? false : true;
    bool _OPEN_FTOI_ITOF_GVN = _CLOSE_ALL_FLOAT_OPT ? false : true;


    bool _STRONG_FUNC_INLINE = true;
    bool _STRONG_FUNC_GCM = false;

    double _HEURISTIC_BASE = 1.45;
    bool _PREC_MB_TOO_MUCH = false;
    std::string OUR_MEMSET_TAG = "__our_memset__";
    bool _GEP_CONST_BROADCAST = true;
    bool USE_ESTIMATE_BASE_SORT = true;
    bool ACROSS_BLOCK_MERGE_MI = false;

    bool _OPEN_LOOP_SWAP = true;
}

#include "ILinkNode.h"

int ILinkNode::id_num = 0;

#include "Value.h"

int Value::value_num = 0;
std::string Value::GLOBAL_PREFIX = "@";
std::string Value::LOCAL_PREFIX = "%";
std::string Value::GLOBAL_NAME_PREFIX = "g";
std::string Value::LOCAL_NAME_PREFIX = "v";
std::string Value::FPARAM_NAME_PREFIX = "f";
std::string Value::tagStrList[24] = {
        "value",
        "func",
        "bb",
        "bino",
        "ashr",
        "icmp",
        "fcmp",
        "fneg",
        "zext",
        "fptosi",
        "sitofp",
        "alloc",
        "load",
        "store",
        "gep",
        "bitcast",
        "call",
        "phi",
        "pcopy",
        "move",
        "branch",
        "jump",
        "ret",
};

#include "mir/Use.h"

int Use::use_num = 0;

#include "Type.h"

VoidType *VoidType::VOID_TYPE = new VoidType();

#include "Instr.h"

std::string INSTR::Alu::OpNameList[16] = {
        "add",
        "fadd",
        "sub",
        "fsub",
        "mul",
        "fmul",
        "sdiv",
        "fdiv",
        "srem",
        "frem",
        "and",
        "or",
        "xor",
        "shl",
        "lshr",
        "ashr",
};
int Instr::LOCAL_COUNT = 0;
int Instr::empty_instr_cnt = 0;

#include "Function.h"

int Function::Param::FPARAM_COUNT = 0;

#include "Constant.h"

int Constant::const_num = 0;

ConstantFloat *ConstantFloat::CONST_0F = new ConstantFloat(0.0f);
ConstantFloat *ConstantFloat::CONST_1F = new ConstantFloat(1.0f);
int ConstantFloat::float_const_cnt = 1;

ConstantInt *ConstantInt::CONST_0 = new ConstantInt(0);
ConstantInt *ConstantInt::CONST_1 = new ConstantInt(1);
std::map<int, ConstantInt *> ConstantInt::const_int_map;

#include "Manager.h"

std::string Manager::MAIN_FUNCTION = "main";
bool Manager::external = true;
std::map<std::string, Function *> *Manager::functions = new std::map<std::string, Function *>;
int Manager::outputLLVMCnt = 0;
int Manager::outputMIcnt = 0;

Function *ExternFunction::GET_INT = new Function(true, "getint", new std::vector<Function::Param *>{},
                                                 BasicType::I32_TYPE);
Function *ExternFunction::GET_CH = new Function(true, "getch", new std::vector<Function::Param *>{},
                                                BasicType::I32_TYPE);
Function *ExternFunction::GET_FLOAT = new Function(true, "getfloat", new std::vector<Function::Param *>{},
                                                   BasicType::F32_TYPE);
Function *ExternFunction::GET_ARR = new Function(true, "getarray",
                                                 new std::vector<Function::Param *>{
                                                         new Function::Param(new PointerType(BasicType::I32_TYPE), 0)},
                                                 BasicType::I32_TYPE);
Function *ExternFunction::GET_FARR = new Function(true, "getfarray",
                                                  new std::vector<Function::Param *>{
                                                          new Function::Param(new PointerType(BasicType::F32_TYPE), 0)},
                                                  BasicType::I32_TYPE);
Function *ExternFunction::PUT_INT = new Function(true, "putint",
                                                 new std::vector<Function::Param *>{
                                                         new Function::Param(BasicType::I32_TYPE, 0)},
                                                 VoidType::VOID_TYPE);
Function *ExternFunction::PUT_CH = new Function(true, "putch",
                                                new std::vector<Function::Param *>{
                                                        new Function::Param(BasicType::I32_TYPE, 0)},
                                                VoidType::VOID_TYPE);
Function *ExternFunction::PUT_FLOAT = new Function(true, "putfloat",
                                                   new std::vector<Function::Param *>{
                                                           new Function::Param(BasicType::F32_TYPE, 0)},
                                                   VoidType::VOID_TYPE);
Function *ExternFunction::PUT_ARR = new Function(true, "putarray",
                                                 new std::vector<Function::Param *>{
                                                         new Function::Param(BasicType::I32_TYPE, 0),
                                                         new Function::Param(
                                                                 new PointerType(BasicType::I32_TYPE), 1)},
                                                 VoidType::VOID_TYPE);
Function *ExternFunction::PUT_FARR = new Function(true, "putfarray",
                                                  new std::vector<Function::Param *>{
                                                          new Function::Param(BasicType::I32_TYPE, 0),
                                                          new Function::Param(new PointerType(BasicType::F32_TYPE), 1)},
                                                  VoidType::VOID_TYPE);
Function *ExternFunction::MEM_SET = new Function(true, "memset",
                                                 new std::vector<Function::Param *>{
                                                         new Function::Param(new PointerType(BasicType::I32_TYPE), 0),
                                                         new Function::Param(BasicType::I32_TYPE, 1),
                                                         new Function::Param(BasicType::I32_TYPE, 2)},
                                                 VoidType::VOID_TYPE);
// Function *ExternFunction::MEM_SET_ZERO = new Function(true, "_our_memset",
//                                                  new std::vector<Function::Param *>{
//                                                          new Function::Param(new PointerType(BasicType::I32_TYPE), 0),
//                                                          new Function::Param(BasicType::I32_TYPE, 1)},
//                                                  VoidType::VOID_TYPE);
Function *ExternFunction::START_TIME = new Function(true, "starttime",
                                                    new std::vector<Function::Param *>{
                                                            new Function::Param(BasicType::I32_TYPE, 0)},
                                                    VoidType::VOID_TYPE);
Function *ExternFunction::STOP_TIME = new Function(true, "stoptime",
                                                   new std::vector<Function::Param *>{
                                                           new Function::Param(BasicType::I32_TYPE, 0)},
                                                   VoidType::VOID_TYPE);
Function *ExternFunction::PARALLEL_START = new Function(true, "parallel_start",
                                                        new std::vector<Function::Param *>{}, BasicType::I32_TYPE);
Function *ExternFunction::PARALLEL_END = new Function(true,
                                                      "parallel_end",
                                                      new std::vector<Function::Param *>{
                                                              new Function::Param(BasicType::I32_TYPE, 0)},
                                                      VoidType::VOID_TYPE);
Function *ExternFunction::LLMMOD = new Function(true,
                                               "llmmod",
                                               new std::vector<Function::Param*>{
                                                        new Function::Param(BasicType::I32_TYPE, 0),
                                                        new Function::Param(BasicType::I32_TYPE, 1),
                                                        new Function::Param(BasicType::I32_TYPE, 2)
                                               },
                                                    BasicType::I32_TYPE);

Function *ExternFunction::LLMD2MOD = new Function(true,
                                               "llmd2mod",
                                               new std::vector<Function::Param*>{
                                                       new Function::Param(BasicType::I32_TYPE, 0),
                                                       new Function::Param(BasicType::I32_TYPE, 1),
                                                       new Function::Param(BasicType::I32_TYPE, 2)
                                               },
                                               BasicType::I32_TYPE);

Function *ExternFunction::LLMAMOD = new Function(true,
                                               "llmamod",
                                               new std::vector<Function::Param*>{
                                                       new Function::Param(BasicType::I32_TYPE, 0),
                                                       new Function::Param(BasicType::I32_TYPE, 1),
                                                       new Function::Param(BasicType::I32_TYPE, 2),
                                                       new Function::Param(BasicType::I32_TYPE, 3)
                                               },
                                               BasicType::I32_TYPE);

Function *ExternFunction::USAT = new Function(true,
                                               "usat",
                                               new std::vector<Function::Param*> {
                                                       new Function::Param(BasicType::I32_TYPE, 0),
                                                       new Function::Param(BasicType::I32_TYPE, 1),
                                                },
                                               BasicType::I32_TYPE);

Manager *Manager::MANAGER = new Manager();

#include "Arm.h"

std::string Arm::condStrList[9] = {
        "eq",
        "ne",
        "gt",
        "ge",
        "lt",
        "le",
        "hi", // >
        "pl", // >=
        ""};
std::string Arm::gprStrList[GPR_NUM] = {
        "r0", "r1", "r2", "r3",
        // local variables (callee saved
        "r4", "r5", "r6", "r7",
        "r8", "r9", "r10", "r11", "r12",
        // some aliases
        //"r11",  // frame pointer (omitted), allocatable
        //"r12",  // ipc scratch register, used in some instructions (caller saved)
        "sp",  // stack pointer
        "lr",  // link register (caller saved) BL指令调用时存放返回地址
        "pc",  // program counter
        "r16",};

Arm::GPRs *Arm::Regs::gpr[GPR_NUM] = {
        new GPRs(0),
        new GPRs(1),
        new GPRs(2),
        new GPRs(3),
        new GPRs(4),
        new GPRs(5),
        new GPRs(6),
        new GPRs(7),
        new GPRs(8),
        new GPRs(9),
        new GPRs(10),
        new GPRs(11),
        new GPRs(12),
        new GPRs(13),
        new GPRs(14),
        new GPRs(15),
        new GPRs(16),
};

Arm::FPRs *Arm::Regs::fpr[FPR_NUM] = {
        new FPRs(0),
        new FPRs(1),
        new FPRs(2),
        new FPRs(3),
        new FPRs(4),
        new FPRs(5),
        new FPRs(6),
        new FPRs(7),
        new FPRs(8),
        new FPRs(9),
        new FPRs(10),
        new FPRs(11),
        new FPRs(12),
        new FPRs(13),
        new FPRs(14),
        new FPRs(15),
        new FPRs(16),
        new FPRs(17),
        new FPRs(18),
        new FPRs(19),
        new FPRs(20),
        new FPRs(21),
        new FPRs(22),
        new FPRs(23),
        new FPRs(24),
        new FPRs(25),
        new FPRs(26),
        new FPRs(27),
        new FPRs(28),
        new FPRs(29),
        new FPRs(30),
        new FPRs(31),
};

Arm::Reg *Arm::Reg::allocGprPool[GPR_NUM] = {
        new Arm::Reg(Arm::GPR::r0),
        new Arm::Reg(Arm::GPR::r1),
        new Arm::Reg(Arm::GPR::r2),
        new Arm::Reg(Arm::GPR::r3),
        new Arm::Reg(Arm::GPR::r4),
        new Arm::Reg(Arm::GPR::r5),
        new Arm::Reg(Arm::GPR::r6),
        new Arm::Reg(Arm::GPR::r7),
        new Arm::Reg(Arm::GPR::r8),
        new Arm::Reg(Arm::GPR::r9),
        new Arm::Reg(Arm::GPR::r10),
        new Arm::Reg(Arm::GPR::r11),
        new Arm::Reg(Arm::GPR::r12),
        new Arm::Reg(Arm::GPR::sp),
        new Arm::Reg(Arm::GPR::lr),
        new Arm::Reg(Arm::GPR::pc),
        new Arm::Reg(Arm::GPR::cspr),
};

Arm::Reg *Arm::Reg::allocFprPool[FPR_NUM] = {
        new Arm::Reg(Arm::FPR::s0),
        new Arm::Reg(Arm::FPR::s1),
        new Arm::Reg(Arm::FPR::s2),
        new Arm::Reg(Arm::FPR::s3),
        new Arm::Reg(Arm::FPR::s4),
        new Arm::Reg(Arm::FPR::s5),
        new Arm::Reg(Arm::FPR::s6),
        new Arm::Reg(Arm::FPR::s7),
        new Arm::Reg(Arm::FPR::s8),
        new Arm::Reg(Arm::FPR::s9),
        new Arm::Reg(Arm::FPR::s10),
        new Arm::Reg(Arm::FPR::s11),
        new Arm::Reg(Arm::FPR::s12),
        new Arm::Reg(Arm::FPR::s13),
        new Arm::Reg(Arm::FPR::s14),
        new Arm::Reg(Arm::FPR::s15),
        new Arm::Reg(Arm::FPR::s16),
        new Arm::Reg(Arm::FPR::s17),
        new Arm::Reg(Arm::FPR::s18),
        new Arm::Reg(Arm::FPR::s19),
        new Arm::Reg(Arm::FPR::s20),
        new Arm::Reg(Arm::FPR::s21),
        new Arm::Reg(Arm::FPR::s22),
        new Arm::Reg(Arm::FPR::s23),
        new Arm::Reg(Arm::FPR::s24),
        new Arm::Reg(Arm::FPR::s25),
        new Arm::Reg(Arm::FPR::s26),
        new Arm::Reg(Arm::FPR::s27),
        new Arm::Reg(Arm::FPR::s28),
        new Arm::Reg(Arm::FPR::s29),
        new Arm::Reg(Arm::FPR::s30),
        new Arm::Reg(Arm::FPR::s31),
};

Arm::Reg *Arm::Reg::gprPool[GPR_NUM] = {
        new Arm::Reg(DataType::I32, Arm::GPR::r0),
        new Arm::Reg(DataType::I32, Arm::GPR::r1),
        new Arm::Reg(DataType::I32, Arm::GPR::r2),
        new Arm::Reg(DataType::I32, Arm::GPR::r3),
        new Arm::Reg(DataType::I32, Arm::GPR::r4),
        new Arm::Reg(DataType::I32, Arm::GPR::r5),
        new Arm::Reg(DataType::I32, Arm::GPR::r6),
        new Arm::Reg(DataType::I32, Arm::GPR::r7),
        new Arm::Reg(DataType::I32, Arm::GPR::r8),
        new Arm::Reg(DataType::I32, Arm::GPR::r9),
        new Arm::Reg(DataType::I32, Arm::GPR::r10),
        new Arm::Reg(DataType::I32, Arm::GPR::r11),
        new Arm::Reg(DataType::I32, Arm::GPR::r12),
        allocGprPool[(int) Arm::GPR::sp],
        new Arm::Reg(DataType::I32, Arm::GPR::lr),
        new Arm::Reg(DataType::I32, Arm::GPR::pc),
        new Arm::Reg(DataType::I32, Arm::GPR::cspr),
};

Arm::Reg *Arm::Reg::fprPool[FPR_NUM] = {
        new Arm::Reg(DataType::F32, Arm::FPR::s0),
        new Arm::Reg(DataType::F32, Arm::FPR::s1),
        new Arm::Reg(DataType::F32, Arm::FPR::s2),
        new Arm::Reg(DataType::F32, Arm::FPR::s3),
        new Arm::Reg(DataType::F32, Arm::FPR::s4),
        new Arm::Reg(DataType::F32, Arm::FPR::s5),
        new Arm::Reg(DataType::F32, Arm::FPR::s6),
        new Arm::Reg(DataType::F32, Arm::FPR::s7),
        new Arm::Reg(DataType::F32, Arm::FPR::s8),
        new Arm::Reg(DataType::F32, Arm::FPR::s9),
        new Arm::Reg(DataType::F32, Arm::FPR::s10),
        new Arm::Reg(DataType::F32, Arm::FPR::s11),
        new Arm::Reg(DataType::F32, Arm::FPR::s12),
        new Arm::Reg(DataType::F32, Arm::FPR::s13),
        new Arm::Reg(DataType::F32, Arm::FPR::s14),
        new Arm::Reg(DataType::F32, Arm::FPR::s15),
        new Arm::Reg(DataType::F32, Arm::FPR::s16),
        new Arm::Reg(DataType::F32, Arm::FPR::s17),
        new Arm::Reg(DataType::F32, Arm::FPR::s18),
        new Arm::Reg(DataType::F32, Arm::FPR::s19),
        new Arm::Reg(DataType::F32, Arm::FPR::s20),
        new Arm::Reg(DataType::F32, Arm::FPR::s21),
        new Arm::Reg(DataType::F32, Arm::FPR::s22),
        new Arm::Reg(DataType::F32, Arm::FPR::s23),
        new Arm::Reg(DataType::F32, Arm::FPR::s24),
        new Arm::Reg(DataType::F32, Arm::FPR::s25),
        new Arm::Reg(DataType::F32, Arm::FPR::s26),
        new Arm::Reg(DataType::F32, Arm::FPR::s27),
        new Arm::Reg(DataType::F32, Arm::FPR::s28),
        new Arm::Reg(DataType::F32, Arm::FPR::s29),
        new Arm::Reg(DataType::F32, Arm::FPR::s30),
        new Arm::Reg(DataType::F32, Arm::FPR::s31),
};

Arm::Shift *Arm::Shift::NONE_SHIFT = new Arm::Shift();

#include "MachineInst.h"

MachineInst *MachineInst::emptyInst = new MachineInst();
int MachineInst::cnt = 1;
int MachineInst::MI_num = 0;

#include "Operand.h"

Operand *Operand::I_ZERO = new Operand(DataType::I32, 0);
Operand *Operand::I_ONE = new Operand(DataType::I32, 1);


#include "Initial.h"

std::string Token::list[100] = {
        "const",
        "int",
        "float",
        "break",
        "continue",
        "if",
        "else",
        "void",
        "while",
        "return",
        // ident
        "IDENT",
        // float const
        "HEX_FLOAT",
        "DEC_FLOAT",
        // int const
        "HEX_INT",
        "OCT_INT",
        "DEC_INT",
        // operator (double char)
        "&&",
        "||",
        "<=",
        ">=",
        "==",
        "!=",
        // operator (single char)
        "<",
        ">",
        "+", // regex
        "-",
        "*",
        "/",
        "%",
        "!",
        "=",
        ";",
        ",",
        "(",
        ")",
        "[",
        "]",
        "{",
        "}",
        "",

};

#include "Visitor.h"

Visitor *Visitor::VISITOR = new Visitor;
SymTable *Visitor::currentSymTable = new SymTable();
int Visitor::loopDepth = 0;
Loop *Visitor::curLoop = nullptr;

#include "MC.h"

std::string McBlock::MB_Prefix = "._MB_";
int McBlock::globIndex = 0;
McProgram *McProgram::MC_PROGRAM = new McProgram();
MachineInst *McProgram::specialMI;


#include "CodeGen.h"

CodeGen *CodeGen::CODEGEN = new CodeGen();
// bool CodeGen::_DEBUG_OUTPUT_MIR_INTO_COMMENT = false;
bool CodeGen::needFPU = false;
bool CodeGen::optMulDiv = true;
McFunction *CodeGen::curMF = nullptr;
Function *CodeGen::curFunc = nullptr;
int CodeGen::rParamCnt = 4;
int CodeGen::sParamCnt = 16;
std::map<std::string, Operand *> CodeGen::name2constFOpd;
MachineInst::Tag CodeGen::tagMap[16] = {
        MachineInst::Tag::Add,
        MachineInst::Tag::FAdd,
        MachineInst::Tag::Sub,
        MachineInst::Tag::FSub,
        MachineInst::Tag::Mul,
        MachineInst::Tag::FMul,
        MachineInst::Tag::Div,
        MachineInst::Tag::FDiv,
        MachineInst::Tag::Mod,
        MachineInst::Tag::FMod,
        MachineInst::Tag::FAnd,
        MachineInst::Tag::FOr,
        MachineInst::Tag::Xor,
        MachineInst::Tag::Lsl,
        MachineInst::Tag::Lsr,
        MachineInst::Tag::Asr,
};

// #include "RegAllocator.h"
#define _ABS(num) ((num)<0?-num:num)

#include "PeepHole.h"

MachineInst *PeepHole::lastGPRsDefMI[GPR_NUM];
MachineInst *PeepHole::lastFPRsDefMI[FPR_NUM];

#include "Parallel.h"

Parallel *Parallel::PARALLEL = new Parallel(McProgram::MC_PROGRAM);
McFunction *Parallel::curMF = nullptr;
McBlock *Parallel::curMB = nullptr;

#endif //FASTER_MEOW_FRONTENDINIT_H
