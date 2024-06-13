#include "../../include/midend/ConstTransFold.h"
#include "../../include/util/CenterControl.h"


ConstTransFold::ConstTransFold(std::vector<Function *> *functions) {
    this->functions = functions;
}

void ConstTransFold::Run() {
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::FPtosi *>(instr) != nullptr &&
                    dynamic_cast<ConstantFloat *>(((INSTR::FPtosi *) instr)->getRVal1()) != nullptr) {
                    float val = (float) ((ConstantFloat *) ((INSTR::FPtosi *) instr)->getRVal1())->get_const_val();
                    int ret = (int) val;

                    instr->modifyAllUseThisToUseA(new ConstantInt(ret));
                    instr->remove();
                } else if (dynamic_cast<INSTR::SItofp *>(instr) != nullptr &&
                           dynamic_cast<ConstantInt *>(((INSTR::SItofp *) instr)->getRVal1()) != nullptr) {
                    int val = (int) ((ConstantInt *) ((INSTR::SItofp *) instr)->getRVal1())->get_const_val();
                    float ret = (float) val;

                    instr->modifyAllUseThisToUseA(new ConstantFloat(ret));
                    instr->remove();
                }
            }
        }
    }
}
