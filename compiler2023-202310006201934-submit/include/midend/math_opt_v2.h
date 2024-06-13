//
// Created by start on 23-8-11.
//

#ifndef FASTER_MEOW_MATH_OPT_V2_H
#define FASTER_MEOW_MATH_OPT_V2_H

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


class math_opt_v2 {
public:
    std::vector<Function*>* functions;
    std::set<INSTR::Alu*>* one_const_add = new std::set<INSTR::Alu*>();
    math_opt_v2(std::vector<Function*>* functions);
    void Run();
    void init();
    void alu_fold();
    void alu_fold_v2();

    int get_const_value(Instr* instr);
    Value* get_none_const_value(Instr* instr);
    bool is_add_none_const(Instr* instr);
    bool is_A_add_B(Instr* instr, Value* A, Value* B);
    bool is_A_add_1(Instr* instr, Value* A);
    bool is_A_sub_1(Instr* instr, Value* A);
    bool is_const_1(Value* A);
    bool A_after_B(Instr* A, Instr* B);
    void patten_1(Instr* instr);

    void patten_2(Instr* instr);
};


#endif //FASTER_MEOW_MATH_OPT_V2_H
