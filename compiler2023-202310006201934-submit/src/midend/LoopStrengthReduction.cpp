//
// Created by start on 23-7-29.
//
#include "../../include/midend/LoopStrengthReduction.h"
#include "../../include/util/CenterControl.h"
#include <cmath>
#include "stl_op.h"


//fixme:记得打开优化

//TODO:识别循环中,使用了迭代变量idc的数学运算
//      可以考虑任何一个常数*idc相关的ADD/SUB指令再进行ADD会/SUB
//      即(i + A) * const + B
//      A和B是循环不变量(def不在当前循环)
//      const为常数
//      循环展开前做

//fixme:必须紧挨LoopFold调用


LoopStrengthReduction::LoopStrengthReduction(std::vector<Function*>* functions) {
    this->functions = functions;
}

void LoopStrengthReduction::Run() {
    divToShift();
//    addConstToMul();
//    if (CenterControl::_OPEN_FLOAT_LOOP_STRENGTH_REDUCTION) {
//        addConstFloatToMul();
//    }
    addConstAndModToMulAndMod();
    addIdcAndModToMulAndMod();
}

void LoopStrengthReduction::addConstToMul() {
    //TODO: ret += const; ret %= mod; i++;
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            if (bb->isLoopHeader) {
                addConstLoopInit(bb->loop);
            }
        }
    }
}

void LoopStrengthReduction::addConstLoopInit(Loop* loop) {
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
    BasicBlock* latch = *loop->getLatchs()->begin();
    BasicBlock* head = loop->getHeader();
    BasicBlock* exit = *loop->getExits()->begin();
    if (exit->precBBs->size() > 1) {
        return;
    }
    std::set<Instr*>* idcInstrs = new std::set<Instr*>();
    INSTR::Alu* add = nullptr;
    INSTR::Phi* phi = nullptr;
    int add_cnt = 0, phi_cnt = 0;
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
                    if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::ADD)) {
                        add = (INSTR::Alu*) instr;
                        add_cnt++;
                    } else {
                        return;
                    }
                } else if (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
                    phi = (INSTR::Phi*) instr;
                    phi_cnt++;
                } else {
                    return;
                }
            }
        }
    }
    if (add_cnt != 1 || phi_cnt != 1) {
        return;
    }
    if (useOutLoop(add, loop) || !useOutLoop(phi, loop)) {
        return;
    }

    //indexOf(latch)
    int latchIndex = std::find(head->precBBs->begin(), head->precBBs->end(), latch) - head->precBBs->begin();
    int enteringIndex = 1 - latchIndex;
    if (phi->useValueList[latchIndex] != (add)) {
        return;
    }
    if (!(std::find(add->useValueList.begin(), add->useValueList.end(), phi) != add->useValueList.end())) {
        return;
    }

    //->indexOf(phi)
    int addConstIndex = 1 - (std::find(add->useValueList.begin(), add->useValueList.end(), phi) - add->useValueList.begin());
    //TODO:待强化,使用的只要是同一个值就可以?
    //      且当前没有考虑float!!!
    if (!(dynamic_cast<ConstantInt*>(add->useValueList[addConstIndex]) != nullptr)) {
        return;
    }
    //此限制是否必须,计算值的初始值为常数
    if (!(dynamic_cast<ConstantInt*>(phi->useValueList[enteringIndex]) != nullptr)) {
        return;
    }
    if (dynamic_cast<ConstantInt*>(loop->getIdcInit()) == nullptr || dynamic_cast<ConstantInt*>(loop->getIdcStep()) == nullptr) {
        return;
    }
    if (!(((INSTR::Alu*) loop->getIdcAlu())->op == INSTR::Alu::Op::ADD)) {
        return;
    }
    if (((INSTR::Icmp*) loop->getIdcCmp())->op != (INSTR::Icmp::Op::SLT)) {
        return;
    }
    int idc_init = (int) ((ConstantInt*) loop->getIdcInit())->get_const_val();
    int idc_step = (int) ((ConstantInt*) loop->getIdcStep())->get_const_val();
    Value* idc_end = loop->getIdcEnd();
    if (idc_init != 0 || idc_step != 1) {
        return;
    }
    int base = (int) ((ConstantInt*) add->useValueList[addConstIndex])->get_const_val();
    int init = (int) ((ConstantInt*) phi->useValueList[enteringIndex])->get_const_val();

    INSTR::Alu* mul = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, new ConstantInt(base), idc_end, exit);
    INSTR::Alu* ret = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, new ConstantInt(init), mul, exit);

    exit->getBeginInstr()->insert_before(ret);
    exit->getBeginInstr()->insert_before(mul);

    phi->modifyAllUseThisToUseA(ret);
    phi->remove();
    add->remove();

}

void LoopStrengthReduction::addConstFloatToMul() {
    //TODO: ret += const; ret %= mod; i++;
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            if (bb->isLoopHeader) {
                addConstFloatLoopInit(bb->loop);
            }
        }
    }
}

