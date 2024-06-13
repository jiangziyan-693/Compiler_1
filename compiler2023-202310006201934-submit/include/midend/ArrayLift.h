#ifndef ARRAY_LIFT_H
#define ARRAY_LIFT_H
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
class ArrayLift {
public:
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    static int length_low_line;
    ArrayLift(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
};
#endif