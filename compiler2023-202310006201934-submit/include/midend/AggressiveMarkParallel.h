#ifndef AGGRESSIVE_MARK_PARALLEL_H
#define AGGRESSIVE_MARK_PARALLEL_H
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
class AggressiveMarkParallel {
public:
    std::vector<Function*>* functions;
    static int parallel_num;
    std::set<BasicBlock*>* know = new std::set<BasicBlock*>();
    std::set<Function*>* goodFunc = new std::set<Function*>();
    std::unordered_map<Function*, std::set<Value*>*>* funcLoadArrays = new std::unordered_map<Function*, std::set<Value*>*>();
    AggressiveMarkParallel(std::vector<Function*>* functions);
    void Run();
    void init();
    bool check(Function* function);
    bool isPureLoop(Loop* loop);
    void markLoop(Loop* loop);
    bool useOutLoops(Value* value, std::set<Loop*>* loops);
};
#endif