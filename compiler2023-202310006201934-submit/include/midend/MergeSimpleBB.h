#ifndef MERGE_SIMPLE_BB_H
#define MERGE_SIMPLE_BB_H
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
class MergeSimpleBB {
public:
    std::vector<Function*>* functions;
    std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>* preMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
    std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>* sucMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
    MergeSimpleBB(std::vector<Function*>* functions);
    void Run();
    void RemoveDeadBB();
    void MakeCFG();
    void removeFuncDeadBB(Function* function);
    void DFS(BasicBlock* bb, std::set<BasicBlock*>* know);
    void makeSingleFuncCFG(Function* function);
};
#endif