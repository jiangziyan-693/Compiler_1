//
// Created by start on 23-7-29.
//

#include "../../include/midend/LoopInfo.h"
#include "../../include/util/CenterControl.h"




//TODO:


LoopInfo::LoopInfo(std::vector<Function*>* functions) {
    this->functions = functions;
}

void LoopInfo::Run() {
    clearLoopInfo();
    makeInfo();
}

void LoopInfo::clearLoopInfo() {
    for (Function* function: *functions) {
        for (BasicBlock* head: *function->loopHeads) {
            Loop* loop = head->loop;
            loop->clearCond();
            loop->clear();
            loop->clearIdcInfo();
        }
    }
}

void LoopInfo::makeInfo() {
    for (Function* function: *functions) {
        makeInfoForFunc(function);
    }
}

//标记loop的 entering header exiting latch exit
void LoopInfo::makeInfoForFunc(Function* function) {
    nowFunc = function;
    std::set<BasicBlock*>* know = new std::set<BasicBlock*>();
    BasicBlock* entry = function->getBeginBB();
    DFS(entry, know);
}

void LoopInfo::DFS(BasicBlock* bb, std::set<BasicBlock*>* know) {
//    if (bb->label == "b44") {
//        printf("in");
//    }
    if (bb->isLoopHeader) {
        nowFunc->addLoopHead(bb);
    }

    if ((know->find(bb) != know->end())) {
        return;
    }

    know->insert(bb);
    //bb->loop->addBB(bb);

    //clear
    if (bb->getLoopDep() > 0) {
        Instr* instr = bb->getBeginInstr();
        Loop* loop = bb->loop;
        while (instr->next != nullptr) {
            if (instr->isInLoopCond()) {
                loop->addCond(instr);
            }
            instr = (Instr*) instr->next;
        }
    }

    //entering
    if (bb->isLoopHeader) {
        for (BasicBlock* pre: *bb->precBBs) {
            Loop* loop = bb->loop;
            if (loop->getNowLevelBB()->find(pre) == loop->getNowLevelBB()->end()) {
                loop->addEntering(pre);
            }
        }
    }

    for (BasicBlock* next: *bb->succBBs) {
        //后向边 latch
        if ((know->find(next) != know->end()) && next->isLoopHeader) {
            //assert next->isLoopHeader();
            Loop* loop = bb->loop;
            loop->addLatch(bb);
        }
        //出循环的边 exiting和exit
        //next->getLoopDep() == bb->getLoopDep() - 1
        if (next->getLoopDep() < bb->getLoopDep() ||
            (next->getLoopDep() == bb->getLoopDep() && next->loop != bb->loop)) {
            Loop* loop = bb->loop;
            loop->addExiting(bb);
            loop->addExit(next);
        }
        DFS(next, know);
    }
}
