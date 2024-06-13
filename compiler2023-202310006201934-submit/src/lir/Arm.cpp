//
// Created by XuRuiyuan on 2023/7/22.
//
#include "lir/Arm.h"
#include "mir/GlobalVal.h"

// #define ER 80

Arm::Reg *Arm::Reg::getR(int i) {
    return gprPool[i];
}

Arm::Reg *Arm::Reg::getR(Arm::GPR i) {
    return gprPool[(int) i];
}

Arm::Reg *Arm::Reg::getR(Arm::GPRs *r) {
    return gprPool[r->value];
}

Arm::Reg *Arm::Reg::getS(int i) {
    return fprPool[i];
}

Arm::Reg *Arm::Reg::getS(Arm::FPR i) {
    return fprPool[(int) i];
}

Arm::Reg *Arm::Reg::getS(Arm::FPRs *s) {
    return fprPool[s->value];
}

Operand *Arm::Reg::getRSReg(Arm::Regs *color) {
    if (color->isGPR) {
        return allocGprPool[color->value];
    } else if (color->isFPR) {
        return allocFprPool[color->value];
    } else {
        exit(101);
    }
}

Operand *Arm::Reg::getRreg(int gid) {
    return allocGprPool[gid];
}

Operand *Arm::Reg::getRreg(Arm::GPR color) {
    return allocGprPool[(int) color];
}

Operand *Arm::Reg::getSreg(int sid) {
    return allocFprPool[sid];
}

Operand *Arm::Reg::getSreg(Arm::FPR color) {
    return allocFprPool[(int) color];
}


std::string Arm::Glob::getGlob() {
    return name;
}

GlobalValue *Arm::Glob::getGlobalValue() {
    return globalValue;
}

Initial *Arm::Glob::getInit() {
    return init;
}


Operand *Arm::Shift::getShiftOpd() {
    return shiftOpd;
}

Arm::Shift::operator std::string() const {
    switch (shiftType) {
        case ShiftType::Asr:
            return "asr " + std::string(*this->shiftOpd);
        case ShiftType::Lsl:
            return "lsl " + std::string(*this->shiftOpd);
        case ShiftType::Lsr:
            return "lsr " + std::string(*this->shiftOpd);
        case ShiftType::Ror:
            return "ror " + std::string(*this->shiftOpd);
        case ShiftType::Rrx:
            return "rrx " + std::string(*this->shiftOpd);
        default :
            exit(81);
    };
}

bool Arm::Shift::hasShift() {
    return shiftType != ShiftType::None;
}

bool Arm::Shift::noShift() {
    return shiftType == ShiftType::None
    // todo : dangerous
    || *shiftOpd == *Operand::I_ZERO;
}

bool Arm::Shift::shiftIsReg() {
    return shiftOpd != nullptr && !shiftOpd->isImm() && shiftOpd->opdType != Operand::FConst;
}

Arm::Glob::Glob(GlobalValue *glob) : Operand(Immediate) {
    // todo !!!
    // todo !!!
    // if(glob->type->is_int32_type()) dataType = DataType::I32;
    name = glob->name;
    this->init = glob->initial;
    globalValue = glob;
}

Arm::Glob::operator std::string() const {
    return name;
}

Arm::GPRs::operator std::string() const {
    return gprStrList[value];
}

Arm::FPRs::operator std::string() const {
    return 's' + std::to_string(value);
}

Arm::Reg::operator std::string() const {
    if (dataType == DataType::I32) {
        return gprStrList[value];
    } else if (dataType == DataType::F32) {
        return "s" + std::to_string(value);
    } else exit(82);
}

Operand *Arm::Reg::getsp() {
    return getRreg(Arm::GPR::sp);
}
// std::ostream &Arm::operator<<(std::ostream &stream, const Arm::GPRs *gpr) {
//     stream << Arm::gprStrList[gpr->value];
//     return stream;
// }
//
// std::ostream &Arm::operator<<(std::ostream &stream, const Arm::FPRs *fpr) {
//     stream << "s" << fpr->value;
// }