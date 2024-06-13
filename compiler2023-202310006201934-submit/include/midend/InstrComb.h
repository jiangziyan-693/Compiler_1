#ifndef INSTR_COMB_H
#define INSTR_COMB_H
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
class InstrComb {
public:
    std::vector<Function*>* functions;
    InstrComb(std::vector<Function*>* functions);
    void Run();
    void InstrCombForFunc(Function* function);
    void combSrcToTags(INSTR::Alu* src, std::vector<INSTR::Alu*>* tags);
    void combSrcToTagsFloat(INSTR::Alu* src, std::vector<INSTR::Alu*>* tags);
    void combSrcToTagsMul(INSTR::Alu* src, std::vector<INSTR::Alu*>* tags);
    void combSrcToTagsMulFloat(INSTR::Alu* src, std::vector<INSTR::Alu*>* tags);
    void combAToB(INSTR::Alu* A, INSTR::Alu* B);
    void combAToBFloat(INSTR::Alu* A, INSTR::Alu* B);
    void combAToBMul(INSTR::Alu* A, INSTR::Alu* B);
    void combAToBMulFloat(INSTR::Alu* A, INSTR::Alu* B);
};
#endif