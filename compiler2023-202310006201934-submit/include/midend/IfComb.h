#ifndef IF_COMB_H
#define IF_COMB_H
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
class IfComb {
public:
    std::set<BasicBlock*>* know = new std::set<BasicBlock*>();
    std::unordered_map<Value*, int>* left = new std::unordered_map<Value*, int>();
    std::unordered_map<Value*, int>* right = new std::unordered_map<Value*, int>();
    std::unordered_map<Value*, int>* leftEq = new std::unordered_map<Value*, int>();
    std::unordered_map<Value*, int>* rightEq = new std::unordered_map<Value*, int>();
    std::vector<Function*>* functions;
    IfComb(std::vector<Function*>* functions);
    void Run();
    void PatternA();
    void DFS(BasicBlock* bb);
    std::set<Value*>* trueValue = new std::set<Value*>();
    std::set<Value*>* falseValue = new std::set<Value*>();
    void PatternB();
    void DFS_B(BasicBlock* bb);
    void PatternC();
};
#endif