#include "../../include/midend/ArraySSA.h"
#include "../../include/util/CenterControl.h"

#include "../../include/util/HashMap.h"

//只考虑局部数组
//全局可以考虑 遍历支配树, 遇到一个Array的store就删除之前的GVNhash,达到GVN的目的

ArraySSA::ArraySSA(std::vector<Function *> *functions, std::unordered_map<GlobalValue *, Initial *> *globalValues) {
    this->functions = functions;
    this->globalValues = globalValues;
}

//TODO:插入Def PHI和merge PHI
void ArraySSA::Run() {
    GetAllocs();
    //局部数组
    for (INSTR::Alloc *alloc: *allocs) {

    }

    //全局数组
    for (GlobalValue *globalValue: KeySet(*globalValues)) {
        if (((PointerType *) globalValue->type)->inner_type->is_array_type() && globalValue->canLocal()) {
            //认为入口块定义全局数组
        }
    }
}

void ArraySSA::GetAllocs() {
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Alloc *>(instr) != nullptr) {
                    allocs->insert((INSTR::Alloc *) instr);
                }
            }
        }
    }
}

void ArraySSA::insertPHIForLocalArray(Instr *instr) {

}

void ArraySSA::insertPHIForGlobalArray(GlobalValue *globalValue) {

}
