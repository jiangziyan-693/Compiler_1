#ifndef FUNC_INFO_H
#define FUNC_INFO_H
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
class FuncInfo {
public:
    std::vector<Function*>* functions;
    FuncInfo(std::vector<Function*>* functions);
    void Run();
    bool checkCanGVN(Function* function);
};
#endif