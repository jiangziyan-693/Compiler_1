
//
// Created by start_0916 on 23-7-20.
//

#ifndef MAIN_MIDENDRUNNER_H
#define MAIN_MIDENDRUNNER_H

#include <vector>
#include "../mir/Function.h"
#include "GlobalVal.h"
#include "Manager.h"
//中间代码优化的函数集合
class MidEndRunner {
public:
    //这是一个指向 Function* 类型向量的指针，可能表示一组待处理的函数
    std::vector<Function*>* functions;
    //一个映射，键是 GlobalValue* 类型的全局值，值是指向 Initial* 的指针。它可能代表了全局变量或函数等的一些初始信息或状态，这部分信息来源于一个名为 Manager 的单例的 globals 成员。
    std::unordered_map<GlobalValue *, Initial *>* globalValues = Manager::MANAGER->globals;
    //接受一个指向 Function* 类型向量的指针作为参数，可能是用来初始化 functions 成员变量。
    MidEndRunner(std::vector<Function*>* functions);
    //：默认实现，不执行任何特殊的清理操作。
    ~MidEndRunner() = default;
    // 一个接收整型参数的运行函数，可能用于执行特定类型的优化，具体类型由参数 opt 指定
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
