#include "../../include/midend/FuncInfo.h"
#include "../../include/util/CenterControl.h"





	FuncInfo::FuncInfo(std::vector<Function*>* functions) {
        this->functions = functions;
    }

    void FuncInfo::Run() {
        for (Function* function: *functions) {
            bool ret = checkCanGVN(function);
            function->isPure = (ret);
        }
    }

    bool FuncInfo::checkCanGVN(Function* function) {
        for (Function::Param* param: *function->params) {
            if (param->type->is_pointer_type()) {
                return false;
            }
        }
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
                    return false;
                }
                for (Value* value: instr->useValueList) {
                    if (dynamic_cast<GlobalValue*>(value) != nullptr) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
