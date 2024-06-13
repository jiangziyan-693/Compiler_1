//
// Created by start on 23-7-29.
//

#ifndef MAIN_LOOPINFO_H
#define MAIN_LOOPINFO_H

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
class LoopInfo {
public:
    std::vector<Function*>* functions;
    Function* nowFunc;
    LoopInfo(std::vector<Function*>* functions);
    void Run();
    void clearLoopInfo();
    void makeInfo();
    void makeInfoForFunc(Function* function);
    void DFS(BasicBlock* bb, std::set<BasicBlock*>* know);
};

#endif //MAIN_LOOPINFO_H
