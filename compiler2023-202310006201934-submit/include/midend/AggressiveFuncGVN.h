#ifndef AGGRESSIVE_FUC_GVN_H
#define AGGRESSIVE_FUC_GVN_H
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
class AggressiveFuncGVN {
public:
    std::unordered_map<std::string, Instr *> *GvnMap = new std::unordered_map<std::string, Instr *>();
    std::unordered_map<std::string, int> *GvnCnt = new std::unordered_map<std::string, int>();

    std::vector<Function*>* functions;
    std::set<Function*>* canGVN = new std::set<Function*>();
    std::unordered_map<Function*, std::set<int>*>* use = new std::unordered_map<Function*, std::set<int>*>();
    std::unordered_map<Function*, std::set<int>*>* def = new std::unordered_map<Function*, std::set<int>*>();
    std::unordered_map<GlobalValue*, std::set<Instr*>*>* defs = new std::unordered_map<GlobalValue*, std::set<Instr*>*>();
    std::unordered_map<GlobalValue*, std::set<Instr*>*>* loads = new std::unordered_map<GlobalValue*, std::set<Instr*>*>();
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    std::unordered_map<Value*, std::set<Instr*>*>* callMap = new std::unordered_map<Value*, std::set<Instr*>*>();
    std::set<Instr*>* removes = new std::set<Instr*>();
    AggressiveFuncGVN(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
    void init();
    void initCallMap();
    void globalArrayDFS(GlobalValue* array, Instr* instr);
    void initAlloc(Value* alloc);
    void allocDFS(Value* alloc, Instr* instr);
    bool check(Function* function);
    void GVN();
    void RPOSearch(BasicBlock* bb);
    void add(std::string str, Instr* instr);
    void remove(std::string str);
    bool addCallToGVN(Instr* call);
    void removeCallFromGVN(Instr* call);
};
#endif