#include "../../include/midend/IfComb.h"
#include "../../include/util/CenterControl.h"


#define _put_all(src, dst) \
    for (auto it = src->begin(); it != src->end(); it++) {   \
        (*dst)[it->first] = it->second;                     \
    }                                                       \



IfComb::IfComb(std::vector<Function *> *functions) {
    this->functions = functions;
}


void IfComb::Run() {
    PatternA();
}


void IfComb::PatternA() {
    for (Function *function: *functions) {
        know->clear();
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            if (!(know->find(bb) != know->end())) {
                left->clear();
                right->clear();
                leftEq->clear();
                rightEq->clear();
                DFS(bb);
            }
        }
    }
}

void IfComb::DFS(BasicBlock *bb) {
    if ((know->find(bb) != know->end())) {
        return;
    }
    std::unordered_map<Value *, int> *tempLeft = new std::unordered_map<Value *, int>();
    std::unordered_map<Value *, int> *tempRight = new std::unordered_map<Value *, int>();
    std::unordered_map<Value *, int> *tempLeftEq = new std::unordered_map<Value *, int>();
    std::unordered_map<Value *, int> *tempRightEq = new std::unordered_map<Value *, int>();

//        for (auto it = tempLeft->begin(); it != tempLeft->end(); it++) {
//
//        }
//
//        for (auto it = tempLeft->begin(); it != tempLeft->end(); it++) {
//            (*left)[it->first] = it->second;
//        }
//        tempLeft-tempLeft->putAll(*left);
//        tempRight->putAll(*right);
//        tempLeftEq->putAll(*leftEq);
//        tempRightEq->putAll(*rightEq);>putAll(*left);
//        tempRight->putAll(*right);
//        tempLeftEq->putAll(*leftEq);
//        tempRightEq->putAll(*rightEq);
    _put_all(left, tempLeft);
    _put_all(right, tempRight);
    _put_all(leftEq, tempLeftEq);
    _put_all(rightEq, tempRightEq);


    know->insert(bb);
    if (dynamic_cast<INSTR::Jump *>(bb->getEndInstr()) != nullptr) {
        DFS((*bb->succBBs)[0]);
    } else if (dynamic_cast<INSTR::Branch *>(bb->getEndInstr()) != nullptr &&
               dynamic_cast<INSTR::Icmp *>(((INSTR::Branch *) bb->getEndInstr())->getCond()) != nullptr) {
        INSTR::Branch *br = (INSTR::Branch *) bb->getEndInstr();
        INSTR::Icmp *icmp = (INSTR::Icmp *) br->getCond();
        BasicBlock *trueBB = br->getThenTarget();
        BasicBlock *falseBB = br->getElseTarget();
        if (dynamic_cast<Constant *>(icmp->getRVal2()) != nullptr) {
            Value *value = icmp->getRVal1();
            int num = (int) ((ConstantInt *) icmp->getRVal2())->get_const_val();
            switch (icmp->op) {
                case INSTR::Icmp::Op::SLT:
                    if ((right->find(value) != right->end()) && (*right)[value] <= num) {
                        br->modifyUse(new ConstantInt(1), 0);
                    } else if ((rightEq->find(value) != rightEq->end()) && (*rightEq)[value] < num) {
                        br->modifyUse(new ConstantInt(1), 0);
                    } else if ((left->find(value) != left->end()) && (*left)[value] >= num - 1) {
                        br->modifyUse(new ConstantInt(0), 0);
                    } else if ((leftEq->find(value) != leftEq->end()) && (*leftEq)[value] >= num) {
                        br->modifyUse(new ConstantInt(0), 0);
                    }
                    if (!(right->find(value) != right->end())) {
                        (*right)[value] = num;
                    }
                    if (num < (*right)[value]) {
                        (*right)[value] = num;
                    }
                    break;
                case INSTR::Icmp::Op::SLE:
                    if ((right->find(value) != right->end()) && (*right)[value] <= num + 1) {
                        br->modifyUse(new ConstantInt(1), 0);
                    } else if ((rightEq->find(value) != rightEq->end()) && (*rightEq)[value] <= num) {
                        br->modifyUse(new ConstantInt(1), 0);
                    } else if ((left->find(value) != left->end()) && (*left)[value] >= num) {
                        br->modifyUse(new ConstantInt(0), 0);
                    } else if ((leftEq->find(value) != leftEq->end()) && (*leftEq)[value] >= num + 1) {
                        br->modifyUse(new ConstantInt(0), 0);
                    }
                    if (!(rightEq->find(value) != rightEq->end())) {
                        (*rightEq)[value] = num;
                    }
                    if (num < (*rightEq)[value]) {
                        (*rightEq)[value] = num;
                    }
                    break;
                case INSTR::Icmp::Op::SGT:
                    if ((right->find(value) != right->end()) && (*right)[value] <= num + 1) {
                        br->modifyUse(new ConstantInt(0), 0);
                    } else if ((rightEq->find(value) != rightEq->end()) && (*rightEq)[value] <= num) {
                        br->modifyUse(new ConstantInt(0), 0);
                    } else if ((left->find(value) != left->end()) && (*left)[value] >= num) {
                        br->modifyUse(new ConstantInt(1), 0);
                    } else if ((leftEq->find(value) != leftEq->end()) && (*leftEq)[value] >= num + 1) {
                        br->modifyUse(new ConstantInt(1), 0);
                    }
                    if (!(left->find(value) != left->end())) {
                        (*left)[value] = num;
                    }
                    if (num > (*left)[value]) {
                        (*left)[value] = num;
                    }
                    break;
                case INSTR::Icmp::Op::SGE:
                    if ((right->find(value) != right->end()) && (*right)[value] <= num) {
                        br->modifyUse(new ConstantInt(0), 0);
                    } else if ((rightEq->find(value) != rightEq->end()) && (*rightEq)[value] <= num - 1) {
                        br->modifyUse(new ConstantInt(0), 0);
                    } else if ((left->find(value) != left->end()) && (*left)[value] >= num - 1) {
                        br->modifyUse(new ConstantInt(1), 0);
                    } else if ((leftEq->find(value) != leftEq->end()) && (*leftEq)[value] >= num) {
                        br->modifyUse(new ConstantInt(1), 0);
                    }
                    if (!(leftEq->find(value) != leftEq->end())) {
                        (*leftEq)[value] = num;
                    }
                    if (num > (*leftEq)[value]) {
                        (*leftEq)[value] = num;
                    }
                    break;
                default:
                    break;
            }
        }
        if (trueBB->precBBs->size() == 1 && !(know->find(trueBB) != know->end())) {
            DFS(trueBB);
        }
        if (falseBB->precBBs->size() == 1 && !(know->find(falseBB) != know->end())) {
            left->clear();
            right->clear();
            leftEq->clear();
            rightEq->clear();

//                left->putAll(tempLeft);
//                right->putAll(tempRight);
//                leftEq->putAll(tempLeftEq);
//                rightEq->putAll(tempRightEq);
            _put_all(tempLeft, left);
            _put_all(tempRight, right);
            _put_all(tempLeftEq, leftEq);
            _put_all(tempRightEq, rightEq);

            if (dynamic_cast<Constant *>(icmp->getRVal2()) != nullptr) {
                Value *value = icmp->getRVal1();
                int num = (int) ((ConstantInt *) icmp->getRVal2())->get_const_val();
                switch (icmp->op) {
                    case INSTR::Icmp::Op::SLT:
                        if (!(leftEq->find(value) != leftEq->end())) {
                            (*leftEq)[value] = num;
                        }
                        if (num > (*leftEq)[value]) {
                            (*leftEq)[value] = num;
                        }
                        break;
                    case INSTR::Icmp::Op::SLE:
                        if (!(left->find(value) != left->end())) {
                            (*left)[value] = num;
                        }
                        if (num > (*left)[value]) {
                            (*left)[value] = num;
                        }
                        break;
                    case INSTR::Icmp::Op::SGT:
                        if (!(rightEq->find(value) != rightEq->end())) {
                            (*rightEq)[value] = num;
                        }
                        if (num < (*rightEq)[value]) {
                            (*rightEq)[value] = num;
                        }
                        break;
                    case INSTR::Icmp::Op::SGE:
                        if (!(right->find(value) != right->end())) {
                            (*right)[value] = num;
                        }
                        if (num < (*right)[value]) {
                            (*right)[value] = num;
                        }
                        break;
                    default:
                        break;
                }
            }

            DFS(falseBB);
        }
    }

    left->clear();
    right->clear();
    leftEq->clear();
    rightEq->clear();

//        left->putAll(tempLeft);
//        right->putAll(tempRight);
//        leftEq->putAll(tempLeftEq);
//        rightEq->putAll(tempRightEq);

    _put_all(tempLeft, left);
    _put_all(tempRight, right);
    _put_all(tempLeftEq, leftEq);
    _put_all(tempRightEq, rightEq);
}


