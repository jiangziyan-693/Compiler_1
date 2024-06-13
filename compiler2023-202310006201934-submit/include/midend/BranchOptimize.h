#ifndef BRANCH_OPTIMIZE_H
#define BRANCH_OPTIMIZE_H
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
class BranchOptimize {
public:
    std::vector<Function*>* functions;
    BranchOptimize(std::vector<Function*>* functions);
    void Run();
    void remakeCFG();
    void RemoveUselessPHI();
    void RemoveUselessJump();
    void ModifyConstBranch();
    void RemoveBBOnlyJump();
    void removeUselessPHIForFunc(Function* function);
    void removeUselessJumpForFunc(Function* function);
    void modifyConstBranchForFunc(Function* function);
    void removeBBOnlyJumpForFunc(Function* function);
    void peep_hole();
};
#endif