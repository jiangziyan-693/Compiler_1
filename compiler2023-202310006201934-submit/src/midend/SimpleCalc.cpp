#include "../../include/midend/SimpleCalc.h"
#include "../../include/util/CenterControl.h"


#include "stl_op.h"


	SimpleCalc::SimpleCalc(std::vector<Function*>* functions) {
        this->functions = functions;
    }

    void SimpleCalc::Run() {
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                std::unordered_map<Value*, std::set<Value*>*>* eq = new std::unordered_map<Value*, std::set<Value*>*>();
                std::unordered_map<Value*, std::set<Value*>*>* inverse = new std::unordered_map<Value*, std::set<Value*>*>();
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    Instr* A = instr;
                    if (A->next->next != nullptr) {
                        Instr* B = (Instr*) instr->next;
                        if (dynamic_cast<INSTR::Alu*>(A) != nullptr && (((INSTR::Alu*) A)->op == INSTR::Alu::Op::ADD) &&
                                dynamic_cast<INSTR::Alu*>(B) != nullptr &&( ((INSTR::Alu*) B)->op == INSTR::Alu::Op::SUB) &&
                                ((INSTR::Alu*) A)->getRVal1() == (((INSTR::Alu*) B)->getRVal1()) &&
                                (((INSTR::Alu*) B)->getRVal2() == A)) {

                            _put_if_absent(inverse, B, new std::set<Value*>());
                            (*inverse)[B]->insert(((INSTR::Alu*) A)->getRVal2());
                        }
                    }
                    if (dynamic_cast<INSTR::Alu*>(A) != nullptr && (((INSTR::Alu*) A)->op == INSTR::Alu::Op::ADD)) {
                        Value *val1 = ((INSTR::Alu*) A)->getRVal1(), *val2 = ((INSTR::Alu*) A)->getRVal2();
                        if (_contains_(get_or_default(inverse, val1, new std::set<Value*>()), val2) ||
                                _contains_(get_or_default(inverse, val2, new std::set<Value*>()), val1)) {
                            A->modifyAllUseThisToUseA(new ConstantInt(0));
                        } else if (dynamic_cast<ConstantInt*>(val1) != nullptr && (int) ((ConstantInt*) val1)->get_const_val() == 0) {
                            A->modifyAllUseThisToUseA(val2);
                            _put_if_absent(eq, A, new std::set<Value*>());
                            (*eq)[A]->insert(val2);
                        } else if (dynamic_cast<ConstantInt*>(val2) != nullptr && (int) ((ConstantInt*) val2)->get_const_val() == 0) {
                            A->modifyAllUseThisToUseA(val1);
                            _put_if_absent(eq, A, new std::set<Value*>());
                            (*eq)[A]->insert(val1);
                        }
                    } else if (dynamic_cast<INSTR::Alu*>(A) != nullptr &&( ((INSTR::Alu*) A)->op == INSTR::Alu::Op::SUB)) {
                        Value *val1 = ((INSTR::Alu*) A)->getRVal1(), *val2 = ((INSTR::Alu*) A)->getRVal2();
                        if ((val1 == val2)) {
                            A->modifyAllUseThisToUseA(new ConstantInt(0));
                        } else if (_contains_(get_or_default(eq, val1, new std::set<Value*>()), val2) ||
                                _contains_(get_or_default(eq, val2, new std::set<Value*>()), val1)) {
                            A->modifyAllUseThisToUseA(new ConstantInt(0));
                        } else if (dynamic_cast<ConstantInt*>(val1) != nullptr && (int) ((ConstantInt*) val1)->get_const_val() == 0) {
                            _put_if_absent(inverse, A, new std::set<Value*>());
                            (*inverse)[A]->insert(val2);
                        } else if (dynamic_cast<ConstantInt*>(val2) != nullptr && (int) ((ConstantInt*) val2)->get_const_val() == 0) {
                            A->modifyAllUseThisToUseA(val1);
                            _put_if_absent(eq, A, new std::set<Value*>());
                            (*eq)[A]->insert(val1);
                        }
                    }
                }
            }
        }
    }

