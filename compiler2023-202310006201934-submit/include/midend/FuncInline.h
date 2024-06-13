//
// Created by start on 23-7-28.
//

#ifndef MAIN_FUNCINLINE_H
#define MAIN_FUNCINLINE_H

#include "../../include/mir/Value.h"
#include "../../include/mir/Instr.h"
#include "../../include/mir/Function.h"
#include "../../include/mir/BasicBlock.h"
#include "../../include/mir/Constant.h"
#include "../../include/mir/Use.h"
#include "../../include/mir/GlobalVal.h"
#include "../../include/mir/Loop.h"
#include <vector>
#include <queue>
#include <unordered_map>
#include <set>
#include <algorithm>
class FuncInline {
public:
    std::vector<Function*>* functions;
    std::vector<Function*>* funcCanInline = new std::vector<Function*>();
    std::set<Function*>* funcCallSelf = new std::set<Function*>();
    std::unordered_map<Function*, std::set<Function*>*>* reserveMap = new std::unordered_map<Function*, std::set<Function*>*>();
    std::unordered_map<Function*, int>* inNum = new std::unordered_map<Function*, int>();
    std::unordered_map<Function*, std::set<Function*>*>* Map = new std::unordered_map<Function*, std::set<Function*>*>();
    std::queue<Function*>* queue  = new std::queue<Function*>();
    FuncInline(std::vector<Function*>* functions);
    void Run();
    void GetFuncCanInline();
    void makeReserveMap();
    void topologySortStrong();
    void topologySort();
    bool canInline(Function* function);
    void inlineFunc(Function* function);
    void transCallToFunc(Function* function, INSTR::Call* call);
};

#endif //MAIN_FUNCINLINE_H
