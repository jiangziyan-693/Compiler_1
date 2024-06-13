//
// Created by start on 23-8-21.
//
#include "arm_func_opt_v2.h"
#include "stl_op.h"
#include "Loop.h"

arm_func_opt_v2::arm_func_opt_v2(std::vector<Function *> *functions,
                                 std::unordered_map<GlobalValue *, Initial *> *globals) {
    this->functions = functions;
    this->globals = globals;

}


int arm_func_opt_v2::bb_instr_num(BasicBlock *bb) {
    int ret = 0;
    for_instr_(bb) {
        ret++;
    }
    return ret;
}

Instr *arm_func_opt_v2::get_instr_at(BasicBlock *bb, int index) {
    for_instr_(bb) {
        index--;
        if (!index) {
            return instr;
        }
    }
    return nullptr;
}

bool arm_func_opt_v2::check(BasicBlock *bb) {
    check_bb_instr_num(bb, 4);
    init_check_trans(bb, 1, INSTR::Call);
    init_check_trans(bb, 2, INSTR::Alu);
    init_check_trans(bb, 3, INSTR::Alu);
    init_check_trans(bb, 4, INSTR::Jump);
    return true;
}

void arm_func_opt_v2::Run() {
    if (functions->size() > 2) return;
    init();
    if (globals->size() != 0) return;
    if (pure_calc_func == nullptr || loop_only_once == nullptr) return;
    for_func_(functions) {
        for_bb_(function) {
            for_instr_(bb) {
                if (is_type(INSTR::Alloc, instr)) {
                    check_all_st_ld(instr);
                }
            }
        }
    }
    if (up_idc_phi == nullptr || call_pure_func == nullptr) return;
    if (!pure_calc_func->retType->is_float_type()) return;
    if (!tag) return;
    if (call_pure_func->bb != *loop_only_once->exits->begin()) return;
    if (!call_pure_func->onlyOneUser()) return;
    if (!is_type(INSTR::Alu, call_pure_func->getBeginUse()->user)) return;
    if (!is_type(INSTR::Phi, call_pure_func->getBeginUse()->user->getBeginUse()->user)) return;



//    return;
    //
    BasicBlock* removed = *loop_only_once->exits->begin();
    if (!check(removed)) return;



    if (removed->getBeginInstr()->next->next->next->next->next != nullptr) return;
    Instr* C_3 = (Instr*) removed->getBeginInstr()->next->next;
    Instr* C_4 = (Instr*) removed->getBeginInstr()->next->next->next;
    printf("opt\n");
    Function* func = loop_only_once->header->function;
    Loop* up_loop = loop_only_once->parentLoop;
    Instr* add_phi = new INSTR::Phi(BasicType::F32_TYPE, new std::vector<Value*>, up_loop->header);
    BasicBlock *A = new BasicBlock(func, up_loop);
    BasicBlock *B = new BasicBlock(func, up_loop);
    BasicBlock *C = new BasicBlock(func, up_loop);
    Instr* A_1 = new INSTR::Icmp(INSTR::Icmp::Op::EQ, up_idc_phi, new ConstantInt(0), A);
    Instr* A_2 = new INSTR::Branch(A_1, B, C, A);

    call_pure_func->delFromNowBB();
    call_pure_func->bb = B;
    B->insertAtHead(call_pure_func);
    Instr* B_2 = new INSTR::Jump(C, B);

    Instr* C_1 = new INSTR::Phi(BasicType::F32_TYPE, new std::vector<Value*>(), C);
    Instr* C_2 = new INSTR::Alu(BasicType::F32_TYPE, INSTR::Alu::Op::FADD, ((Instr*) removed->getBeginInstr())->useValueList[0], C_1, C);
    C_3->delFromNowBB();
    C_3->bb = C;
    C->insertAtEnd(C_3);
    C_4->delFromNowBB();
    C_4->bb = C;
    C->insertAtEnd(C_4);

    A->precBBs->push_back(loop_only_once->header);
    A->succBBs->push_back(B);
    A->succBBs->push_back(C);

    B->precBBs->push_back(A);
    B->succBBs->push_back(C);

    C->precBBs->push_back(A);
    C->precBBs->push_back(B);
    C->succBBs->push_back((*removed->succBBs)[0]);

    BasicBlock* E = (*removed->succBBs)[0];
    E->modifyPre(removed, C);
    loop_only_once->header->modifySuc(removed, A);
    loop_only_once->header->getEndInstr()->modifyUse(A, 2);


    removed->remove();
    for_instr_(removed) {
        instr->remove();
    }

    ((INSTR::Phi*) add_phi)->addOptionalValue(new ConstantFloat(0.0));
    ((INSTR::Phi*) add_phi)->addOptionalValue(C_1);

    ((INSTR::Phi*) C_1)->addOptionalValue(add_phi);
    ((INSTR::Phi*) C_1)->addOptionalValue(call_pure_func);

    ((Instr*) C_2->useValueList[0])->modifyUse(C_2, 1);

}

