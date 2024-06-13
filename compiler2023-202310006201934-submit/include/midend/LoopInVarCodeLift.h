//
// Created by start on 23-7-29.
//

#ifndef MAIN_LOOPINVARCODELIFT_H
#define MAIN_LOOPINVARCODELIFT_H


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
class LoopInVarCodeLift {
public:
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    std::unordered_map<INSTR::Alloc*, std::set<Instr*>*>* allocDefs = new std::unordered_map<INSTR::Alloc*, std::set<Instr*>*>();
    std::unordered_map<INSTR::Alloc*, std::set<Instr*>*>* allocUsers = new std::unordered_map<INSTR::Alloc*, std::set<Instr*>*>();
    std::unordered_map<Value*, std::set<Instr*>*>* defs = new std::unordered_map<Value*, std::set<Instr*>*>();
    std::unordered_map<Value*, std::set<Instr*>*>* users = new std::unordered_map<Value*, std::set<Instr*>*>();
    std::unordered_map<Value*, std::set<Loop*>*>* defLoops = new std::unordered_map<Value*, std::set<Loop*>*>();
    std::unordered_map<Value*, std::set<Loop*>*>* useLoops = new std::unordered_map<Value*, std::set<Loop*>*>();
    std::set<Instr*>* loadCanGCM = new std::set<Instr*>();
    std::unordered_map<Function*, std::set<Function*>*>* callMap = new std::unordered_map<Function*, std::set<Function*>*>();
    //HashMap<Value*, std::set<Function*>*>* useGlobalFuncs = new std::unordered_map<>();
    //HashMap<Value*, std::set<Function*>*>* defGlobalFuncs = new std::unordered_map<>();
    std::unordered_map<Function*, std::set<Value*>*>* funcUseGlobals = new std::unordered_map<Function*, std::set<Value*>*>();
    std::unordered_map<Function*, std::set<Value*>*>* funcDefGlobals = new std::unordered_map<Function*, std::set<Value*>*>();
    LoopInVarCodeLift(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
    void init();
    void arrayConstDefLift();
    void arrayConstDefLiftForFunc(Function* function);
    void tryLift(INSTR::Alloc* alloc);
    void DFS(INSTR::Alloc* alloc, Instr* instr);
    void loopInVarLoadLift();
    void DFSArray(Value* value, Instr* instr);
    void loopInVarLoadLiftForArray(Value* array);
    bool check(Loop* useLoop, Loop* defLoop);
};

#endif //MAIN_LOOPINVARCODELIFT_H
