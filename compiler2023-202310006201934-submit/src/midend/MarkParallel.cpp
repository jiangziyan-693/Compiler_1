#include "../../include/midend/MarkParallel.h"
#include "../../include/util/CenterControl.h"
#include "stl_op.h"
#include "Manager.h"


MarkParallel::MarkParallel(std::vector<Function *> *functions) {
    this->functions = functions;
}

void MarkParallel::Run() {
    for (Function *function: *functions) {
        know->clear();
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            if (!(know->find(bb) != know->end()) && bb->isLoopHeader && bb->loop->isCanAggressiveParallel()) {
                doMarkParallel(bb->loop);
            } else if (!(know->find(bb) != know->end()) && bb->isLoopHeader) {
                markLoop(bb->loop);
//                markLoopDebug(bb->loop);
            }
        }
    }
}

bool MarkParallel::isPureLoop(Loop *loop) {
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

void MarkParallel::markLoop(Loop *loop) {
    if (loop->getHeader()->label == "b33") {
        printf("debug\n");
    }
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

    std::set<Value *> *load = new std::set<Value *>(), *store = new std::set<Value *>();
    std::unordered_map<Value *, std::set<Instr *>*> *loadGep = new std::unordered_map<Value *, std::set<Instr *>*>(),
            *storeGep = new std::unordered_map<Value *, std::set<Instr *>*>();

    for (BasicBlock *bb: *bbs) {
        for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
//            std::cout << instr->operator std::string() << std::endl;
//            if (instr->operator std::string() == "%v40 = load i32, i32* %v39") {
//                printf("debug\n");
//            }
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
            else if (is_type(INSTR::Store, instr)) {
//                printf("fuck");
                Value* array = ((INSTR::Store*) instr)->getPointer();
                while (is_type(INSTR::GetElementPtr, array)) {
                    array = ((INSTR::GetElementPtr*) array)->getPtr();
                }
                store->insert(array);
                (*storeGep)[array] = new std::set<Instr*>();
                Instr* gep = (Instr*) ((INSTR::Store*) instr)->getPointer();
                while (is_type(INSTR::GetElementPtr, gep)) {
                    (*storeGep)[array]->insert(gep);
                    gep = (Instr*) ((INSTR::GetElementPtr*) gep)->getPtr();
                }
            }
            else if (is_type(INSTR::Load, instr)) {
//                printf("fuck\n");

                Value* array = ((INSTR::Load*) instr)->getPointer();
                while (is_type(INSTR::GetElementPtr, array)) {
                    array = ((INSTR::GetElementPtr*) array)->getPtr();
                }
                load->insert(array);
                (*loadGep)[array] = new std::set<Instr*>();
                Instr* gep = (Instr*) ((INSTR::Load*) instr)->getPointer();
                while (is_type(INSTR::GetElementPtr, gep)) {
                    (*loadGep)[array]->insert(gep);
                    gep = (Instr*) ((INSTR::GetElementPtr*) gep)->getPtr();
                }
            }
        }
    }

    if (store->size() == 1 && _contains_(load, *store->begin())) {
        for (Instr* gep: *(*storeGep)[*store->begin()]) {
            if (!_contains_((*loadGep)[*store->begin()], gep)) {
                return;
            }
        }
    }


    Value *idcEnd = loop->getIdcEnd();
    //TODO:check again
//    if (dynamic_cast<Constant *>(idcEnd) != nullptr) {
//        return;
//    }
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

    _set_add_all(bbs, know);

    load->clear();
    store->clear();
    loadGep->clear();
    storeGep->clear();
}

void MarkParallel::DFS(std::set<BasicBlock *> *bbs, std::set<Value *> *idcVars, std::set<Loop *> *loops, Loop *loop) {
    loops->insert(loop);
    _set_add_all(loop->getNowLevelBB(), bbs);
    idcVars->insert(loop->getIdcPHI());
    for (Loop *next: *loop->getChildrenLoops()) {
        DFS(bbs, idcVars, loops, next);
    }
}

bool MarkParallel::useOutLoops(Value *value, std::set<Loop *> *loops) {
    for (Use *use = value->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
        Instr *user = use->user;
        if (!_contains_(loops, user->bb->loop)) {
            return true;
        }
    }
    return false;
}

