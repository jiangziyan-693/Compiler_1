//
// Created by start on 23-8-11.
//

#ifndef FASTER_MEOW_MATH_CAST_H
#define FASTER_MEOW_MATH_CAST_H


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
#include <stack>


//TODO:bitset
class math_cast {
public:
    std::vector<Function*>* functions;
    std::set<Value*>* value_1_or_0 = new std::set<Value*>();
    explicit math_cast(std::vector<Function*>* functions);
    void init();
};

#endif //FASTER_MEOW_MATH_CAST_H
