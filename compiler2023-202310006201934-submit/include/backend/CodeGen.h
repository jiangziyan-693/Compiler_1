
#ifndef FASTER_MEOW_CODEGEN_H
#define FASTER_MEOW_CODEGEN_H


#include <vector>
#include <map>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include "lir/MachineInst.h"

namespace Arm {
    enum class Cond;

    class Glob;
}
namespace INSTR {
    class GetElementPtr;

    class Alu;
}
class BasicBlock;

class Initial;

class MachineInst;

class McFunction;

class Function;

class Value;

class GlobalValue;

class Operand;

class McBlock;

class CMPAndArmCond {
public:
    ~CMPAndArmCond() = default;

public:

    MachineInst *CMP = nullptr;
    Arm::Cond ArmCond;

    explicit CMPAndArmCond(MachineInst *cmp, Arm::Cond cond);

};


class CodeGen {
public:
    virtual ~CodeGen() = default;

public:

    static CodeGen *CODEGEN;
    // static bool _DEBUG_OUTPUT_MIR_INTO_COMMENT;
    static bool needFPU;
    static bool optMulDiv;
    static McFunction *curMF;
    static Function *curFunc;
    // std::unordered_map<std::string, Function *> *fMap;
    std::map<INSTR::Alloc *, Operand *> allocMap;
    std::map<std::string, Function *> *fMap;
    // std::unordered_map<Value *, Operand *> value2opd;
    std::map<GlobalValue *, Arm::Glob *, CompareGlobalValue> globPtr2globNameOpd;
    std::unordered_map<Function *, McFunction *> f2mf;
    std::unordered_map<BasicBlock *, McBlock *> bb2mb;
    // std::unordered_map<GlobalValue *, Initial *> *globalMap;
    std::map<GlobalValue *, Initial *, CompareGlobalValue> *globalMap;
    McBlock *curMB = nullptr;
    static int rParamCnt;
    static int sParamCnt;
    std::stack<BasicBlock *> nextBBList;

    std::unordered_set<McBlock *> visitBBSet;
    int cnt = 0;
    std::unordered_map<Value *, CMPAndArmCond *> cmpInst2MICmpMap;
    static std::map<std::string, Operand *> name2constFOpd;
    static MachineInst::Tag tagMap[16];

    CodeGen();

    void gen();

    void Push(McFunction *mf);

    void Pop(McFunction *mf);

    void dealParam();

    void genBB(std::vector<BasicBlock *>::iterator it);

    void singleTotalOff(INSTR::GetElementPtr *gep, Operand *curAddrVR, int totalOff);

    void genGepAdd(INSTR::GetElementPtr *gep, Operand *curAddrVR, int offBase1, int offBase2);

    void dealCall(INSTR::Call *call_inst);

    static bool LdrStrImmEncode(int off);

    static bool vLdrStrImmEncode(int off);

    static bool AddSubImmEncode(int off);

    void genGlobal();

    bool isFBino(INSTR::Alu::Op op);

    void genBinaryInst(INSTR::Alu *instr);

    void genCmp(Instr *instr);

    static Arm::Cond getIcmpOppoCond(Arm::Cond cond);

    static Arm::Cond getFcmpOppoCond(Arm::Cond cond);

    static bool immCanCode(unsigned int imm);

    static bool floatCanCode(float imm);

    Operand *newSVR();

    Operand *newVR();

    Operand *newSVR(Value *value);

    Operand *newVR(Value *value);

    Operand *getVR_no_imm(Value *value);

    Operand *getImmVR(int imm);

    Operand *getFConstVR(ConstantFloat *constF);

    Operand *getOp_may_imm(Value *value);

    Operand *getVR_may_imm(Value *value);

    Operand *getVR_from_ptr(Value *value);

};

#endif //FASTER_MEOW_CODEGEN_H
