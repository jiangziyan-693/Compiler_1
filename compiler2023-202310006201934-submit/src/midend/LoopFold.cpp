//
// Created by start on 23-7-29.
//
#include "../../include/midend/LoopFold.h"
#include "../../include/util/CenterControl.h"




//折叠重复计算的循环
//对于calc循环,如果可以优化,替换 -- 除法->位移

LoopFold::LoopFold(std::vector<Function*>* functions) {
    this->functions = functions;
}

void LoopFold::Run() {
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            if (bb->isLoopHeader) {
                loops->insert(bb->loop);
                calcLoopInit(bb->loop);
            }
        }
    }
    for (Loop* loop: *loops) {
        if (!(removes->find(loop) != removes->end())) {
            tryFoldLoop(loop);
        }
    }
    for (Loop* loop: *removes) {
        BasicBlock* entering = nullptr;
        for (BasicBlock* bb: *loop->getEnterings()) {
            entering = bb;
        }
        BasicBlock* exit = nullptr;
        for (BasicBlock* bb: *loop->getExits()) {
            exit = bb;
        }
        loop->getParentLoop()->getChildrenLoops()->erase(loop);
        entering->modifyBrAToB(loop->getHeader(), exit);
        exit->modifyPre(loop->getHeader(), entering);
    }
}

void LoopFold::calcLoopInit(Loop* loop) {
    loop->clearCalcLoopInfo();
    if (!loop->isSimpleLoop() || !loop->isIdcSet()) {
        return;
    }
    if (loop->hasChildLoop()) {
        return;
    }
    //只有head和latch的简单循环
    for (BasicBlock* bb: *loop->getNowLevelBB()) {
        if (!bb->isLoopHeader && !bb->isLoopLatch) {
            return;
        }
    }
    if (!loop->getHeader()->isLoopExiting) {
        return;
    }
    BasicBlock* latch = nullptr;
    for (BasicBlock* bb: *loop->getLatchs()) {
        latch = bb;
    }
    BasicBlock* head = loop->getHeader();
    std::set<Instr*>* idcInstrs = new std::set<Instr*>();
    INSTR::Alu* alu = nullptr;
    INSTR::Phi* phi = nullptr;
    int alu_cnt = 0, phi_cnt = 0;
    idcInstrs->insert((Instr*) loop->getIdcPHI());
    idcInstrs->insert((Instr*) loop->getIdcCmp());
    idcInstrs->insert((Instr*) loop->getIdcAlu());
    idcInstrs->insert(head->getEndInstr());
    idcInstrs->insert(latch->getEndInstr());
    for (Instr* idcInstr: *idcInstrs) {
        if (useOutLoop(idcInstr, loop)) {
            return;
        }
    }
    for (BasicBlock* bb: *loop->getNowLevelBB()) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (!(idcInstrs->find(instr) != idcInstrs->end())) {
                if (dynamic_cast<INSTR::Alu*>(instr) != nullptr) {
                    alu = (INSTR::Alu*) instr;
                    alu_cnt++;
                } else if (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
                    phi = (INSTR::Phi*) instr;
                    phi_cnt++;
                } else {
                    return;
                }
            }
        }
    }
    if (alu_cnt != 1 || phi_cnt != 1) {
        return;
    }
    if (useOutLoop(alu, loop) || !useOutLoop(phi, loop)) {
        return;
    }
    //indexOf(latch)
    int latchIndex = std::find(head->precBBs->begin(), head->precBBs->end(), latch) - head->precBBs->begin();
    int enteringIndex = 1 - latchIndex;
    if (phi->useValueList[latchIndex] != (alu)) {
        return;
    }
    if (!(std::find(alu->useValueList.begin(), alu->useValueList.end(), phi) != alu->useValueList.end())) {
        return;
    }
    //->indexOf(phi)
    int aluOtherIndex = 1 - (std::find(alu->useValueList.begin(), alu->useValueList.end(), phi) - alu->useValueList.begin());
    //TODO:待强化,使用的只要是同一个值就可以?
    //      且当前没有考虑float!!!
    if (!(dynamic_cast<ConstantInt*>(alu->useValueList[aluOtherIndex]) != nullptr)) {
        return;
    }
    Value* aluPhiEnterValue = phi->useValueList[enteringIndex];
    loop->setCalcLoopInfo(aluPhiEnterValue, alu, phi, aluOtherIndex);
}

void LoopFold::tryFoldLoop(Loop* loop) {
    if (!loop->isCalcLoop) {
        return;
    }
    if (loop->getExits()->size() != 1) {
        return;
    }
    BasicBlock* exit = nullptr;
    for (BasicBlock* bb: *loop->getExits()) {
        exit = bb;
    }
    if (exit->succBBs->size() != 1) {
        return;
    }
    BasicBlock* exitNext = (*exit->succBBs)[0];
    if (!exitNext->isLoopHeader) {
        return;
    }
    Loop* nextLoop = exitNext->loop;
    if (!nextLoop->isCalcLoop) {
        return;
    }
    if (nextLoop->getEnterings()->size() != 1) {
        return;
    }
    if (!(loop->getIdcInit() == nextLoop->getIdcInit()) ||
        !(loop->getIdcStep() == nextLoop->getIdcStep()) ||
        !(loop->getIdcEnd() == nextLoop->getIdcEnd())) {
        return;
    }
    if (dynamic_cast<INSTR::Icmp*>(loop->getIdcCmp()) != nullptr) {
        if (((INSTR::Icmp*) loop->getIdcCmp())->op != (((INSTR::Icmp*) loop->getIdcCmp())->op)) {
            return;
        }
    } else if (dynamic_cast<INSTR::Fcmp*>(loop->getIdcCmp()) != nullptr) {
        if (((INSTR::Fcmp*) loop->getIdcCmp())->op != (((INSTR::Fcmp*) loop->getIdcCmp())->op)) {
            return;
        }
    } else {
        //assert false;
    }
    INSTR::Alu* preLoopAlu = (INSTR::Alu*) loop->getCalcAlu();
    INSTR::Phi* preLoopPhi = (INSTR::Phi*) loop->getCalcPhi();
    Value* preLoopEnterValue = loop->getAluPhiEnterValue();
    int index1 = loop->getAluOtherIndex();

    INSTR::Alu* sucLoopAlu = (INSTR::Alu*) nextLoop->getCalcAlu();
    INSTR::Phi* sucLoopPhi = (INSTR::Phi*) nextLoop->getCalcPhi();
    Value* sucLoopEnterValue = nextLoop->getAluPhiEnterValue();
    int index2 = loop->getAluOtherIndex();

    if (preLoopEnterValue != (sucLoopEnterValue)) {
        return;
    }

    int val1 = (int) ((ConstantInt*) preLoopAlu->useValueList[index1])->get_const_val();
    int val2 = (int) ((ConstantInt*) sucLoopAlu->useValueList[index2])->get_const_val();

    if (preLoopAlu->op != (sucLoopAlu->op) || val1 != val2 || index1 != index2) {
        return;
    }

    sucLoopPhi->modifyAllUseThisToUseA(preLoopPhi);
    removes->insert(nextLoop);

}

bool LoopFold::useOutLoop(Instr* instr, Loop* loop) {
    for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        Instr* user = use->user;
        if (!(user->bb->loop == loop)) {
            return true;
        }
    }
    return false;
}

