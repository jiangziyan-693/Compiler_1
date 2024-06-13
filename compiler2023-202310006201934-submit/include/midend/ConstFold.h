//
// Created by start on 23-7-29.
//

#ifndef MAIN_CONSTFOLD_H
#define MAIN_CONSTFOLD_H

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
class ConstFold {
public:
    std::vector<Function*>* functions;
    std::unordered_map<GlobalValue*, Initial*>* globalValues;
    std::set<Value*>* constGlobalPtrs = new std::set<Value*>();
    ConstFold(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues);
    void Run();
    void condConstFold();
    void condConstFoldForFunc(Function* function);
    void globalConstPtrInit();
    bool globalArrayIsConst(Value* value);
    bool check(Instr* instr);
    void arrayConstFold();
    void arrayConstFoldForFunc(Function* function);
    Value* getGlobalConstArrayValue(Value* ptr, std::vector<Value*>* indexs);
    void getArraySize(Type* type, std::vector<int>* ret);
    void singleBBMemoryFold();
    void singleBBMemoryFoldForBB(BasicBlock* bb);
};


#endif //MAIN_CONSTFOLD_H
