//
// Created by start on 23-7-31.
//

#ifndef FASTER_COMPILER_OUTPARAM_H
#define FASTER_COMPILER_OUTPARAM_H

class OutParam {
public:
    static bool ONLY_OUTPUT_PRE_SUC;
    static bool BB_NEED_CFG_INFO;
    static bool BB_NEED_LOOP_INFO;
    static bool COND_CNT_DEBUG_FOR_LC;
    static bool NEED_BB_USE_IN_BR_INFO;

};

#endif //FASTER_COMPILER_OUTPARAM_H
