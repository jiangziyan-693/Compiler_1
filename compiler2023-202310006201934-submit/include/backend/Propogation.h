//
// Created by XuRuiyuan on 2023/8/10.
//

#ifndef FASTER_MEOW_PROPOGATION_H
#define FASTER_MEOW_PROPOGATION_H

#include "MC.h"

class PtrPorpogation {

public:
    PtrPorpogation(McProgram *p) : p(p) {}

    McProgram *p;
    McBlock *curMB = nullptr;

    void ptr_propogation();

    void sp_base_ptr_propogation(McFunction *mf);

    void glob_base_ptr_propogation(McFunction *mf);
};

#endif //FASTER_MEOW_PROPOGATION_H
