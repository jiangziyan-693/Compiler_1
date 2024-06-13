//
// Created by XuRuiyuan on 2023/8/3.
//
#include "Parallel.h"
#include "MC.h"
#include "Arm.h"
#include "I.h"

Parallel::Parallel(McProgram *p) {
    this->p = p;
    mf_parallel_start = new McFunction("parallel_start");
    mf_parallel_end = new McFunction("parallel_end");
}

void Parallel::gen() {
    p->endMF->insert_before(mf_parallel_start);
    p->endMF->insert_before(mf_parallel_end);
    Arm::Glob *g = new Arm::Glob(start_r5);
    McProgram::MC_PROGRAM->globList.push_back(g);
    tmpGlob[start_r5] = g;
    g = new Arm::Glob(start_r7);
    McProgram::MC_PROGRAM->globList.push_back(g);
    tmpGlob[start_r7] = g;
    g = new Arm::Glob(start_lr);
    McProgram::MC_PROGRAM->globList.push_back(g);
    tmpGlob[start_lr] = g;
    g = new Arm::Glob(end_r7);
    McProgram::MC_PROGRAM->globList.push_back(g);
    tmpGlob[end_r7] = g;
    g = new Arm::Glob(end_lr);
    McProgram::MC_PROGRAM->globList.push_back(g);
    tmpGlob[end_lr] = g;
    curMF = mf_parallel_start;
    mb_parallel_start = new McBlock(".parallel_start", curMF);
    mb_parallel_start1 = new McBlock(".parallel_start1", curMF);
    mb_parallel_start2 = new McBlock(".parallel_start2", curMF);
    curMF = mf_parallel_end;
    mb_parallel_end = new McBlock(".parallel_end", curMF);
    mb_parallel_end1 = new McBlock(".parallel_end1", curMF);
    mb_parallel_end2 = new McBlock(".parallel_end2", curMF);
    mb_parallel_end3 = new McBlock(".parallel_end3", curMF);
    mb_parallel_end4 = new McBlock(".parallel_end4", curMF);
    genStart();
    genEnd();
}

Arm::Glob *Parallel::getGlob(std::string name) {
    return tmpGlob[name];
}

void Parallel::genStart() {
    curMF = mf_parallel_start;
    curMB = mb_parallel_start;
    new I::Mov(Arm::Reg::getRreg(2), getGlob(start_r7), curMB);
    new I::Str(Arm::Reg::getRreg(7), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Mov(Arm::Reg::getRreg(2), getGlob(start_r5), curMB);
    new I::Str(Arm::Reg::getRreg(5), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Mov(Arm::Reg::getRreg(2), getGlob(start_lr), curMB);
    new I::Str(Arm::Reg::getRreg(Arm::GPR::lr), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Mov(Arm::Reg::getRreg(5), new Operand(DataType::I32, 4), curMB);
    curMB = mb_parallel_start1;
    new I::Binary(MachineInst::Tag::Sub, Arm::Reg::getRreg(5), Arm::Reg::getRreg(5), new Operand(DataType::I32, 1),
                  curMB);
    new I::Cmp(Arm::Cond::Eq, Arm::Reg::getRreg(5), Operand::I_ZERO, curMB);
    new GDBranch(Arm::Cond::Eq, mb_parallel_start2, curMB);
    new I::Mov(Arm::Reg::getRreg(7), new Operand(DataType::I32, 120), curMB);
    new I::Mov(Arm::Reg::getRreg(0), new Operand(DataType::I32, 273), curMB);
    new I::Mov(Arm::Reg::getRreg(1), Arm::Reg::getRreg(Arm::GPR::sp), curMB);
    new I::Swi(curMB);
    new I::Cmp(Arm::Cond::Ne, Arm::Reg::getRreg(0), new Operand(DataType::I32, 0), curMB);
    new GDBranch(Arm::Cond::Ne, mb_parallel_start1, curMB);
    curMB = mb_parallel_start2;
    new I::Mov(Arm::Reg::getRreg(0), Arm::Reg::getRreg(5), curMB);
    new I::Mov(Arm::Reg::getRreg(2), getGlob(start_r7), curMB);
    new I::Ldr(Arm::Reg::getRreg(7), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Mov(Arm::Reg::getRreg(2), getGlob(start_r5), curMB);
    new I::Ldr(Arm::Reg::getRreg(5), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Mov(Arm::Reg::getRreg(2), getGlob(start_lr), curMB);
    new I::Ldr(Arm::Reg::getRreg(Arm::GPR::lr), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Ret(curMB);
}

void Parallel::genEnd() {
    curMF = mf_parallel_end;
    curMB = mb_parallel_end;
    new I::Cmp(Arm::Cond::Eq, Arm::Reg::getRreg(0), Operand::I_ZERO, curMB);
    new GDBranch(Arm::Cond::Eq, mb_parallel_end2, curMB);
    curMB = mb_parallel_end1;
    new I::Mov(Arm::Reg::getRreg(7), Operand::I_ONE, curMB);
    new I::Swi(curMB);
    curMB = mb_parallel_end2;
    new I::Mov(Arm::Reg::getRreg(2), getGlob(end_r7), curMB);
    new I::Str(Arm::Reg::getRreg(7), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Mov(Arm::Reg::getRreg(2), getGlob(end_lr), curMB);
    new I::Str(Arm::Reg::getRreg(Arm::GPR::lr), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Mov(Arm::Reg::getRreg(7), new Operand(DataType::I32, 4), curMB);
    curMB = mb_parallel_end3;
    new I::Binary(MachineInst::Tag::Sub, Arm::Reg::getRreg(7), Arm::Reg::getRreg(7), Operand::I_ONE, curMB);
    new I::Cmp(Arm::Cond::Eq, Arm::Reg::getRreg(7), Operand::I_ZERO, curMB);
    new GDBranch(Arm::Cond::Eq, mb_parallel_end4, curMB);
    new I::Binary(MachineInst::Tag::Sub, Arm::Reg::getRreg(0), Arm::Reg::getRreg(Arm::GPR::sp),
                  new Operand(DataType::I32, 4), curMB);
    new I::Binary(MachineInst::Tag::Sub, Arm::Reg::getRreg(Arm::GPR::sp), Arm::Reg::getRreg(Arm::GPR::sp),
                  new Operand(DataType::I32, 4), curMB);
    new I::Wait(curMB);
    new I::Binary(MachineInst::Tag::Add, Arm::Reg::getRreg(Arm::GPR::sp), Arm::Reg::getRreg(Arm::GPR::sp),
                  new Operand(DataType::I32, 4), curMB);
    new GDJump(mb_parallel_end3, curMB);
    curMB = mb_parallel_end4;
    new I::Mov(Arm::Reg::getRreg(2), getGlob(end_r7), curMB);
    new I::Ldr(Arm::Reg::getRreg(7), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Mov(Arm::Reg::getRreg(2), getGlob(end_lr), curMB);
    new I::Ldr(Arm::Reg::getRreg(Arm::GPR::lr), Arm::Reg::getRreg(2), Operand::I_ZERO, curMB);
    new I::Ret(curMB);
}

#include "Parallel.h"
