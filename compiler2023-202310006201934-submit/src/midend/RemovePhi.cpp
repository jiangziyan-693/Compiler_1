//
// Created by start on 23-7-29.
//

#include "../../include/midend/RemovePhi.h"
#include "../../include/util/CenterControl.h"





RemovePhi::RemovePhi(std::vector<Function*>* functions) {
    this->functions = functions;
}

void RemovePhi::Run() {
    RemovePhiAddPCopy();
    ReplacePCopy();
}

void RemovePhi::RemovePhiAddPCopy() {
    for (Function* function: *functions) {
        removeFuncPhi(function);
    }
}

void RemovePhi::ReplacePCopy() {
    for (Function* function: *functions) {
        replacePCopyForFunc(function);
    }
}


void RemovePhi::removeFuncPhi(Function* function) {
    BasicBlock* bb = function->getBeginBB();
    while (bb->next != nullptr) {
        if (dynamic_cast<INSTR::Phi *>(bb->getBeginInstr()) == nullptr) {
            bb = (BasicBlock*) bb->next;
            continue;
        }
        std::vector<BasicBlock*>* pres = new std::vector<BasicBlock*>();
        for (BasicBlock* b: *bb->precBBs) {
            pres->push_back(b);
        }
        std::vector<INSTR::PCopy*>* PCopys = new std::vector<INSTR::PCopy*>();
        //TODO:使用迭代器遍历会导致报错:遍历中修改元素 ?
        for (int i = 0; i < pres->size(); i++) {
            BasicBlock* incomeBB = (*pres)[i];

            if (incomeBB->succBBs->size() > 1) {
                // TODO: 这里不知道优化的时候， incomeBB的loop是不是null
                // BasicBlock* mid = new BasicBlock(function, bb->loop);
                BasicBlock *mid = nullptr;
                if (incomeBB->loop->getLoopDepth() > 0 && incomeBB->loop->getLoopDepth() == bb->loop->getLoopDepth() && incomeBB->loop != bb->loop && incomeBB->loop->getParentLoop() == bb->loop->getParentLoop()) {
                    mid = new BasicBlock(function, incomeBB->loop->getParentLoop());
                } else {
                    mid = new BasicBlock(function, bb->loop);
                }
                INSTR::PCopy* pCopy = new INSTR::PCopy(new std::vector<Value*>(), new std::vector<Value*>(), mid);
                PCopys->push_back(pCopy);
                addMidBB(incomeBB, mid, bb);
            } else {
                Instr* endInstr = incomeBB->getEndInstr();
                INSTR::PCopy* pCopy = new INSTR::PCopy(new std::vector<Value*>(), new std::vector<Value*>(), incomeBB);
                endInstr->insert_before(pCopy);
                PCopys->push_back(pCopy);
            }

        }

        Instr* instr = bb->getBeginInstr();
        while (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
            std::vector<Value*>* phiRHS = &(instr->useValueList);
            for (int i = 0; i < phiRHS->size(); i++) {
                (*PCopys)[i]->addToPC(instr, (*phiRHS)[i]);
            }
            instr = (Instr*) instr->next;
        }

        instr = bb->getBeginInstr();
        while (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
            Instr* temp = instr;
            instr = (Instr*) instr->next;
            //temp->remove();
            temp->delFromNowBB();
        }

        bb = (BasicBlock*) bb->next;
    }
}

void RemovePhi::addMidBB(BasicBlock* src, BasicBlock* mid, BasicBlock* tag) {
    src->succBBs->erase(
            std::remove(src->succBBs->begin(), src->succBBs->end(), tag),
            src->succBBs->end());
    src->succBBs->push_back(mid);
    mid->precBBs->push_back(src);
    mid->succBBs->push_back(tag);
    tag->precBBs->erase(
            std::remove(tag->precBBs->begin(), tag->precBBs->end(), src),
            tag->precBBs->end());
    tag->precBBs->push_back(mid);

    Instr* instr = src->getEndInstr();
    //assert dynamic_cast<INSTR::Branch*>(instr) != nullptr;
    BasicBlock* thenBB = ((INSTR::Branch*) instr)->getThenTarget();
    BasicBlock* elseBB = ((INSTR::Branch*) instr)->getElseTarget();

    if ((tag == thenBB)) {
        ((INSTR::Branch*) instr)->setThenTarget(mid);
        INSTR::Jump* jump = new INSTR::Jump(tag, mid);
    } else if (tag == elseBB) {
        ((INSTR::Branch*) instr)->setElseTarget(mid);
        INSTR::Jump* jump = new INSTR::Jump(tag, mid);
    } else {
        //System->err.println("Panic At Remove PHI addMidBB");
    }

}

