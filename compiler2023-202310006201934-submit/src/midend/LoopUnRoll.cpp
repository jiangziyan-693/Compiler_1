#include "../../include/midend/LoopUnRoll.h"
#include "../../include/util/CenterControl.h"
#include <cmath>
#include "../../include/util/HashMap.h"


//TODO:分类:
// 1->归纳变量只有归纳作用:只有一条ALU和PHI->归纳变量可以删除
// (fix:情况1貌似不用考虑,直接展开然后让死代码消除来做这件事情)
// 2->归纳变量有其他user

//LCSSA: 循环内定义的变量 只在循环内使用
//LCSSA的PHI是对于那些在循环中定义在循环外使用的变量插入的
//在循环中定义,外部还能用,意味着在循环前一定有"声明"
//循环中有定义,意味着,Head块中一定有一条它的PHI



LoopUnRoll::LoopUnRoll(std::vector<Function*>* functions) {
    this->functions = functions;
    init();
}

void LoopUnRoll::init() {
    for (Function* function: *functions) {
        std::set<Loop*>* loops = new std::set<Loop*>();
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            loops->insert(bb->loop);
        }
        (*loopsByFunc)[function] = loops;
        (*loopOrdered)[function] = new std::vector<Loop*>();
    }
    for (Function* function: *functions) {
        Loop* pos = function->getBeginBB()->loop;
        DFSForLoopOrder(function, pos);
    }
}

void LoopUnRoll::DFSForLoopOrder(Function* function, Loop* loop) {
    for (Loop* next: *(loop->childrenLoops)) {
        DFSForLoopOrder(function, next);
    }
    (*loopOrdered)[function]->push_back(loop);
}

void LoopUnRoll::Run() {
    constLoopUnRoll();

}

//idc init/step/end 均为常数
void LoopUnRoll::constLoopUnRoll() {
    for (Function* function: *functions) {
        constLoopUnRollForFunc(function);
    }
}

void LoopUnRoll::constLoopUnRollForFunc(Function* function) {
    for (Loop* loop: *((*loopOrdered)[function])) {
        if (loop->isSimpleLoop() && loop->isIdcSet()) {
            constLoopUnRollForLoop(loop);
//            if (loop->idcTimes != 0) {
//                return;
//            }
        }
    }

}

