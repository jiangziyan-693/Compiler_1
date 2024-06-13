#ifndef ARRAY_SSA_H
#define ARRAY_SSA_H
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
class ArraySSA {
public:
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    std::set<INSTR::Alloc*>* allocs = new std::set<INSTR::Alloc*>();
    ArraySSA(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
    void GetAllocs();
    void insertPHIForLocalArray(Instr* instr);
    void insertPHIForGlobalArray(GlobalValue* globalValue);
};
#endif