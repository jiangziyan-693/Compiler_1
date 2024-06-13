#ifndef ARRAY_GVN_GCM_H
#define ARRAY_GVN_GCM_H
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
class ArrayGVNGCM {
public:
    std::set<Instr*>* pinnedInstr = new std::set<Instr*>();
    std::unordered_map<BasicBlock*, Instr*>* insertPos = new std::unordered_map<BasicBlock*, Instr*>();
    static std::set<Instr*>* know;
    BasicBlock* root;
    Function* function;
    Value* array;
    std::set<Instr*>* canGVNGCM;

    std::unordered_map<std::string, Instr *> *GvnMap = new std::unordered_map<std::string, Instr *>();
    std::unordered_map<std::string, int> *GvnCnt = new std::unordered_map<std::string, int>();


    void Init();
    void GCM();
    void GVN();
    void globalArrayGVN(Value* array, Function* function);
    void RPOSearch(BasicBlock* bb);
    void add(std::string str, Instr* instr);
    void remove(std::string str);
    bool addLoadToGVN(Instr* load);
    void removeLoadFromGVN(Instr* load);
    void scheduleEarlyForFunc(Function* function);
    void scheduleEarly(Instr* instr);
    void scheduleLateForFunc(Function* function);
    void scheduleLate(Instr* instr);
    void move();
    BasicBlock* findLCA(BasicBlock* a, BasicBlock* b);
    Instr* findInsertPos(Instr* instr, BasicBlock* bb);
    bool isPinned(Instr* instr);

public:
    ArrayGVNGCM(Function* function, std::set<Instr*>* canGVNGCM);

    void Run();
};
#endif