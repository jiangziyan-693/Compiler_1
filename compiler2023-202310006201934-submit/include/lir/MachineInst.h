//
// Created by XuRuiyuan on 2023/7/22.
//

#ifndef FASTER_MEOW_MACHINEINST_H
#define FASTER_MEOW_MACHINEINST_H

#include <utility>
#include <vector>
#include <map>
#include <stack>
#include <unordered_map>
#include <set>
#include "../mir/Value.h"
#include "../mir/Instr.h"
#include "../mir/Type.h"
// #include "../backend/h"
// #include "../backend/CodeGen.h"
#include "../mir/GlobalVal.h"
// #include "../lir/Arm.h"
#include "util/util.h"
#include "frontend/Lexer.h"

class McFunction;

class CodeGen;

class McBlock;

class Operand;
namespace Arm {

    enum class Cond;
    enum class ShiftType;

    class Regs;

    class Reg;

    class Glob;

    class Shift;
}
class MachineInst : public ILinkNode {
public:
    ~MachineInst() override = default;

public:
    enum class Tag {
        Add,
        FAdd,
        Sub,
        FSub,
        Rsb,
        Mul,
        FMul,
        Div,
        FDiv,
        Mod,
        FMod,
        Lt,
        Le,
        Ge,
        Gt,
        Eq,
        Ne,
        And,
        FAnd,
        Or,
        Bic,
        Bfc,
        FOr,
        Xor,
        Lsl,
        Lsr,
        Asr,
        LongMul,
        FMA,
        IMov,
        VMov,
        VCvt,
        VNeg,
        Branch,
        Jump,
        IRet,
        VRet,
        Ldr,
        VLdr,
        Str,
        VStr,
        ICmp,
        VCmp,
        Call,
        USAT,
        Global,
        Push,
        Pop,
        VPush,
        VPop,
        Swi,
        Wait,
        Comment,
        Empty,
    };

    static MachineInst *emptyInst;
    bool singleDef = false;
    MachineInst *theLastUserOfDef = nullptr;
    Arm::Cond cond;
    Arm::Shift *shift;
    McFunction *callee = nullptr;
    ENUM::STACK_FIX fixType = ENUM::STACK_FIX::NO_NEED;
    bool isSideEff = false;
    bool setSideEff = false;
    std::string name = "";
    McBlock *mb = nullptr;
    Tag tag = Tag::Empty;
    std::vector<Operand *> defOpds;
    std::vector<Operand *> useOpds;
    int hash = 0;
    static int cnt;
    bool is_glob_addr = false;
    static int MI_num;
    int id = MI_num++;

    int getMIId() { return id; }

    bool isNeedFix();

    McFunction *getCallee();

    void setCallee(McFunction *callee);

    void setNeedFix(ENUM::STACK_FIX stack_fix);

    void setNeedFix(McFunction *callee, ENUM::STACK_FIX Estack_fix);

    void setUse(int i, Operand *set);

    Tag getTag() const;

    virtual Arm::Cond getCond() const;

    void setCond(Arm::Cond cond);

    virtual void calCost();

    Arm::Shift *getShift() const;

    bool isComment();

    void setDef(Operand *operand);

    ENUM::STACK_FIX getFixType();

    void clearNeedFix();

    bool isOf(Tag tag);

    virtual bool isNoCond() const;

    bool isJump();

    bool sideEff();

    bool noShift();

    bool lastUserIsNext();

    std::string getSTB();

    bool isNotLastInst();

    void addShift(Arm::Shift *shift);

    bool noShiftAndCond();

    bool hasCond();

    void setNext(MachineInst *mi);

    bool isIMov();

    bool isVMov();

    bool isMovOfDataType(DataType dataType);

    bool isCall();

    bool isReturn();

    bool isBranch();

    McBlock *getMb();

    MachineInst(Tag tag, McBlock *mb);

    MachineInst(Tag tag, MachineInst *inst);

    MachineInst(MachineInst *insertAfter, Tag tag);

    MachineInst();

    MachineInst(MachineInst *theLastUserOfDef, Arm::Cond cond, Arm::Shift *shift, McFunction *callee,
                ENUM::STACK_FIX fixType, bool isSideEff, bool setSideEff, const std::string &name, McBlock *mb, Tag tag,
                const std::vector<Operand *> &defOpds, const std::vector<Operand *> &useOpds, int hash, bool isGlobAddr,
                int id);

    virtual void genDefUse() {};

    // void output(PrintStream
    //             os, McFunction *f);

    void setGlobAddr();

    bool isGlobAddr();

    bool isV();

    virtual MachineInst *clone();

    virtual void copyFieldTo(MachineInst *mi);

    virtual explicit operator std::string() const;

    virtual Operand *getDst() const {
        return defOpds[0];
    }

    virtual Operand *getSrc() const {
        return useOpds[0];
    }

    virtual bool directColor() const;
};

