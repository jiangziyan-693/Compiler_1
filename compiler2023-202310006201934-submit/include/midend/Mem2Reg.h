//
// Created by start on 23-7-26.
//

#ifndef MAIN_MEM2REG_H
#define MAIN_MEM2REG_H


#include "../../include/mir/Value.h"
#include "../../include/mir/Instr.h"
#include "../../include/mir/Function.h"
#include "../../include/mir/BasicBlock.h"
#include "../../include/mir/Constant.h"
#include "../../include/mir/Use.h"
#include "../../include/mir/GlobalVal.h"
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <stack>
class Mem2Reg{
public:
    std::vector<Function*>* functions;
    Mem2Reg(std::vector<Function*>* functions);
    void Run();
    void removeAlloc();
    void removeFuncAlloc(Function* function);
    void removeBBAlloc(BasicBlock* basicBlock);
    void remove(Instr* instr);
    bool check(std::set<BasicBlock*>* defBBs, std::set<BasicBlock*>* useBBs);
    void RenameDFS(std::stack<Value*>* S, BasicBlock* X, std::set<Instr*>* useInstrs, std::set<Instr*>* defInstrs);
    BasicBlock* getRandFromHashSet(std::set<BasicBlock*>* hashSet);
    Value* getStackTopValue(std::stack<Value*> *S);
};

#endif //MAIN_MEM2REG_H
