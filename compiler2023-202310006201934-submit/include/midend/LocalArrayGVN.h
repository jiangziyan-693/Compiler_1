#ifndef LOCAL_ARRAY_GVN_H
#define LOCAL_ARRAY_GVN_H
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
class LocalArrayGVN {
public:
    static bool _STRONG_CHECK_;
    static std::set<Instr*>* know;
    BasicBlock* root;
    std::unordered_map<std::string, Instr*>* GvnMap = new std::unordered_map<std::string, Instr*>();
    std::unordered_map<std::string, int>* GvnCnt = new std::unordered_map<std::string, int>();
    std::vector<Function*>* functions;
    std::set<Instr*>* instrCanGCM = new std::set<Instr*>();
    //HashMap<GlobalValue*, Initial> globalValues;
    std::string label;
    std::unordered_map<BasicBlock*, std::unordered_map<std::string, Instr*>>* GvnMapByBB = new std::unordered_map<BasicBlock*, std::unordered_map<std::string, Instr*>>();
    std::unordered_map<BasicBlock*, std::unordered_map<std::string, int>>* GvnCntByBB = new std::unordered_map<BasicBlock*, std::unordered_map<std::string, int>>();
    LocalArrayGVN(std::vector<Function*>* functions, std::string label);
    void Run();
    void Init();
    void initAlloc(Value* alloc);
    void allocDFS(Value* alloc, Instr* instr);
    void GVN();
    void localArrayGVN(Function* function);
    std::set<Function*>* goodFuncs = new std::set<Function*>();
    void GVNInit();
    bool check_good_func(Function* function);
    void RPOSearch(BasicBlock* bb);
    void add(std::string str, Instr* instr);
    void remove(std::string str);
    bool addLoadToGVNStrong(Instr* load, std::unordered_map<std::string, Instr*>* tempGvnMap);
    void removeLoadFromGVNStrong(Instr* load, std::unordered_map<std::string, Instr*>* tempGvnMap);
    bool addLoadToGVN(Instr* load);
    void removeLoadFromGVN(Instr* load);
    std::unordered_map<Value*, Instr*>* reachDef = new std::unordered_map<Value*, Instr*>();
    std::unordered_map<Function*, std::set<Instr*>*>* pinnedInstrMap = new std::unordered_map<Function*, std::set<Instr*>*>();
    void GCM();
    void GCMInit();
    void DFS(BasicBlock* bb);
    void scheduleEarlyForFunc(Function* function);
    void scheduleEarly(Instr* instr);
    void scheduleLateForFunc(Function* function);
    void scheduleLate(Instr* instr);
    BasicBlock* findLCA(BasicBlock* a, BasicBlock* b);
    Instr* findInsertPos(Instr* instr, BasicBlock* bb);
    bool isPinned(Instr* instr);
};
#endif