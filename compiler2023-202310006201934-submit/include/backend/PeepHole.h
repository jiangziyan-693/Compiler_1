//
// Created by XuRuiyuan on 2023/7/25.
//

#ifndef FASTER_MEOW_PEEPHOLE_H
#define FASTER_MEOW_PEEPHOLE_H

#include "MC.h"
#include "lir/MachineInst.h"

class PeepHole {
public:
    McProgram *p;
    McBlock *curMB = nullptr;

    static MachineInst *lastGPRsDefMI[GPR_NUM];
    static MachineInst *lastFPRsDefMI[FPR_NUM];

    explicit PeepHole(McProgram *p) : p(p) {}

    void run();

    void runOneStage();

    void threeStage(McFunction *mf);

    bool oneStage(McFunction *mf);

    bool oneStageMov(MachineInst *mi, MachineInst *nextInst);

    bool oneStageLdr(MachineInst::Tag tag, MachineInst *mi, MachineInst *prevInst);

    MachineInst *getLastDefiner(Operand *opd);

    void putLastDefiner(Operand *opd, MachineInst *mi);

    bool twoStage(McFunction *mf);

    bool isSimpleIMov(MachineInst *mi);
};

#endif //FASTER_MEOW_PEEPHOLE_H
