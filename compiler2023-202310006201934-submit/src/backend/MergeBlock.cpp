//
// Created by XuRuiyuan on 2023/7/29.
//

#include "backend/MergeBlock.h"
#include "backend/MC.h"
#include "lir/MachineInst.h"
#include "lir/I.h"
#include "lir/V.h"
#include "lir/Operand.h"
#include "util/util.h"

void MergeBlock::run(bool yesCondMov) {
    this->yesCondMov = yesCondMov;
    FOR_ITER_MF(mf, p) {
        FOR_ITER_REVERSE_MB(mb, mf) {
            McBlock *curMB = (McBlock *) mb;
            bool hasCmp = false;
            bool hasCond = false;
            bool hasCall = false;
            bool hasV = false;
            bool hasGlobAddr = false;
            bool hasMem = false;
            int branchNum = 0;
            int jumpNum = 0;
            int miNum = 0;
            FOR_ITER_MI(mi, curMB) {
                if (mi->isComment()) continue;
                miNum++;
                if (mi->isGlobAddr()) hasGlobAddr = true;
                if (mi->hasCond()) hasCond = true;
                if (mi->isV()) hasV = true;
                if (mi->tag == MachineInst::Tag::ICmp
                    || mi->tag == MachineInst::Tag::VCmp) {
                    hasCmp = true;
                } else if (mi->tag == MachineInst::Tag::Call) {
                    hasCall = true;
                } else if (mi->tag == MachineInst::Tag::Branch) {
                    branchNum++;
                } else if (mi->tag == MachineInst::Tag::Jump) {
                    jumpNum++;
                }
                if (mi->tag == MachineInst::Tag::Ldr
                    || mi->tag == MachineInst::Tag::Str) {
                    hasMem = true;
                }
            }
            if (!CenterControl::_PREC_MB_TOO_MUCH
                && curMB->predMBs.size() == 1
                && curMB->succMBs.size() == 1) {
                McBlock *predMb = *curMB->predMBs.begin();
                McBlock *succMb = curMB->succMBs[0];
                /*
                 * A:
                 *  xxx
                 *  br cond, B, C
                 * B:
                 *  xxx
                 *  b C
                 * C:
                 *  xxx
                 * */
                if (predMb->succMBs.size() == 2
                    && predMb->falseSucc() == curMB
                    && predMb->trueSucc() == (McBlock *) curMB->next
                    && succMb == (McBlock *) curMB->next) {
                    if (yesCondMov) {
                        if (hasMem || hasGlobAddr || hasV || hasCmp || hasCall || hasCond ||
                            (miNum - branchNum - jumpNum) > 5) {
                            continue;
                        }
                        // assert(false);
                        // removeList.push_back(curMB);
                        MachineInst *node = dynamic_cast<MachineInst *>(predMb->endMI->prev);
                        while (node->tag != MachineInst::Tag::Branch) {
                            node = dynamic_cast<MachineInst *>(node->prev);
                        }
                        GDBranch *bj = dynamic_cast<GDBranch *>(node);
                        std::vector<MachineInst *> list;
                        FOR_ITER_MI(mi, curMB) {
                            if (mi->tag != MachineInst::Tag::Comment
                                && mi->tag != MachineInst::Tag::Jump
                                && mi->tag != MachineInst::Tag::Branch) {
                                list.push_back(mi);
                            }
                        }
                        for (MachineInst *mi: list) {
                            MachineInst *newMI = mi->clone();
                            newMI->setCond(bj->getOppoCond());
                            bj->insert_before(newMI);
                        }
                        predMb->setFalseSucc(succMb);
                        if (predMb->trueSucc() == predMb->falseSucc()) {
                            bj->remove();
                            predMb->succMBs.clear();
                            predMb->succMBs.push_back(succMb);
                        }
                        curMB->predMBs.erase(predMb);
                        if (curMB->predMBs.empty() && mb != ((McBlock *) mf->beginMB->next)) {
                            curMB->remove();
                            for (McBlock *succ: curMB->succMBs) {
                                succ->predMBs.erase(curMB);
                            }
                        }
                        continue;
                    }
                }
            }

            std::vector<McBlock *> removeList;
            if (curMB->succMBs.size() == 1) {
                McBlock *onlySuccMB = curMB->succMBs[0];
                for (McBlock *predMb: curMB->predMBs) {
                    if ((predMb->succMBs.size() == 1
                         || curMB == predMb->falseSucc())
                        && predMb != ((McBlock *) curMB->prev)) {
                        GDJump *bj = dynamic_cast<GDJump *>(predMb->endMI->prev);
                        std::vector<MachineInst *> list;
                        FOR_ITER_MI(mi, curMB) {
                            if (mi->tag != MachineInst::Tag::Comment
                                && mi->tag != MachineInst::Tag::Jump
                                && mi->tag != MachineInst::Tag::Branch) {
                                list.push_back(mi);
                            }
                        }
                        for (MachineInst *mi: list) {
                            // TODO
                            bj->insert_before(mi->clone());
                            // mi->mb = bj->mb;
                        }
                        if (((McBlock *) predMb->next) == onlySuccMB) {
                            bj->remove();
                        } else {
                            bj->target = onlySuccMB;
                        }
                        removeList.push_back(predMb);
                        if (predMb->succMBs.size() == 1) {
                            predMb->setTrueSucc(onlySuccMB);
                        } else {
                            predMb->setFalseSucc(onlySuccMB);
                        }
                        onlySuccMB->predMBs.insert(predMb);
                    } else if (predMb->succMBs.size() > 1 && predMb->trueSucc() == curMB) {
                        assert(predMb->succMBs.size() == 2);
                        if (predMb == ((McBlock *) curMB->prev) && predMb->falseSucc() == curMB) {
                            dealPred(predMb);
                        } else if (yesCondMov) {
                            if (hasMem || hasGlobAddr || hasV || hasCmp || hasCall || hasCond ||
                                (miNum - branchNum - jumpNum) > 5) {
                                continue;
                            }
                            removeList.push_back(predMb);
                            MachineInst *node = dynamic_cast<MachineInst *>(predMb->endMI->prev);
                            while (node->tag != MachineInst::Tag::Branch) {
                                node = dynamic_cast<MachineInst *>(node->prev);
                            }
                            GDBranch *bj = dynamic_cast<GDBranch *>(node);
                            std::vector<MachineInst *> list;
                            FOR_ITER_MI(mi, curMB) {
                                if (mi->tag != MachineInst::Tag::Comment
                                    && mi->tag != MachineInst::Tag::Jump
                                    && mi->tag != MachineInst::Tag::Branch) {
                                    list.push_back(mi);
                                }
                            }
                            for (MachineInst *mi: list) {
                                MachineInst *newMI = mi->clone();
                                newMI->setCond(bj->getCond());
                                bj->insert_before(newMI);
                            }
                            bj->target = onlySuccMB;
                            predMb->setTrueSucc(onlySuccMB);
                            if (predMb->trueSucc() == predMb->falseSucc()) {
                                bj->remove();
                            }
                            onlySuccMB->predMBs.insert(predMb);
                        }
                    }
                }
            } else if (curMB->succMBs.size() == 2) {
                for (McBlock *predMb: curMB->predMBs) {
                    if (predMb == ((McBlock *) curMB->prev)) {
                        if (curMB == predMb->trueSucc() && predMb->falseSucc() == predMb->trueSucc()) {
                            dealPred(predMb);
                        }
                    } else if (((MachineInst *) curMB->endMI->prev)->tag != MachineInst::Tag::Jump) {
                        continue;
                    } else if (predMb->succMBs.size() == 1) {
                        assert(((MachineInst *) predMb->endMI->prev)->isJump());
                        GDJump *j = dynamic_cast<GDJump *>(predMb->endMI->prev);
                        McBlock *newMB = new McBlock(curMB, predMb);
                        FOR_ITER_MI(mi, curMB) {
                            newMB->endMI->insert_before(mi->clone());
                        }
                        j->remove();
                        removeList.push_back(predMb);
                        predMb->succMBs[0] = newMB;
                        for (McBlock *mb: curMB->succMBs) {
                            newMB->succMBs.push_back(mb);
                        }
                        newMB->predMBs.insert(predMb);
                    } else if (predMb->succMBs.size() == 2 && predMb->falseSucc() == curMB) {
                        assert(((MachineInst *) predMb->endMI->prev)->isJump());
                        GDJump *j = dynamic_cast<GDJump *>(predMb->endMI->prev);
                        McBlock *newMB = new McBlock(curMB, predMb);
                        FOR_ITER_MI(mi, curMB) {
                            newMB->endMI->insert_before(mi->clone());
                        }
                        j->remove();
                        removeList.push_back(predMb);
                        predMb->succMBs[1] = newMB;
                        for (McBlock *mb: curMB->succMBs) {
                            newMB->succMBs.push_back(mb);
                        }
                        newMB->predMBs.insert(predMb);
                    }
                }
            }
            for (McBlock *pred: removeList) {
                bool flag = true;
                for (McBlock *succ: pred->succMBs) {
                    if (curMB == succ) {
                        flag = false;
                        break;
                    }
                }
                if (flag) curMB->predMBs.erase(pred);
            }
            if (curMB->predMBs.empty() && mb != ((McBlock *) mf->beginMB->next)) {
                curMB->remove();
                for (McBlock *succ: curMB->succMBs) {
                    succ->predMBs.erase(curMB);
                }
            }
        }
    }
}