void LoopStrengthReduction::addConstFloatLoopInit(Loop* loop) {
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
    BasicBlock* latch = *loop->getLatchs()->begin();
    BasicBlock* head = loop->getHeader();
    BasicBlock* exit = *loop->getExits()->begin();
    if (exit->precBBs->size() > 1) {
        return;
    }
    std::set<Instr*>* idcInstrs = new std::set<Instr*>();
    INSTR::Alu* add = nullptr;
    INSTR::Phi* phi = nullptr;
    int add_cnt = 0, phi_cnt = 0;
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
                    if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::FADD)) {
                        add = (INSTR::Alu*) instr;
                        add_cnt++;
                    } else {
                        return;
                    }
                } else if (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
                    phi = (INSTR::Phi*) instr;
                    phi_cnt++;
                } else {
                    return;
                }
            }
        }
    }
    if (add_cnt != 1 || phi_cnt != 1) {
        return;
    }
    if (useOutLoop(add, loop) || !useOutLoop(phi, loop)) {
        return;
    }

    //indexOf(latch)
    int latchIndex = std::find(head->precBBs->begin(), head->precBBs->end(), latch) - head->precBBs->begin();
    int enteringIndex = 1 - latchIndex;
    if (phi->useValueList[latchIndex] != (add)) {
        return;
    }
    if (!(std::find(add->useValueList.begin(), add->useValueList.end(), phi) != add->useValueList.end())) {
        return;
    }
    //
    int addConstIndex = 1 - (std::find(add->useValueList.begin(), add->useValueList.end(), phi) - add->useValueList.begin());
    //TODO:待强化,使用的只要是同一个值就可以?
    //      且当前没有考虑float!!!
    if (!(dynamic_cast<ConstantFloat*>(add->useValueList[addConstIndex]) != nullptr)) {
        return;
    }
    //此限制是否必须,计算值的初始值为常数
    if (!(dynamic_cast<ConstantFloat*>(phi->useValueList[enteringIndex]) != nullptr)) {
        return;
    }
    if (dynamic_cast<ConstantInt*>(loop->getIdcInit()) == nullptr || dynamic_cast<ConstantInt*>(loop->getIdcStep()) == nullptr) {
        return;
    }
    if (((INSTR::Alu *) loop->getIdcAlu())->op != INSTR::Alu::Op::ADD) {
        return;
    }
    if (((INSTR::Icmp*) loop->getIdcCmp())->op != INSTR::Icmp::Op::SLT) {
        return;
    }
    //int idc_init_ = ((ConstantInt*) loop->getIdcInit())->get_const_val();
    int idc_init = (int) ((ConstantInt*) loop->getIdcInit())->get_const_val();
    int idc_step = (int) ((ConstantInt*) loop->getIdcStep())->get_const_val();
    Value* idc_end = loop->getIdcEnd();
    if (idc_init != 0 || idc_step != 1) {
        return;
    }
    float base = (float) ((ConstantFloat*) add->useValueList[addConstIndex])->get_const_val();
    float init = (float) ((ConstantFloat*) phi->useValueList[enteringIndex])->get_const_val();

    INSTR::SItofp* sItofp = new INSTR::SItofp(idc_end, exit);
    INSTR::Alu* mul = new INSTR::Alu(BasicType::F32_TYPE, INSTR::Alu::Op::FMUL, new ConstantFloat(base), sItofp, exit);
    INSTR::Alu* ret = new INSTR::Alu(BasicType::F32_TYPE, INSTR::Alu::Op::FADD, new ConstantFloat(init), mul, exit);

    exit->getBeginInstr()->insert_before(ret);
    exit->getBeginInstr()->insert_before(mul);
    exit->getBeginInstr()->insert_before(sItofp);

    phi->modifyAllUseThisToUseA(ret);
    phi->remove();
    add->remove();

}

void LoopStrengthReduction::divToShift() {
    for (Function* function: *functions) {
        divToShiftForFunc(function);
    }
}

void LoopStrengthReduction::divToShiftForFunc(Function* function) {
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        if (bb->isLoopHeader && bb->loop->isCalcLoop) {
            tryReduceLoop(bb->loop);
        }
    }
}

