//
// Created by XuRuiyuan on 2023/7/29.
//

#ifndef FASTER_MEOW_MERGEBLOCK_H
#define FASTER_MEOW_MERGEBLOCK_H

class McProgram;
class McBlock;
class MergeBlock {
public:
    explicit MergeBlock(McProgram *p) : p(p) {}
    McProgram *p;
    bool yesCondMov = true;
    void run(bool yesCondMov);
    void dealPred(McBlock *predMb);
};


#endif //FASTER_MEOW_MERGEBLOCK_H