void LoopUnRoll::constLoopUnRollForLoop(Loop* loop) {
    Value* idcInit = loop->getIdcInit();
    Value* idcStep = loop->getIdcStep();
    Value* idcEnd = loop->getIdcEnd();
    INSTR::Alu* idcAlu = (INSTR::Alu*) loop->getIdcAlu();


    if (!(dynamic_cast<Constant*>(idcInit) != nullptr) || !(dynamic_cast<Constant*>(idcStep) != nullptr) || !(dynamic_cast<Constant*>(idcEnd) != nullptr)) {
        return;
    }
    //for (i = init; i < end; i+=step)
    //先只考虑int的情况
    if (!idcAlu->type->is_int32_type()) {
        return;
    }
    INSTR::Icmp* idcCmp = (INSTR::Icmp*) loop->getIdcCmp();
    int init = (int) ((ConstantInt*) idcInit)->get_const_val();
    int step = (int) ((ConstantInt*) idcStep)->get_const_val();
    int end = (int) ((ConstantInt*) idcEnd)->get_const_val();
    if (step == 0) {
        return;
    }
    int times = 0;
    int max_time = (int) 1e9;

//    double val = log(end / init) / log(step);
//    bool tag = init * pow(step, val) == end;
    switch (idcAlu->op) {
        case INSTR::Alu::Op::ADD:
//            switch (idcCmp->op) {
//                case EQ -> times = (init == end)? 1:0;
//                case NE -> times = ((end - init) % step == 0)? (end - init) / step : -1;
//                case SGE, SLE -> times = (end - init) / step + 1;
//                case SGT, SLT -> times = ((end - init) % step == 0)? (end - init) / step : (end - init) / step + 1;
//            }
            switch (idcCmp->op) {
                case INSTR::Icmp::Op::EQ:
                    times = (init == end)? 1:0;
                    break;
                case INSTR::Icmp::Op::NE:
                    times = ((end - init) % step == 0)? (end - init) / step : -1;
                    break;
                case INSTR::Icmp::Op::SGE:
                case INSTR::Icmp::Op::SLE:
                    times = (end - init) / step + 1;
                    break;
                case INSTR::Icmp::Op::SGT:
                case INSTR::Icmp::Op::SLT:
                    times = ((end - init) % step == 0)? (end - init) / step : (end - init) / step + 1;
                    break;
                default:
                    break;
            }
            break;
        case INSTR::Alu::Op::SUB:
//            switch (idcCmp->op) {
//                case EQ -> times = (init == end)? 1:0;
//                case NE -> times = ((init - end) % step == 0)? (init - end) / step : -1;
//                case SGE, SLE -> times = (init - end) / step + 1;
//                case SGT, SLT -> times = ((init - end) % step == 0)? (init - end) / step : (init - end) / step + 1;
//            }
            switch (idcCmp->op) {
                case INSTR::Icmp::Op::EQ:
                    times = (init == end)? 1:0;
                    break;
                case INSTR::Icmp::Op::NE:
                    times = ((init - end) % step == 0)? (init - end) / step : -1;
                    break;
                case INSTR::Icmp::Op::SGE:
                case INSTR::Icmp::Op::SLE:
                    times = (init - end) / step + 1;
                    break;
                case INSTR::Icmp::Op::SGT:
                case INSTR::Icmp::Op::SLT:
                    times = ((init - end) % step == 0)? (init - end) / step : (init - end) / step + 1;
                    break;
                default:
                    break;
            }
            break;
        case INSTR::Alu::Op::MUL:
//            //TODO:把乘法除法也纳入考虑
//            double val = Math->log(end / init) / Math->log(step);
//            bool tag = init * Math->pow(step, val) == end;
//            switch (idcCmp->op) {
//                case EQ -> times = (init == end)? 1:0;
//                case NE -> times = tag ? (int) val : -1;
//                case SGE, SLE -> times = (int) val + 1;
//                case SGT, SLT -> times = tag ? (int) val : (int) val + 1;
//            }
        {
            double val = log(end / init) / log(step);
            bool tag = init * pow(step, val) == end;
            switch (idcCmp->op) {
                case INSTR::Icmp::Op::EQ:
                    times = (init == end) ? 1 : 0;
                    break;
                case INSTR::Icmp::Op::NE:
                    times = tag ? (int) val : -1;
                    break;
                case INSTR::Icmp::Op::SGE:
                case INSTR::Icmp::Op::SLE:
                    times = (int) val + 1;
                    break;
                case INSTR::Icmp::Op::SGT:
                case INSTR::Icmp::Op::SLT:
                    times = tag ? (int) val : (int) val + 1;
                    break;
                default:
                    break;
            }
            break;
        }
        case INSTR::Alu::Op::DIV:
            break;
        default:
            break;
    }

    //循环次数为负-->循环无数次
    if (times < 0) {
        return;
    }
    loop->setIdcTimes(times);
    DoLoopUnRoll(loop);
}

void LoopUnRoll::checkDFS(Loop* loop, std::set<BasicBlock*>* allBB, std::set<BasicBlock*>* exits) {
//    allBB->insertAll(loop->getNowLevelBB());
//    exits->insertAll(loop->getExits());
    for (auto bb: *loop->getNowLevelBB()) {
        allBB->insert(bb);
    }
    for (auto bb: *loop->getExits()) {
        exits->insert(bb);
    }
    for (Loop* loop1: *(loop->childrenLoops)) {
        checkDFS(loop1, allBB, exits);
    }
}

