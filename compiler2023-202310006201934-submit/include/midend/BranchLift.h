#ifndef BRANCH_LIFT_H
#define BRANCH_LIFT_H
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
class BranchLift {
public:
    std::vector<Function*>* functions;
    BranchLift(std::vector<Function*>* functions);
    void Run();
    void branchLift();
    void branchLiftForFunc(Function* function);
    void liftBrOutLoop(INSTR::Branch* thenBr, Loop* thenLoop);
};
#endif