void LoopStrengthReduction::tryReduceLoop(Loop* loop) {
    //要求了迭代变量是整数
    if (!(dynamic_cast<ConstantInt*>(loop->getIdcInit()) != nullptr) || !(dynamic_cast<ConstantInt*>(loop->getIdcStep()) != nullptr)) {
        return;
    }
    int idcInit = (int) ((ConstantInt*) loop->getIdcInit())->get_const_val();
    int idcStep = (int) ((ConstantInt*) loop->getIdcStep())->get_const_val();
    if (idcInit != 0 || idcStep != 1) {
        return;
    }

    //要求了除法是整数
    if (!(((INSTR::Alu*) loop->getCalcAlu())->op == INSTR::Alu::Op::DIV) ||
        !(dynamic_cast<Constant*>(((INSTR::Alu*) loop->getCalcAlu())->getRVal2()) != nullptr)) {
        return;
    }
    int div = (int) ((ConstantInt*) ((INSTR::Alu*) loop->getCalcAlu())->getRVal2())->get_const_val();
    double a = log(div) / log(2);
    if (pow(2, a) != div) {
        return;
    }
    int shift = (int) a;

    if (((INSTR::Icmp*) loop->getIdcCmp())->op != INSTR::Icmp::Op::SLT) {
        return;
    }

    Type* type = ((INSTR::Alu*) loop->getCalcAlu())->type;
    //TODO:只考虑整数,待强化
    if (!type->is_int32_type()) {
        return;
    }
    BasicBlock* head = loop->getHeader();
    BasicBlock* entering = nullptr;
    for (BasicBlock* bb: *loop->getEnterings()) {
        entering = bb;
    }
    BasicBlock* exit = nullptr;
    for (BasicBlock* bb: *loop->getExits()) {
        exit = bb;
    }
    BasicBlock* exiting = nullptr;
    for (BasicBlock* bb: *loop->getExitings()) {
        exiting = bb;
    }
    Function* function = loop->getHeader()->function;
    Loop* parentLoop = loop->getParentLoop();
    Value* times = loop->getIdcEnd();
    Value* reach = loop->getAluPhiEnterValue();
    INSTR::Alu* calcAlu = (INSTR::Alu*) loop->getCalcAlu();
    INSTR::Phi* calcPhi = (INSTR::Phi*) loop->getCalcPhi();

    BasicBlock* A = new BasicBlock(function, parentLoop);
    BasicBlock* B = new BasicBlock(function, parentLoop);

    //A
    std::vector<BasicBlock*>* Apres = new std::vector<BasicBlock*>();
    Apres->push_back(entering);
    std::vector<BasicBlock*>* Asucc = new std::vector<BasicBlock*>();
    Asucc->push_back(B);
    Asucc->push_back(head);

    A->modifyPres(Apres);
    A->modifySucs(Asucc);

    INSTR::Alu* mul = new INSTR::Alu(type, INSTR::Alu::Op::MUL, times, new ConstantInt(shift), A);
    INSTR::Alu* ashr = new INSTR::Alu(type, INSTR::Alu::Op::ASHR, reach, mul, A);
    INSTR::Icmp* icmp = new INSTR::Icmp(INSTR::Icmp::Op::SGE, reach, new ConstantInt(0), A);
    INSTR::Branch* br = new INSTR::Branch(icmp, B, head, A);

//    int cnt = ++Visitor::curLoopCondCount;
    //TODO:fix with visitor
    int cnt = 0;
    icmp->setCondCount(cnt);
    br->setCondCount(cnt);
    if (parentLoop->getLoopDepth() > 0) {
        icmp->setInLoopCond();
        br->setInLoopCond();
    }

    //B
    //Instr->Alu rem = new INSTR::Alu(type, INSTR::Alu::Op::REM, )
    std::vector<BasicBlock*>* Bpres = new std::vector<BasicBlock*>();
    Bpres->push_back(A);
    Bpres->push_back(head);
    std::vector<BasicBlock*>* Bsucc = new std::vector<BasicBlock*>();
    Bsucc->push_back(exit);

    B->modifyPres(Bpres);
    B->modifySucs(Bsucc);

    std::vector<Value*>* values = new std::vector<Value*>();
    values->push_back(ashr);
    values->push_back(calcPhi);
    INSTR::Phi* mergePhi = new INSTR::Phi(type, values, B);
    INSTR::Jump* jump = new INSTR::Jump(exit, B);

    //C

    //D

    //E

    head->modifyPre(entering, A);
    head->modifyBrAToB(exit, B);
    head->modifySuc(exit, B);

    entering->modifyBrAToB(head, A);
    entering->modifySuc(head, A);

    exit->modifyPre(exiting, B);

    for (Use* use = calcPhi->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        Instr* user = use->user;
        if (user != (calcAlu) && user != (mergePhi)) {
            user->modifyUse(mergePhi, use->idx);
        }
    }
}


void LoopStrengthReduction::addConstAndModToMulAndMod() {
    //TODO: ret += const; ret %= mod; i++;
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            if (bb->isLoopHeader) {
                addConstAndModLoopInit(bb->loop);
            }
        }
    }
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            if (bb->isLoopHeader && bb->loop->isAddAndModLoop) {
                addConstAndModToMulAndModForLoop(bb->loop);
            }
        }
    }
}

void LoopStrengthReduction::addConstAndModLoopInit(Loop* loop) {
    loop->clearAddAndModLoopInfo();
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
    INSTR::Alu* add = nullptr;
    INSTR::Alu* rem = nullptr;
    INSTR::Phi* phi = nullptr;
    int add_cnt = 0, phi_cnt = 0, rem_cnt = 0;
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
                    if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::ADD)) {
                        add = (INSTR::Alu*) instr;
                        add_cnt++;
                    } else if((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::REM)) {
                        rem = (INSTR::Alu*) instr;
                        rem_cnt++;
                    } else {
                        return;
                    }
                } else if (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
                    phi = (INSTR::Phi*) instr;
                    phi_cnt++;
                } else {
                    return;
                }
            }
        }
    }
    if (add_cnt != 1 || rem_cnt != 1 || phi_cnt != 1) {
        return;
    }
    if (useOutLoop(add, loop) || useOutLoop(rem, loop) || !useOutLoop(phi, loop)) {
        return;
    }
    if (!(dynamic_cast<ConstantInt*>(rem->getRVal2()) != nullptr) || !(rem->getRVal1() == add)) {
        return;
    }

    //->indexOf(latch);
    int latchIndex = std::find(head->precBBs->begin(), head->precBBs->end(), latch) - head->precBBs->begin();
    int enteringIndex = 1 - latchIndex;
    if (phi->useValueList[latchIndex] != (rem)) {
        return;
    }
    if (!(std::find(add->useValueList.begin(), add->useValueList.end(), phi) != add->useValueList.end())) {
        return;
    }
    int addConstIndex = 1 - (std::find(add->useValueList.begin(), add->useValueList.end(), phi) - add->useValueList.begin());
    //TODO:待强化,使用的只要是同一个值就可以?
    //      且当前没有考虑float!!!
    if (!(dynamic_cast<ConstantInt*>(add->useValueList[addConstIndex]) != nullptr)) {
        return;
    }
    //此限制是否必须,计算值的初始值为常数
    if (!(dynamic_cast<ConstantInt*>(phi->useValueList[enteringIndex]) != nullptr)) {
        return;
    }
    int base = (int) ((ConstantInt*) add->useValueList[addConstIndex])->get_const_val();
    int mod = (int) ((ConstantInt*) rem->getRVal2())->get_const_val();
    int init = (int) ((ConstantInt*) phi->useValueList[enteringIndex])->get_const_val();
    loop->setAddAndModLoopInfo(phi, add, rem, init, base ,mod, addConstIndex);
}