void LoopUnRoll::DoLoopUnRoll(Loop* loop) {
    //check子循环不能跳出此循环
    std::set<BasicBlock*>* allBB = new std::set<BasicBlock*>();
    std::set<BasicBlock*>* exits = new std::set<BasicBlock*>();
//    allBB->insertAll(loop->getNowLevelBB());
    for (auto bb: *loop->getNowLevelBB()) {
        allBB->insert(bb);
    }
    for (Loop* loop1: *(loop->childrenLoops)) {
        checkDFS(loop1, allBB, exits);
    }

    for (BasicBlock* bb: *exits) {
        if (!(allBB->find(bb) != allBB->end())) {
            return;
        }
    }

    CloneInfoMap::clear();
    Function* function = loop->getHeader()->function;
    int times = loop->getIdcTimes();
    int cnt = cntDFS(loop);
    if ((long) cnt * times > loop_unroll_up_lines) {
        if (times != 1022 && times != 510) {
            return;
        }
        if (loop->childrenLoops->size() != 0) {
            return;
        }
        DoLoopUnRollForSeveralTimes(loop, cnt);
        //System->err.println("loop_cnt " + String->valueOf(cnt * times));
        return;
    }

    BasicBlock* head = loop->getHeader();
    BasicBlock *exit = nullptr, *entering = nullptr, *latch = nullptr, *headNext = nullptr;
    for (BasicBlock* bb: *(head->precBBs)) {
        if (bb->isLoopLatch) {
            latch = bb;
        } else {
            entering = bb;
        }
    }
    for (BasicBlock* bb: *(loop->getExits())) {
        exit = bb;
    }
    for (BasicBlock* bb: *(head->succBBs)) {
        if (bb != (exit)) {
            headNext = bb;
        }
    }

    //TODO:不要直接删head
    //entering->modifySucAToB(head, headNext);
    //latch->modifySucAToB(head, exit);


    if (headNext->precBBs->size() != 1) {
        return;
    }

    //headNext 只有head一个前驱
    //assert headNext->precBBs->size() == 1;


    std::unordered_map<Value*, Value*>* reachDefBeforeHead = new std::unordered_map<Value*, Value*>();
    std::unordered_map<Value*, Value*>* reachDefAfterLatch = new std::unordered_map<Value*, Value*>();
    std::unordered_map<Value*, Value*>* beginToEnd = new std::unordered_map<Value*, Value*>();
    std::unordered_map<Value*, Value*>* endToBegin = new std::unordered_map<Value*, Value*>();
    for (Instr* instr = head->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(dynamic_cast<INSTR::Phi*>(instr) != nullptr)) {
            break;
        }
//        int index = head->precBBs->indexOf(entering);
        int index = std::find(head->precBBs->begin(), head->precBBs->end(), entering) - head->precBBs->begin();
        (*reachDefBeforeHead)[instr] = instr->useValueList[index];
        //(*reachDefAfterLatch)[instr->useValueList[index]] = instr->useValueList->get(1 - index);

    }


    //修正对head中phi的使用 old_version to line222
    BasicBlock* latchNext = new BasicBlock(function, loop);
    //int latch_index = head->precBBs->indexOf(latch);
    int latch_index = std::find(head->precBBs->begin(), head->precBBs->end(), latch) - head->precBBs->begin();
    for (Instr* instr = head->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(dynamic_cast<INSTR::Phi*>(instr) != nullptr)) {
            break;
        }
        Use* use = instr->useList[latch_index];
        std::vector<Value*>* values = new std::vector<Value*>();
        values->push_back(instr->useValueList[latch_index]);
        Instr* phiInLatchNext = new INSTR::Phi(instr->type, values, latchNext);
        use->remove();
        instr->useList.erase(instr->useList.begin() + latch_index);
        instr->useValueList.erase(instr->useValueList.begin() + latch_index);

        (*reachDefAfterLatch)[instr] = phiInLatchNext;
        (*beginToEnd)[instr] = phiInLatchNext;
        (*endToBegin)[phiInLatchNext] = instr;
    }
    std::vector<BasicBlock*>* headNewPres = new std::vector<BasicBlock*>();
    headNewPres->push_back(entering);
    head->modifyPres(headNewPres);
    std::vector<BasicBlock*>* headNewSucs = new std::vector<BasicBlock*>();
    headNewSucs->push_back(headNext);
    head->modifySucs(headNewSucs);
    loop->removeBB(head);
    head->modifyLoop(loop->getParentLoop());
    head->modifyBrToJump(headNext);
    if (loop->getLoopDepth() == 1) {
        for (Instr* instr = head->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (instr->isInLoopCond()) {
                instr->setNotInLoopCond();
            }
        }
    }

    Instr* jumpInLatchNext = new INSTR::Jump(exit, latchNext);
    latch->modifySuc(head, latchNext);
    latch->modifyBrAToB(head, latchNext);
    latchNext->addPre(latch);
    latchNext->addSuc(exit);

    //fixme:只有一个Exit不代表exit只有一个入口 07-20-00:31
    exit->modifyPre(head, latchNext);



    //维护bbInWhile这些块的Loop信息
    //维护块中指令的isInWhileCond信息
    std::vector<BasicBlock*>* bbInWhile = new std::vector<BasicBlock*>();
    getAllBBInLoop(loop, bbInWhile);
    if (loop->getLoopDepth() == 1) {
        for (BasicBlock* bb: *(loop->getNowLevelBB())) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                if (instr->isInLoopCond()) {
                    instr->setNotInLoopCond();
                }
            }
        }
    }

    Loop* parentLoop = loop->getParentLoop();
    parentLoop->getChildrenLoops()->erase(loop);
    for (BasicBlock* bb: *loop->getNowLevelBB()) {
        bb->modifyLoop(parentLoop);
    }

    for (Loop* child: *loop->getChildrenLoops()) {
        child->setParentLoop(parentLoop);
    }

    //loop->getNowLevelBB()->clear();

