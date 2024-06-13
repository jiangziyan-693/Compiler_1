//
// Created by start on 23-7-29.
//

#ifndef MAIN_LOOPSTRENGTHREDUCTION_H
#define MAIN_LOOPSTRENGTHREDUCTION_H
#include "../../include/mir/Value.h"
#include "../../include/mir/Instr.h"
#include "../../include/mir/Function.h"
#include "../../include/mir/BasicBlock.h"
#include "../../include/mir/Constant.h"
#include "../../include/mir/Use.h"
#include "../../include/mir/GlobalVal.h"
#include "../../include/mir/Loop.h"
#include "../../include/frontend//Visitor.h"
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
class LoopStrengthReduction {
public:
    std::vector<Function*>* functions;
    static int lift_times;
    LoopStrengthReduction(std::vector<Function*>* functions);
    void Run();
    void addConstToMul();
    void addConstLoopInit(Loop* loop);
    void addConstFloatToMul();
    void addConstFloatLoopInit(Loop* loop);
    void divToShift();
    void divToShiftForFunc(Function* function);
    void tryReduceLoop(Loop* loop);
    void addConstAndModToMulAndMod();
    void addConstAndModLoopInit(Loop* loop);
    bool useOutLoop(Instr* instr, Loop* loop);
    void addConstAndModToMulAndModForLoop(Loop* loop);
    void addIdcAndModToMulAndMod();
    void addIdcAndModLoopInit(Loop* loop);
    void addIdcAndModToMulAndModForLoop(Loop* loop);
    void addIdcAndModToMulAndModForLoopForBigMod(Loop* loop);
};
#endif //MAIN_LOOPSTRENGTHREDUCTION_H
