#ifndef GLOBAL_ARRAY_GVN_H
#define GLOBAL_ARRAY_GVN_H
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
class GlobalArrayGVN {
public:
    std::unordered_map<std::string, Instr*>* GvnMap = new std::unordered_map<std::string, Instr*>();
    std::unordered_map<std::string, int>* GvnCnt = new std::unordered_map<std::string, int>();
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    std::unordered_map<GlobalValue*, std::set<Instr*>*>* defs = new std::unordered_map<GlobalValue*, std::set<Instr*>*>();
    std::unordered_map<GlobalValue*, std::set<Instr*>*>* loads = new std::unordered_map<GlobalValue*, std::set<Instr*>*>();
    GlobalArrayGVN(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
    void init();
    void DFS(GlobalValue* array, Instr* instr);
    void GVN();
    void globalArrayGVN(Function* function);
    void RPOSearch(BasicBlock* bb);
    void add(std::string str, Instr* instr);
    void remove(std::string str);
    bool addLoadToGVN(Instr* load);
    void removeLoadFromGVN(Instr* load);
};
#endif