bool LoopStrengthReduction::useOutLoop(Instr* instr, Loop* loop) {
    for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        Instr* user = use->user;
        if (user->bb->loop != (loop)) {
            return true;
        }
    }
    return false;
}

void LoopStrengthReduction::addConstAndModToMulAndModForLoop(Loop* loop) {
    if (!(dynamic_cast<ConstantInt*>(loop->getIdcInit()) != nullptr) || !(dynamic_cast<ConstantInt*>(loop->getIdcStep()) != nullptr)) {
        return;
    }
    if (!(((INSTR::Alu*) loop->getIdcAlu())->op == INSTR::Alu::Op::ADD)) {
        return;
    }
    //get index
    BasicBlock* loopLatch = nullptr;
    for (BasicBlock* bb: *loop->getLatchs()) {
        loopLatch = bb;
    }
    BasicBlock* loopExit = nullptr;
    for (BasicBlock* bb: *loop->getExits()) {
        loopExit = bb;
    }
    //->indexOf(loopLatch)
    int index = 1 - (std::find(loop->getHeader()->precBBs->begin(), loop->getHeader()->precBBs->end(), loopLatch) - loop->getHeader()->precBBs->begin());
    int i_init = (int) ((ConstantInt*) loop->getIdcInit())->get_const_val();
    int i_step = (int) ((ConstantInt*) loop->getIdcStep())->get_const_val();
    Value* i_end = loop->getIdcEnd();

//    if (i_step != 1) {
//        return;
//    }




    Loop* preLoop = new Loop(loop->getParentLoop());
    Function* func = loop->getHeader()->function;


    int base = loop->getBase();
    int mod = loop->getMod();
    int ans_init = loop->getInit();

    if (base < 0) {
        return;
    }

    if (ans_init != 0) {
        return;
    }

    std::vector<Value*>* params = new std::vector<Value*>();
    params->push_back(i_end);
    params->push_back(new ConstantInt(base));
    params->push_back(new ConstantInt(mod));
    BasicBlock* loop_entering = *loop->enterings->begin();
    INSTR::Call* call = new INSTR::Call(ExternFunction::LLMMOD, params, loop_entering);
    loop_entering->getEndInstr()->insert_before(call);
    BasicBlock* loop_head = loop->header;
    if (dynamic_cast<INSTR::Jump*>(loop_entering->getEndInstr()) == nullptr) {
        return;
    }
    INSTR::Jump* br = (INSTR::Jump*) loop_entering->getEndInstr();
    br->modifyUse(loopExit, 0);
    loop_entering->modifySuc(loop_head, loopExit);
    loopExit->modifyPre(loop_head, loop_entering);
    loop->modPhi->modifyAllUseThisToUseA(call);
//    loop_head->remove();
//    for_instr_(loop_head) {
//        instr->remove();
//    }
//    loopLatch->remove();
//    for_instr_(loopLatch) {
//        instr->remove();
//    }
    return;


//old_version
    int time_init = (mod - ans_init) / base + 1;
    BasicBlock* entering = nullptr;
    for (BasicBlock* bb: *loop->getEnterings()) {
        entering = bb;
    }
    BasicBlock* head = new BasicBlock(func, preLoop);
    BasicBlock* latch = new BasicBlock(func, preLoop);
    BasicBlock* exit = new BasicBlock(func, preLoop);
    head->setLoopHeader();
    preLoop->addBB(head);
    preLoop->addBB(latch);
    preLoop->setHeader(head);

    //entering
    entering->modifyBrAToB(loop->getHeader(), head);
    entering->modifySuc(loop->getHeader(), head);

    //head
    INSTR::Phi* i_phi = new INSTR::Phi(BasicType::I32_TYPE, new std::vector<Value*>(), head);
    INSTR::Phi* time_phi = new INSTR::Phi(BasicType::I32_TYPE, new std::vector<Value*>(), head);
    INSTR::Phi* ans_phi = new INSTR::Phi(BasicType::I32_TYPE, new std::vector<Value*>(), head);
    INSTR::Alu* head_add = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, i_phi, time_phi, head);
    INSTR::Icmp* icmp = new INSTR::Icmp(((INSTR::Icmp*) loop->getIdcCmp())->op, head_add, i_end, head);
    INSTR::Branch* head_br = new INSTR::Branch(icmp, latch, exit, head);

    head->addPre(entering);
    head->addPre(latch);
    head->addSuc(latch);
    head->addSuc(exit);

    //latch
    INSTR::Alu* latch_mul = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, new ConstantInt(base), time_phi, latch);
    INSTR::Alu* latch_add_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, ans_phi, latch_mul, latch);
    INSTR::Alu* latch_rem = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::REM, latch_add_1, new ConstantInt(mod), latch);
    INSTR::Alu* latch_sub = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SUB, new ConstantInt(mod), latch_rem, latch);
    INSTR::Alu* latch_div = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, latch_sub, new ConstantInt(15), latch);
    INSTR::Alu* latch_add_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, latch_div, new ConstantInt(1), latch);
    INSTR::Jump* latch_jump = new INSTR::Jump(head, latch);

    //exit
    INSTR::Alu* exit_sub = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SUB, i_end, i_phi, exit);
    INSTR::Alu* exit_mul = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, new ConstantInt(15), exit_sub, exit);
    INSTR::Alu* exit_add = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, ans_phi, exit_mul, exit);
    INSTR::Alu* exit_rem = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::REM, exit_add, new ConstantInt(1500000001), exit);
    INSTR::Jump* exit_jump = new INSTR::Jump(loopExit, exit);
    exit->addPre(head);
    exit->addSuc(loopExit);

    //head-phi
    i_phi->addOptionalValue(new ConstantInt(i_init));
    i_phi->addOptionalValue(head_add);
    ans_phi->addOptionalValue(new ConstantInt(ans_init));
    ans_phi->addOptionalValue(latch_rem);
    time_phi->addOptionalValue(new ConstantInt(time_init));
    time_phi->addOptionalValue(latch_add_2);

    //next-loop-head
    //loop->getHeader()->modifyPre(entering, exit);
    loopExit->modifyPre(loop->getHeader(), exit);


    ((Instr*) loop->getIdcPHI())->modifyUse(i_phi, index);
    ((Instr*) loop->getModPhi())->modifyUse(ans_phi, index);

    loop->getModPhi()->modifyAllUseThisToUseA(exit_rem);

    //remove loop
    std::set<BasicBlock*>* removes = new std::set<BasicBlock*>();
