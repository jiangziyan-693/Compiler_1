#ifndef REM_DIV_MUL_SUB_H
#define REM_DIV_MUL_SUB_H
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
class Rem2DivMulSub {
public:
    std::vector<Function*>* functions;
    std::set<INSTR::Alu*>* rems = new std::set<INSTR::Alu*>();
    std::set<INSTR::Alu*>* frems = new std::set<INSTR::Alu*>();
    Rem2DivMulSub(std::vector<Function*>* functions);
    void Run();
    void init();
    void transRemToDivMulSub();
};
#endif