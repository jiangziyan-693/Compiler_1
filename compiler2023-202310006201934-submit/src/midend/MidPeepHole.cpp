#include "../../include/midend/MidPeepHole.h"
#include "../../include/util/CenterControl.h"
#include "stl_op.h"


#include <cmath>



	MidPeepHole::MidPeepHole(std::vector<Function*>* functions) {
        this->functions = functions;
    }

    void MidPeepHole::Run(){
        // time = b - 1; add = init + base; mul = time * base; ret = add + mul
        // mul = b * base; ret = add + mul;
        peepHoleA();
        // A = load ptr_A, store A ptr_B, store A ptr_A
        // A = load ptr_A, store A ptr_B
        peepHoleB();
        //
        peepHoleC();
        // B = A / const_A, sge B const_B
        peepHoleD();
        // DFS 判断是否单前驱
        // if_comb
        // math
    }



    void MidPeepHole::peepHoleA() {
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {

                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (dynamic_cast<INSTR::Alu*>(instr) != nullptr && (((INSTR::Alu*) instr)->op == INSTR::Alu::Op::ADD)) {
                        Instr* add_A = instr;
                        Instr* mul = (Instr*) instr->next;
                        Instr* add_B = (Instr*) mul->next;
                        if (dynamic_cast<INSTR::Alu*>(mul) != nullptr && (((INSTR::Alu*) mul)->op == INSTR::Alu::Op::MUL) &&
                                dynamic_cast<INSTR::Alu*>(add_B) != nullptr &&( ((INSTR::Alu*) add_B)->op == INSTR::Alu::Op::ADD)) {
                            Value* A = ((INSTR::Alu*) add_A)->getRVal1();
                            Value* B = ((INSTR::Alu*) add_A)->getRVal2();
                            Value* C = ((INSTR::Alu*) mul)->getRVal1();
                            Value* D = ((INSTR::Alu*) mul)->getRVal2();
                            Value* E = ((INSTR::Alu*) add_B)->getRVal1();
                            Value* F = ((INSTR::Alu*) add_B)->getRVal2();

                            Value *init, *time, *base;
                            int time_index;
                            if ((((E == add_A) && F == (mul))) || (((F == add_A) && E == (mul)))) {
                                if ((A == C)) {
                                    base = A;
                                    init = B;
                                    time = D;
                                    time_index = 1;
                                } else if ((A == D)) {
                                    base = A;
                                    init = B;
                                    time = C;
                                    time_index = 0;
                                } else if ((B == C)) {
                                    base = B;
                                    init = A;
                                    time = D;
                                    time_index = 1;
                                } else if ((B == D)) {
                                    base = B;
                                    init = A;
                                    time = C;
                                    time_index = 0;
                                } else {
                                    continue;
                                }

                                if (!(dynamic_cast<INSTR::Alu*>(time) != nullptr) || !(((INSTR::Alu*) time)->op == INSTR::Alu::Op::SUB)) {
                                    continue;
                                }
                                Value* val = ((INSTR::Alu*) time)->getRVal2();
                                if (!(dynamic_cast<ConstantInt*>(val) != nullptr) || ((int) ((ConstantInt*) val)->get_const_val()) != 1) {
                                    return;
                                }
                                if (!time->onlyOneUser()) {
                                    return;
                                }
                                mul->modifyUse(((INSTR::Alu*) time)->getRVal1(), time_index);
                                int init_index =( (F == mul))? 0:1;
                                add_B->modifyUse(init, init_index);
                                instr = add_B;
                                add_A->remove();
                                time->remove();
                            }
                        }
                    }
                }
            }
        }
    }

    void MidPeepHole::peepHoleB() {
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                std::vector<Instr*>* instrs = new std::vector<Instr*>();
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (dynamic_cast<INSTR::Load*>(instr) != nullptr || dynamic_cast<INSTR::Store*>(instr) != nullptr) {
                        instrs->push_back(instr);
                    }
                }
                if (instrs->size() != 3 || !(dynamic_cast<INSTR::Load*>((*instrs)[0]) != nullptr) ||
                        !(dynamic_cast<INSTR::Store*>((*instrs)[1]) != nullptr) || !(dynamic_cast<INSTR::Store*>((*instrs)[2]) != nullptr)) {
                    continue;
                }
                INSTR::Load* load = (INSTR::Load*) (*instrs)[0];
                INSTR::Store* storeA = (INSTR::Store*) (*instrs)[1];
                INSTR::Store* storeB = (INSTR::Store*) (*instrs)[2];

                if (!(storeA->getValue() == load) || !(storeB->getValue() == load) ||
                        !(storeB->getPointer() == load->getPointer())) {
                    continue;
                }
                storeB->remove();
            }
        }
    }


    void MidPeepHole::peepHoleC() {
        peepHoleCInit();
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                checkC(bb);
            }
        }
    }

    void MidPeepHole::peepHoleCInit() {
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (dynamic_cast<INSTR::Store*>(instr) != nullptr || dynamic_cast<INSTR::Call*>(instr) != nullptr) {
                        bb_has_store_call->insert(bb);
                        break;
                    }
                }
            }
        }
    }

    void MidPeepHole::checkC(BasicBlock* A) {
//        if (A->getLabel()->equals("b30")) {
//            int a = 1;
//        }
        if (A->succBBs->size() != 2) {
            return;
        }
        BasicBlock *B = nullptr, *C = nullptr;
        if ((*A->succBBs)[0]->succBBs->size() == 1) {
            C = (*A->succBBs)[0];
            B = (*A->succBBs)[1];
        } else if ((*A->succBBs)[1]->succBBs->size() == 1) {
            B = (*A->succBBs)[0];
            C = (*A->succBBs)[1];
        } else {
            return;
        }

        if (B->precBBs->size() != 1 || B->succBBs->size() != 2) {
            return;
        }

        if ((bb_has_store_call->find(A) != bb_has_store_call->end()) || (bb_has_store_call->find(B) != bb_has_store_call->end())) {
            return;
        }

        BasicBlock *B_left = (*B->succBBs)[0], *B_right = (*B->succBBs)[1];
        BasicBlock *D = nullptr, *G = nullptr;
        if (patternC(B_left, B_right)) {
            D = B_left;
            G = B_right;
        } else if (patternC(B_right, B_left)) {
            D = B_right;
            G = B_left;
        } else {
            return;
        }

        std::unordered_map<std::string, Instr*>* gvn = new std::unordered_map<std::string, Instr*>();
        for (Instr* instr = A->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
                std::string hash = "load " + ((INSTR::Load*) instr)->getPointer()->name;
                (*gvn)[hash] = instr;
            }
        }

        for (Instr* instr = E->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
                std::string hash = "load " + ((INSTR::Load*) instr)->getPointer()->name;
                if ((gvn->find(hash) != gvn->end())) {
                    instr->modifyAllUseThisToUseA((*gvn)[hash]);
                }
            }
        }

        for (Instr* instr = G->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
                std::string hash = "load " + ((INSTR::Load*) instr)->getPointer()->name;
                if ((gvn->find(hash) != gvn->end())) {
                    instr->modifyAllUseThisToUseA((*gvn)[hash]);
                }
            }
        }
    }

    bool MidPeepHole::patternC(BasicBlock* D, BasicBlock* G) {
        if (D->succBBs->size() != 2) {
            return false;
        }
        if (D->precBBs->size() != 1) {
            return false;
        }
        if (G->precBBs->size() != 2) {
            return false;
        }
        BasicBlock *D_left = (*D->succBBs)[0], *D_right = (*D->succBBs)[1];
        if (D_right->precBBs->size() == 2 && D_right->succBBs->size() == 1 &&
                D_left->precBBs->size() == 1 && D_left->succBBs->size() == 1 &&
(                (*D_left->succBBs)[0] == D_right)) {
            E = D_left;
            F = D_right;
        } else if (D_left->precBBs->size() == 2 && D_left->succBBs->size() == 1 &&
                D_right->precBBs->size() == 1 && D_right->succBBs->size() == 1 &&
(                (*D_right->succBBs)[0] == D_left)) {
            E = D_right;
            F = D_left;
        } else {
            return false;
        }

        if ((*F->succBBs)[0] != (G)) {
            return false;
        }



        if ((bb_has_store_call->find(D) != bb_has_store_call->end()) || (bb_has_store_call->find(E) != bb_has_store_call->end()) ||
                (bb_has_store_call->find(F) != bb_has_store_call->end()) || (bb_has_store_call->find(G) != bb_has_store_call->end())) {
            return false;
        }

        return true;
    }


    void MidPeepHole::peepHoleD() {
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (dynamic_cast<INSTR::Alu*>(instr) != nullptr && (((INSTR::Alu*) instr)->op == INSTR::Alu::Op::DIV) &&
                            instr->onlyOneUser() && dynamic_cast<INSTR::Icmp*>(instr->getBeginUse()->user) != nullptr) {
                        INSTR::Alu* div = (INSTR::Alu*) instr;
                        INSTR::Icmp* cmp = (INSTR::Icmp*) instr->getBeginUse()->user;
                        if (dynamic_cast<ConstantInt*>(div->getRVal2()) != nullptr && dynamic_cast<ConstantInt*>(cmp->getRVal2()) != nullptr) {
                            int val1 = (int) ((ConstantInt*) div->getRVal2())->get_const_val();
                            int val2 = (int) ((ConstantInt*) cmp->getRVal2())->get_const_val();
                            if (val1 < max_base && val2 < max_base) {
                                cmp->modifyUse(div->getRVal1(), 0);
                                cmp->modifyUse(new ConstantInt(val1 * val2), 1);
                            }
                        }
                    }
                }
            }
        }
    }

void MidPeepHole::peepHoleE() {

}
