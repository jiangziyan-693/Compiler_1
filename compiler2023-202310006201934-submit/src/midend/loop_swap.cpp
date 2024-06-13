//
// Created by start on 23-8-12.
//
#include <valarray>
#include "loop_swap.h"
#include "Manager.h"
#include "stl_op.h"
#include "HashMap.h"


loop_swap::loop_swap(std::vector<Function *> *functions) {
    this->functions = functions;
}

void loop_swap::swap_nn_loop() {
    if (functions->size() != 1) {
        return;
    }
    if (!ExternFunction::USAT->onlyOneUser()) {
        return;
    }
    for_bb_(*functions->begin()) {
        Loop *loop_A = bb->loop;
        if (loop_A->childrenLoops->size() != 1) {
            continue;
        }
//        if (loop_A->header)
        Instr* br = bb->getEndInstr();
        if (!is_type(INSTR::Branch, br)) {
            continue;
        }
        BasicBlock* entering_B = (BasicBlock*) br->useValueList[1];
        Loop *loop_B = *(loop_A->childrenLoops->begin());
        if (!loop_A->isIdcSet() || !loop_B->isIdcSet()) {
            continue;
        }
        if (*(loop_B->enterings->begin()) != entering_B) {
            continue;
        }
        if (entering_B->getBeginInstr() != entering_B->getEndInstr()) {
            continue;
        }
//        printf("use\n");
        INSTR::Phi *idc_phi_A = (INSTR::Phi *) loop_A->idcPHI,
                *idc_phi_B = (INSTR::Phi *) loop_B->idcPHI;


        INSTR::Icmp *idc_cmp_A = (INSTR::Icmp *) loop_A->idcCmp,
                *idc_cmp_B = (INSTR::Icmp *) loop_B->idcCmp;

        INSTR::Alu *idc_alu_A = (INSTR::Alu *) loop_A->idcAlu,
                *idc_alu_B = (INSTR::Alu *) loop_B->idcAlu;

        INSTR::Alu* tmp = (INSTR::Alu*) idc_alu_B->cloneToBB(idc_alu_B->bb);
        idc_alu_B->insert_after(tmp);
        idc_alu_B = tmp;

        BasicBlock *head_A = loop_A->header,
                *head_B = loop_B->header;


        idc_phi_A->delFromNowBB();
        idc_cmp_A->delFromNowBB();
        idc_phi_B->delFromNowBB();
        idc_cmp_B->delFromNowBB();
        head_A->getEndInstr()->insert_before(idc_phi_B);
        head_A->getEndInstr()->insert_before(idc_cmp_B);
        idc_phi_B->bb = head_A;
        idc_cmp_B->bb = head_A;


        head_B->getEndInstr()->insert_before(idc_phi_A);
        head_B->getEndInstr()->insert_before(idc_cmp_A);
        idc_phi_A->bb = head_B;
        idc_cmp_A->bb = head_B;

        idc_alu_A->modifyUse(idc_phi_B, 0);
        idc_alu_B->modifyUse(idc_phi_A, 0);

        idc_phi_A->modifyUse(idc_alu_B, 1);
        idc_phi_B->modifyUse(idc_alu_A, 1);

        head_A->getEndInstr()->modifyUse(idc_cmp_B, 0);
        head_B->getEndInstr()->modifyUse(idc_cmp_A, 0);



        break;
    }
}

void loop_swap::Run() {
    swap_nn_loop();
    swap_nest_loop();
}

