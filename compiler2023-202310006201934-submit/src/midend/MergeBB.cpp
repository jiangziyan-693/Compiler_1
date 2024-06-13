#include "../../include/midend/MergeBB.h"
#include "../../include/util/CenterControl.h"


MergeBB::MergeBB(std::vector<Function *> *functions) {
    this->functions = functions;
}

void MergeBB::Run() {
    std::set<BasicBlock *> *removes = new std::set<BasicBlock *>();
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            if ((bb->getBeginInstr() == bb->getEndInstr()) &&
                dynamic_cast<INSTR::Jump *>(bb->getBeginInstr()) != nullptr &&
                bb->function->getBeginBB() != bb) {
                removes->insert(bb);
            }
        }
    }
    for (BasicBlock *bb: *removes) {
        INSTR::Jump *jump = (INSTR::Jump *) bb->getBeginInstr();
        BasicBlock *target = jump->getTarget();
        for (BasicBlock *pre: *bb->precBBs) {
            pre->modifyBrAToB(bb, target);
            target->precBBs->push_back(pre);
        }
        target->precBBs->erase(
                std::remove(target->precBBs->begin(), target->precBBs->end(), bb),
                            target->precBBs->end());
//            target->precBBs->erase(bb);
        bb->remove();
    }
}
