//
// Created by XuRuiyuan on 2023/7/22.
//
#include "lir/Operand.h"
#include "lir/Arm.h"
#include "math.h"
#include "mir/Constant.h"
#include "util/CenterControl.h"
#include "CodeGen.h"
#include "MC.h"

int Operand::get_I_Imm() {
    return value;
}

bool Operand::is_I_Imm() {
    return opdType == Immediate && dataType == DataType::I32;
}

void Operand::addAdj(Operand *v) {
    adjOpdSet.insert(v);
}

bool Operand::isPreColored(DataType dataType) {
    return opdType == PreColored && this->dataType == dataType;
}

bool Operand::needColor(DataType dataType) {
    return dataType == this->dataType && (opdType == PreColored || opdType == Virtual);
}

bool Operand::isAllocated() {
    return opdType == Allocated;
}

bool Operand::hasReg() {
    return opdType == Allocated || opdType == PreColored;
}

Arm::Regs * Operand::getReg() const {
    return reg;
}

bool Operand::isVirtual(DataType dataType) {
    return this->dataType == dataType && opdType == Virtual;
}

void Operand::setValue(int i) {
    value = i;
}

Operand *Operand::getAlias() {
    return alias;
}

void Operand::setAlias(Operand *u) {
    alias = u;
}

int Operand::getValue() {
    return value;
}

bool Operand::isGlobPtr() {
    if (auto *p = dynamic_cast<Arm::Glob *>(this)) {
        return true;
    }
    return false;
}

std::string Operand::getGlob() {
    return "! unimplemented Operand::getGlob !";
}

bool Operand::fNotConst() {
    return opdType == Virtual && dataType == DataType::F32;
}

bool Operand::isI32() {
    return dataType == DataType::I32;
}

bool Operand::isDataType(DataType dataType) {
    return this->dataType == dataType;
}

bool Operand::isPureImmWithOutGlob(DataType dataType) {
    if (auto *p = dynamic_cast<Arm::Glob *>(this)) return false;
    return opdType == Immediate && this->dataType == dataType;
}

bool Operand::isImm() {
    return opdType == Immediate;
}

MachineInst *Operand::getDefMI() {
    return defMI;
}

int Operand::getCost() {
    return cost;
}

void Operand::setDefCost(MachineInst *def, int cost) {
    if (this->defMI == nullptr && !isMultiDef && !unReCal) {
        defMI = def;
        this->cost = cost;
    } else {
        defMI = nullptr;
        isMultiDef = true;
        this->cost = 10;
    }
}

void Operand::setDefCost(Operand *oldVR) {
    if (this->defMI == nullptr && !isMultiDef && !unReCal) {
        defMI = oldVR->defMI;
        cost = oldVR->cost;
    } else {
        defMI = nullptr;
        isMultiDef = true;
        cost = 10;
    }
}

void Operand::setUnReCal() {
    unReCal = true;
    cost = 10;
}

bool Operand::isGlobAddr() {
    return is_glob_addr;
}

bool Operand::isConst() {
    return opdType == Immediate || opdType == OpdType::FConst;
}

void Operand::setGlobAddr() {
    is_glob_addr = true;
}

bool Operand::needRegOf(DataType dataType) {
    return (opdType == PreColored || opdType == Virtual) && dataType == this->dataType;
}

Operand::Operand(OpdType opdType) {
    this->opdType = opdType;
    switch (opdType) {
        case Virtual : {
            prefix = "v";
            break;
        }
        case Allocated:
        case PreColored: {
            prefix = "r";
            break;
        }
        case Immediate: {
            prefix = "#";
            break;
        }
        case FConst: {
            prefix = "";
            break;
        }
    };
}

Operand::Operand(int virtualRegCnt, DataType dataType) {
    this->opdType = Virtual;
    this->dataType = dataType;
    if (dataType == DataType::I32) {
        value = virtualRegCnt;
        prefix = "v";
    } else if (dataType == DataType::F32) {
        value = virtualRegCnt;
        prefix = "fv";
    } else {
    }
}

Operand::Operand(DataType dataType, int imm) {
    opdType = Immediate;
    prefix = "#";
    this->dataType = dataType;
    this->value = imm;
}

Operand::Operand(DataType dataType, float imm) {
    opdType = FConst;
    prefix = "#";
    this->fConst = imm;
    this->dataType = dataType;
    this->isFConst = true;
}

Operand::Operand(DataType dataType, ConstantFloat *constF) {
    opdType = FConst;
    this->constF = constF;
}

Operand::Operand(OpdType opdType, DataType dataType) {
    this->opdType = opdType;
    this->dataType = dataType;
    if (opdType == Virtual) {
        if (dataType == DataType::I32) {
            prefix = "v";
        } else if (dataType == DataType::F32) {
            prefix = "fv";
        } else {
            exit(105);
        }
    } else if (opdType == Allocated || opdType == PreColored) {

        if (dataType == DataType::I32) {
            prefix = "r";
        } else if (dataType == DataType::F32) {
            prefix = "s";
        } else {
            exit(106);
        }
    } else if (opdType == Immediate) {
        prefix = "#";
    } else if (opdType == FConst) {
        prefix = "";
    }
}

Operand::Operand(Arm::Regs *reg) {
    this->opdType = Allocated;
    if (reg->isGPR) {
        prefix = "r";
    } else if (reg->isFPR) {
        prefix = "s";
        dataType = DataType::F32;
    } else {
    }
    this->reg = reg;
    value = reg->value;
}

std::string Operand::getPrefix() const {
    std::string prefix;
    if (opdType == Virtual) {
        if (dataType == DataType::I32) {
            prefix = "v";
        } else if (dataType == DataType::F32) {
            prefix = "fv";
        } else {
            exit(108);
        }
    } else if (opdType == Allocated || opdType == PreColored) {
        if (dataType == DataType::I32) {
            prefix = "s";
        } else if (dataType == DataType::F32) {
            prefix = "r";
        } else {
            exit(109);
        }
    } else if (opdType == Immediate) {
        prefix = "#";
    } else if (opdType == FConst) {
        prefix = "";
    }
    return prefix;
}

double Operand::heuristicVal() {
    return (degree << 10) / pow(CenterControl::_HEURISTIC_BASE, loopCounter);
}

bool Operand::isF32() {
    return dataType == DataType::F32;
}

Operand *Operand::select(Operand *o) {
    return heuristicVal() < o->heuristicVal() ? this : o;
}

Operand::operator std::string() const {
    if(CAST_IF(p, this, const Arm::Reg)) {
        return std::string(*getReg());
    } else if(opdType == FConst) {
        if(isFConst) {
            return "#" + std::to_string(fConst);
        }
        return constF->getAsmName();
    } else {
        return getPrefix() + std::to_string(value);
    }
    return "";
}

bool CompareOperand::operator()(const Operand *lhs, const Operand *rhs) const {
    return *lhs < *rhs;
}
