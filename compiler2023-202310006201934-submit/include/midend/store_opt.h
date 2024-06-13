//
// Created by start on 23-8-20.
//

#ifndef FASTER_MEOW_STORE_OPT_H
#define FASTER_MEOW_STORE_OPT_H
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
class store_opt {
public:
    std::vector<Function*>* functions;
    std::set<Instr*>* atomic_array = new std::set<Instr*>();
    std::set<Instr*>* geps = new std::set<Instr*>();
    Value* st_val = nullptr;
    store_opt(std::vector<Function*>* functions);
    void Run();
    void patten_A();
    bool is_patten_A(Instr* instr);
    bool check_A(Instr* instr);
    bool A_after_B(Instr *A, Instr *B);
    bool store_once_at_head(Instr* instr);
};
#endif //FASTER_MEOW_STORE_OPT_H
