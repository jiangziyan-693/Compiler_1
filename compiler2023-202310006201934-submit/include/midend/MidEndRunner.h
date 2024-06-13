//
// Created by start_0916 on 23-7-20.
//

#ifndef MAIN_MIDENDRUNNER_H
#define MAIN_MIDENDRUNNER_H

#include <vector>
#include "../mir/Function.h"
#include "GlobalVal.h"
#include "Manager.h"
class MidEndRunner {
public:
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue *, Initial *>* globalValues = Manager::MANAGER->globals;
    MidEndRunner(std::vector<Function*>* functions);
    ~MidEndRunner() = default;
    void Run(int opt);
    void base_opt();
    void merge_simple_bb();
    void re_calc();
    void merge_bb();
    void if_comb();
    void simple_calc();
    void loop_opt();
    void br_opt();
    void output(std::string filename);
    void gep_fuse();
    void gep_split();
    void loop_strength_reduction();
    void loop_fold();
    void loop_fuse();
    void loop_in_var_code_lift();
    void global_array_gvn();
    void local_array_gvn();
    void func_gvn();
    void func_gcm();
    void remove_use_same_phi();
    void simplify_phi();
    void shorter_lift_time();
    void rem_2_div_mul_sub();
    void br_opt_use_at_end();
    void test_peephole();
    void mark_parallel();
    void do_parallel();
    void dlnce();
    void add_idc_instr(std::set<Instr*>* instr, Loop* loop);
};

#endif //MAIN_MIDENDRUNNER_H