//    removes->insertAll(loop->getNowLevelBB());
    for (auto t: *loop->getNowLevelBB()) {
        removes->insert(t);
    }
    for (BasicBlock* bb: *removes) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            instr->remove();
        }
        bb->remove();
    }

}

void LoopStrengthReduction::addIdcAndModToMulAndMod() {
    //TODO: ret += const; ret %= mod; i++;
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            if (bb->isLoopHeader) {
                addIdcAndModLoopInit(bb->loop);
            }
        }
    }
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            if (bb->isLoopHeader && bb->loop->isAddAndModLoop) {
                addIdcAndModToMulAndModForLoop(bb->loop);
            }
        }
    }
}


void LoopStrengthReduction::addIdcAndModLoopInit(Loop* loop) {
    loop->clearAddAndModLoopInfo();
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
    INSTR::Alu* add = nullptr;
    INSTR::Alu* rem = nullptr;
    INSTR::Phi* phi = nullptr;
    int add_cnt = 0, phi_cnt = 0, rem_cnt = 0;
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
                    if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::ADD)) {
                        add = (INSTR::Alu*) instr;
                        add_cnt++;
                    } else if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::REM)) {
                        rem = (INSTR::Alu*) instr;
                        rem_cnt++;
                    } else {
                        return;
                    }
                } else if (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
                    phi = (INSTR::Phi*) instr;
                    phi_cnt++;
                } else {
                    return;
                }
            }
        }
    }
    if (add_cnt != 1 || rem_cnt != 1 || phi_cnt != 1) {
        return;
    }
    if (useOutLoop(add, loop) || useOutLoop(rem, loop) || !useOutLoop(phi, loop)) {
        return;
    }
    if (!(dynamic_cast<ConstantInt*>(rem->getRVal2()) != nullptr) || !(rem->getRVal1() == add)) {
        return;
    }
    //rem->getRVal1
    int latchIndex = std::find(head->precBBs->begin(), head->precBBs->end(), latch) - head->precBBs->begin();
    int enteringIndex = 1 - latchIndex;
    if (phi->useValueList[latchIndex] != (rem)) {
        return;
    }
    if (!(std::find(add->useValueList.begin(), add->useValueList.end(), phi) != add->useValueList.end())) {
        return;
    }
    int addIdcIndex = 1 - (std::find(add->useValueList.begin(), add->useValueList.end(), phi) - add->useValueList.begin());
    //TODO:待强化,使用的只要是同一个值就可以?
    //      且当前没有考虑float!!!
    if (!((add->useValueList[addIdcIndex] == loop->getIdcPHI()))) {
        return;
    }
    //此限制是否必须,计算值的初始值为常数
    if (!(dynamic_cast<ConstantInt*>(phi->useValueList[enteringIndex]))) {
        return;
    }
    int base = 0;
    int mod = (int) ((ConstantInt*) rem->getRVal2())->get_const_val();
    int init = (int) ((ConstantInt*) phi->useValueList[enteringIndex])->get_const_val();
    loop->setAddAndModLoopInfo(phi, add, rem, init, base ,mod, addIdcIndex);
}

