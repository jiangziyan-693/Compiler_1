#ifndef GVN_GCM_H
#define GVN_GCM_H
#include "../../include/mir/Value.h"
#include "../../include/mir/Instr.h"
#include "../../include/mir/Function.h"
#include "../../include/mir/BasicBlock.h"
#include "../../include/mir/Constant.h"
#include "../../include/mir/Use.h"
#include "../../include/mir/GlobalVal.h"
#include <vector>
#include <unordered_map>
#include <set>
#include <algorithm>
class GVNAndGCM {
public:
    std::vector<Function*>* functions;
    std::unordered_map<Function*, std::set<Instr*>*>* pinnedInstrMap = new std::unordered_map<Function*, std::set<Instr*>*>();
    std::unordered_map<BasicBlock*, Instr*>* insertPos = new std::unordered_map<BasicBlock*, Instr*>();
    static std::set<Instr*>* know;
    BasicBlock* root;
    std::unordered_map<std::string, Instr*>* GvnMap = new std::unordered_map<std::string, Instr*>();
    std::unordered_map<std::string, int>* GvnCnt = new std::unordered_map<std::string, int>();
    GVNAndGCM(std::vector<Function*>* functions);
    void Run();
    void Init();
    void GCM();
    void GVN();
    void do_gvn();
    void RPOSearch(BasicBlock* bb);
    bool canGVN(Instr* instr);
    void schedule_early();
    void schedule_early_instr(Instr* instr);
    void schedule_late();
    void schedule_late_instr(Instr* instr);
    void move();
    BasicBlock* find_lca(BasicBlock* a, BasicBlock* b);
    Instr* findInsertPos(Instr* instr, BasicBlock* bb);
    bool isPinned(Instr* instr);
    void add(std::string str, Instr* instr);
    void remove(std::string str);
    bool addInstrToGVN(Instr* instr);
    void removeInstrFromGVN(Instr* instr);
    Constant* calc(INSTR::Alu* alu);
};
#endif