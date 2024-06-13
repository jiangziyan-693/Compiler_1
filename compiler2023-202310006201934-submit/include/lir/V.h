#ifndef FASTER_MEOW_V_H
#define FASTER_MEOW_V_H

#include "Arm.h"
#include "MachineInst.h"
#include "Operand.h"

class VMI : public MachineInst {
public:
    ~VMI() override = default;

    static std::string getMvSuffixTypeSimply(Operand *dst);

    // static std::vector<ConstantFloat *> CONST_FLOAT_POLL;

    VMI(MachineInst::Tag tag, McBlock *insertAtEnd);

    VMI(MachineInst::Tag tag, MachineInst *insertBefore);

    VMI(MachineInst *insertAfter, MachineInst::Tag tag);

    virtual explicit operator std::string() const override { return "!VMI!"; };

    VMI() = default;
};
namespace V {
    static std::string getMvSuffixTypeSimply(Operand *dst);

    enum CvtType {
        f2i,
        i2f,
        None
    };

    class Ldr : public VMI, public MachineMemInst {
    public:
        ~Ldr() override = default;

    public:

        Ldr(Operand *data, Operand *addr, McBlock *insertAtEnd);

        Ldr(Operand *data, Operand *addr, Operand *offset, MachineInst *insertBefore);

        Ldr(Operand *svr, Operand *dstAddr, MachineInst *firstUse);

        Operand *getData() const override;

        Operand *getAddr() const override;

        Operand *getOffset() const override;

        void setAddr(Operand *lOpd) override;

        void setOffSet(Operand *offSet) override;

        void remove() override;

        bool isNoCond() const override;

        Arm::Shift *getShift() const override;

        void addShift(Arm::Shift *shift) override;

        Arm::Cond getCond() const override;

        virtual explicit operator std::string() const override;

        Ldr() = default;

        MachineInst *clone() override;
    };

    class Str : public VMI, public MachineMemInst {
    public:
        ~Str() override = default;

    public:

        Str(Operand *data, Operand *addr, Operand *offset, McBlock *insertAtEnd);

        Str(Operand *data, Operand *addr, McBlock *insertAtEnd);

        Str(MachineInst *insertAfter, Operand *data, Operand *addr, Operand *offset);

        Str(MachineInst *insertAfter, Operand *data, Operand *addr);

        Operand *getData() const override;

        Operand *getAddr() const override;

        Operand *getOffset() const override;

        void setAddr(Operand *lOpd) override;

        void setOffSet(Operand *offSet) override;

        void remove() override;

        bool isNoCond() const override;

        Arm::Shift *getShift() const override;

        void addShift(Arm::Shift *shift) override;

        Arm::Cond getCond() const override;

        virtual explicit operator std::string() const override;

        Str() = default;

        MachineInst *clone() override;
    };

    class Mov : public VMI, public ActualDefMI {
    public:
        ~Mov() override = default;

    public:

        Mov(Operand *dst, Operand *src, McBlock *insertAtEnd);

        Mov(Operand *dst, Operand *src, MachineInst *insertBefore);

        Operand *getDst() const override;

        Operand *getSrc() const override;

        bool isNoCond() const override;

        void remove() override;

        void setSrc(Operand *offset_opd);

        void setDst(Operand *dst);

        Operand *getDef() const override {
            return defOpds[0];
        }

        bool directColor() const override;

        virtual explicit operator std::string() const override;

        Mov() = default;

        MachineInst *clone() override;
    };

    class Cvt : public VMI {
    public:
        ~Cvt() override = default;

    public:

        CvtType cvtType = CvtType::None;
        std::string old = "";

        Cvt(CvtType cvtType, Operand *dst, Operand *src, McBlock *insertAtEnd);

        Operand *getDst() const override;

        Operand *getSrc() const override;

        virtual explicit operator std::string() const override;

        Cvt() = default;

        MachineInst *clone() override;
    };

    // TODO: public ActualDef
    class Binary : public VMI {
    public:
        ~Binary() override = default;

    public:

        Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, McBlock *insertAtEnd);

        Operand *getDst() const override;

        Operand *getLOpd() const;

        Operand *getROpd() const;

        virtual explicit operator std::string() const override;

        Binary() = default;

        MachineInst *clone() override;
    };

    class Neg : public VMI {
    public:
        ~Neg() override = default;

    public:

        Neg(Operand *dst, Operand *src, McBlock *insertAtEnd);

        Operand *getDst() const override;

        Operand *getSrc() const override;

        virtual explicit operator std::string() const override;

        Neg() = default;

        MachineInst *clone() override;
    };

    class Cmp : public VMI {
    public:
        ~Cmp() override = default;

    public:

        Cmp(Arm::Cond cond, Operand *lOpd, Operand *rOpd, McBlock *insertAtEnd);

        Operand *getLOpd() const;

        Operand *getROpd() const;

        virtual explicit operator std::string() const override;

        Cmp() = default;

        MachineInst *clone() override;
    };

    class Ret : public VMI {
    public:
        ~Ret() override = default;

    public:

        McFunction *savedRegsMf = nullptr;

        Ret(McFunction *mf, Arm::Reg *s0, McBlock *curMB);

        virtual explicit operator std::string() const override;

        Ret() = default;

        MachineInst *clone() override;
    };

};
#endif