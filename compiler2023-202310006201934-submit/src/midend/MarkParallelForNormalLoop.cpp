#include "../../include/midend/MarkParallelForNormalLoop.h"
#include "../../include/util/CenterControl.h"
#include "stl_op.h"
#include "Manager.h"


MarkParallelForNormalLoop::MarkParallelForNormalLoop(std::vector<Function *> *functions) {
    this->functions = functions;
}

void MarkParallelForNormalLoop::Run() {
    for (Function *function: *functions) {
        know->clear();
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            if (!(know->find(bb) != know->end()) && bb->isLoopHeader && bb->loop->isCanAggressiveParallel()) {
                doMarkParallel(bb->loop);
            } else if (!(know->find(bb) != know->end()) && bb->isLoopHeader) {
                //markLoop(bb->loop);
                markLoopDebug(bb->loop);
            }
        }
    }
}

bool MarkParallelForNormalLoop::isPureLoop(Loop *loop) {
    if (!loop->hasChildLoop()) {
        return true;
    }
    if (loop->getChildrenLoops()->size() > 1) {
        return false;
    }
    if (!loop->isSimpleLoop() || !loop->isIdcSet()) {
        return false;
    }
    return isPureLoop(*loop->getChildrenLoops()->begin());
}

void MarkParallelForNormalLoop::markLoop(Loop *loop) {
    if (!isPureLoop(loop)) {
        return;
    }
    std::set<BasicBlock *> *bbs = new std::set<BasicBlock *>();
    std::set<Value *> *idcVars = new std::set<Value *>();
    std::set<Loop *> *loops = new std::set<Loop *>();
    DFS(bbs, idcVars, loops, loop);

    for (Loop *temp: *loops) {
        for (Instr *instr = temp->getHeader()->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
            if (dynamic_cast<INSTR::Phi *>(instr) != nullptr && instr != (temp->getIdcPHI())) {
                return;
            }
        }
    }

    std::set<Value *> *load = new std::set<Value *>(),
            *store = new std::set<Value *>();
    std::unordered_map<Value *, Instr *> *loadGep = new std::unordered_map<Value *, Instr *>(),
            *storeGep = new std::unordered_map<Value *, Instr *>();

    for (BasicBlock *bb: *bbs) {
        for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
            if (dynamic_cast<INSTR::Call *>(instr) != nullptr) {
                return;
            }
            if (useOutLoops(instr, loops)) {
                return;
            }
            //trivel写法
            if (dynamic_cast<INSTR::GetElementPtr *>(instr) != nullptr) {
                if (!(dynamic_cast<GlobalValue *>(((INSTR::GetElementPtr *) instr)->getPtr()) != nullptr)) {
                    return;
                }
                for (Value *idc: *((INSTR::GetElementPtr *) instr)->getIdxList()) {
                    if (dynamic_cast<Constant *>(idc) != nullptr && (int) ((ConstantInt *) idc)->get_const_val() == 0) {
                        continue;
                    }
                    if (!(idcVars->find(idc) != idcVars->end())) {
                        return;
                    }
                }
            }
        }
    }


    Value *idcEnd = loop->getIdcEnd();
    if (dynamic_cast<Constant *>(idcEnd) != nullptr) {
        return;
    }
    if (loop->getEnterings()->size() > 1) {
        return;
    }
    BasicBlock *entering = *loop->getEnterings()->begin();
    BasicBlock *head = loop->getHeader();
    BasicBlock *latch = *loop->getLatchs()->begin();
    BasicBlock *exiting = *loop->getExitings()->begin();
    BasicBlock *exit = *loop->getExits()->begin();
    int index = 1 - _index_of(head->precBBs, latch);
    INSTR::Phi *idcPhi = (INSTR::Phi *) loop->getIdcPHI();
    INSTR::Icmp *cmp = (INSTR::Icmp *) loop->getIdcCmp();
    Function *func = loop->getHeader()->function;
    BasicBlock *parallelStartBB = new BasicBlock(func, loop->getParentLoop());
    BasicBlock *parallelEndBB = new BasicBlock(func, loop->getParentLoop());

    if (cmp->op != (INSTR::Icmp::Op::SLT)) {
        return;
    }

    //start BB
    INSTR::Call *startCall = new INSTR::Call(ExternFunction::PARALLEL_START, new std::vector<Value *>(),
                                             parallelStartBB);
    INSTR::Alu *mul_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, startCall, idcEnd, parallelStartBB);
    INSTR::Alu *div_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, mul_1, new ConstantInt(parallel_num),
                                       parallelStartBB);
    idcPhi->modifyUse(div_1, index);
    INSTR::Alu *add_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, startCall, new ConstantInt(1),
                                       parallelStartBB);
    INSTR::Alu *mul_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, add_1, idcEnd, parallelStartBB);
    INSTR::Alu *div_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, mul_2, new ConstantInt(parallel_num),
                                       parallelStartBB);
    cmp->modifyUse(div_2, 1);
    INSTR::Jump *jump_1 = new INSTR::Jump(head, parallelStartBB);

    entering->modifyBrAToB(head, parallelStartBB);
    entering->modifySuc(head, parallelStartBB);
    parallelStartBB->addPre(entering);
    parallelStartBB->addSuc(head);
    head->modifyPre(entering, parallelStartBB);


    //end BB
    std::vector<Value *> *args = new std::vector<Value *>();
    args->push_back(startCall);
    INSTR::Call *endCall = new INSTR::Call(ExternFunction::PARALLEL_END, args, parallelEndBB);
    INSTR::Jump *jump_2 = new INSTR::Jump(exit, parallelEndBB);


    exiting->modifyBrAToB(exit, parallelEndBB);
    exiting->modifySuc(exit, parallelEndBB);
    parallelEndBB->addPre(exiting);
    parallelEndBB->addSuc(exit);
    exit->modifyPre(exiting, parallelEndBB);