//    bbInWhile->erase(head);
    bbInWhile->erase(std::remove(bbInWhile->begin(), bbInWhile->end(), head), bbInWhile->end());
    //bbInWhile->insert(head);

    //fixme:复制time / time - 1?份
    BasicBlock* oldBegin = headNext;
    BasicBlock* oldLatchNext = latchNext;
    //times = 0;
    for (int i = 0; i < times - 1; i++) {
        CloneInfoMap::clear();
        for (BasicBlock* bb: *bbInWhile) {
            bb->cloneToFunc_LUR(function, parentLoop);
        }
        for (BasicBlock* bb: *bbInWhile) {
            BasicBlock* newBB = (BasicBlock*) CloneInfoMap::getReflectedValue(bb);
            newBB->fixPreSuc(bb);
            newBB->fix();
        }
        BasicBlock* newBegin = (BasicBlock*) CloneInfoMap::getReflectedValue(oldBegin);
        BasicBlock* newLatchNext = (BasicBlock*) CloneInfoMap::getReflectedValue(oldLatchNext);

        oldLatchNext->modifySuc(exit, newBegin);
        exit->modifyPre(oldLatchNext, newLatchNext);
        std::vector<BasicBlock*>* pres = new std::vector<BasicBlock*>();
        pres->push_back(oldLatchNext);
        newBegin->modifyPres(pres);
        oldLatchNext->modifyBrAToB(exit, newBegin);


        std::set<Value*>* keys = new std::set<Value*>();
        for (Value* value: KeySet(*reachDefAfterLatch)) {
            keys->insert(value);
        }

        for (Value* A: *keys) {
            (*reachDefAfterLatch)[CloneInfoMap::getReflectedValue(A)] = (*reachDefAfterLatch)[A];
            if (CloneInfoMap::valueMap.find(A) != CloneInfoMap::valueMap.end()) {
                reachDefAfterLatch->erase(A);
            }
        }

        //修改引用,更新reach_def_after_latch
        //fixme:考虑const的情况
        for (BasicBlock* temp: *bbInWhile) {
            BasicBlock* bb = (BasicBlock*) CloneInfoMap::getReflectedValue(temp);
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                std::vector<Value*>* values = &(instr->useValueList);
                for (int j = 0; j < values->size(); j++) {
                    Value* B = (*values)[j];
                    if ((reachDefAfterLatch->find(B) != reachDefAfterLatch->end())) {
                        Value* C = (*reachDefAfterLatch)[B];
                        Value* D = CloneInfoMap::getReflectedValue((*reachDefAfterLatch)[B]);

                        //TODO:不能立刻更改
                        instr->modifyUse(C, j);
                    }
                }
            }
        }



        keys = new std::set<Value*>();
        for (Value* value: KeySet(*reachDefAfterLatch)) {
            keys->insert(value);
        }

        for (Value* AA: *keys) {
            Value* D = (*reachDefAfterLatch)[AA];
            Value* DD = CloneInfoMap::getReflectedValue((*reachDefAfterLatch)[AA]);
            (*reachDefAfterLatch)[D] = DD;
            reachDefAfterLatch->erase(AA);
        }

        keys = new std::set<Value*>();
        for (Value* value: KeySet(*beginToEnd)) {
            keys->insert(value);
        }

        for (Value* A: *keys) {
            Value* DD = CloneInfoMap::getReflectedValue((*beginToEnd)[A]);
            (*beginToEnd)[A] = DD;
        }


        oldBegin = newBegin;
        oldLatchNext = newLatchNext;
        std::vector<BasicBlock*>* newBBInWhile = new std::vector<BasicBlock*>();
        for (BasicBlock* bb: *bbInWhile) {
            newBBInWhile->push_back((BasicBlock*) CloneInfoMap::getReflectedValue(bb));
        }
        bbInWhile = newBBInWhile;
    }

    //修正exit的LCSSA PHI
    for (Instr* instr = exit->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(dynamic_cast<INSTR::Phi*>(instr) != nullptr)) {
            break;
        }
        //fixme:测试
        // 当前采用的写法基于一些特性,如定义一定是PHI?,这些特性需要测试
        for (Value* value: instr->useValueList) {
            //assert dynamic_cast<Instr*>(value) != nullptr;
            if ((((Instr*) value)->bb == head)) {
                // instr->useValueList->indexOf(value)
                int index = std::find(instr->useValueList.begin(), instr->useValueList.end(), value) - instr->useValueList.begin();
                instr->modifyUse((*beginToEnd)[value], index);
            }
        }
    }
    //fixme:07-18-01:13 是否需要这样修正exit的 LCSSA 下叙方法有错,head中应该使用beginToEnd的key对应的value

    //删除head
}

