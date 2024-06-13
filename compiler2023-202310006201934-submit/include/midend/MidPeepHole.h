#ifndef MIDEND_PEEP_HOLE_H
#define MIDEND_PEEP_HOLE_H
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
class MidPeepHole {
public:
    std::vector<Function*>* functions;
    static int max_base;
    MidPeepHole(std::vector<Function*>* functions);
    void Run();
    void peepHoleA();
    void peepHoleB();
    std::set<BasicBlock*>* bb_has_store_call = new std::set<BasicBlock*>();
    BasicBlock* E = nullptr, *F = nullptr;
    void peepHoleC();
    void peepHoleCInit();
    void checkC(BasicBlock* A);
    bool patternC(BasicBlock* D, BasicBlock* G);
    void peepHoleD();
    void peepHoleE();
};
#endif