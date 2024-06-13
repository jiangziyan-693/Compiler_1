

#ifndef FASTER_MEOW_I_H
#define FASTER_MEOW_I_H

#include "Arm.h"

class Operand;

class McBlock;

class McFunction;

#include "MachineInst.h"

class IMI : public MachineInst {
public:
    ~IMI() override = default;

    explicit IMI(MachineInst::Tag tag, McBlock *insertAtEnd);

    explicit IMI(MachineInst::Tag tag, MachineInst *insertBefore);

    explicit IMI(MachineInst *insertAfter, MachineInst::Tag tag);

    IMI() = default;

    virtual explicit operator std::string() const override;
};

namespace I {
// public:
//

    class Ldr : public IMI, public ActualDefMI, public MachineMemInst {
    public:
        ~Ldr() override = default;

    public:

        explicit Ldr(Operand *data, Operand *addr, Operand *offset, McBlock *insertAtEnd);

        explicit Ldr(Operand *data, Operand *addr, Operand *offset, MachineInst *insertBefore);

        Ldr(Operand *data, Operand *addr, Operand *offset, Arm::Shift *lsl, MachineInst *insertBefore);

        Operand *getData() const override;

        Operand *getAddr() const override;

        Operand *getOffset() const override;

        void setAddr(Operand *addr) override;

        void setOffSet(Operand *offSet) override;

        void remove() override;

        bool isNoCond() const override;

        Arm::Shift *getShift() const override;

        void addShift(Arm::Shift *shift) override;

        Arm::Cond getCond() const override;

        void calCost() override;

        virtual explicit operator std::string() const override;

        Operand *getDef() const override;

        Ldr() = default;

        MachineInst *clone() override;
    };

    class Str : public IMI, public MachineMemInst {
    public:
        ~Str() override = default;

    public:

        Str(MachineInst *insertAfter, Operand *data, Operand *addr);

        Str(MachineInst *insertAfter, Operand *data, Operand *addr, Operand *offset, Arm::Shift *lsl);

        Str(Operand *data, Operand *addr, McBlock *insertAtEnd);

        Str(Operand *data, Operand *addr, Operand *offset, McBlock *insertAtEnd);

        Str(MachineInst *insertAfter, Operand *data, Operand *addr, Operand *offset);

        Operand *getData() const override;

        Operand *getAddr() const override;

        Operand *getOffset() const override;

        void setOffSet(Operand *offSet) override;

        void setAddr(Operand *addr) override;

        void remove() override;

        bool isNoCond() const override;

        Arm::Shift *getShift() const override;

        void addShift(Arm::Shift *shift) override;

        Arm::Cond getCond() const override;

        virtual explicit operator std::string() const override;

        Str() = default;

        MachineInst *clone() override;
    };

    class Ret : public IMI {
    public:
        ~Ret() override = default;

    public:

        McFunction *savedRegsMf = nullptr;

        Ret(McFunction *mf, Arm::Reg *retReg, McBlock *insertAtEnd);

        Ret(McFunction *mf, McBlock *insertAtEnd);

        Ret(McBlock *insertAtEnd);

        virtual explicit operator std::string() const override;

        Ret() = default;

        MachineInst *clone() override;
    };

    class Mov : public IMI, public ActualDefMI {
    public:
        explicit Mov(MachineInst *pInst, Operand *pOperand, Operand *pOperand1, Arm::Shift *pShift);

        ~Mov() override = default;

    public:
        bool isMvn = false;

        std::string oldToString = "";

        Mov() = default;

        Mov(Operand *dOpd, Operand *sOpd, McBlock *insertAtEnd);

        Mov(Operand *dOpd, Operand *sOpd, Arm::Shift *shift, McBlock *insertAtEnd);

        Mov(Operand *dOpd, Operand *sOpd, Arm::Shift *shift, MachineInst *firstUse);

        Mov(MachineInst *inst, Operand *dOpd, Operand *sOpd);

        Mov(Operand *dOpd, Operand *sOpd, MachineInst *inst);

        Mov(Arm::Cond cond, Operand *dOpd, Operand *sOpd, McBlock *insertAtEnd);

        Operand *getDst() const override;

        bool directColor() const override;

        Operand *getSrc() const override;

        bool isNoCond() const override;