void LoopStrengthReduction::addIdcAndModToMulAndModForLoop(Loop* loop) {
    if (!(dynamic_cast<ConstantInt*>(loop->getIdcInit()) != nullptr) || !(dynamic_cast<ConstantInt*>(loop->getIdcStep()) != nullptr)) {
        return;
    }
    if (!(((INSTR::Alu*) loop->getIdcAlu())->op == INSTR::Alu::Op::ADD)) {
        return;
    }
    if (((INSTR::Icmp*) loop->getIdcCmp())->op != (INSTR::Icmp::Op::SLT)) {
        return;
    }
    //get index
    BasicBlock *loopLatch = *loop->getLatchs()->begin(),
            *loopExit = *loop->getExits()->begin(),
            *loopHead = loop->getHeader();
    //->indexOf(loopLatch)
    int index = 1 - (std::find(loop->getHeader()->precBBs->begin(), loop->getHeader()->precBBs->end(), loopLatch) - loop->getHeader()->precBBs->begin());
    long i_init = (int) ((ConstantInt*) loop->getIdcInit())->get_const_val();
    long i_step = (int) ((ConstantInt*) loop->getIdcStep())->get_const_val();
    Value* i_end = loop->getIdcEnd();
    long ret_init = loop->getInit();
    long mod = loop->getMod();
    BasicBlock* entering = *loop->getEnterings()->begin();
    if (i_step < 0 || i_init < 0 || ret_init < 0) {
        return;
    }

    if (i_step != 1) {
        return;
    }

    if (mod > 2147483647 / 2) {
        addIdcAndModToMulAndModForLoopForBigMod(loop);
        return;
    }

    std::vector<Value*>* params = new std::vector<Value*>();

    Value* n = i_end;
    Value* n_sub_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SUB,
                                    n, new ConstantInt(1), entering);
    params->push_back(n);
    params->push_back(n_sub_1);
    params->push_back(new ConstantInt(mod));
//    BasicBlock* loop_entering = *loop->enterings->begin();
    INSTR::Call* call = new INSTR::Call(ExternFunction::LLMD2MOD, params, entering);

    entering->getEndInstr()->insert_before(n_sub_1);
    entering->getEndInstr()->insert_before(call);
//    BasicBlock* loop_head = loop->header;
    if (dynamic_cast<INSTR::Jump*>(entering->getEndInstr()) == nullptr) {
        return;
    }
    INSTR::Jump* br = (INSTR::Jump*) entering->getEndInstr();
    br->modifyUse(loopExit, 0);
    entering->modifySuc(loopHead, loopExit);
    loopExit->modifyPre(loopHead, entering);
    loop->modPhi->modifyAllUseThisToUseA(call);

    return;

    //insert in entering
//    Instr* old_br = entering->getEndInstr();
//    Value* n = i_end;
//    Value* n_sub_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, n, new ConstantInt(1), entering);
////    INSTR::Phi* n_phi = nullptr;
////    INSTR::Phi* n_add_1_phi = nullptr;
//
//    BasicBlock* L = new BasicBlock(entering->function, entering->loop);
//    BasicBlock* R = new BasicBlock(entering->function, entering->loop);
//    //L: n / 2 R: (n + 1) / 2
//    INSTR::Alu* n_div_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, n, new ConstantInt(2), L);
//
//    INSTR::Alu* n_add_1_div_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, n_sub_1, new ConstantInt(2), L);
//    INSTR::Jump* L_jump = new INSTR::Jump(loopExit, L);
//    INSTR::Jump* R_jump = new INSTR::Jump(loopExit, R);
//
//    INSTR::Alu* n_mod_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::REM, n, new ConstantInt(2), entering);
//    INSTR::Icmp* cmp = new INSTR::Icmp(INSTR::Icmp::Op::EQ, n_mod_2, new ConstantInt(0), entering);
//    INSTR::Branch* new_br = new INSTR::Branch(cmp, L, R, entering);
//
//
//    std::vector<BasicBlock*>* entering_suc = new std::vector<BasicBlock*>();
//    entering_suc->push_back(L);
//    entering_suc->push_back(R);
//    entering->modifySucs(entering_suc);
//
//
//    old_br->insert_before(n_sub_1);
//    old_br->insert_before(n_mod_2);
//    old_br->insert_before(cmp);
//    old_br->insert_before(new_br);
//    old_br->remove();
//
//    std::vector<BasicBlock*>* exit_pres = new std::vector<BasicBlock*>();
//    exit_pres->push_back(L);
//    exit_pres->push_back(R);
//    loopExit->modifyPres(exit_pres);
//    std::vector<Value*> *n_phi_values = new std::vector<Value*>(), *n_add_1_phi_values = new std::vector<Value*>();
//    n_phi_values->push_back(n_div_2);
//    n_phi_values->push_back(n);



//    INSTR::Phi* n_phi =









    return;




