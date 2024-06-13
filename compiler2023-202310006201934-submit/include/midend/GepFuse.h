//
// Created by start on 23-7-29.
//

#ifndef MAIN_GEPFUSE_H
#define MAIN_GEPFUSE_H

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
class GepFuse {
public:
    std::vector<Function*>* functions;
    std::vector<INSTR::GetElementPtr*>* Geps = new std::vector<INSTR::GetElementPtr*>();
    GepFuse(std::vector<Function*>* functions);
    void Run();
    void gepFuse();
    void gepFuseForFunc(Function* function);
    bool isZeroOffsetGep(INSTR::GetElementPtr* gep);
    void fuseDFS(Instr* instr);
    void fuse();
};


#endif //MAIN_GEPFUSE_H