        void remove() override;

        void setSrc(Operand *offset_opd);

        void setDst(Operand *dst);

        void calCost() override;

        virtual explicit operator std::string() const override;

        Operand *getDef() const override {
            return defOpds[0];
        }

        MachineInst *clone() override;
    };

    class Binary : public IMI, public ActualDefMI {
    public:
        ~Binary() override = default;

    public:

        bool setCSPR = false;

        Binary(MachineInst *insertAfter, MachineInst::Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd);

        Binary(MachineInst *insertAfter, Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, Arm::Shift *shift);

        Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, McBlock *insertAtEnd);

        Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, Arm::Shift *shift, McBlock *insertAtEnd);

        Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, Arm::Shift *shift, MachineInst *firstUse);

        Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, MachineInst *firstUse);

        // Binary(Tag tag, Operand *dstAddr, Arm::Reg *rSP, Operand *offset, MachineInst *firstUse);

        Operand *getDst() const override;

        Operand *getLOpd() const;

        Operand *getROpd() const;

        void setDst(Operand *pOperand);

        void setROpd(Operand *o);

        std::string tagName(Tag tag) const;

        virtual explicit operator std::string() const override;

        void calCost() override;

        Operand *getDef() const override {
            return defOpds[0];
        }

        Binary() = default;

        MachineInst *clone() override;
    };

    class Cmp : public IMI {
    public:
        ~Cmp() override = default;

    public:

        bool cmn = false;

        Cmp(Arm::Cond cond, Operand *lOpd, Operand *rOpd, McBlock *insertAtEnd);

        Operand *getLOpd() const;

        Operand *getROpd() const;

        void setROpd(Operand *src);

        virtual explicit operator std::string() const override;

        Cmp() = default;

        MachineInst *clone() override;
    };

    class Fma : public IMI, public ActualDefMI {
    public:
        ~Fma() override = default;

    public:
        Operand *getDst() const override;

        Operand *getlOpd() const;

        Operand *getrOpd() const;

        Operand *getAcc() const;

        bool isSign() const;

        // Arm::Cond getCond() const ;

        bool add;
        bool sign;
        bool cloned = false;
        // Arm::Cond cond;

        Fma(bool add, bool sign,
            Operand *dst, Operand *lOpd, Operand *rOpd, Operand *acc,
            McBlock *insertAtEnd) : IMI(MachineInst::Tag::FMA, insertAtEnd) {
            //dst = acc +(-) lhs * rhs
            this->add = add;
            this->sign = sign;
            defOpds.push_back(dst);
            useOpds.push_back(lOpd);
            useOpds.push_back(rOpd);
            useOpds.push_back(acc);
        }

        Fma(MachineInst *insertAfter, bool add, bool sign,
            Operand *dst, Operand *lOpd, Operand *rOpd, Operand *acc)
                : IMI(insertAfter, MachineInst::Tag::FMA) {
            //dst = acc +(-) lhs * rhs
            this->add = add;
            this->sign = sign;
            defOpds.push_back(dst);
            useOpds.push_back(lOpd);
            useOpds.push_back(rOpd);
            useOpds.push_back(acc);
        }

        // std::string toString() {
        //     String res = "";
        //     if (sign) {
        //         res += "sm";
        //     }
        //     String op;
        //     if (add) {
        //         op = "mla";
        //     } else {
        //         op = "mls";
        //     }
        //     res += op + cond + "\t" + getDst().toString() + ",\t" + getlOpd().toString() + ",\t" +
        //            getrOpd().toString() + ",\t" + getAcc().toString() const ;
        //     return res;
        // }

        Operand *getDef() const override;

        void calCost() override;

        friend std::ostream &operator<<(std::ostream &stream, const I::Fma &fma);

        virtual explicit operator std::string() const override;

        Fma() = default;

        MachineInst *clone() override;
    };

    class Swi : public IMI {
    public:
        ~Swi() override = default;

    public:

        explicit Swi(McBlock *insertAtEnd);

        virtual explicit operator std::string() const override;
    };

    class Wait : public IMI {
    public:
        ~Wait() override = default;

    public:

        explicit Wait(McBlock *insertAtEnd);

        virtual explicit operator std::string() const override;
    };

};

#endif //FASTER_MEOW_I_H