//
// Created by start on 23-8-13.
//

#ifndef FASTER_MEOW_IF_COMB_H
#define FASTER_MEOW_IF_COMB_H
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


class if_comb_v2 {
public:
    std::vector<Function*>* functions;
    std::vector<Instr*>* instrs = new std::vector<Instr*>();
    if_comb_v2(std::vector<Function*>* functions);
    BasicBlock *A = nullptr, *B = nullptr, *C = nullptr,
                *D = nullptr, *E = nullptr, *F = nullptr,
                *G = nullptr, *H = nullptr, *head = nullptr;
    void Run();
    void patten_A();
    void patten_B();
    void patten_C();
    void patten_D();
    bool is_patten_A_bb(BasicBlock* bb);
    bool is_patten_B_bb(BasicBlock* bb);
    bool is_patten_C_bb(BasicBlock* bb);
    bool is_patten_D_bb(BasicBlock* bb);
    int bb_instr_num(BasicBlock* bb);
    Instr* get_instr_at(BasicBlock* bb, int index);
    Instr* get_instr_end_at(BasicBlock* bb, int index);
    bool is_all_range(Instr* cmp1, Instr* cmp2);
};


#endif //FASTER_MEOW_IF_COMB_H