void IfComb::PatternB() {
    for (Function *function: *functions) {
        know->clear();
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            if (!(know->find(bb) != know->end())) {
                DFS_B(bb);
            }
        }
    }
}


void IfComb::DFS_B(BasicBlock *bb) {
    if ((know->find(bb) != know->end())) {
        return;
    }
    know->insert(bb);
    if (dynamic_cast<INSTR::Jump *>(bb->getEndInstr()) != nullptr) {
        DFS_B((*bb->succBBs)[0]);
    } else if (dynamic_cast<INSTR::Branch *>(bb->getEndInstr()) != nullptr &&
               dynamic_cast<INSTR::Icmp *>(((INSTR::Branch *) bb->getEndInstr())->getCond()) != nullptr) {
        INSTR::Branch *br = (INSTR::Branch *) bb->getEndInstr();
        Value *cond = br->getCond();
        BasicBlock *trueBB = br->getThenTarget();
        BasicBlock *falseBB = br->getElseTarget();
        if ((trueValue->find(cond) != trueValue->end())) {
            DFS_B(trueBB);
        } else if ((falseValue->find(cond) != falseValue->end())) {
            DFS_B(falseBB);
        } else {
            if (trueBB->precBBs->size() == 1) {
                trueValue->insert(cond);
                DFS_B(trueBB);
                trueValue->erase(cond);
            }
            if (falseBB->precBBs->size() == 1) {
                falseValue->insert(cond);
                DFS_B(falseBB);
                falseValue->erase(cond);
            }
        }


    }
}

int cnt = 0;

void IfComb::PatternC() {
    for (Function *function: *functions) {
        know->clear();
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            if (dynamic_cast<INSTR::Branch *>(bb->getEndInstr()) != nullptr) {
                INSTR::Branch *br = (INSTR::Branch *) bb->getEndInstr();
                Value *cond = br->getCond();
                BasicBlock *trueBB = br->getThenTarget();
                BasicBlock *falseBB = br->getElseTarget();
                if (trueBB->succBBs->size() == 1 && (*trueBB->succBBs)[0] == (falseBB) &&
                    dynamic_cast<INSTR::Branch *>(falseBB->getEndInstr()) != nullptr &&
                    (((INSTR::Branch *) falseBB->getEndInstr())->getCond() == cond)) {
                    cnt++;
                }
            }
        }
    }
    if (cnt > 10) {
        //System->exit(55);
    }
}
