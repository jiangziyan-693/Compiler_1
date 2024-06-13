#include "../../include/midend/ArrayLift.h"
#include "../../include/util/CenterControl.h"




ArrayLift::ArrayLift(std::vector<Function *> *functions, std::unordered_map<GlobalValue *, Initial *> *globalValues) {
    this->functions = functions;
    this->globalValues = globalValues;
}

void ArrayLift::Run() {
    for (Function *function: *functions) {
        if (function->name == ("main") || function->onlyOneUser()) {
            for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
                for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                    if (dynamic_cast<INSTR::Alloc *>(instr) != nullptr && instr->bb->loop->getLoopDepth() == 0) {
                        //TODO:
                        Type *type = ((PointerType *) instr->type)->inner_type;
                        ZeroInit *init = new ZeroInit(type);
                        GlobalValue *val = new GlobalValue(type, "lift_" + instr->name, init);
                        (*globalValues)[val] = init;
                        instr->modifyAllUseThisToUseA(val);
                        instr->remove();
                    }
                }
            }
        }
    }
}