void loop_swap::swap_nest_loop() {
//    if (functions->size() != 1) return;
    for_func_(functions) {
        for_bb_(function) {

            if (!bb->isLoopHeader) continue;
            Loop* loop_A = bb->loop;
            if (loop_A->childrenLoops->size() != 1 || !loop_A->isIdcSet() || !loop_A->isSimpleLoop()) continue;
            Loop* loop_B = *loop_A->childrenLoops->begin();
            if (loop_B->childrenLoops->size() != 1 || !loop_B->isIdcSet() || !loop_B->isSimpleLoop()) continue;
            Loop* loop_C = *loop_B->childrenLoops->begin();
            if (!loop_C->childrenLoops->empty() || !loop_C->isIdcSet() || !loop_C->isSimpleLoop()) continue;
            if (!is_type(ConstantInt, loop_A->getIdcStep())
                || ((ConstantInt*) loop_A->getIdcStep())->get_const_val() != 1) continue;
            if (!is_type(ConstantInt, loop_B->getIdcStep())
                || ((ConstantInt*) loop_B->getIdcStep())->get_const_val() != 1) continue;
            if (!is_type(ConstantInt, loop_C->getIdcStep())
                || ((ConstantInt*) loop_C->getIdcStep())->get_const_val() != 1) continue;

            if ((*loop_A->latchs->begin())->getBeginInstr() != loop_A->idcAlu) continue;
            if ((*loop_B->latchs->begin())->getBeginInstr() != loop_B->idcAlu) continue;
            int num_A = 0, num_B = 0, num_C = 0;
            for (BasicBlock* bb: *loop_A->nowLevelBB) {
                if (bb != loop_A->header && bb != *(loop_A->exitings->begin()) && bb != *(loop_A->latchs->begin())) {
                    num_A++;
                    if (std::find(bb->precBBs->begin(), bb->precBBs->end(), loop_A->header) == bb->precBBs->end()) {
                        num_A++;
                    }
                }
            }
            for (BasicBlock* bb: *loop_B->nowLevelBB) {
                if (bb != loop_B->header && bb != *(loop_B->exitings->begin()) && bb != *(loop_B->latchs->begin())) {
                    num_B++;
                    if (std::find(bb->precBBs->begin(), bb->precBBs->end(), loop_B->header) == bb->precBBs->end()) {
                        num_B++;
                    }
                }
            }
            for (BasicBlock* bb: *loop_C->nowLevelBB) {
                if (bb != loop_C->header && bb != *(loop_C->exitings->begin()) && bb != *(loop_C->latchs->begin())) {
                    num_C++;
                }
            }
            if (num_A > 1 || num_B > 1 || num_C > 0) continue;

            bool tag = true;
            for (BasicBlock* bb: *loop_A->nowLevelBB) {
                for_instr_(bb) {
                    if (is_type(INSTR::Load, instr) &&
                            !is_type(INSTR::GetElementPtr, ((INSTR::Load*) instr)->getPointer())) {
                        tag = false;
                    } else if (is_type(INSTR::Store, instr) &&
                            !is_type(INSTR::GetElementPtr, ((INSTR::Store*) instr)->getPointer())) {
                        tag = false;
                    }
                }
            }
            if (!tag) continue;

            for (BasicBlock* bb: *loop_B->nowLevelBB) {
                for_instr_(bb) {
                    if (is_type(INSTR::Load, instr) &&
                        !is_type(INSTR::GetElementPtr, ((INSTR::Load*) instr)->getPointer())) {
                        tag = false;
                    } else if (is_type(INSTR::Store, instr) &&
                               !is_type(INSTR::GetElementPtr, ((INSTR::Store*) instr)->getPointer())) {
                        tag = false;
                    }
                }
            }
            if (!tag) continue;

            for (BasicBlock* bb: *loop_C->nowLevelBB) {
                for_instr_(bb) {
                    if (is_type(INSTR::Load, instr) &&
                        !is_type(INSTR::GetElementPtr, ((INSTR::Load*) instr)->getPointer())) {
                        tag = false;
                    } else if (is_type(INSTR::Store, instr) &&
                               !is_type(INSTR::GetElementPtr, ((INSTR::Store*) instr)->getPointer())) {
                        tag = false;
                    }
                }
            }
            if (!tag) continue;

            geps->clear();
            move->clear();

            add_instr_in_loop(loop_A);
            add_instr_in_loop(loop_B);


            printf("move\n");
            for (Instr* instr: *move) {
                std::cout << instr->operator std::string() << std::endl;
            }

            for (BasicBlock* bb: *loop_A->nowLevelBB) {
                for_instr_(bb) {
                    if (is_type(INSTR::Load, instr)) {
                        geps->push_back((INSTR::GetElementPtr*) ((INSTR::Load*) instr)->getPointer());
                    } else if (is_type(INSTR::Store, instr)) {
                        geps->push_back((INSTR::GetElementPtr*) ((INSTR::Store*) instr)->getPointer());
                    }
                }
            }

            for (BasicBlock* bb: *loop_B->nowLevelBB) {
                for_instr_(bb) {
                    if (is_type(INSTR::Load, instr)) {
                        geps->push_back((INSTR::GetElementPtr*) ((INSTR::Load*) instr)->getPointer());
                    } else if (is_type(INSTR::Store, instr)) {
                        geps->push_back((INSTR::GetElementPtr*) ((INSTR::Store*) instr)->getPointer());
                    }
                }
            }

            for (BasicBlock* bb: *loop_C->nowLevelBB) {
                for_instr_(bb) {
                    if (is_type(INSTR::Load, instr)) {
                        geps->push_back((INSTR::GetElementPtr*) ((INSTR::Load*) instr)->getPointer());
                    } else if (is_type(INSTR::Store, instr)) {
                        geps->push_back((INSTR::GetElementPtr*) ((INSTR::Store*) instr)->getPointer());
                    }
                }
            }
            printf("geps\n");
            for (Instr* instr: *geps) {
                std::cout << instr->operator std::string() << std::endl;
            }
            init_permutations(3);
            cost_model(loop_A->idcPHI, loop_B->idcPHI, loop_C->idcPHI);

//            if (sorted->size() != permutations->size()) continue;
            int min = 2147483647, pos = -1;
            for (int i = 0; i < score->size(); ++i) {
                if ((*score)[i] < min) {
                    min = (*score)[i];
                    pos = i;
                }
            }
            auto* ret = (*base)[pos];
            if ((*ret)[0] == loop_A->idcPHI) {
                if ((*ret)[1] != loop_B->idcPHI) {
                    printf("swap\n");
                    //
                    CenterControl::_OPEN_LOOP_SWAP = false;
                    BasicBlock* latch = *loop_C->getLatchs()->begin();
                    for (Instr* instr: *move) {
                        if (instr->bb != latch) {
                            instr->delFromNowBB();
                            instr->bb = latch;
                            latch->getBeginInstr()->insert_before(instr);
                        }
                    }


                    INSTR::Phi *idc_phi_A = (INSTR::Phi *) loop_B->idcPHI,
                            *idc_phi_B = (INSTR::Phi *) loop_C->idcPHI;


                    INSTR::Icmp *idc_cmp_A = (INSTR::Icmp *) loop_B->idcCmp,
                            *idc_cmp_B = (INSTR::Icmp *) loop_C->idcCmp;

                    INSTR::Alu *idc_alu_A = (INSTR::Alu *) loop_B->idcAlu,
                            *idc_alu_B = (INSTR::Alu *) loop_C->idcAlu;

                    INSTR::Alu* tmp = (INSTR::Alu*) idc_alu_B->cloneToBB(idc_alu_B->bb);
                    idc_alu_B->insert_after(tmp);
                    idc_alu_B = tmp;

                    BasicBlock *head_A = loop_B->header,
                            *head_B = loop_C->header;


                    idc_phi_A->delFromNowBB();
                    idc_cmp_A->delFromNowBB();
                    idc_phi_B->delFromNowBB();
                    idc_cmp_B->delFromNowBB();
                    head_A->getEndInstr()->insert_before(idc_phi_B);
                    head_A->getEndInstr()->insert_before(idc_cmp_B);
                    idc_phi_B->bb = head_A;
                    idc_cmp_B->bb = head_A;


                    head_B->getEndInstr()->insert_before(idc_phi_A);
                    head_B->getEndInstr()->insert_before(idc_cmp_A);
                    idc_phi_A->bb = head_B;
                    idc_cmp_A->bb = head_B;

                    idc_alu_A->modifyUse(idc_phi_B, 0);
                    idc_alu_B->modifyUse(idc_phi_A, 0);

                    idc_phi_A->modifyUse(idc_alu_B, 1);
                    idc_phi_B->modifyUse(idc_alu_A, 1);

                    head_A->getEndInstr()->modifyUse(idc_cmp_B, 0);
                    head_B->getEndInstr()->modifyUse(idc_cmp_A, 0);

                }
            } else if ((*ret)[2] == loop_C->idcPHI) {
                if ((*ret)[1] != loop_B->idcPHI) {
                    printf("swap\n");
                    CenterControl::_OPEN_LOOP_SWAP = false;
                    BasicBlock* latch = *loop_C->getLatchs()->begin();
                    for (auto it = move->rbegin(); it != move->rend(); it++) {
                        Instr* instr = *it;
                        if (instr->bb != latch) {
                            instr->delFromNowBB();
                            instr->bb = latch;
                            latch->insertAtHead(instr);
                        }
                    }

                    INSTR::Phi *idc_phi_A = (INSTR::Phi *) loop_A->idcPHI,
                            *idc_phi_B = (INSTR::Phi *) loop_B->idcPHI;


                    INSTR::Icmp *idc_cmp_A = (INSTR::Icmp *) loop_A->idcCmp,
                            *idc_cmp_B = (INSTR::Icmp *) loop_B->idcCmp;

                    INSTR::Alu *idc_alu_A = (INSTR::Alu *) loop_A->idcAlu,
                            *idc_alu_B = (INSTR::Alu *) loop_B->idcAlu;

                    INSTR::Alu* tmp = (INSTR::Alu*) idc_alu_B->cloneToBB(idc_alu_B->bb);
                    idc_alu_B->insert_after(tmp);
                    idc_alu_B = tmp;

                    BasicBlock *head_A = loop_A->header,
                            *head_B = loop_B->header;


                    idc_phi_A->delFromNowBB();
                    idc_cmp_A->delFromNowBB();
                    idc_phi_B->delFromNowBB();
                    idc_cmp_B->delFromNowBB();
                    head_A->getEndInstr()->insert_before(idc_phi_B);
                    head_A->getEndInstr()->insert_before(idc_cmp_B);
                    idc_phi_B->bb = head_A;
                    idc_cmp_B->bb = head_A;


                    head_B->getEndInstr()->insert_before(idc_phi_A);
                    head_B->getEndInstr()->insert_before(idc_cmp_A);
                    idc_phi_A->bb = head_B;
                    idc_cmp_A->bb = head_B;

                    idc_alu_A->modifyUse(idc_phi_B, 0);
                    idc_alu_B->modifyUse(idc_phi_A, 0);

                    idc_phi_A->modifyUse(idc_alu_B, 1);
                    idc_phi_B->modifyUse(idc_alu_A, 1);

                    head_A->getEndInstr()->modifyUse(idc_cmp_B, 0);
                    head_B->getEndInstr()->modifyUse(idc_cmp_A, 0);

                }
            }

        }
    }
}

