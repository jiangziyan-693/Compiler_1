//
// Created by XuRuiyuan on 2023/8/18.
//

#ifndef FASTER_MEOW_MERGEMI_H
#define FASTER_MEOW_MERGEMI_H

#include "Function.h"
#include "MC.h"

class MergeMI {
public:
    static void gen_def_use_pos(McFunction *mf);

    static void ssa_merge_shift_to_binary(McFunction *mf);

    static void ssa_merge_add_to_load_store(McFunction *mf);

    static void ssa_merge_add_sub_to_mul(McFunction *mf);

    static void mergeAll(McProgram *mp);
};


#endif //FASTER_MEOW_MERGEMI_H
