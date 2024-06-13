//
// Created by XuRuiyuan on 2023/7/22.
//

#ifndef FASTER_MEOW_OPERAND_H
#define FASTER_MEOW_OPERAND_H

#include "string"
#include "unordered_map"
#include "set"
#include "unordered_set"
#include "mir/Type.h"
#include "BasicBlock.h"
#include <deque>
#include <functional>
#include <list>
#include <set>
#include <variant>
#include "optional"

namespace Arm {
    class Regs;

    class Glob;
}
class MachineMove;

class MachineInst;

class ConstantFloat;

class Operand {
public:
    virtual ~Operand() = default;

public:

    static Operand *I_ZERO;
    static Operand *I_ONE;
    int loopCounter = 0;
    std::string prefix = "";
    int value;
    std::unordered_set<Operand *> adjOpdSet;
    std::unordered_set<MachineInst *> movSet;
    int degree = 0;
    Operand *alias = nullptr;
    Arm::Regs *reg = nullptr;
    int cost = 10;
    MachineInst *defMI = nullptr;
    Operand *old = nullptr;
    bool isMultiDef = false;
    bool unReCal = false;
    bool is_glob_addr = false;
public:
    enum OpdType {
        PreColored,
        Virtual,
        Allocated,
        Immediate,
        FConst,
    };
    OpdType opdType;
    DataType dataType;
    float fConst = 0.0f;
    bool isFConst = false;
    ConstantFloat *constF = nullptr;

    Operand(OpdType opdType);

    Operand(int virtualRegCnt, DataType dataType);

    Operand(DataType dataType, int imm);

    Operand(DataType dataType, float imm);

    Operand(DataType dataType, ConstantFloat *constF);

    Operand(OpdType opdType, DataType dataType);

    Operand(Arm::Regs *reg);

    int get_I_Imm();

    bool is_I_Imm();

    void addAdj(Operand *v);

    bool isPreColored(DataType dataType);

    bool needColor(DataType dataType);

    bool isAllocated();

    bool hasReg();

    Arm::Regs *getReg() const;

    bool isVirtual(DataType dataType);

    void setValue(int i);

    Operand *getAlias();

    void setAlias(Operand *u);

    int getValue();

    bool isGlobPtr();

    virtual std::string getGlob();

    bool fNotConst();

    bool isI32();

    bool isDataType(DataType dataType);

    bool isPureImmWithOutGlob(DataType dataType);

    bool isImm();

    MachineInst *getDefMI();

    int getCost();

    void setDefCost(MachineInst *def, int cost);

    void setDefCost(Operand *oldVR);

    void setUnReCal();

    bool isGlobAddr();

    bool isConst();

    void setGlobAddr();

    bool needRegOf(DataType dataType);

    std::string getPrefix() const;

    double heuristicVal();

    bool isF32();

    Operand *select(Operand *o);

    bool operator==(const Operand &other) const {
        return dataType == other.dataType
               && value == other.value
               && opdType == other.opdType;
    }

    // todo !!!
    bool operator<(const Operand &other) const {
        if (dataType != other.dataType) {
            return dataType < other.dataType;
        }
        if (opdType != other.opdType) {
            return opdType < other.opdType;
        }
        return value < other.value;
    }

    bool operator!=(const Operand &other) const {
        return !operator==(other);
    }

    virtual explicit operator std::string() const;

    int storeWeight = 0;
    int loadWeight = 0;
};
namespace std {
    template<>
    struct hash<Operand> {
        std::size_t operator()(Operand const &m) const noexcept {
            // state (2), value (14)
            return ((((size_t) m.opdType) << 14u) | (unsigned int) m.value) & 0xFFFFu;
        }
    };
}
struct CompareOperand {
    bool operator()(const Operand *lhs, const Operand *rhs) const;
};

struct SpBasedOffAndSize {
    int size; // 大小
    int offset; // offset: 对于sp的偏移
};
//
// struct OccurPoint {
//     BasicBlock *bb;
//     std::list<std::unique_ptr<Instr>>::iterator instr;
//     int index;
//
//     friend bool operator<(OccurPoint const &lhs, OccurPoint const &rhs) {
//         return lhs.bb == rhs.bb ? lhs.index < rhs.index : lhs.bb < rhs.bb;
//     }
// };

// using RegFilter = std::function<bool(const Operand *)>;

// 单赋值虚拟寄存器的一些特殊取值
enum RegValueType {
    Immediate = 0,
    GlobBase = 1,
    StackAddr = 2,
};

using RegValue = std::variant<int, Arm::Glob *, SpBasedOffAndSize>;

#endif //FASTER_MEOW_OPERAND_H
