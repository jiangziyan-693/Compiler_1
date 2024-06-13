//
// Created by XuRuiyuan on 2023/8/1.
//

#ifndef FASTER_MEOW_INIT_H
#define FASTER_MEOW_INIT_H
#include "Type.h"
#include "AggressiveFuncGCM.h"
#include "AggressiveMarkParallel.h"
#include "LoopUnRoll.h"
#include "ArrayGVNGCM.h"
#include "ArrayLift.h"
#include "GVNAndGCM.h"
#include "LocalArrayGVN.h"
#include "LoopStrengthReduction.h"
#include "MarkParallel.h"
#include "MarkParallelForNormalLoop.h"
#include "MathOptimize.h"
#include "MidPeepHole.h"
#include "OutParam.h"
#include <cmath>
//frontend
BasicType *BasicType::I32_TYPE = new BasicType(DataType::I32);
BasicType *BasicType::I1_TYPE = new BasicType(DataType::I1);
BasicType *BasicType::F32_TYPE = new BasicType(DataType::F32);
int BasicBlock::bb_count = 0;
int BasicBlock::empty_bb_count = 0;

//midend init
std::set<Instr *> *AggressiveFuncGCM::know = new std::set<Instr *>();
int AggressiveMarkParallel::parallel_num = 4;
int LoopUnRoll::loop_unroll_up_lines = 3000;
std::set<Instr *> *ArrayGVNGCM::know = new std::set<Instr *>();
int ArrayLift::length_low_line = 10;
std::set<Instr*>* GVNAndGCM::know = new std::set<Instr*>();
bool LocalArrayGVN::_STRONG_CHECK_ = false;
std::set<Instr*>* LocalArrayGVN::know = new std::set<Instr*>();
int LoopStrengthReduction::lift_times = 4;
int MarkParallel::parallel_num = 4;
int MarkParallelForNormalLoop::parallel_num = 4;
int MathOptimize::adds_to_mul_boundary = 10;
int MidPeepHole::max_base = (int) (sqrt(2147483647) - 1);

bool OutParam::ONLY_OUTPUT_PRE_SUC = false;
bool OutParam::BB_NEED_CFG_INFO = false;
bool OutParam::BB_NEED_LOOP_INFO = false;
bool OutParam::COND_CNT_DEBUG_FOR_LC = false;
bool OutParam::NEED_BB_USE_IN_BR_INFO = false;

#endif //FASTER_MEOW_INIT_H
