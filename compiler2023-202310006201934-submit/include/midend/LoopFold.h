//
// Created by start on 23-7-29.
//

#ifndef MAIN_LOOPFOLD_H
#define MAIN_LOOPFOLD_H

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
class LoopFold {
public:
    std::vector<Function*>* functions;
    std::set<Loop*>* loops = new std::set<Loop*>();
    std::set<Loop*>* removes = new std::set<Loop*>();
    LoopFold(std::vector<Function*>* functions);
    void Run();
    void calcLoopInit(Loop* loop);
    void tryFoldLoop(Loop* loop);
    bool useOutLoop(Instr* instr, Loop* loop);
};

#endif //MAIN_LOOPFOLD_H
