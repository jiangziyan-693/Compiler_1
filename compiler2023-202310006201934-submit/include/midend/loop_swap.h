//
// Created by start on 23-8-12.
//

#ifndef FASTER_MEOW_LOOP_SWAP_H
#define FASTER_MEOW_LOOP_SWAP_H
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
#include <stack>


class loop_swap {
public:
    int weight = 32;
    std::vector<Function*>* functions;
    std::vector<int>* score = new std::vector<int>();
    std::vector<std::vector<int>*>* permutations = new std::vector<std::vector<int>*>();
    std::vector<INSTR::GetElementPtr*>* geps = new std::vector<INSTR::GetElementPtr*>();
//    std::vector<std::vector<Value *> *>* sorted = new std::vector<std::vector<Value *> *>();
    std::vector<std::vector<Value *> *>* base = new std::vector<std::vector<Value *> *>();
    std::vector<Instr*>* move = new std::vector<Instr*>();
    loop_swap(std::vector<Function*>* functions);
    void Run();
    void swap_nn_loop();
    void swap_nest_loop();
    void cost_model(Value* i, Value* j, Value* k);
    void init_permutations(int n);
    void add_instr_in_loop(Loop* loop);
};

#endif //FASTER_MEOW_LOOP_SWAP_H
