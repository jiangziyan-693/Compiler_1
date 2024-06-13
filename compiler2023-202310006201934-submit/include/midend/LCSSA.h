//
// Created by start on 23-7-29.
//

#ifndef MAIN_LCSSA_H
#define MAIN_LCSSA_H


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
#include <stack>
class LCSSA {
public:
    std::vector<Function*>* functions;
    LCSSA(std::vector<Function*>* functions);
    void Run();
    void addPhi();
    void addPhiForFunc(Function* function);
    void addPhiForLoop(Loop* loop);
    void addDef(std::set<Instr*>* defs, Instr* instr);
    void addPhiAtExitBB(Value* value, BasicBlock* bb, Loop* loop);
    void RenameDFS(std::stack<Value*>* S, BasicBlock* X, std::unordered_map<Instr*, int>* useInstrMap, std::set<Instr*>* defInstrs);
    Value* getStackTopValue(std::stack<Value*>* S);
    bool usedOutLoop(Value* value, Loop* loop);
};


#endif //MAIN_LCSSA_H