void LoopUnRoll::getAllBBInLoop(Loop* loop, std::vector<BasicBlock*>* bbs) {
    for (BasicBlock* bb: (*loop->getNowLevelBB())) {
        bbs->push_back(bb);
    }
    for (Loop* next: (*loop->getChildrenLoops())) {
        getAllBBInLoop(next, bbs);
    }
}

int LoopUnRoll::cntDFS(Loop* loop) {
    int cnt = 0;
    for (BasicBlock* bb: (*loop->getNowLevelBB())) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            cnt++;
        }
    }
    for (Loop* next: (*loop->getChildrenLoops())) {
        cnt += cntDFS(next);
    }
    return cnt;
}

void LoopUnRoll::DoLoopUnRollForSeveralTimes(Loop* loop, int codeCnt) {
    Function* function = loop->getHeader()->function;
    int times = getUnrollTime(loop->getIdcTimes(), codeCnt);
    BasicBlock *head = loop->getHeader();
    BasicBlock *exit = nullptr, *entering = nullptr, *latch = nullptr, *headNext = nullptr;
    for (BasicBlock* bb: *head->precBBs) {
        if (bb->isLoopLatch) {
            latch = bb;
        } else {
            entering = bb;
        }
    }
    for (BasicBlock* bb: *loop->getExits()) {
        exit = bb;
    }
    for (BasicBlock* bb: *head->succBBs) {
        if (bb != (exit)) {
            headNext = bb;
        }
    }
    //assert headNext->precBBs->size() == 1;


    std::unordered_map<Value*, Value*>* reachDefBeforeHead = new std::unordered_map<Value*, Value*>();
    std::unordered_map<Value*, Value*>* reachDefAfterLatch = new std::unordered_map<Value*, Value*>();
    std::unordered_map<Value*, Value*>* beginToEnd = new std::unordered_map<Value*, Value*>();
    std::unordered_map<Value*, Value*>* endToBegin = new std::unordered_map<Value*, Value*>();
    for (Instr* instr = head->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(dynamic_cast<INSTR::Phi*>(instr) != nullptr)) {
            break;
        }
        //int index = head->precBBs->indexOf(entering);
        int index = std::find(head->precBBs->begin(), head->precBBs->end(), entering) - head->precBBs->begin();
        (*reachDefBeforeHead)[instr] = instr->useValueList[index];
    }


    BasicBlock* latchNext = new BasicBlock(function, loop);
//    int latch_index = head->precBBs->indexOf(latch);
    int latch_index = std::find(head->precBBs->begin(), head->precBBs->end(), latch) - head->precBBs->begin();
    for (Instr* instr = head->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(dynamic_cast<INSTR::Phi*>(instr) != nullptr)) {
            break;
        }
        //Use use = instr->useList[latch_index];
        std::vector<Value*>* values = new std::vector<Value*>();
        values->push_back(instr->useValueList[latch_index]);
        Instr* phiInLatchNext = new INSTR::Phi(instr->type, values, latchNext);
        //use->remove();
        //instr->useList->remove(latch_index);
        //instr->useValueList->remove(latch_index);

        (*reachDefAfterLatch)[instr] = phiInLatchNext;
        (*beginToEnd)[instr->useValueList[latch_index]] = phiInLatchNext;
        (*endToBegin)[phiInLatchNext] = instr->useValueList[latch_index];
    }

    Instr* jumpInLatchNext = new INSTR::Jump(head, latchNext);
    latch->modifySuc(head, latchNext);
    latch->modifyBrAToB(head, latchNext);
    latch->setNotLatch();
    latchNext->addPre(latch);
    latchNext->addSuc(head);
    head->modifyPre(latch, latchNext);

    std::vector<BasicBlock*>* bbInWhile = new std::vector<BasicBlock*>();
    getAllBBInLoop(loop, bbInWhile);

