#include "AllocaMove.h"
#include <algorithm>
#include "stl_op.h"
#include "Instr.h"
#include "Value.h"
#include "BasicBlock.h"

AllocaMove::AllocaMove(std::vector<Function*>* functions): functions(functions) {}

void AllocaMove::Run() {
    for (Function *f: *functions) {
        BasicBlock *entry = f->getBeginBB();
        std::vector<INSTR::Alloc *> allocas;
        for_bb_(f) {
            for_instr_(bb) {
                if (instr->isAlloc()) {
                    allocas.push_back((INSTR::Alloc *) instr);
                }
            }
        }
        BasicBlock *bballoca = new BasicBlock(f, f->entry->loop, false);
        for (INSTR::Alloc *alloca: allocas) {
            alloca->delFromNowBB();
            bballoca->insertAtEnd(alloca);
        }
        new INSTR::Jump(entry, bballoca);
        f->insertAtBegin(bballoca);
        f->entry = bballoca;
    }
}