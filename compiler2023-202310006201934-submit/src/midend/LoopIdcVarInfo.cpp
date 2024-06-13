//
// Created by start on 23-7-29.
//

#include "../../include/midend/LoopIdcVarInfo.h"
#include "../../include/util/CenterControl.h"





LoopIdcVarInfo::LoopIdcVarInfo(std::vector<Function*>* functions) {
    this->functions = functions;
}

void LoopIdcVarInfo::Run() {
    GetInductionVar();
}

void LoopIdcVarInfo::GetInductionVar() {
    for (Function* function: *functions) {
        GetInductionVarForFunc(function);
    }
}

void LoopIdcVarInfo::GetInductionVarForFunc(Function* function) {
    std::set<Loop*>* loops = new std::set<Loop*>();
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        loops->insert(bb->loop);
    }
    for (Loop* loop: *loops) {
        GetInductionVarForLoop(loop);
    }
}

void LoopIdcVarInfo::GetInductionVarForLoop(Loop* loop) {
    if (!loop->isSimpleLoop()) {
        return;
    }
    //fixme:认为函数的Head和exiting是一个块的时候再进行展开
    if (loop->getExitings()->find(loop->getHeader()) == loop->getEnterings()->end()) {
        return;
    }

    Instr* headBr = loop->getHeader()->getEndInstr(); // br i1 %v6, label %b3, label %b4
    if (!(dynamic_cast<INSTR::Branch*>(headBr) != nullptr)) {
        return;
    }
    //assert dynamic_cast<INSTR::Branch*>(headBr) != nullptr;
    Value* headBrCond = ((INSTR::Branch*) headBr)->getCond(); // %v6 = icmp slt i32 %v19, 10
    Value *idcPHI, *idcEnd;
    if (dynamic_cast<INSTR::Icmp*>(headBrCond) != nullptr) {
        if (dynamic_cast<INSTR::Phi*>(((INSTR::Icmp*) headBrCond)->getRVal1()) != nullptr) {
            idcPHI = ((INSTR::Icmp*) headBrCond)->getRVal1();
            idcEnd = ((INSTR::Icmp*) headBrCond)->getRVal2();
        } else if (dynamic_cast<INSTR::Phi*>(((INSTR::Icmp*) headBrCond)->getRVal2()) != nullptr) {
            idcPHI = ((INSTR::Icmp*) headBrCond)->getRVal2();
            idcEnd = ((INSTR::Icmp*) headBrCond)->getRVal1();
        } else {
            return;
        }
    } else if (dynamic_cast<INSTR::Fcmp*>(headBrCond) != nullptr) {
        if (dynamic_cast<INSTR::Phi*>(((INSTR::Fcmp*) headBrCond)->getRVal1()) != nullptr) {
            idcPHI = ((INSTR::Fcmp*) headBrCond)->getRVal1();
            idcEnd = ((INSTR::Fcmp*) headBrCond)->getRVal2();
        } else if (dynamic_cast<INSTR::Phi*>(((INSTR::Fcmp*) headBrCond)->getRVal2()) != nullptr) {
            idcPHI = ((INSTR::Fcmp*) headBrCond)->getRVal2();
            idcEnd = ((INSTR::Fcmp*) headBrCond)->getRVal1();
        } else {
            return;
        }
    } else {
        return;
    }

    if (!(((Instr*) idcPHI)->bb == loop->getHeader())) {
        return;
    }


    //assert dynamic_cast<INSTR::Phi*>(idcPHI) != nullptr;
    Value* phiRVal1 = ((INSTR::Phi*) idcPHI)->useValueList[0];
    Value* phiRVal2 = ((INSTR::Phi*) idcPHI)->useValueList[1];
    BasicBlock* head = loop->getHeader();
    Value *idcAlu, *idcInit;
    if (loop->getLatchs()->find((*head->precBBs)[0]) != loop->getLatchs()->end()) {
        idcAlu = phiRVal1;
        idcInit = phiRVal2;
    } else {
        idcAlu = phiRVal2;
        idcInit = phiRVal1;
    }
    if (!(dynamic_cast<INSTR::Alu*>(idcAlu) != nullptr)) {
        return;
    }
    Value* idcStep;
    if ((((INSTR::Alu*) idcAlu)->getRVal1() == idcPHI)) {
        idcStep = ((INSTR::Alu*) idcAlu)->getRVal2();
    } else if (((INSTR::Alu*) idcAlu)->getRVal2() == idcPHI) {
        idcStep = ((INSTR::Alu*) idcAlu)->getRVal1();
    } else {
        return;
    }

    if (!((((INSTR::Alu*) idcAlu)->op == INSTR::Alu::Op::ADD) ||
          (((INSTR::Alu*) idcAlu)->op == INSTR::Alu::Op::SUB) ||
          (((INSTR::Alu*) idcAlu)->op == INSTR::Alu::Op::FADD) ||
          (((INSTR::Alu*) idcAlu)->op == INSTR::Alu::Op::FSUB) ||
          (((INSTR::Alu*) idcAlu)->op == INSTR::Alu::Op::MUL) ||
          (((INSTR::Alu*) idcAlu)->op == INSTR::Alu::Op::FMUL))) {
        return;
    }
//    (        //assert idcAlu->getType() == idcPHI->getType());
//    (        //assert idcPHI->getType() == idcInit->getType());
//    (        //assert idcInit->getType() == idcEnd->getType());
//    (        //assert idcEnd->getType() == idcStep->getType());
    //i = 10 - i; i = 10 / i;
    //fixme:上述归纳方式,暂时不考虑
    if ((((INSTR::Alu*) idcAlu)->op == INSTR::Alu::Op::SUB) || (((INSTR::Alu*) idcAlu)->op == INSTR::Alu::Op::DIV)) {
        if (dynamic_cast<INSTR::Phi*>(((INSTR::Alu*) idcAlu)->getRVal2()) != nullptr) {
            return;
        }
    }
    loop->setIdc(idcAlu, idcPHI, headBrCond, idcInit, idcEnd, idcStep);
}
