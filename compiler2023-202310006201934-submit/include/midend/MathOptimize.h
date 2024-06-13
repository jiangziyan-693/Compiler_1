#ifndef MATH_OPTIMIZE_H
#define MATH_OPTIMIZE_H
#include "../../include/mir/Value.h"
#include "../../include/mir/Instr.h"
#include "../../include/mir/Function.h"
#include "../../include/mir/BasicBlock.h"
#include "../../include/mir/Constant.h"
#include "../../include/mir/Use.h"
#include "../../include/mir/GlobalVal.h"
#include "../../include/mir/Loop.h"
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
class MathOptimize {
public:
    static int adds_to_mul_boundary;
    std::vector<Function*>* functions;
    //HashSet<MulPair> mulSet;
    std::unordered_map<Instr*, int>* mulMap = new std::unordered_map<Instr*, int>();
    std::unordered_map<Instr*, std::string>* fmulMap = new std::unordered_map<Instr*, std::string>();
    MathOptimize(std::vector<Function*>* functions);
    void Run();
    void continueAddToMul();
    void continueFAddToMul();
    void continueFAddToMulForInstr(Instr* instr);
    bool faddCanTransToMul(Instr* instr, Value* value);
    void continueAddToMulForInstr(Instr* instr);
    bool addCanTransToMul(Instr* instr, Value* value);
    Instr* getOneUser(Instr* instr);
    void addZeroFold();
    void mulDivFold();
    void RPOSearch(BasicBlock* bb);
    void faddZeroFold();
    void fmulFDivFold();
    void RPOSearchFloat(BasicBlock* bb);
};
#endif