//
// Created by start on 23-7-29.
//

#ifndef MAIN_GEPSPLIT_H
#define MAIN_GEPSPLIT_H

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
class GepSplit {
public:
    std::vector<Function*>* functions;
    GepSplit(std::vector<Function*>* functions);
    void Run();
    void _GepSplit();
    void gepSplitForFunc(Function* function);
    void split(INSTR::GetElementPtr* gep);
    void RemoveUselessGep();
    bool isZeroOffsetGep(INSTR::GetElementPtr* gep);
};


#endif //MAIN_GEPSPLIT_H
