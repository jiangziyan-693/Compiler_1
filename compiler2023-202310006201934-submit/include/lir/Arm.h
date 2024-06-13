//
// Created by XuRuiyuan on 2023/7/22.
//

#ifndef FASTER_MEOW_ARM_H
#define FASTER_MEOW_ARM_H

// #include "lir/MachineInst.h"
// #include "backend/h"
#include <utility>

#include "string"
#include "lir/Operand.h"
#include "util/util.h"

class Operand;

class GlobalValue;

class Initial;

namespace Arm {
    extern std::string condStrList[9];
    extern std::string gprStrList[GPR_NUM];
    // extern std::string fprStrList[FPR_NUM];
    enum class Cond {
        Eq,
        Ne,
        Gt,
        Ge,
        Lt,
        Le,
        Hi,
        Pl,
        Any,
    };
    enum class ShiftType {
        None,
        Asr,
        Lsl,
        Lsr,
        Ror,
        Rrx,
    };


    enum class GPR {
        // args and return value (caller saved)
        r0, r1, r2, r3,
        // local variables (callee saved)
        r4, r5, r6, r7,
        r8, r9, r10, r11, r12,
        // some aliases
        // fp,  // frame pointer (omitted), allocatable
        // ip,  // ipc scratch register, used in some instructions (caller saved)
        sp,  // stack pointer
        lr,  // link register (caller saved) BL指令调用时存放返回地址
        pc,  // program counter
        cspr,
    };

    enum class FPR {
        // args and return value (caller saved)
        s0, s1, s2, s3,
        // local vasiables (callee saved)
        s4, s5, s6, s7,
        s8, s9, s10, s11,
        s12, s13, s14, s15,
        s16, s17, s18, s19,
        s20, s21, s22, s23,
        s24, s25, s26, s27,
        s28, s29, s30, s31
    };


    class GPRs;

    class FPRs;

    class Regs {
    public:
        virtual ~Regs() = default;

        int value;
        bool isGPR = false;
        bool isFPR = false;
        static GPRs *gpr[GPR_NUM];
        static FPRs *fpr[FPR_NUM];

        explicit Regs(int value) : value(value) {}

        virtual explicit operator std::string() const = 0;
    };

    class GPRs : public Regs {
    public:
        // int id = -1;

        ~GPRs() override = default;

        explicit GPRs(int id) : Regs(id) { isGPR = true; }

        virtual explicit operator std::string() const override;

        // friend std::ostream &operator<<(std::ostream &stream, const GPRs *gpr);
    };

    class FPRs : public Regs {
    public:
        int id = -1;

        ~FPRs() override = default;

        explicit FPRs(int id) : Regs(id) { isFPR = true; }

        virtual explicit operator std::string() const override;

        // friend std::ostream &operator<<(std::ostream &stream, const FPRs *fpr);
    };


    class Reg : public Operand {
    public:
        ~Reg() override = default;

    public:
        Reg(DataType dataType, Arm::FPR fpr) : Operand(PreColored, dataType) {
            this->dataType = dataType;
            this->value = (int) fpr;
            reg = Arm::Regs::fpr[this->value];
        }

        Reg(DataType dataType, Arm::GPR gpr) : Operand(PreColored, dataType) {
            this->dataType = dataType;
            this->value = (int) gpr;
            reg = Arm::Regs::gpr[this->value];
        }

        explicit Reg(Arm::GPR gpr) : Operand(Arm::Regs::gpr[(int) gpr]) {
            this->dataType = DataType::I32;
            this->value = (int) gpr;
            reg = Arm::Regs::gpr[this->value];
        }

        explicit Reg(Arm::FPR fpr) : Operand(Arm::Regs::fpr[(int) fpr]) {
            this->dataType = DataType::F32;
            this->value = (int) fpr;
            reg = Arm::Regs::fpr[this->value];
        }

        // FPRs *fpr = nullptr;
        // GPRs *gpr = nullptr;
        static Reg *gprPool[GPR_NUM];
        static Reg *fprPool[FPR_NUM];
        static Reg *allocGprPool[GPR_NUM];
        static Reg *allocFprPool[FPR_NUM];
        // static Reg[]
        // fprPool = new Reg[Regs.FPRs.values().length];
        // static Reg[]
        // allocGprPool = new Reg[Regs.GPRs.values().length];
        // static Reg[]
        // allocFprPool = new Reg[Regs.FPRs.values().length];
        // static {
        //     for (Regs.GPRs gpr : Regs.GPRs.values()) {
        //         gprPool[gpr.ordinal()] = new Reg(I32, gpr);
        //         allocGprPool[gpr.ordinal()] = new Reg(gpr);
        //         if (gpr == Regs.GPRs.sp) {
        //             gprPool[gpr.ordinal()] = allocGprPool[gpr.ordinal()];
        //         }
        //     }
        //     for (Regs.FPRs fpr : Regs.FPRs.values()) {
        //         fprPool[fpr.ordinal()] = new Reg(F32, fpr);
        //         allocFprPool[fpr.ordinal()] = new Reg(fpr);
        //     }
        // }
        // public static Reg[]
        //
        //     getGPRPool() {
        //         return *gprPool = nullptr;
        //     }
        //
        // public static Reg[]
        //
        //     getFPRPool() {
        //         return *fprPool = nullptr;
        //     }

        static Reg *getR(int i);

        static Reg *getR(GPR i);

        static Reg *getR(GPRs *r);

        static Reg *getS(int i);

        static Reg *getS(FPR i);

        static Reg *getS(FPRs *s);

        static Operand *getRSReg(Regs *color);

        static Operand *getRreg(int gid);

        static Operand *getRreg(Arm::GPR color);

        static Operand *getSreg(int sid);

        static Operand *getSreg(Arm::FPR color);

        virtual explicit operator std::string() const override;

        static Operand *getsp();
    };

    class Glob : public Operand {
    public:
        ~Glob() override = default;

        explicit Glob(std::string name) : Operand(Immediate), name(std::move(name)) {}

        explicit Glob(GlobalValue *glob);

    public:

        std::string name = "";
        GlobalValue *globalValue = nullptr;
        Initial *init = nullptr;

        std::string getGlob() override;

        GlobalValue *getGlobalValue();

        Initial *getInit();

        virtual explicit operator std::string() const override;

    };

    class Shift {
    public:
        ~Shift() = default;

    public:

        ShiftType shiftType = ShiftType::None;
        static Shift *NONE_SHIFT;
        Operand *shiftOpd = nullptr;

        Shift() {
            shiftOpd = new Operand(DataType::I32, 0);
            shiftType = ShiftType::None;
        }

        Shift(ShiftType shiftType, int shiftOpd) {
            this->shiftType = shiftType;
            this->shiftOpd = new Operand(DataType::I32, shiftOpd);
        }

        // public Operand shiftReg = null;
        Shift(ShiftType shiftType, Operand *shiftOpd) {
            this->shiftType = shiftType;
            // this->shiftReg = shiftOpd;
            this->shiftOpd = shiftOpd;
        }

        Operand *getShiftOpd();

        virtual explicit operator std::string() const;

        bool hasShift();

        bool noShift();

        bool shiftIsReg();

        bool operator==(const Shift &other) const {
            return shiftType == other.shiftType && shiftOpd == other.shiftOpd;
        }
    };
};


#endif //FASTER_MEOW_ARM_H