bool arm_func_opt_v2::check_loop_only_once(Loop *loop) {
    if (loop->header->label == "b12") {
        printf("debug\n");
    }
    if (loop->isSimpleLoop()) return false;
    if (loop->isIdcSet()) return false;
    if (!(loop->header->precBBs->size() == 3 && loop->latchs->size() == 1 && loop->exitings->size() == 1 && loop->exits->size() == 1)) {
        return false;
    }
    for (BasicBlock* bb: *loop->nowLevelBB) {
        if (bb != loop->header && bb != *loop->latchs->begin()) {
            return false;
        }
    }
    BasicBlock *head = loop->header, *latch = *loop->latchs->begin();
    int num = 0;
    for_instr_(head) {
        if (!is_type(INSTR::Phi, instr) && !is_type(INSTR::Icmp, instr)
                && !instr->isBJ()) {
            return false;
        }
        if (is_type(INSTR::Icmp, instr)) {
            num++;
            if (instr->useValueList[0] != head->getBeginInstr()) {
                return false;
            }
        }
    }
    Instr* idc_phi = head->getBeginInstr();
    if (!idc_phi->type->is_int32_type()) return false;
    if (num > 1) return false;
    if (head->precBBs->size() != 3) return false;
    if (idc_phi->useValueList[0] != idc_phi->useValueList[1]) return false;
    if (!is_type(INSTR::Phi, idc_phi->useValueList[0])) return false;
    Instr* up_phi = (Instr*) idc_phi->useValueList[0];
    if (!is_type(INSTR::Alu, idc_phi->useValueList[2])) return false;
    INSTR::Alu* alu = (INSTR::Alu*) idc_phi->useValueList[2];
    check_alu_v1_op_const(alu, idc_phi, INSTR::Alu::Op::ADD, 1);
    if (!up_phi->type->is_int32_type()) return false;
    if (!is_type(ConstantInt, up_phi->useValueList[0])) return false;
    if (((ConstantInt*) up_phi->useValueList[0])->get_const_val() != 0) return false;
    if (up_phi->useValueList[1] != idc_phi) return false;
    if (idc_phi->bb->loop->parentLoop != up_phi->bb->loop) return false;
    for_instr_(loop->parentLoop->header) {
        if (instr != up_phi && is_type(INSTR::Phi, instr) && instr->type->is_int32_type()) {
            if (up_idc_phi != nullptr) {
                return false;
            }
            up_idc_phi = instr;
        }
    }
    return true;
}

void arm_func_opt_v2::init() {
    for_func_(functions) {
        if (check_pure_calc_func(function)) {
            printf("find pure loop func\n");
            pure_calc_func = function;
        }
    }
    if (pure_calc_func == nullptr) return;
    for_func_(functions) {
        if (function == pure_calc_func) continue;
        for_bb_(function) {
//            if (bb->label == "b12") {
//                printf("debug2\n");
//            }
            if (!bb->isLoopHeader) continue;
            if (check_loop_only_once(bb->loop)) {
                printf("find loop once\n");
                loop_only_once = bb->loop;
            }
        }
    }
}

bool arm_func_opt_v2::check_pure_calc_func(Function *function) {
    if (function->params->size() == 0) return false;
    if (!is_type(INSTR::Jump, function->entry->getBeginInstr())) return false;
    BasicBlock* head = (*function->entry->succBBs)[0];
    if (!head->isLoopHeader) return false;
    Loop* loop = head->loop;
    if (!loop->isSimpleLoop() || !loop->isIdcSet()) return false;
    for (BasicBlock* bb: *loop->nowLevelBB) {
        if (bb != head && bb != *loop->latchs->begin()) {
            return false;
        }
    }
    BasicBlock *exit = *loop->exits->begin(), *latch = *loop->latchs->begin();
    if (!is_type(INSTR::Return, exit->getBeginInstr())) return false;
    if (exit->getBeginInstr()->useValueList.size() == 0) return false;
    if (exit->next->next != nullptr) return false;
    for_instr_(head) {
        if (instr != loop->idcPHI && instr != loop->idcCmp && !instr->isBJ()) {
            if (!is_type(INSTR::Phi, instr)) {
                return false;
            }
            if (exit->getBeginInstr()->useValueList[0] != instr) {
                return false;
            }
            if (!is_type(INSTR::Alu, instr->useValueList[1])) {
                return false;
            }
            if (((Instr*) instr->useValueList[1])->bb != latch) {
                return false;
            }
        }
    }
    for_instr_(latch) {
        if (is_type(INSTR::Store, instr)) {
            return false;
        }
        if (is_type(INSTR::Load, instr)) {
            if (!is_type(INSTR::GetElementPtr, ((INSTR::Load*) instr)->getPointer()))
                return false;
            Value* t = ((INSTR::GetElementPtr*) ((INSTR::Load*) instr)->getPointer())->useValueList[0];
            if (std::find(function->params->begin(), function->params->end(), t) == function->params->end())
                return false;
        }
    }
    return true;
}

void arm_func_opt_v2::check_all_st_ld(Instr *alloc) {
    for_use_(alloc) {
        Instr* user = use->user;
        if (is_type(INSTR::Store, user)) {
            if (user->bb != *loop_only_once->latchs->begin()) {
                tag = false;
            }
        } else if (is_type(INSTR::Load, user)) {
            if (user->bb != *loop_only_once->latchs->begin()) {
                tag = false;
            }
        } else if (is_type(INSTR::Call, user)) {
            if (user->useValueList[0] != pure_calc_func) {
                tag = false;
            }
            if (call_pure_func != nullptr && call_pure_func != user) {
                tag = false;
            }
            call_pure_func = (INSTR::Call*) user;
        } else if (is_type(INSTR::GetElementPtr, user)) {
            check_all_st_ld(user);
        }
    }
}

