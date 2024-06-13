#include "../../include/midend/RemoveUselessStore.h"
#include "../../include/util/CenterControl.h"


#include "stl_op.h"

//如果想跨基本快做,需要当一个store所有的idoms都没有在下一次store前use的时候,才能删除

RemoveUselessStore::RemoveUselessStore(std::vector<Function *> *functions) {
    this->functions = functions;
}

void RemoveUselessStore::Run() {
    std::set<Instr *> *removes = new std::set<Instr *>();
    std::unordered_map<Value *, Instr *> *storeAddress = new std::unordered_map<Value *, Instr *>();
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            storeAddress->clear();
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
                    if (_contains_(storeAddress, ((INSTR::Store *) instr)->getPointer())) {
                        removes->insert(storeAddress->find(((INSTR::Store *) instr)->getPointer())->second);
                    }
                    (*storeAddress)[((INSTR::Store *) instr)->getPointer()] = instr;
                } else if (!(dynamic_cast<INSTR::Alu *>(instr) != nullptr)) {
                    storeAddress->clear();
                }
            }
        }
    }
    for (Instr *instr: *removes) {
        instr->remove();
    }
}
