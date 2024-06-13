#ifndef AGGRESSIVE_FUNC_GCM_H
#define AGGRESSIVE_FUNC_GCM_H
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
class AggressiveFuncGCM {
public:
    std::vector<Function*>* functions;
    std::set<Function*>* canGCMFunc = new std::set<Function*>();
    std::set<Instr*>* canGCMInstr = new std::set<Instr*>();
    static std::set<Instr*>* know;
    BasicBlock* root;
    std::unordered_map<Function*, std::set<Instr*>*>* pinnedInstrMap = new std::unordered_map<Function*, std::set<Instr*>*>();
    std::unordered_map<Function*, std::set<int>*>* use = new std::unordered_map<Function*, std::set<int>*>();
    std::unordered_map<Function*, std::set<int>*>* def = new std::unordered_map<Function*, std::set<int>*>();
    std::unordered_map<Function*, std::set<Value*>*>* useGlobals = new std::unordered_map<Function*, std::set<Value*>*>();
    std::unordered_map<Function*, std::set<Value*>*>* defGlobals = new std::unordered_map<Function*, std::set<Value*>*>();
    AggressiveFuncGCM(std::vector<Function*>* functions);
    void Run();
    void init();
    bool check(Function* function);
    void GCM();
    void noUserCallDelete();
    void scheduleEarlyForFunc(Function* function);
    void scheduleEarly(Instr* instr);
    void scheduleLateForFunc(Function* function);
    void scheduleLate(Instr* instr);
    void move();
    BasicBlock* findLCA(BasicBlock* a, BasicBlock* b);
    Instr* findInsertPos(Instr* instr, BasicBlock* bb);
    bool canGCM(Instr* instr);
    bool isPinned(Instr* instr);
};
#endif