//        know->addAll(bbs);
    _set_add_all(bbs, know);
}


void MarkParallelForNormalLoop::DFS(std::set<BasicBlock *> *bbs, std::set<Value *> *idcVars, std::set<Loop *> *loops,
                                    Loop *loop) {
    loops->insert(loop);
//        bbs->addAll(loop->getNowLevelBB());
    _set_add_all(loop->getNowLevelBB(), bbs);
    idcVars->insert(loop->getIdcPHI());
    for (Loop *next: *loop->getChildrenLoops()) {
        DFS(bbs, idcVars, loops, next);
    }
}

bool MarkParallelForNormalLoop::useOutLoops(Value *value, std::set<Loop *> *loops) {
    for (Use *use = value->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
        Instr *user = use->user;
        if (!_contains_(loops, user->bb->loop)) {
            return true;
        }
    }
    return false;
}

void MarkParallelForNormalLoop::doMarkParallel(Loop *loop) {
    std::set<BasicBlock *> *bbs = new std::set<BasicBlock *>();
    std::set<Value *> *idcVars = new std::set<Value *>();
    std::set<Loop *> *loops = new std::set<Loop *>();
    DFS(bbs, idcVars, loops, loop);
    Value *idcEnd = loop->getIdcEnd();
    if (dynamic_cast<Constant *>(idcEnd) != nullptr) {
        return;
    }
    if (loop->getEnterings()->size() > 1) {
        return;
    }
    BasicBlock *entering = *loop->getEnterings()->begin();
    BasicBlock *head = loop->getHeader();
    BasicBlock *latch = *loop->getLatchs()->begin();
    BasicBlock *exiting = *loop->getExitings()->begin();
    BasicBlock *exit = *loop->getExits()->begin();
    int index = 1 - _index_of(head->precBBs, latch);
    INSTR::Phi *idcPhi = (INSTR::Phi *) loop->getIdcPHI();
    INSTR::Icmp *cmp = (INSTR::Icmp *) loop->getIdcCmp();
    Function *func = loop->getHeader()->function;
    BasicBlock *parallelStartBB = new BasicBlock(func, loop->getParentLoop());
    BasicBlock *parallelEndBB = new BasicBlock(func, loop->getParentLoop());

    if (cmp->op != (INSTR::Icmp::Op::SLT)) {
        return;
    }

    //start BB
    INSTR::Call *startCall = new INSTR::Call(ExternFunction::PARALLEL_START, new std::vector<Value *>(),
                                             parallelStartBB);
    INSTR::Alu *mul_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, startCall, idcEnd, parallelStartBB);
    INSTR::Alu *div_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, mul_1, new ConstantInt(parallel_num),
                                       parallelStartBB);
    idcPhi->modifyUse(div_1, index);
    INSTR::Alu *add_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, startCall, new ConstantInt(1),
                                       parallelStartBB);
    INSTR::Alu *mul_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, add_1, idcEnd, parallelStartBB);
    INSTR::Alu *div_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, mul_2, new ConstantInt(parallel_num),
                                       parallelStartBB);
    cmp->modifyUse(div_2, 1);
    INSTR::Jump *jump_1 = new INSTR::Jump(head, parallelStartBB);

    entering->modifyBrAToB(head, parallelStartBB);
    entering->modifySuc(head, parallelStartBB);
    parallelStartBB->addPre(entering);
    parallelStartBB->addSuc(head);
    head->modifyPre(entering, parallelStartBB);


    //end BB
    std::vector<Value *> *args = new std::vector<Value *>();
    args->push_back(startCall);
    INSTR::Call *endCall = new INSTR::Call(ExternFunction::PARALLEL_END, args, parallelEndBB);
    INSTR::Jump *jump_2 = new INSTR::Jump(exit, parallelEndBB);


    exiting->modifyBrAToB(exit, parallelEndBB);
    exiting->modifySuc(exit, parallelEndBB);
    parallelEndBB->addPre(exiting);
    parallelEndBB->addSuc(exit);
    exit->modifyPre(exiting, parallelEndBB);