void MergeBlock::dealPred(McBlock *predMb) {
    if (!yesCondMov) return;
    MachineInst *node = dynamic_cast<MachineInst *>(predMb->endMI->prev);
    while (node->tag != MachineInst::Tag::Branch && node->prev != nullptr) {
        node = dynamic_cast<MachineInst *>(node->prev);
    }
    if (node->prev != nullptr) {
        bool hasCmp = false;
        bool hasCond = false;
        bool hasCall = false;
        bool hasV = false;
        bool hasGlobAddr = false;
        bool hasMem = false;
        int brNum = 0;
        int jNum = 0;
        int miNum = 0;//非comment数量
        GDBranch *branch = dynamic_cast<GDBranch *>(node);

        for (MachineInst *mi = dynamic_cast<MachineInst *>(branch->next);
             mi->next != nullptr; mi = dynamic_cast<MachineInst *>(mi->next)) {
            if (!(mi->isComment())) {
                miNum++;
                if (mi->hasCond()) {
                    hasCond = true;
                }
                if (mi->isGlobAddr()) {
                    hasGlobAddr = true;
                }
                if (mi->tag == MachineInst::Tag::ICmp
                    || mi->tag == MachineInst::Tag::VCmp) {
                    hasCmp = true;
                } else if (mi->tag == MachineInst::Tag::Call) {
                    hasCall = true;
                } else if (mi->tag == MachineInst::Tag::Jump) {
                    jNum++;
                } else if (mi->tag == MachineInst::Tag::Branch) {
                    brNum++;
                }
                if (__IS__(mi, VMI)) {
                    hasV = true;
                }
                if (mi->tag == MachineInst::Tag::Ldr
                    || mi->tag == MachineInst::Tag::Str)
                    hasMem = true;
            }
        }
        if (!hasMem && !hasGlobAddr && !hasV && !hasCmp && !hasCall && !hasCond && (miNum) <= 5 && brNum <= 0 &&
            jNum <= 0) {
            for (MachineInst *mi = dynamic_cast<MachineInst *>(branch->next);
                 mi->next != nullptr; mi = dynamic_cast<MachineInst *>(mi->next)) {
                mi->setCond(branch->getOppoCond());
            }
            branch->remove();
        }
    }
}
