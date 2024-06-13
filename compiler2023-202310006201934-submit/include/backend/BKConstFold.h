//
// Created by XuRuiyuan on 2023/8/15.
//

#ifndef FASTER_MEOW_BKCONSTFOLD_H
#define FASTER_MEOW_BKCONSTFOLD_H


#include "util.h"
#include <optional>
#include "Operand.h"
#include "MC.h"
#include "set"
#include "Arm.h"

class BKConstFold {
public:
    McFunction *curMF = nullptr;

    // std::map<Operand *, BKConstVal, CompareOperand> constMap;

    // std::optional<int> get_imm(Operand *o);

    void const_fold(McFunction *mf);

    std::optional<int> eval_operand(Operand *o, Arm::Shift *shift);

    std::optional<bool> const_bool_deal(Arm::Cond cond, MachineInst *cmp);

    void run(McProgram *p);

    void gep_broadcast(McFunction *mf);
};


#endif //FASTER_MEOW_BKCONSTFOLD_H
