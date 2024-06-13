#include "../../include/midend/BranchLift.h"
#include "../../include/util/CenterControl.h"
#include "HashMap.h"


BranchLift::BranchLift(std::vector<Function *> *functions) {
    this->functions = functions;
}

void BranchLift::Run() {
    branchLift();
}

void BranchLift::branchLift() {
    for (Function *function: *functions) {
        branchLiftForFunc(function);
    }
}

void BranchLift::branchLiftForFunc(Function *function) {
    //TODO:一次只能提升一个循环的一个Branch
    for (BasicBlock *head: *function->loopHeads) {
        Loop *loop = head->loop;
        std::unordered_map<int, std::set<Instr *> *> *conds = loop->getConds();
        for (int key: KeySet(*conds)) {
            if ((*conds)[key]->size() == 1) {
                Instr *br = nullptr;
                for (Instr *temp: *(*conds)[key]) {
                    br = temp;
                }
                if (!(dynamic_cast<INSTR::Branch *>(br) != nullptr)) {
                    continue;
                }
                liftBrOutLoop((INSTR::Branch *) br, loop);
                //TODO:只提升一个branch就直接return
                return;
            }
        }
    }
}

void BranchLift::liftBrOutLoop(INSTR::Branch *thenBr, Loop *thenLoop) {
    CloneInfoMap::clear();
    Function *function = thenLoop->getHeader()->function;
    BasicBlock *thenHead = thenLoop->getHeader();
    thenLoop->cloneToFunc(function);

    thenLoop->fix();
    Loop *elseLoop = CloneInfoMap::getReflectedLoop(thenLoop);
    BasicBlock *elseHead = elseLoop->getHeader();

    BasicBlock *transBB = new BasicBlock(function, thenLoop->getParentLoop());
    std::set<BasicBlock *> *enterings = thenLoop->getEnterings();

    //entering --> transBB
    for (BasicBlock *entering: *enterings) {
        Instr *instr = entering->getEndInstr();
        if (dynamic_cast<INSTR::Jump *>(instr) != nullptr) {
            instr->modifyUse(transBB, 0);
        } else if (dynamic_cast<INSTR::Branch *>(instr) != nullptr) {
            std::vector<Value *> *values = &instr->useValueList;
            if (values->size() == 1) {
                instr->modifyUse(transBB, 0);
            } else {
                //->indexOf(thenHead)
                int index = std::find(values->begin(), values->end(), thenHead) - values->begin();
                instr->modifyUse(transBB, index);
            }
        } else {
            //System->err.println("error");
        }
        entering->modifySuc(thenHead, transBB);
        transBB->addPre(entering);
        thenHead->modifyPre(entering, transBB);
        elseHead->modifyPre(entering, transBB);

        //transBB->addPre(entering);
    }

    //Clone br
    // entering --> transBB
    // transBB --> thenHead & elseHead
    transBB->addSuc(thenHead);
    transBB->addSuc(elseHead);


    INSTR::Branch *elseBr = (INSTR::Branch *) CloneInfoMap::getReflectedValue(thenBr);
    Instr *brInTransBB = thenBr->cloneToBB(transBB);
    if (transBB->getLoopDep() > 0) {
        brInTransBB->setInLoopCond();
        brInTransBB->setCondCount(thenBr->getCondCount());
    }

    brInTransBB->modifyUse(thenHead, 1);
    brInTransBB->modifyUse(elseHead, 2);

    //修改thenLoop和elseLoop中的br为无条件转跳
    //thenBr->modifyUse(new ConstantInt(1), 0);
    INSTR::Jump *thenJump = new INSTR::Jump((BasicBlock *) thenBr->useValueList[1], thenBr->bb);
    thenBr->insert_before(thenJump);


    //elseBr->modifyUse(new ConstantInt(0), 0);
    INSTR::Jump *elseJump = new INSTR::Jump((BasicBlock *) elseBr->useValueList[2], elseBr->bb);
    elseBr->insert_before(elseJump);

    thenBr->remove();
    elseBr->remove();


    //修改exiting的数据流
    //
    //修改exits的(冗余)phi
    std::set<BasicBlock *> *exits = thenLoop->getExits();
    for (BasicBlock *bb: *exits) {
        std::vector<BasicBlock *> *addBBs = new std::vector<BasicBlock *>();
        for (BasicBlock *pre: *bb->precBBs) {
            if (CloneInfoMap::valueMap.find(pre) != CloneInfoMap::valueMap.end()) {
                addBBs->push_back((BasicBlock *) CloneInfoMap::getReflectedValue(pre));
            }
        }
        for (BasicBlock *add: *addBBs) {
            bb->addPre(add);
        }

        Instr *instr = bb->getBeginInstr();
        while (instr->next != nullptr) {
            if (dynamic_cast<INSTR::Phi *>(instr) != nullptr) {
                std::vector<Value *> *adds = new std::vector<Value *>();
                for (Value *used: instr->useValueList) {
                    //->indexOf(used)
                    int index = std::find(instr->useValueList.begin(), instr->useValueList.end(), used) -
                                instr->useValueList.begin();
                    if (CloneInfoMap::valueMap.find((*bb->precBBs)[index]) != CloneInfoMap::valueMap.end()) {
                        adds->push_back(CloneInfoMap::getReflectedValue(used));
                    }

                    //adds->push_back(CloneInfoMap->getReflectedValue(used));
                }
                for (Value *add: *adds) {
                    ((INSTR::Phi *) instr)->addOptionalValue(add);
                }
                //assert instr->bb->precBBs->size() == instr->useValueList->size();
            }
            instr = (Instr *) instr->next;
        }
    }
}