//old version


    Function* fuc = entering->function;
    Loop* enterLoop = entering->loop;

    BasicBlock* transBB = new BasicBlock(fuc, enterLoop);
    BasicBlock* phiBB = new BasicBlock(fuc, enterLoop);

    entering->modifyBrAToB(loopHead, transBB);
    entering->modifySuc(loopHead, transBB);
    transBB->addPre(entering);


    phiBB->addSuc(loopHead);
    loopHead->modifyPre(entering, phiBB);

    INSTR::Phi* idc_phi = new INSTR::Phi(BasicType::I32_TYPE, new std::vector<Value*>(), phiBB);
    INSTR::Phi* ret_phi = new INSTR::Phi(BasicType::I32_TYPE, new std::vector<Value*>(), phiBB);
    INSTR::Jump* PhiBB_Jump = new INSTR::Jump(loopHead, phiBB);

    INSTR::Alu* nn = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, i_end, i_end, transBB);
    INSTR::Alu* _2_i_sub_step_n = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL,
                                                 new ConstantInt((int) (2 * i_init - i_step)), i_end, transBB);
    INSTR::Alu* nn_step = nn;
    if (i_step != 1) {
        nn_step = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL,
                                 new ConstantInt((int) i_step), nn, transBB);
    }
    INSTR::Alu* add = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD,
                                     nn_step, _2_i_sub_step_n, transBB);
    INSTR::Alu* div = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV,
                                     add, new ConstantInt(2), transBB);

    for (int cnt = 0; cnt < lift_times; cnt++) {
        double a = (double) i_step, b = ((double) 2 * i_init - i_step), c = ((double) 2 * (ret_init - mod));
        double det  = (b * b - 4 * a * c);
        if (det <= 0) {
            return;
        }
        double ans1 = (-b - sqrt(det)) / (2 * a), ans2 = (-b + sqrt(det)) / (2 * a);
        long length = ans1 < -1? (long) ans2 + 1 : (long) ans1 + 1;
        //insert cmp br
        BasicBlock* bb = new BasicBlock(fuc, enterLoop);
        INSTR::Icmp* icmp = new INSTR::Icmp(INSTR::Icmp::Op::SLT, new ConstantInt((int) (i_init + length)), i_end, transBB);
        INSTR::Branch* br = new INSTR::Branch(icmp, bb, phiBB, transBB);
        transBB->addSuc(bb);
        transBB->addSuc(phiBB);
        bb->addPre(transBB);

        phiBB->addPre(transBB);
        //TODO:考虑溢出,先除
        if (cnt == 0) {
            idc_phi->addOptionalValue(i_end);
            ret_phi->addOptionalValue(div);
        } else {
            idc_phi->addOptionalValue(new ConstantInt((int) i_init));
            ret_phi->addOptionalValue(new ConstantInt((int) ret_init));
        }
        //calc in bb
        long val1 = (i_init + i_init + (length - 1) * i_step), val2 = length;
        if (val1 % 2 == 0) {
            val1 /= 2;
        } else {
            val2 /= 2;
        }
        //assert val1 * val2 > 0;
        ret_init = (int) ((ret_init + (val1 * val2) % mod) % mod);
        i_init = i_init + length * i_step;
        //
        transBB = bb;
    }

    //transBB -> phiBB
    INSTR::Jump* jump = new INSTR::Jump(phiBB, transBB);
    transBB->addSuc(phiBB);
    phiBB->addPre(transBB);
    idc_phi->addOptionalValue(new ConstantInt((int) i_init));
    ret_phi->addOptionalValue(new ConstantInt((int) ret_init));


    ((INSTR::Phi*) loop->getIdcPHI())->modifyUse(idc_phi, index);
    ((INSTR::Phi*) loop->getModPhi())->modifyUse(ret_phi, index);

    //loop->getRemIS()->modifyAllUseThisToUseA(loop->getAddIS());

}

