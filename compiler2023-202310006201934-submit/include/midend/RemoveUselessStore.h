#ifndef REMOVE_USELESS_PHI_H
#define REMOVE_USELESS_PHI_H
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
class RemoveUselessStore {
public:
    std::vector<Function*>* functions;
    RemoveUselessStore(std::vector<Function*>* functions);
    void Run();
};
#endif