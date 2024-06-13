#ifndef MEMSET_OPTIMIZE_H
#define MEMSET_OPTIMIZE_H
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
class MemSetOptimize {
public:
    std::vector<Function*>* functions;
    std::set<Instr*>* idcPhis = new std::set<Instr*>();
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    std::set<Loop*>* globalArrayInitLoops = new std::set<Loop*>();
    std::set<Loop*>* localArrayInitLoops = new std::set<Loop*>();
    std::set<Instr*>* canGVN = new std::set<Instr*>();

    std::unordered_map<std::string, Instr*>* GvnMap = new std::unordered_map<std::string, Instr*>();
    std::unordered_map<std::string, int>* GvnCnt = new std::unordered_map<std::string, int>();
    std::set<Loop*>* allLoop = new std::set<Loop*>();
    MemSetOptimize(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
    void init();
    void globalArrayFold();
    void initLoopToMemSet();
    bool check(Value* array);
    void getLoops(Loop* loop, std::set<Loop*>* loops);
    bool checkArrayInit(Loop* loop);
};
#endif