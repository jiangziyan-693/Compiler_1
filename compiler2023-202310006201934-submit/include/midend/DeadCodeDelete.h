//
// Created by start on 23-7-28.
//

#ifndef MAIN_DEADCODEDELETE_H
#define MAIN_DEADCODEDELETE_H


#include "../../include/mir/Value.h"
#include "../../include/mir/Instr.h"
#include "../../include/mir/Function.h"
#include "../../include/mir/BasicBlock.h"
#include "../../include/mir/Constant.h"
#include "../../include/mir/Use.h"
#include "../../include/mir/GlobalVal.h"
#include "../../include/mir/Loop.h"
#include <vector>
#include <stack>
#include <unordered_map>
#include <set>
#include <algorithm>
class DeadCodeDelete {
public:
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    std::unordered_map<Instr*, Instr*>* root;
    std::unordered_map<Instr*, int>* deep;
    DeadCodeDelete(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
    void removeUselessRet();
    bool retCanRemove(Function* function);
    void check_instr();
    void noUserCodeDelete();
    void deadCodeElimination();
    std::unordered_map<Instr*, std::set<Instr*>*>* edge = new std::unordered_map<Instr*, std::set<Instr*>*>();
    std::unordered_map<Instr*, int>* low = new std::unordered_map<Instr*, int>();
    std::unordered_map<Instr*, int>* dfn = new std::unordered_map<Instr*, int>();
    std::stack<Instr*>* stack = new std::stack<Instr*>();
    std::unordered_map<Instr*, bool>* inStack = new std::unordered_map<Instr*, bool>();
    std::unordered_map<Instr*, int>* color = new std::unordered_map<Instr*, int>();
    std::unordered_map<int, std::set<Instr*>*>* instr_in_color = new std::unordered_map<int, std::set<Instr*>*>();
    std::unordered_map<int, int>* color_in_deep = new std::unordered_map<int, int>();
    std::set<Instr*>* all_instr = new std::set<Instr*>();
    //HashSet<Instr*> know_tarjan = new std::set<>();
    int color_num = 0;
    int index = 0;
    void strongDCE();
    void strongDCEForFunc(Function* function);
    void tarjan(Instr* x);
    void deadCodeEliminationForFunc(Function* function);
    Instr* find(Instr* x);
    void union_(Instr* a, Instr* b);
    bool hasEffect(Instr* instr);
    void removeUselessGlobalVal();
    void deleteInstr(Instr* instr, bool tag);
    void removeUselessLocalArray();
    bool tryRemoveUselessArray(Value* value);
    bool check(Value* value, std::set<Instr*>* know);
    void removeUselessLoop();
    bool tryRemoveLoop(Loop* loop);
    bool loopCanRemove(Loop* loop);
    bool hasStrongEffect(Instr* instr);
};

#endif //MAIN_DEADCODEDELETE_H
