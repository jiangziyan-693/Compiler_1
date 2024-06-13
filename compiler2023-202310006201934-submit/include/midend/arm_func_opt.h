//
// Created by start on 23-8-12.
//

#ifndef FASTER_MEOW_ARM_FUNC_OPT_H
#define FASTER_MEOW_ARM_FUNC_OPT_H
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

class arm_func_opt {
public:
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue *, Initial *>* globals;
    std::unordered_map<Function*, int>* log_num = new std::unordered_map<Function*, int>();
    BasicBlock *A = nullptr, *B = nullptr, *C = nullptr, *D = nullptr, *E = nullptr,
                *F = nullptr, *G = nullptr, *H = nullptr, *I = nullptr, *J = nullptr,
                *K = nullptr, *L = nullptr, *M = nullptr;
    Value *v_A = nullptr, *v_B = nullptr, *v_C = nullptr,
            *v_D = nullptr, *v_E = nullptr, *v_F = nullptr, *N = nullptr;
    Value *param_0 = nullptr, *param_1 = nullptr, *param_2 = nullptr, *param_3 = nullptr;
    std::set<int> mod_set {
        100007, 998244353,
        19260817, 18375299,
        18373046, 20373481,
        18375354
    };
    std::vector<Instr*>* instrs = new std::vector<Instr*>();
    int mod = 0;
    int pow_n = 0;
    arm_func_opt(std::vector<Function*>* functions, std::unordered_map<GlobalValue *, Initial *>* globals);
    void Run();
    void memorize();
    bool can_memorize(Function* function);
    void ustat();
    void llmmod();
    bool is_ustat(Function* function);
    bool is_llmmod(Function* function);
    bool is_rem_const(Instr* instr);
    int bb_instr_num(BasicBlock* bb);
    bool is_zero(Value* value);
    bool is_2_pow_sub_1(Value* value);
    int get_log(Value* value);
    Instr* get_instr_at(BasicBlock* bb, int index);
    void patten_A();
    bool is_patten_A(BasicBlock* bb);
    void patten_B();
    bool try_simplify_loop_nest(BasicBlock* bb);
    void patten_C();
    void check_patten_C(BasicBlock* bb);
    int get_n(int num);
};


#endif //FASTER_MEOW_ARM_FUNC_OPT_H
