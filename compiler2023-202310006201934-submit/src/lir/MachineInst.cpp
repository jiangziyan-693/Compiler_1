//
// Created by XuRuiyuan on 2023/7/22.
//

#include "lir/MachineInst.h"
#include "util/ILinkNode.h"
#include "backend/MC.h"
#include "lir/Arm.h"
#include "lir/I.h"
#include "lir/V.h"
#include "backend/CodeGen.h"
#include "Function.h"
#include "sstream"
#include "Manager.h"


bool MachineInst::isNeedFix() {
    return fixType != ENUM::STACK_FIX::NO_NEED;
}

McFunction *MachineInst::getCallee() {
    return callee;
}

void MachineInst::setCallee(McFunction *callee) {
    this->callee = callee;
}

void MachineInst::setNeedFix(ENUM::STACK_FIX stack_fix) {
    McProgram::MC_PROGRAM->needFixList.push_back(dynamic_cast<IMI *>(this));
    fixType = stack_fix;
}

void MachineInst::setNeedFix(McFunction *callee, ENUM::STACK_FIX stack_fix) {
    setCallee(callee);
    setNeedFix(stack_fix);
}

void MachineInst::setUse(int i, Operand *set) {
    useOpds[i] = set;
}

MachineInst::Tag MachineInst::getTag() const {
    return tag;
}

Arm::Cond MachineInst::getCond() const {
    return cond;
}

void MachineInst::setCond(Arm::Cond cond) {
    this->cond = cond;
}

void MachineInst::calCost() {

}

Arm::Shift *MachineInst::getShift() const {
    return shift;
}

bool MachineInst::isComment() {
    return tag == Tag::Comment;
}

void MachineInst::setDef(Operand *operand) {
    defOpds[0] = operand;
}

ENUM::STACK_FIX MachineInst::getFixType() {
    return fixType;
}

void MachineInst::clearNeedFix() {
    fixType = ENUM::STACK_FIX::NO_NEED;
}

bool MachineInst::isOf(Tag tag) {
    return (this->tag == tag);
}

bool MachineInst::isNoCond() const {
    return cond == Arm::Cond::Any;
}

bool MachineInst::isJump() {
    return tag == Tag::Jump;
}

bool MachineInst::sideEff() {
    if (!setSideEff) {
        isSideEff = (tag == Tag::Branch || tag == Tag::Jump
                     || tag == Tag::Str || tag == Tag::VStr
                     || tag == Tag::Ldr || tag == Tag::VLdr
                     || tag == Tag::VCvt
                     || tag == Tag::Call
                     || tag == Tag::IRet || tag == Tag::VRet
                     || tag == Tag::Comment);
    }
    return isSideEff;
}

bool MachineInst::noShift() {
    return shift->noShift();
}

bool MachineInst::lastUserIsNext() {
    if (theLastUserOfDef == nullptr) return false;
    if (MachineInst *p = dynamic_cast<MachineInst *>(this->next)) {
        return theLastUserOfDef->getMIId() == p->getMIId();
    } else {
        return false;
    }
}

std::string MachineInst::getSTB() {
    return "!MI!";
}

bool MachineInst::isNotLastInst() {
    return next != nullptr && next->next != nullptr;
}

void MachineInst::addShift(Arm::Shift *shift) {
    Operand *shiftOpd = this->shift->shiftOpd;
    int recId = -1;
    for(int i = 0; i < useOpds.size(); i++) {
        if(useOpds[i] == shiftOpd) {
            recId = i;
        }
    }
    if(recId > 0) {
        useOpds[recId] = shift->shiftOpd;
    }
    if (shift->shiftIsReg()) {
        this->useOpds.push_back(shift->getShiftOpd());
    }
    this->shift = new Arm::Shift(shift->shiftType, shift->getShiftOpd());
}

bool MachineInst::noShiftAndCond() {
    return shift->noShift() && cond == Arm::Cond::Any;
}