//        know->addAll(bbs);
    _set_add_all(bbs, know);
}


void MarkParallelForNormalLoop::markLoopDebug(Loop *loop) {
//        if (loop->getHash() != 16) {
//            //
//            return;
//        }
//
//        std::set<BasicBlock*>* bbs = new std::set<BasicBlock*>();
//        std::set<Value*>* idcVars = new std::set<Value*>();
//        std::set<Loop*>* loops = new std::set<Loop*>();
//        DFS(bbs, idcVars, loops, loop);
//
//        Value* idcEnd = loop->getIdcEnd();
//        if (dynamic_cast<Constant*>(idcEnd) != nullptr) {
//            return;
//        }
//        if (loop->getEnterings()->size() > 1) {
//            return;
//        }
//        BasicBlock* entering = loop->getEnterings()->iterator()->next();
//        BasicBlock* head = loop->getHeader();
//        BasicBlock* latch = loop->getLatchs()->iterator()->next();
//        BasicBlock* exiting = loop->getExitings()->iterator()->next();
//        BasicBlock* exit = loop->getExits()->iterator()->next();
//        int index = 1 - _index_of_(head->precBBs, latch);
//        INSTR::Phi* idcPhi = (INSTR::Phi*) loop->getIdcPHI();
//        INSTR::Icmp* cmp = (INSTR::Icmp*) loop->getIdcCmp();
//        Function* func = loop->getHeader()->function;
//        BasicBlock* parallelStartBB = new BasicBlock(func, loop->getParentLoop());
//        BasicBlock* parallelEndBB = new BasicBlock(func, loop->getParentLoop());
//
//        if (!cmp->op->equals(INSTR::Icmp->Op.SLT)) {
//            return;
//        }
//
//        //start BB
//        INSTR::Call* startCall = new INSTR::Call(Manager->ExternFunction.PARALLEL_START, new std::vector<>(), parallelStartBB);
//        INSTR::Alu* mul_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, startCall, idcEnd, parallelStartBB);
//        INSTR::Alu* div_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, mul_1, new ConstantInt(parallel_num), parallelStartBB);
//        idcPhi->modifyUse(div_1, index);
//        INSTR::Alu* add_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, startCall, new ConstantInt(1), parallelStartBB);
//        INSTR::Alu* mul_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, add_1, idcEnd, parallelStartBB);
//        INSTR::Alu* div_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, mul_2, new ConstantInt(parallel_num), parallelStartBB);
//        cmp->modifyUse(div_2, 1);
//        INSTR::Jump* jump_1 = new INSTR::Jump(head, parallelStartBB);
//
//        entering->modifyBrAToB(head, parallelStartBB);
//        entering->modifySuc(head, parallelStartBB);
//        parallelStartBB->addPre(entering);
//        parallelStartBB->addSuc(head);
//        head->modifyPre(entering, parallelStartBB);
//
//
//        //end BB
//        std::vector<Value*>* args = new std::vector<Value*>();
//        args->push_back(startCall);
//        INSTR::Call* endCall = new INSTR::Call(Manager->ExternFunction.PARALLEL_END, args, parallelEndBB);
//        INSTR::Jump* jump_2 = new INSTR::Jump(exit, parallelEndBB);
//
//
//        exiting->modifyBrAToB(exit, parallelEndBB);
//        exiting->modifySuc(exit, parallelEndBB);
//        parallelEndBB->addPre(exiting);
//        parallelEndBB->addSuc(exit);
//        exit->modifyPre(exiting, parallelEndBB);
//
//        know->addAll(bbs);
    return;
}