struct CompareMI {
    bool operator()(const MachineInst *lhs, const MachineInst *rhs) const {
        return lhs->id < rhs->id;
    }
};

class Compare {
public:
    virtual ~Compare() = default;

public:

    virtual void setCond(Arm::Cond cond);

};

class MachineMemInst {
public:
    virtual ~MachineMemInst() = default;

public:

    virtual Operand *getData() const = 0;

    virtual Operand *getAddr() const = 0;

    virtual Operand *getOffset() const = 0;

    virtual void remove() = 0;

    virtual bool isNoCond() const = 0;

    virtual Arm::Shift *getShift() const = 0;

    virtual void setAddr(Operand *lOpd) = 0;

    virtual void setOffSet(Operand *rOpd) = 0;

    virtual void addShift(Arm::Shift *shift) = 0;

    virtual Arm::Cond getCond() const = 0;

};

// class MachineMove {
// public:
//     virtual ~MachineMove() = default;
//
//     virtual Operand *getDst() const = 0;
//
//     virtual Operand *getSrc() const = 0;
//
//     virtual void remove() = 0;
//
//     virtual bool isNoCond() const = 0;
//
//     virtual bool directColor() const = 0;
//
// };

class ActualDefMI {
public:
    virtual ~ActualDefMI() = default;

    virtual Operand *getDef() const = 0;

};

class MIComment : public MachineInst {
public:
    ~MIComment() override = default;

    std::string content;

    MIComment(std::string content, McBlock *insertAtEnd)
            : MachineInst(Tag::Comment, insertAtEnd), content(std::move(content)) {}

    explicit operator std::string() const override {
        return "@" + content;
    }

};

class MICall : public MachineInst {
public:
    ~MICall() override = default;

    McFunction *callee;

    MICall(McFunction *callee, McBlock *insertAtEnd)
            : MachineInst(Tag::Call, insertAtEnd), callee(callee) {
        genDefUse();
    }

    void genDefUse() override;

    virtual explicit operator std::string() const override;

    MICall() = default;

    MachineInst *clone() override;
};

class BJ : public MachineInst {
public:
    ~BJ() override = default;

    BJ(Tag tag, McBlock *insertAtEnd) : MachineInst(tag, insertAtEnd) {
    }

    BJ(Tag tag, MachineInst *before) : MachineInst(tag, before) {
    }

    virtual McBlock *getTrueBlock() {
        return nullptr;
    }

    virtual McBlock *getFalseBlock() {
        return nullptr;
    }

    virtual void setTarget(McBlock *onlySuccMB) = 0;

    virtual explicit operator std::string() const override = 0;
};

class GDJump : public BJ {
public:
    McBlock *getTarget() const {
        return target;
    }

    McBlock *target;

    GDJump(McBlock *target, McBlock *insertAtEnd) : BJ(Tag::Jump, insertAtEnd) {
        this->target = target;
    }

    GDJump(McBlock *target, MachineInst *before) : BJ(Tag::Jump, before) {
        this->target = target;
    }

//     GDJump(Arm.Cond cond, Block target, Block insertAtEnd) {
//     super(Jump, insertAtEnd);
//     this->target = target;
//     this->cond = cond;
// }

    McBlock *getTrueBlock() override {
        return target;
    }

    McBlock *getFalseBlock() override;

    //
    // void output(PrintStream os, McFunction f) {
    //     os.println("\t" + this);
    // }


    // std::string toString() {
    //     return tag.toString() + "\t" + target.getLabel();
    // }

    explicit operator std::string() const override;

    void setTarget(McBlock *mcBlock) override {
        target = mcBlock;
    }
};


class GDBranch : public BJ {
public:
    Arm::Cond getCond() const override {
        return cond;
    }

    McBlock *getTrueTargetBlock() const {
        return target;
    }

    McBlock *getFalseTargetBlock();

    Arm::Cond cond;
    McBlock *target;
// 条件不满足则跳这个块
// Block falseTargetBlock;
    bool isIcmp = true;

    GDBranch(Arm::Cond cond, McBlock *target, McBlock *insertAtEnd
    ) : BJ(Tag::Branch, insertAtEnd) {
        this->cond = cond;
        this->target = target;
    }

    GDBranch(bool isFcmp, Arm::Cond cond, McBlock *target, McBlock *insertAtEnd
    ) : BJ(Tag::Branch, insertAtEnd) {
        this->cond = cond;
        this->target = target;
        isIcmp = !isFcmp;
    }


    // std::string toString() {
    //     return tag->toString() + cond + "\t" + target;
    // }

    explicit operator std::string() const override;

    McBlock *getTrueBlock() override {
        return target;
    }

//     Block getFalseBlock() {
//     return falseTargetBlock;
// }

    void setTarget(McBlock *onlySuccMB) override {
        target = onlySuccMB;
    }

    Arm::Cond getOppoCond();
};

#endif //FASTER_MEOW_MACHINEINST_H