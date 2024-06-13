//
// Created by start on 23-7-29.
//

#ifndef MAIN_LOOPIDCVARINFO_H
#define MAIN_LOOPIDCVARINFO_H

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
class LoopIdcVarInfo {
public:
    std::vector<Function*>* functions;
    LoopIdcVarInfo(std::vector<Function*>* functions);
    void Run();
    void GetInductionVar();
    void GetInductionVarForFunc(Function* function);
    void GetInductionVarForLoop(Loop* loop);
};

#endif //MAIN_LOOPIDCVARINFO_H