void LoopStrengthReduction::addIdcAndModToMulAndModForLoopForBigMod(Loop* loop) {
    if (!(dynamic_cast<ConstantInt*>(loop->getIdcInit()) != nullptr) || !(dynamic_cast<ConstantInt*>(loop->getIdcStep()) != nullptr)) {
        return;
    }
    if (!(((INSTR::Alu*) loop->getIdcAlu())->op == INSTR::Alu::Op::ADD)) {
        return;
    }
    if (((INSTR::Icmp*) loop->getIdcCmp())->op != (INSTR::Icmp::Op::SLT)) {
        return;
    }
    //get index
    BasicBlock *loopLatch = *loop->getLatchs()->begin(),
            *loopExit = *loop->getExits()->begin(),
            *loopHead = loop->getHeader();

    //->indexOf(loopLatch)
    int index = 1 - (std::find(loop->getHeader()->precBBs->begin(), loop->getHeader()->precBBs->end(), loopLatch) - loop->getHeader()->precBBs->begin());
    long i_init = (int) ((ConstantInt*) loop->getIdcInit())->get_const_val();
    long i_step = (int) ((ConstantInt*) loop->getIdcStep())->get_const_val();
    Value* i_end = loop->getIdcEnd();
    long ret_init = loop->getInit();
    long mod = loop->getMod();
    BasicBlock* entering = *loop->getEnterings()->begin();
    if (i_step < 0 || i_init < 0 || ret_init < 0) {
        return;
    }

    //TODO:待加强,如果放开这个限制,需要修改 Begin/end/then/else BB 等
    if (i_init != 0 || i_step != 1 || ret_init != 0) {
        return;
    }

    Function* fuc = entering->function;
    Loop* enterLoop = entering->loop;

    BasicBlock* transBB = new BasicBlock(fuc, enterLoop);
    BasicBlock* phiBB = new BasicBlock(fuc, enterLoop);

    entering->modifyBrAToB(loopHead, transBB);
    entering->modifySuc(loopHead, transBB);
    transBB->addPre(entering);


    phiBB->addSuc(loopHead);
    loopHead->modifyPre(entering, phiBB);

    INSTR::Phi* idc_phi = new INSTR::Phi(BasicType::I32_TYPE, new std::vector<Value*>(), phiBB);
    INSTR::Phi* ret_phi = new INSTR::Phi(BasicType::I32_TYPE, new std::vector<Value*>(), phiBB);
    INSTR::Jump* PhiBB_Jump = new INSTR::Jump(loopHead, phiBB);


    BasicBlock* beginBB = new BasicBlock(fuc, enterLoop);
    BasicBlock* thenBB = new BasicBlock(fuc, enterLoop);
    BasicBlock* elseBB = new BasicBlock(fuc, enterLoop);
    BasicBlock* endBB = new BasicBlock(fuc, enterLoop);
    //TODO:维护第一个

    //int val1 = n, val2 = n - 1; if (n % 2 == 0)
    INSTR::Alu* val2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SUB, i_end, new ConstantInt(1), beginBB);
    INSTR::Alu* n_rem_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::REM, i_end, new ConstantInt(2), beginBB);
    INSTR::Icmp* icmp_begin_bb = new INSTR::Icmp(INSTR::Icmp::Op::EQ, n_rem_2, new ConstantInt(0), beginBB);
    INSTR::Branch* br_begin_bb = new INSTR::Branch(icmp_begin_bb, thenBB, elseBB, beginBB);
    beginBB->addSuc(thenBB);
    beginBB->addSuc(elseBB);


    //
    thenBB->addPre(beginBB);
    INSTR::Alu* val1_half = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, i_end, new ConstantInt(2), thenBB);
    INSTR::Jump* jump_then_BB = new INSTR::Jump(endBB, thenBB);
    thenBB->addSuc(endBB);


    //
    elseBB->addPre(beginBB);
    INSTR::Alu* val2_half = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, val2, new ConstantInt(2), elseBB);
    INSTR::Jump* jump_else_BB = new INSTR::Jump(endBB, elseBB);
    elseBB->addSuc(endBB);

    //
    endBB->addPre(thenBB);
    endBB->addPre(elseBB);
    std::vector<Value*>* values1 = new std::vector<Value*>();
    values1->push_back(val1_half);
    values1->push_back(i_end);
    INSTR::Phi* val1_phi = new INSTR::Phi(BasicType::I32_TYPE, values1, endBB);
    std::vector<Value*>* values2 = new std::vector<Value*>();
    values2->push_back(val2);
    values2->push_back(val2_half);
    INSTR::Phi* val2_phi = new INSTR::Phi(BasicType::I32_TYPE, values2, endBB);
    INSTR::Alu* val1_val2_half = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, val1_phi, val2_phi, endBB);
    INSTR::Jump* jump_end_bb = new INSTR::Jump(phiBB, endBB);
    endBB->addSuc(phiBB);
    phiBB->addPre(endBB);


    for (int cnt = 0; cnt < lift_times; cnt++) {
        double a = (double) i_step, b = ((double) 2 * i_init - i_step), c = ((double) 2 * (ret_init - mod));
        double det  = (b * b - 4 * a * c);
        if (det <= 0) {
            return;
        }
        double ans1 = (-b - sqrt(det)) / (2 * a), ans2 = (-b + sqrt(det)) / (2 * a);
        long length = ans1 < -1? (long) ans2 + 1 : (long) ans1 + 1;
        //insert cmp br
        BasicBlock* bb = new BasicBlock(fuc, enterLoop);
        INSTR::Icmp* icmp = new INSTR::Icmp(INSTR::Icmp::Op::SLT, new ConstantInt((int) (i_init + length)), i_end, transBB);
        if (cnt == 0) {
            INSTR::Branch* br = new INSTR::Branch(icmp, bb, beginBB, transBB);
            transBB->addSuc(bb);
            transBB->addSuc(beginBB);
            bb->addPre(transBB);
            beginBB->addPre(transBB);
        } else {
            INSTR::Branch* br = new INSTR::Branch(icmp, bb, phiBB, transBB);
            transBB->addSuc(bb);
            transBB->addSuc(phiBB);
            bb->addPre(transBB);
            phiBB->addPre(transBB);
        }

        if (cnt == 0) {
            idc_phi->addOptionalValue(i_end);
            ret_phi->addOptionalValue(val1_val2_half);
        } else {
            idc_phi->addOptionalValue(new ConstantInt((int) i_init));
            ret_phi->addOptionalValue(new ConstantInt((int) ret_init));
        }
        //calc in bb
        long value1 = (i_init + i_init + (length - 1) * i_step), value2 = length;
        if (value1 % 2 == 0) {
            value1 /= 2;
        } else {
            value2 /= 2;
        }
        //assert val1 * val2 > 0;
        ret_init = (int) ((ret_init + (value1 * value2) % mod) % mod);
        i_init = i_init + length * i_step;
        //
        transBB = bb;
    }

    //transBB -> phiBB
    INSTR::Jump* jump = new INSTR::Jump(phiBB, transBB);
    transBB->addSuc(phiBB);
    phiBB->addPre(transBB);
    idc_phi->addOptionalValue(new ConstantInt((int) i_init));
    ret_phi->addOptionalValue(new ConstantInt((int) ret_init));


    ((INSTR::Phi*) loop->getIdcPHI())->modifyUse(idc_phi, index);
    ((INSTR::Phi*) loop->getModPhi())->modifyUse(ret_phi, index);

}