bool MachineInst::hasCond() {
    return cond != Arm::Cond::Any;
}

void MachineInst::setNext(MachineInst *mi) {
    this->mb = mi->mb;
    mi->insert_before(this);
    assert(this->next == mi);
}

bool MachineInst::isIMov() {
    return tag == Tag::IMov;
}

bool MachineInst::isVMov() {
    return tag == Tag::VMov;
}

bool MachineInst::isMovOfDataType(DataType dataType) {
    return (dataType == DataType::I32 && tag == Tag::IMov) || (dataType == DataType::F32 && tag == Tag::VMov);
}

bool MachineInst::isCall() {
    return tag == Tag::Call;
}

bool MachineInst::isReturn() {
    return tag == Tag::IRet || tag == Tag::VRet;
}

bool MachineInst::isBranch() {
    return tag == Tag::Branch;
}

McBlock *MachineInst::getMb() {
    return mb;
}

MachineInst::MachineInst(Tag tag, McBlock *mb) {
    this->cond = Arm::Cond::Any;
    this->shift = Arm::Shift::NONE_SHIFT;
    this->mb = mb;
    this->tag = tag;
    this->hash = cnt++;
    mb->insertAtEnd(this);
}

MachineInst::MachineInst(Tag tag, MachineInst *inst) {
    this->cond = Arm::Cond::Any;
    this->shift = Arm::Shift::NONE_SHIFT;
    this->mb = inst->mb;
    this->tag = tag;
    this->hash = cnt++;
    inst->insert_before(this);
}

MachineInst::MachineInst(MachineInst *insertAfter, Tag tag) {
    this->cond = Arm::Cond::Any;
    this->shift = Arm::Shift::NONE_SHIFT;
    this->mb = insertAfter->mb;
    this->tag = tag;
    this->hash = cnt++;
    insertAfter->insert_after(this);
}

MachineInst::MachineInst() {
    this->cond = Arm::Cond::Any;
    this->shift = Arm::Shift::NONE_SHIFT;
    this->tag = Tag::Empty;
}

// void MachineInst::output(PrintStream os, McFunction *f) {
//
// }

void MachineInst::setGlobAddr() {
    is_glob_addr = true;
}

bool MachineInst::isGlobAddr() {
    return is_glob_addr;
}

bool MachineInst::isV() {
    return dynamic_cast<VMI *>(this) != nullptr;
}

MachineInst *MachineInst::clone() {
    MachineInst *mi = new MachineInst();
    this->copyFieldTo(mi);
    return mi;
}

void MachineInst::copyFieldTo(MachineInst *mi) {
    mi->theLastUserOfDef = this->theLastUserOfDef;
    mi->cond = this->cond;
    mi->shift = this->shift;
    mi->callee = this->callee;
    mi->fixType = this->fixType;
    mi->isSideEff = this->isSideEff;
    mi->setSideEff = this->setSideEff;
    mi->name = this->name;
    mi->mb = this->mb;
    mi->tag = this->tag;
    mi->defOpds = this->defOpds;
    mi->useOpds = this->useOpds;
    mi->hash = this->hash;
    mi->is_glob_addr = this->is_glob_addr;
    mi->id = this->id;
}

MachineInst::MachineInst(MachineInst *theLastUserOfDef, Arm::Cond cond, Arm::Shift *shift, McFunction *callee,
                         ENUM::STACK_FIX fixType, bool isSideEff, bool setSideEff, const std::string &name, McBlock *mb,
                         MachineInst::Tag tag, const std::vector<Operand *> &defOpds,
                         const std::vector<Operand *> &useOpds, int hash, bool isGlobAddr, int id) : theLastUserOfDef(
        theLastUserOfDef), cond(cond), shift(shift), callee(callee), fixType(fixType), isSideEff(isSideEff), setSideEff(
        setSideEff), name(name), mb(mb), tag(tag), defOpds(defOpds), useOpds(useOpds), hash(hash), is_glob_addr(
        isGlobAddr), id(id) {}