void MarkParallel::doMarkParallel(Loop *loop) {
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

    //know->addAll(bbs);
    _set_add_all(bbs, know);
}

//void MarkParallel::markLoopDebug(Loop *loop) {
//    return;
//
//}


    void MarkParallel::markLoopDebug(Loop* loop) {
//        if (loop->getHash() != 8) {
//            //
//            return;
//        }
//        return;
        if (loop->getHeader()->label != "b13") {
            return;
        }

//        if (loop->loopDepth == 1) {
//            if (loop->childrenLoops->size() != 1) {
//                return;
//            }
//            Loop* loop1 = *loop->childrenLoops->begin();
//            if (loop1->childrenLoops->size() != 1) {
//                return;
//            }
//            Loop* loop2 = *loop1->childrenLoops->begin();
//            if (loop2->hasChildLoop()) {
//                return;
//            }
//        }

        std::set<BasicBlock*>* bbs = new std::set<BasicBlock*>();
        std::set<Value*>* idcVars = new std::set<Value*>();
        std::set<Loop*>* loops = new std::set<Loop*>();
        DFS(bbs, idcVars, loops, loop);

        Value* idcEnd = loop->getIdcEnd();
//        if (dynamic_cast<Constant*>(idcEnd) != nullptr) {
//            return;
//        }
        if (loop->getEnterings()->size() > 1) {
            return;
        }
        BasicBlock* entering = *loop->getEnterings()->begin();
        BasicBlock* head = loop->getHeader();
        BasicBlock* latch = *loop->getLatchs()->begin();
        BasicBlock* exiting = *loop->getExitings()->begin();
        BasicBlock* exit = *loop->getExits()->begin();
        int index = 1 - _index_of(head->precBBs, latch);
        INSTR::Phi* idcPhi = (INSTR::Phi*) loop->getIdcPHI();
        INSTR::Icmp* cmp = (INSTR::Icmp*) loop->getIdcCmp();
        Function* func = loop->getHeader()->function;
        BasicBlock* parallelStartBB = new BasicBlock(func, loop->getParentLoop());
        BasicBlock* parallelEndBB = new BasicBlock(func, loop->getParentLoop());

        if (cmp->op != INSTR::Icmp::Op::SLT) {
            return;
        }

        //start BB
        INSTR::Call* startCall = new INSTR::Call(ExternFunction::PARALLEL_START, new std::vector<Value*>(), parallelStartBB);
        INSTR::Alu* mul_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, startCall, idcEnd, parallelStartBB);
        INSTR::Alu* div_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, mul_1, new ConstantInt(parallel_num), parallelStartBB);
        idcPhi->modifyUse(div_1, index);
        INSTR::Alu* add_1 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, startCall, new ConstantInt(1), parallelStartBB);
        INSTR::Alu* mul_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, add_1, idcEnd, parallelStartBB);
        INSTR::Alu* div_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, mul_2, new ConstantInt(parallel_num), parallelStartBB);
        cmp->modifyUse(div_2, 1);
        INSTR::Jump* jump_1 = new INSTR::Jump(head, parallelStartBB);

        entering->modifyBrAToB(head, parallelStartBB);
        entering->modifySuc(head, parallelStartBB);
        parallelStartBB->addPre(entering);
        parallelStartBB->addSuc(head);
        head->modifyPre(entering, parallelStartBB);


        //end BB
        std::vector<Value*>* args = new std::vector<Value*>();
        args->push_back(startCall);
        INSTR::Call* endCall = new INSTR::Call(ExternFunction::PARALLEL_END, args, parallelEndBB);
        INSTR::Jump* jump_2 = new INSTR::Jump(exit, parallelEndBB);


        exiting->modifyBrAToB(exit, parallelEndBB);
        exiting->modifySuc(exit, parallelEndBB);
        parallelEndBB->addPre(exiting);
        parallelEndBB->addSuc(exit);
        exit->modifyPre(exiting, parallelEndBB);

//        know->addAll(bbs);
        _set_add_all(bbs, know);
    }

