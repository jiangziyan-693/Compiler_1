//
// Created by start on 23-7-28.
//

#ifndef MAIN_LOOPUNROLL_H
#define MAIN_LOOPUNROLL_H

#include "../../include/mir/Value.h"
#include "../../include/mir/Instr.h"
#include "../../include/mir/Function.h"
#include "../../include/mir/BasicBlock.h"
#include "../../include/mir/Constant.h"
#include "../../include/mir/Use.h"
#include "../../include/mir/GlobalVal.h"
#include "../../include/mir/Loop.h"
#include "../../include/mir/CloneInfoMap.h"
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
class LoopUnRoll {
public:
    static int loop_unroll_up_lines;
    std::vector<Function*>* functions;
    std::unordered_map<Function*, std::set<Loop*>*>* loopsByFunc = new std::unordered_map<Function*, std::set<Loop*>*>();
    std::unordered_map<Function*, std::vector<Loop*>*>* loopOrdered = new std::unordered_map<Function*, std::vector<Loop*>*>();
    LoopUnRoll(std::vector<Function*>* functions);
    void init();
    void DFSForLoopOrder(Function* function, Loop* loop);
    void Run();
    void constLoopUnRoll();
    void constLoopUnRollForFunc(Function* function);
    void constLoopUnRollForLoop(Loop* loop);
    void checkDFS(Loop* loop, std::set<BasicBlock*>* allBB, std::set<BasicBlock*>* exits);
    void DoLoopUnRoll(Loop* loop);
    void getAllBBInLoop(Loop* loop, std::vector<BasicBlock*>* bbs);
    int cntDFS(Loop* loop);
    void DoLoopUnRollForSeveralTimes(Loop* loop, int codeCnt);
    int getUnrollTime(int times, int codeCnt);
};
#endif //MAIN_LOOPUNROLL_H
