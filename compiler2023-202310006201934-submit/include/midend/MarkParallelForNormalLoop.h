#ifndef MARK_PARALLEL_FOR_NORMAL_LOOP_H
#define MARK_PARALLEL_FOR_NORMAL_LOOP_H
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
class MarkParallelForNormalLoop {
public:
    std::vector<Function*>* functions;
    static int parallel_num;
    std::set<BasicBlock*>* know = new std::set<BasicBlock*>();
    MarkParallelForNormalLoop(std::vector<Function*>* functions);
    void Run();
    bool isPureLoop(Loop* loop);
    void markLoop(Loop* loop);
    void DFS(std::set<BasicBlock*>* bbs, std::set<Value*>*  idcVars, std::set<Loop*>* loops, Loop* loop);
    bool useOutLoops(Value* value, std::set<Loop*>* loops);
    void doMarkParallel(Loop* loop);
    void markLoopDebug(Loop* loop);
};
#endif