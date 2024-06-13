//
// Created by XuRuiyuan on 2023/8/3.
//

#ifndef FASTER_MEOW_PARALLEL_H
#define FASTER_MEOW_PARALLEL_H

#include "util.h"
#include "MC.h"

class Parallel {
public:
    virtual ~Parallel() = default;

public:

    static Parallel *PARALLEL;
    McProgram *p = nullptr;
    McFunction *mf_parallel_start;
    McFunction *mf_parallel_end;
    static McFunction *curMF;
    std::unordered_map<std::string, Arm::Glob *> tmpGlob;
    std::string start_r5 = "$start_r5";
    std::string start_r7 = "$start_r7";
    std::string start_lr = "$start_lr";
    std::string end_r7 = "$end_r7";
    std::string end_lr = "$end_lr";
    McBlock *mb_parallel_start = nullptr;
    McBlock *mb_parallel_start1 = nullptr;
    McBlock *mb_parallel_start2 = nullptr;
    McBlock *mb_parallel_end = nullptr;
    McBlock *mb_parallel_end1 = nullptr;
    McBlock *mb_parallel_end2 = nullptr;
    McBlock *mb_parallel_end3 = nullptr;
    McBlock *mb_parallel_end4 = nullptr;
    static McBlock *curMB;

    explicit Parallel(McProgram *p);

    void gen();

    Arm::Glob *getGlob(std::string name);

    void genStart();

    void genEnd();

};


#endif //FASTER_MEOW_PARALLEL_H
