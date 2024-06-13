//
// Created by start on 23-7-29.
//

#ifndef MAIN_REMOVEPHI_H
#define MAIN_REMOVEPHI_H

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
class RemovePhi {
public:
    std::vector<Function*>* functions;
    RemovePhi(std::vector<Function*>* functions);
    void Run();
    void RemovePhiAddPCopy();
    void ReplacePCopy();
    void removeFuncPhi(Function* function);
    void addMidBB(BasicBlock* src, BasicBlock* mid, BasicBlock* tag);
    void replacePCopyForFunc(Function* function);
    bool checkPCopy(std::vector<Value*>* tag, std::vector<Value*>* src);
    void removeUndefCopy(std::vector<Value*>* tag, std::vector<Value*>* src,
                         std::set<std::string>* tagNames, std::set<std::string>* srcNames);
};

#endif //MAIN_REMOVEPHI_H