MachineInst::operator std::string() const {
    return "!MI!";
}

bool MachineInst::directColor() const {
    return false;
}

McBlock *GDBranch::getFalseTargetBlock() {
    return (McBlock *) mb->next;
}

Arm::Cond GDBranch::getOppoCond() {
    return isIcmp ? CodeGen::getIcmpOppoCond(cond) : CodeGen::getFcmpOppoCond(cond);
}

GDBranch::operator std::string() const {
    std::stringstream ss;
    ss << "b" << Arm::condStrList[(int) cond] << "\t" << getTrueTargetBlock()->getLabel();
    return ss.str();
}

GDJump::operator std::string() const {
    std::stringstream ss;
    ss << "b\t" << getTarget()->getLabel();
    return ss.str();
}

McBlock *GDJump::getFalseBlock() {
    if (dynamic_cast<McBlock *>(mb->next) == nullptr) {
        exit(123);
    }
    return (McBlock *) this->mb->next;
}

void MICall::genDefUse() {
    // McFunction *mf = this->getCallee();
    // nullptr ?
    // Function *func = this->getCallee()->mFunc;
    // std::cerr << func->getName();
    if (this->callee->mFunc->isExternal && this->callee->mFunc != ExternFunction::MEM_SET) {
        defOpds.push_back(Arm::Reg::getR(Arm::GPR::r12));
    }
    defOpds.push_back(Arm::Reg::getR(Arm::GPR::lr));

    // todo
    if (Lexer::detect_float) {
        for (int i = 0; i < 2; i++) {
            useOpds.push_back(Arm::Reg::getS(i));
            defOpds.push_back(Arm::Reg::getS(i));
        }
    }
    // todo
    // todo: 这里除非能分析出自定义函数没有调用过系统外函数, 否则无法处理
    for (int i = 0; i < 4; i++) {
        defOpds.push_back(Arm::Reg::getR(i));
    }
    if (Lexer::detect_float) {
        for (int i = 0; i < 16; i++) {
            defOpds.push_back(Arm::Reg::getS(i));
        }
    }
    for (int i = 0; i < MIN(callee->intParamCount, 4); i++) {
        useOpds.push_back(Arm::Reg::getR(i));
    }
    if (Lexer::detect_float) {
        for (int i = 0; i < MIN(callee->floatParamCount, 16); i++) {
            useOpds.push_back(Arm::Reg::getS(i));
        }
    }
}

MICall::operator std::string() const {
    if (callee->mFunc->isExternal) {
        if (callee->mFunc->getName() == ExternFunction::LLMMOD->getName()) {
            return "@ \nsmull r0, r1, r0, r1\nasr r3, r2, #31\nbl __aeabi_ldivmod\nmov r0, r2\n";
        } else if (callee->mFunc->getName() == ExternFunction::LLMD2MOD->getName()) {
            return "@ \nsmull r0, r1, r0, r1\nand r3, r1, #1\nasr r1, r1, #1\nlsr r0, r0, #1\norr r0, r0, r3, lsl #31\nasr r3, r2, #31\nbl __aeabi_ldivmod\nmov r0, r2\n";
        } else if (callee->mFunc->getName() == ExternFunction::LLMAMOD->getName()) {
            return "@ \nsmull r0, r1, r0, r1\nadds r0, r0, r2\nadc r1, #0\nmov r2, r3\nasr r3, r2, #31\nbl __aeabi_ldivmod\nmov r0, r2";
        }
    }
    // todo: for bf fixup
    if(callee->mFunc == ExternFunction::MEM_SET) return "nop\n\tbl\t" + CenterControl::OUR_MEMSET_TAG;
    return "bl\t" + callee->mFunc->getName();
}

MachineInst *MICall::clone() {
    MICall *mcall = new MICall;
    mcall->callee = this->callee;
    this->copyFieldTo(mcall);
    return dynamic_cast<MachineInst *>(mcall);
}
