#include "../../include/midend/BranchOptimize.h"
#include "../../include/util/CenterControl.h"

#include "MakeDFG.h"
#include "HashMap.h"
#include "stl_op.h"
#include "Manager.h"

BranchOptimize::BranchOptimize(std::vector<Function *> *functions) {
    this->functions = functions;
}

void BranchOptimize::Run() {
    if (!CenterControl::_O2) {
        RemoveUselessPHI();
        ModifyConstBranch();
    }

    RemoveUselessPHI();
    RemoveUselessJump();
    remakeCFG();
    ModifyConstBranch();
    remakeCFG();
//    peep_hole();
    remakeCFG();
    //fixme:check貌似是错的
    //RemoveBBOnlyJump();
}

void BranchOptimize::remakeCFG() {
    auto *makeDFG = new MakeDFG(functions);
    makeDFG->Run();
}

//删除只有一个use的PHI(冗余PHI)
void BranchOptimize::RemoveUselessPHI() {
    for (Function *function: *functions) {
        removeUselessPHIForFunc(function);
    }
}

void BranchOptimize::RemoveUselessJump() {
    for (Function *function: *functions) {
        removeUselessJumpForFunc(function);
    }
}

//branch条件为恒定值的时候,变为JUMP
void BranchOptimize::ModifyConstBranch() {
    for (Function *function: *functions) {
        modifyConstBranchForFunc(function);
    }
}

void BranchOptimize::RemoveBBOnlyJump() {
    for (Function *function: *functions) {
        removeBBOnlyJumpForFunc(function);
    }
}

void BranchOptimize::removeUselessPHIForFunc(Function *function) {
    BasicBlock *bb = function->getBeginBB();
    while (bb->next != nullptr) {
        Instr *instr = bb->getBeginInstr();
        while (instr->next != nullptr) {
            if (!(dynamic_cast<INSTR::Phi *>(instr) != nullptr)) {
                break;
            }
            if (instr->useValueList.size() == 1) {
                Value *value = instr->useValueList[0];
                instr->modifyAllUseThisToUseA(value);
                instr->remove();
            }

            instr = (Instr *) instr->next;
        }

        bb = (BasicBlock *) bb->next;
    }
}

void BranchOptimize::removeUselessJumpForFunc(Function *function) {
    std::set<BasicBlock *> *bbCanRemove = new std::set<BasicBlock *>();
    BasicBlock *bb = function->getBeginBB();
    while (bb->next != nullptr) {
        if (bb->precBBs->size() == 1) {
            bbCanRemove->insert(bb);
        }
        bb = (BasicBlock *) bb->next;
    }
    for (BasicBlock *mid: *bbCanRemove) {
        // bbs -> pre -> mid -> bbs
        // bbs -> pre -> bbs
        // move all instr in mid to pre
//        if (mid->label == "b33") {
//            printf("rm b33\n");
//        }
        BasicBlock *pre = (*mid->precBBs)[0];
        if (pre->succBBs->size() != 1) {
            continue;
        }
        pre->getEndInstr()->remove();
        std::vector<Instr *> *instrInMid = new std::vector<Instr *>();
        for (Instr *instr = mid->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
            instrInMid->push_back(instr);
        }
        for (Instr *instr: *instrInMid) {
            instr->bb = pre;
            pre->insertAtEnd(instr);
        }
        pre->setSuccBBs(mid->succBBs);
        for (BasicBlock *temp: *mid->succBBs) {
            temp->modifyPre(mid, pre);
        }
        mid->remove();
    }
}

void BranchOptimize::modifyConstBranchForFunc(Function *function) {
    std::unordered_map<INSTR::Branch *, bool> *modifyBrMap = new std::unordered_map<INSTR::Branch *, bool>();
    for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
        for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
            if (dynamic_cast<INSTR::Branch *>(instr) != nullptr) {
                Value *cond = ((INSTR::Branch *) instr)->getCond();
                if (dynamic_cast<Constant *>(cond) != nullptr) {
                    int val = (int) ((ConstantInt *) cond)->get_const_val();
                    bool tag = val == 1;
                    (*modifyBrMap)[(INSTR::Branch *) instr] = tag;
                }
            }
        }
    }

    for (INSTR::Branch *br: KeySet(*modifyBrMap)) {
        BasicBlock *tagBB = nullptr;
        BasicBlock *parentBB = br->bb;
        if ((*modifyBrMap)[br]) {
            tagBB = br->getThenTarget();
        } else {
            tagBB = br->getElseTarget();
        }
        //br->remove();
        INSTR::Jump *jump = new INSTR::Jump(tagBB, parentBB);
        br->insert_before(jump);
        //br->getCond()->remove();
        br->remove();

    }
}

void BranchOptimize::removeBBOnlyJumpForFunc(Function *function) {
    std::set<BasicBlock *> *removes = new std::set<BasicBlock *>();
    for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
        if (function->entry != (bb)) {
            continue;
        }
        if ((bb->getBeginInstr() == bb->getEndInstr())) {
            Instr *instr = bb->getBeginInstr();
            if (dynamic_cast<INSTR::Jump *>(instr) != nullptr) {
                //TODO:
                Value *next = ((INSTR::Jump *) instr)->getTarget();
                BasicBlock *target = ((INSTR::Jump *) instr)->getTarget();
                bb->modifyAllUseThisToUseA(next);
                for (BasicBlock *pre: *bb->precBBs) {
                    pre->modifySuc(bb, target);
                    target->modifyPre(bb, pre);
                }
                removes->insert(bb);
            }
        }
    }
    for (BasicBlock *bb: *removes) {
        bb->remove();
        for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
            instr->remove();
        }
    }
}

void BranchOptimize::peep_hole() {
    for (Function* function: *functions) {
        for_bb_(function) {
            if (_first<BasicBlock, Instr>(bb) == _last<BasicBlock, Instr>(bb) &&
                bb->getBeginUse() == bb->getEndUse()) {
                Instr* instr = _first<BasicBlock, Instr>(bb);
                Instr* user = bb->getBeginUse()->user;
                int index = bb->getBeginUse()->idx, other_idx = 3 - index;
                if (dynamic_cast<INSTR::Jump*>(instr) != nullptr &&
                    dynamic_cast<INSTR::Branch*>(user) != nullptr) {
                    BasicBlock* A = ((INSTR::Jump*) instr)->getTarget();
                    Value* B = user->useValueList[index];
                    Value* C = user->useValueList[other_idx];
//                    assert(A == B);
                    if (A != C) {
                        if (user->bb->label == "b44") {
                            printf("label\n");
                        }
                        user->modifyUse(A, index);
                        user->bb->modifySuc(bb, A);
                        A->modifyPre(bb, user->bb);
//                        bb->remove();
//                        instr->remove();
                    }
                }
            }
        }
    }
}