void RemovePhi::replacePCopyForFunc(Function* function) {
    BasicBlock* bb = function->getBeginBB();
    while (bb->next != nullptr) {
        Instr* instr = bb->getBeginInstr();
        std::vector<Instr*>* moves = new std::vector<Instr*>();
        std::vector<Instr*>* PCopys = new std::vector<Instr*>();
        while (instr->next != nullptr) {
            if (dynamic_cast<INSTR::PCopy *>(instr) == nullptr) {
                instr = (Instr *) instr->next;
                continue;
            }
            PCopys->push_back(instr);
            std::vector<Value*>* tags = ((INSTR::PCopy*) instr)->getLHS();
            std::vector<Value*>* srcs = ((INSTR::PCopy*) instr)->getRHS();

            std::set<std::string>* tagNameSet = new std::set<std::string>();
            std::set<std::string>* srcNameSet = new std::set<std::string>();

            removeUndefCopy(tags, srcs, tagNameSet, srcNameSet);

            while (!checkPCopy(tags, srcs)) {
                bool temp = false;
                for (int i = 0; i < tags->size(); i++) {
                    std::string tagName = (*tags)[i]->getName();
                    if (!(srcNameSet->find(tagName) != srcNameSet->end())) {
                        Instr* move = new INSTR::Move((*tags)[i]->type, (*tags)[i], (*srcs)[i], bb);
                        moves->push_back(move);

                        tagNameSet->erase((*tags)[i]->getName());
                        srcNameSet->erase((*srcs)[i]->getName());

                        tags->erase(tags->begin() + i);
                        srcs->erase(srcs->begin() + i);

                        temp = true;
                        break;
                    }
                }
                //temp = true 表示存在a,b 满足b <- a  且b没有被使用过,且已经处理过
                //否则需要执行操作
                if (!temp) {
                    for (int i = 0; i < tags->size(); i++) {
                        std::string srcName = (*srcs)[i]->getName();
                        Value* src = (*srcs)[i];
                        Value* tag = (*tags)[i];
                        if ((*srcs)[i]->getName() != (*tags)[i]->getName()) {
                            VirtualValue* newSrc = new VirtualValue(tag->type);
                            Instr* move = new INSTR::Move(tag->type, newSrc, src, bb);
                            moves->push_back(move);
                            (*srcs)[i] = newSrc;

                            srcNameSet->erase(srcName);
                            srcNameSet->insert(move->getName());
                        }
                    }
                }

            }
            instr = (Instr*) instr->next;
        }
        for (Instr* instr1: *PCopys) {
            instr1->remove();
        }
        for (Instr* instr1: *moves) {
            bb->getEndInstr()->insert_before(instr1);
        }

        bb = (BasicBlock*) bb->next;
    }
}

bool RemovePhi::checkPCopy(std::vector<Value*>* tag, std::vector<Value*>* src) {
    for (int i = 0; i < tag->size(); i++) {
        if ((*tag)[i]->getName() != (*src)[i]->getName()) {
            return false;
        }
    }
    return true;
}

void RemovePhi::removeUndefCopy(std::vector<Value *> *tag, std::vector<Value *> *src, std::set<std::string> *tagNames,
                                std::set<std::string> *srcNames) {
    std::vector<Value*>* tempTag = new std::vector<Value*>();
    std::vector<Value*>* tempSrc = new std::vector<Value*>();
    for (int i = 0; i < tag->size(); i++) {
        if (dynamic_cast<UndefValue*>((*src)[i]) != nullptr) {
            continue;
        }
        tempTag->push_back((*tag)[i]);
        tempSrc->push_back((*src)[i]);
    }
    tag->clear();
    src->clear();
//    tag->insertAll(tempTag);
//    src->insertAll(tempSrc);
    for (auto t: *tempTag) {
        tag->push_back(t);
    }

    for (auto t: *tempSrc) {
        src->push_back(t);
    }


    for (Value* value: *tag) {
        tagNames->insert(value->getName());
    }
    for (Value* value: *src) {
        srcNames->insert(value->getName());
    }
}

