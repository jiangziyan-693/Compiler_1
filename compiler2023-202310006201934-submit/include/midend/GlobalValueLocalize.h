#ifndef GLOBAL_VALUE_LOCALIZE_H
#define GLOBAL_VALUE_LOCALIZE_H
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
class GlobalValueLocalize {
public:
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    std::vector<Function*>* functions;
    std::set<Value*>* removedGlobal = new std::set<Value*>();
    GlobalValueLocalize(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
    void localizeSingleValue(Value* value);
};
#endif