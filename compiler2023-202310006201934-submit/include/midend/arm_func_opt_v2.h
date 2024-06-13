//
// Created by start on 23-8-21.
//

#ifndef FASTER_MEOW_ARM_FUNC_OPT_V2_H
#define FASTER_MEOW_ARM_FUNC_OPT_V2_H


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

class arm_func_opt_v2 {
public:
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue *, Initial *>* globals;
    arm_func_opt_v2(std::vector<Function*>* functions, std::unordered_map<GlobalValue *, Initial *>* globals);
    Function* pure_calc_func = nullptr;
    Loop* loop_only_once = nullptr;
    INSTR::Call* call_pure_func = nullptr;
    Instr* up_idc_phi = nullptr;
    std::set<Instr*>* st_ld = new std::set<Instr*>();
    bool tag = true;
    void Run();
    bool check_loop_only_once(Loop* loop);
    bool check_pure_calc_func(Function* function);
    void init();
    void check_all_st_ld(Instr* alloc);
    bool check(BasicBlock* bb);
    int bb_instr_num(BasicBlock *bb);
    Instr* get_instr_at(BasicBlock *bb, int index);
};
#endif //FASTER_MEOW_ARM_FUNC_OPT_V2_H
