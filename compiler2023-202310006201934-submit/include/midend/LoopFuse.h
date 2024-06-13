//
// Created by start on 23-7-29.
//

#ifndef MAIN_LOOPFUSE_H
#define MAIN_LOOPFUSE_H

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
class LoopFuse {
public:
    std::vector<Function*>* functions;
    std::set<Loop*>* loops = new std::set<Loop*>();
    std::set<Loop*>* removes = new std::set<Loop*>();
    LoopFuse(std::vector<Function*>* functions);
    void Run();
    void init();
    void loopFuse();
    void tryFuseLoop(Loop* preLoop);
    bool hasReflectInstr(std::unordered_map<Value*, Value*>* map, Instr* instr, std::set<Instr*>* instrs);
    bool check(Instr* A, Instr* B, std::unordered_map<Value*, Value*>* map);
    Value* getReflectValue(Value* value, std::unordered_map<Value*, Value*>* map);
    bool useOutLoop(Instr* instr, Loop* loop);
};

#endif //MAIN_LOOPFUSE_H
