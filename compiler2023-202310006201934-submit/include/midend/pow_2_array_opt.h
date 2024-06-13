//
// Created by start on 23-8-11.
//

#ifndef FASTER_MEOW_POW_2_ARRAY_OPT_H
#define FASTER_MEOW_POW_2_ARRAY_OPT_H
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

class pow_2_array_opt {
public:
    std::vector<Function*>* functions;
    std::set<Instr*>* know = new std::set<Instr*>();
    pow_2_array_opt(std::vector<Function*>* functions);
    void Run();
    bool is_pow_2_array_alloc(Instr* alloc);
    void init();
    void alloc_load_DFS(Value* alloc, Instr* instr);
};



#endif //FASTER_MEOW_POW_2_ARRAY_OPT_H