void loop_swap::init_permutations(int n) {
    std::vector<int>* t = new std::vector<int>();
    int num = 0;
    permutations->clear();
    for (int i = 0; i < n; ++i) {
        t->push_back(i);
    }
    permutations->push_back(new std::vector<int>());
    _vector_add_all(t, (*permutations)[num]);
    while (std::next_permutation(t->begin(), t->end())) {
        num++;
        permutations->push_back(new std::vector<int>());
        _vector_add_all(t, (*permutations)[num]);
    }
    for (int i = 0; i < num; ++i) {
        for (int j = 0; j < n; ++j) {
            printf("%d ", (*(*permutations)[i])[j]);
        }
        printf("\n");
    }
}

void loop_swap::cost_model(Value *i, Value *j, Value *k) {
//    auto* base = new std::vector<std::vector<Value *> *>();
//    auto* score = new std::vector<int>();
    base->clear();
    score->clear();
//    sorted->clear();
    for (int l = 0; l < permutations->size(); ++l) {
        base->push_back(new std::vector<Value*>());
        score->push_back(0);
        for (int m = 0; m < 3; ++m) {
            (*base)[l]->push_back(new ConstantInt(0));
        }
    }
    for (int l = 0; l < permutations->size(); ++l) {
        (*(*base)[l])[(*(*permutations)[l])[0]] = i;
        (*(*base)[l])[(*(*permutations)[l])[1]] = j;
        (*(*base)[l])[(*(*permutations)[l])[2]] = k;
    }


    for (int l = 0; l < base->size(); l++) {
        auto* t = (*base)[l];
        for (Instr* gep: *geps) {
            int len = gep->useValueList.size();
            for (int m = 1; m < len; m ++) {
                int index = std::find(t->begin(), t->end(), gep->useValueList[m]) - t->begin();
                (*score)[l] = (*score)[l] + pow(weight, index + len - m);
            }
        }
    }

    std::cout << "checks:\n" << std::endl;
    for (int l = 0; l < permutations->size(); ++l) {
        std::cout << "checks: " << l << "\n" << std::endl;
        for (int m = 0; m < 3; ++m) {
            std::cout << (*(*base)[l])[m]->operator std::string() << std::endl;
        }
        printf("score: %d\n", (*score)[l]);
    }


}

void loop_swap::add_instr_in_loop(Loop *loop) {
    for_instr_(loop->header) {
        if (!instr->isBJ() && instr != loop->idcAlu && instr != loop->idcCmp && instr != loop->idcPHI) {
            move->push_back(instr);
            if (is_type(INSTR::GetElementPtr, instr)) {
                geps->push_back((INSTR::GetElementPtr*) instr);
            }
        }
    }
    for(BasicBlock* bb: *loop->nowLevelBB) {
        if (bb != loop->header && bb != *(loop->exitings->begin()) && bb != *(loop->latchs->begin())) {
            for_instr_(bb) {
                if (!instr->isBJ() && instr != loop->idcAlu && instr != loop->idcCmp && instr != loop->idcPHI) {
                    move->push_back(instr);
                    if (is_type(INSTR::Load, instr)) {
                        geps->push_back((INSTR::GetElementPtr*) ((INSTR::Load*) instr)->getPointer());
                    } else if (is_type(INSTR::Store, instr)) {
                        geps->push_back((INSTR::GetElementPtr*) ((INSTR::Store*) instr)->getPointer());
                    }
                }
            }
        }
    }

}