//    bbInWhile->erase(head);
    bbInWhile->erase(std::remove(bbInWhile->begin(), bbInWhile->end(), head), bbInWhile->end());

    BasicBlock* oldBegin = headNext;
    BasicBlock* oldLatchNext = latchNext;
    for (int i = 0; i < times - 1; i++) {
        CloneInfoMap::clear();
        for (BasicBlock* bb: *bbInWhile) {
            bb->cloneToFunc_LUR(function, loop);
        }
        for (BasicBlock* bb: *bbInWhile) {
            BasicBlock* newBB = (BasicBlock*) CloneInfoMap::getReflectedValue(bb);
            newBB->fixPreSuc(bb);
            newBB->fix();
        }
        BasicBlock* newBegin = (BasicBlock*) CloneInfoMap::getReflectedValue(oldBegin);
        BasicBlock* newLatchNext = (BasicBlock*) CloneInfoMap::getReflectedValue(oldLatchNext);

        oldLatchNext->modifySuc(head, newBegin);
        head->modifyPre(oldLatchNext, newLatchNext);
        std::vector<BasicBlock*>* pres = new std::vector<BasicBlock*>();
        pres->push_back(oldLatchNext);
        newBegin->modifyPres(pres);
        oldLatchNext->modifyBrAToB(head, newBegin);


        std::set<Value*>* keys = new std::set<Value*>();
        for (Value* value: KeySet(*reachDefAfterLatch)) {
            keys->insert(value);
        }

        for (Value* A: *keys) {
            (*reachDefAfterLatch)[CloneInfoMap::getReflectedValue(A)] = (*reachDefAfterLatch)[A];
            if (CloneInfoMap::valueMap.find(A) != CloneInfoMap::valueMap.end()) {
                reachDefAfterLatch->erase(A);
            }
        }

        //修改引用,更新reach_def_after_latch
        //fixme:考虑const的情况
        for (BasicBlock* temp: *bbInWhile) {
            BasicBlock* bb = (BasicBlock*) CloneInfoMap::getReflectedValue(temp);
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                std::vector<Value*>* values = &(instr->useValueList);
                for (int j = 0; j < values->size(); j++) {
                    Value* B = (*values)[j];
                    if ((reachDefAfterLatch->find(B) != reachDefAfterLatch->end())) {
                        Value* C = (*reachDefAfterLatch)[B];
                        instr->modifyUse(C, j);
                    }
                }
            }
        }


        keys = new std::set<Value*>();
        for (Value* value: KeySet(*reachDefAfterLatch)) {
            keys->insert(value);
        }

        for (Value* AA: *keys) {
            Value* D = (*reachDefAfterLatch)[AA];
            Value* DD = CloneInfoMap::getReflectedValue((*reachDefAfterLatch)[AA]);
            (*reachDefAfterLatch)[D] = DD;
            reachDefAfterLatch->erase(AA);
        }

        keys = new std::set<Value*>();
        for (Value* value: KeySet(*beginToEnd)) {
            keys->insert(value);
        }

        for (Value* A: *keys) {
            Value* DD = CloneInfoMap::getReflectedValue((*beginToEnd)[A]);
            (*beginToEnd)[A] = DD;
        }


        oldBegin = newBegin;
        oldLatchNext = newLatchNext;
        std::vector<BasicBlock*>* newBBInWhile = new std::vector<BasicBlock*>();
        for (BasicBlock* bb: *bbInWhile) {
            newBBInWhile->push_back((BasicBlock*) CloneInfoMap::getReflectedValue(bb));
        }
        bbInWhile = newBBInWhile;
    }

    //exit 中 LCSSA的phi并不需要修正
    for (Instr* instr = head->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(dynamic_cast<INSTR::Phi*>(instr) != nullptr)) {
            break;
        }
        instr->modifyUse(beginToEnd->at(instr->useValueList[latch_index]), latch_index);
    }

}

int LoopUnRoll::getUnrollTime(int times, int codeCnt) {
    if (times == 1022 || times == 512) {
        return 2;
    }
    int ret = 1;
    for (int i = 2; i < ((int) sqrt(times)); i++) {
        if (i * codeCnt > loop_unroll_up_lines) {
            break;
        }
        if (times % i == 0) {
            ret = i;
        }
    }
    return ret;
}
