//
// Created by XuRuiyuan on 2023/7/22.
//

#ifndef FASTER_MEOW_STACKCTRL_H
#define FASTER_MEOW_STACKCTRL_H

#include "../backend/MC.h"
#include "../lir/MachineInst.h"

namespace StackCtl {
    class StackCtrl : public MachineInst {
    public:
        ~StackCtrl() override = default;

    public:

        StackCtrl(MachineInst::Tag tag, McBlock *mb)
                : MachineInst(tag, mb) {}

        // virtual explicit operator std::string() const override;
    };

    class Push : public StackCtrl {
    public:
        ~Push() override = default;

    public:

        McFunction *savedRegsMf = nullptr;
        McBlock *mb = nullptr;

        explicit Push(McFunction *savedRegsMf, McBlock *mb)
                : StackCtrl(MachineInst::Tag::Push, mb) {
            this->savedRegsMf = savedRegsMf;
            this->mb = mb;
        }

        virtual explicit operator std::string() const override;
    };

    class Pop : public MachineInst {
    public:
        ~Pop() override = default;

    public:

        McFunction *savedRegsMf = nullptr;
        McBlock *mb = nullptr;

        explicit Pop(McFunction *savedRegsMf, McBlock *mb) : MachineInst(MachineInst::Tag::Pop, mb) {
            this->savedRegsMf = savedRegsMf;
            this->mb = mb;
        }
        virtual explicit operator std::string() const override;
    };

    class VPush : public StackCtrl {
    public:
        ~VPush() override = default;

    public:

        McFunction *savedRegsMf = nullptr;
        McBlock *mb = nullptr;

        VPush(McFunction *savedRegsMf, McBlock *mb)
                : StackCtrl(MachineInst::Tag::VPush, mb) {
            this->savedRegsMf = savedRegsMf;
            this->mb = mb;
        }

        virtual explicit operator std::string() const override;
    };

    class VPop : public MachineInst {
    public:
        ~VPop() override = default;

    public:

        McFunction *savedRegsMf = nullptr;
        McBlock *mb = nullptr;

        VPop(McFunction *savedRegsMf, McBlock *mb)
                : MachineInst(MachineInst::Tag::VPop, mb) {
            this->savedRegsMf = savedRegsMf;
            this->mb = mb;
        }

        virtual explicit operator std::string() const override;
    };

}


#endif //FASTER_MEOW_STACKCTRL_H
