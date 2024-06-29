//
// Created by start_0916 on 23-7-20.
//

#ifndef MAIN_MIDENDRUNNER_H
#define MAIN_MIDENDRUNNER_H

#include <vector>
#include "../mir/Function.h"
#include "GlobalVal.h"
#include "Manager.h"

// 中间代码优化的函数集合
class MidEndRunner {
public:
    // 指向 Function* 类型向量的指针，表示待处理的一组函数
    std::vector<Function*>* functions;

    // 一个映射，键是 GlobalValue* 类型的全局值，值是指向 Initial* 的指针
    // 可能代表了全局变量或函数的一些初始信息或状态
    std::unordered_map<GlobalValue *, Initial *>* globalValues = Manager::MANAGER->globals;

    // 构造函数，接受一个指向 Function* 类型向量的指针，用于初始化 functions 成员变量
    MidEndRunner(std::vector<Function*>* functions);

    // 默认析构函数，不执行任何特殊的清理操作
    ~MidEndRunner() = default;

    // 接收整型参数的运行函数，可能用于执行特定类型的优化，由参数 opt 指定
    void Run(int opt);

    // 基础优化
    void base_opt();

    // 合并简单基本块
    void merge_simple_bb();

    // 重新计算
    void re_calc();

    // 合并基本块
    void merge_bb();

    // if 合并
    void if_comb();

    // 简单计算优化
    void simple_calc();

    // 循环优化
    void loop_opt();

    // 分支优化
    void br_opt();

    // 输出结果到指定文件
    void output(std::string filename);

    // GEP（GetElementPtr）合并
    void gep_fuse();

    // GEP 拆分
    void gep_split();

    // 循环强度削减
    void loop_strength_reduction();

    // 循环折叠
    void loop_fold();

    // 循环融合
    void loop_fuse();

    // 循环不变代码提升
    void loop_in_var_code_lift();

    // 全局数组常量传播
    void global_array_gvn();

    // 局部数组常量传播
    void local_array_gvn();

    // 函数级全局值编号
    void func_gvn();

    // 函数级全局公共子表达式消除
    void func_gcm();

    // 移除使用相同 phi 指令
    void remove_use_same_phi();

    // 简化 phi 指令
    void simplify_phi();

    // 缩短提升时间
    void shorter_lift_time();

    // 将余数替换为除法或乘法和减法
    void rem_2_div_mul_sub();

    // 使用末端分支优化
    void br_opt_use_at_end();

    // 试验性窥孔优化
    void test_peephole();

    // 标记并行代码
    void mark_parallel();

    // 执行并行化
    void do_parallel();

    // 动态循环嵌套消除
    void dlnce();

    // 在循环中添加 IDC 指令
    void add_idc_instr(std::set<Instr*>* instr, Loop* loop);
};

#endif // MAIN_MIDENDRUNNER_H
