//
// Created by XuRuiyuan on 2023/7/30.
//
#include "lir/I.h"
#include "lir/I.h"
#include "backend/MC.h"
#include "sstream"
#include "lir/V.h"
#include "mir/Function.h"
#include "backend/CodeGen.h"
#include "util/util.h"

// #define ER 110

IMI::IMI(MachineInst::Tag tag, McBlock *insertAtEnd) : MachineInst(tag, insertAtEnd) {}

IMI::IMI(MachineInst::Tag tag, MachineInst *insertBefore) : MachineInst(tag, insertBefore) {}

IMI::IMI(MachineInst *insertAfter, MachineInst::Tag tag) : MachineInst(insertAfter, tag) {}

IMI::operator std::string() const {
    return "!IMI!";
}

// IMI::IMI() {}

I::Ldr::Ldr(Operand *data, Operand *addr, Operand *offset, McBlock *insertAtEnd)
        : IMI(MITAG(Ldr), insertAtEnd) {
    defOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
}

I::Ldr::Ldr(Operand *data, Operand *addr, Operand *offset, MachineInst *insertBefore)
        : IMI(MITAG(Ldr), insertBefore) {
    defOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
}

I::Ldr::Ldr(Operand *data, Operand *addr, Operand *offset, Arm::Shift *lsl, MachineInst *insertBefore)
        : IMI(MITAG(Ldr), insertBefore) {
    defOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
    shift = lsl;
    if (lsl->shiftIsReg()) useOpds.push_back(lsl->shiftOpd);
}

Operand *I::Ldr::getDef() const {
    return defOpds[0];
}

Operand *I::Ldr::getData() const {
    return defOpds[0];
}

Operand *I::Ldr::getAddr() const {
    return useOpds[0];
}

Operand *I::Ldr::getOffset() const {
    return useOpds[1];
}

void I::Ldr::setAddr(Operand *addr) {
    useOpds[0] = addr;
}

void I::Ldr::setOffSet(Operand *offSet) {
    if (useOpds.size() > 1) {
        useOpds[1] = offSet;
    } else {
        useOpds.push_back(offSet);
    }
}

void I::Ldr::remove() {
    ILinkNode::remove();
}

bool I::Ldr::isNoCond() const {
    return MachineInst::isNoCond();
}

Arm::Shift *I::Ldr::getShift() const {
    return MachineInst::getShift();
}

void I::Ldr::addShift(Arm::Shift *shift) {
    MachineInst::addShift(shift);
}

Arm::Cond I::Ldr::getCond() const {
    return MachineInst::getCond();
}

I::Ldr::operator std::string() const {
    return "ldr" + Arm::condStrList[(int) cond] + '\t' + std::string(*getData()) +
           ",\t[" + std::string(*getAddr()) +
           ((*getOffset() == *Operand::I_ZERO) ? "" : ",\t" + std::string(*getOffset())) +
           (shift->shiftType == Arm::ShiftType::None ? "" : ("\t," + std::string(*shift))) + "]";
}

void I::Ldr::calCost() {
    Operand *data = getData();
    Operand *addr = getAddr();
    Operand *offset = getOffset();
    if (!data->hasReg()) {
        if (addr->isVirtual(DataType::I32) && addr->isGlobAddr()) {
            data->setDefCost(this, 1);
        } else {
            int cost = 4;
            if (addr->hasReg()) {
                data->setUnReCal();
            } else {
                cost += addr->cost;
            }
            if (!offset->isImm()) {
                if (offset->hasReg()) {
                    data->setUnReCal();
                } else {
                    cost += offset->cost;
                }
            }
            data->setDefCost(this, cost);
        }
    }
}

MachineInst *I::Ldr::clone() {
    I::Ldr *ldr = new I::Ldr();
    this->copyFieldTo(ldr);
    return dynamic_cast<I::Ldr *>(ldr);
}

I::Str::Str(MachineInst *insertAfter, Operand *data, Operand *addr)
        : IMI(insertAfter, MITAG(Str)) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(new Operand(DataType::I32, 0));
}


I::Str::Str(MachineInst *insertAfter, Operand *data, Operand *addr, Operand *offset, Arm::Shift *lsl)
        : IMI(insertAfter, Tag::Str) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
    shift = lsl;
    if (lsl->shiftIsReg()) useOpds.push_back(lsl->shiftOpd);
}

I::Str::Str(Operand *data, Operand *addr, McBlock *insertAtEnd) :
        IMI(Tag::Str, insertAtEnd) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(new Operand(DataType::I32, 0));
}

I::Str::Str(Operand *data, Operand *addr, Operand *offset, McBlock *insertAtEnd) :
        IMI(Tag::Str, insertAtEnd) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
}

I::Str::Str(MachineInst *insertAfter, Operand *data, Operand *addr, Operand *offset) :
        IMI(insertAfter, Tag::Str) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
}

Operand *I::Str::getData() const {
    return useOpds[0];
}

Operand *I::Str::getAddr() const {
    return useOpds[1];
}

Operand *I::Str::getOffset() const {
    if (useOpds.size() < 3) return new Operand(DataType::I32, 0);
    return useOpds[2];
}

void I::Str::setOffSet(Operand *offSet) {
    if (useOpds.size() > 2) useOpds[2] = offSet;
    else useOpds.push_back(offSet);
}

void I::Str::setAddr(Operand *addr) {
    useOpds[1] = addr;
}

void I::Str::remove() {
    ILinkNode::remove();
}

bool I::Str::isNoCond() const {
    return MachineInst::isNoCond();
}

Arm::Shift *I::Str::getShift() const {
    return MachineInst::getShift();
}

void I::Str::addShift(Arm::Shift *shift) {
    MachineInst::addShift(shift);
}

Arm::Cond I::Str::getCond() const {
    return MachineInst::getCond();
}

I::Str::operator std::string() const {
    return "str" + Arm::condStrList[(int) cond] + "\t" + std::string(*getData())
           + ",\t[" + std::string(*getAddr()) +
           (*getOffset() == *Operand::I_ZERO ? "" : ",\t" + std::string(*getOffset())) +
           (shift->shiftType == Arm::ShiftType::None ? "" : ("\t," + std::string(*shift)))
           + "]";
}

MachineInst *I::Str::clone() {
    I::Str *str = new Str();
    this->copyFieldTo(str);
    return dynamic_cast<MachineInst *>(str);
}

I::Ret::Ret(McFunction *mf, Arm::Reg *retReg, McBlock *insertAtEnd) :
        IMI(Tag::IRet, insertAtEnd) {
    useOpds.push_back(retReg);
    savedRegsMf = mf;
}

I::Ret::Ret(McFunction *mf, McBlock *insertAtEnd) :
        IMI(Tag::IRet, insertAtEnd) {
    savedRegsMf = mf;
}

I::Ret::Ret(McBlock *insertAtEnd)
        : IMI(Tag::IRet, insertAtEnd) {
}

I::Ret::operator std::string() const {
    if (savedRegsMf == nullptr) {
        return "bx\tlr";
    }

    std::string sb;

    if (savedRegsMf->mFunc->isExternal) {
        // throw new AssertionError("pop in external func");
        // return "\tpop\t{r0,r1,r2,r3}";
        sb.append("pop\t{r2,r3}");
    } else {
        // StringBuilder sb = new StringBuilder();
        if (savedRegsMf->usedCalleeSavedGPRs != 0) {
            sb.append("pop\t{");
            unsigned int num = savedRegsMf->usedCalleeSavedGPRs;
            int i = 0;
            bool flag = true;
            while (num) {
                if ((num & 1u) != 0) {
                    if (flag) {
                        flag = false;
                    } else {
                        sb.append(",");
                    }
                    sb.append(Arm::gprStrList[i]);
                }
                i++;
                num >>= 1;
            }
            sb.append("}\n");
        }
    }
    if (CodeGen::needFPU) {
        if (savedRegsMf->mFunc->isExternal) {
            sb.append("\tvpop\t{s2-s").append(std::to_string(CodeGen::sParamCnt - 1)).append("}");
        } else {
            if (savedRegsMf->usedCalleeSavedFPRs != 0) {
                unsigned int fprBit = savedRegsMf->usedCalleeSavedFPRs;
                int end = FPR_NUM - 1;
                while (end > -1) {
                    // std::cerr << "end start" << std::endl;
                    while (end > -1 && ((fprBit & (1u << end)) == 0))
                        end--;
                    // std::cerr << "end end" << std::endl;
                    if (end == -1)
                        break;
                    int start = end;
                    // std::cerr << "start start" << std::endl;
                    while (start > -1 && (fprBit & (1u << start)))
                        start--;
                    // std::cerr << "start end" << std::endl;
                    start++;
                    if (start == end) {
                        exit(110);
                    } else if (start < end) {
                        if (end - start > 15) {
                            start = end - 15;
                        }
                        sb.append("\tvpop\t{s").append(std::to_string(start)).
                                append("-s").append(std::to_string(end)).append("}\n");
                    }
                    end = start - 1;
                }
            }
        }
    }
    sb.append("\n\tbx\tlr");
    return sb;
}

MachineInst *I::Ret::clone() {
    I::Ret *ret = new I::Ret();
    ret->savedRegsMf = this->savedRegsMf;
    this->copyFieldTo(ret);
    return dynamic_cast<MachineInst *>(ret);
}


I::Mov::Mov(Operand *dOpd, Operand *sOpd, McBlock *insertAtEnd)
        : IMI(Tag::IMov, insertAtEnd) {
    defOpds.push_back(dOpd);
    useOpds.push_back(sOpd);
}

I::Mov::Mov(Operand *dOpd, Operand *sOpd, Arm::Shift *shift, McBlock *insertAtEnd) :
        IMI(Tag::IMov, insertAtEnd) {
    defOpds.push_back(dOpd);
    useOpds.push_back(sOpd);
    if (shift->shiftIsReg()) {
        useOpds.push_back(shift->getShiftOpd());
    }
    this->shift = shift;
}

I::Mov::Mov(Operand *dOpd, Operand *sOpd, Arm::Shift *shift, MachineInst *firstUse) :
        IMI(Tag::IMov, firstUse) {
    defOpds.push_back(dOpd);
    useOpds.push_back(sOpd);
    if (shift->shiftIsReg()) {
        useOpds.push_back(shift->shiftOpd);
    }
    this->shift = shift;
}

I::Mov::Mov(MachineInst *inst, Operand *dOpd, Operand *sOpd) :
        IMI(inst, Tag::IMov) {
    defOpds.push_back(dOpd);
    useOpds.push_back(sOpd);
}

I::Mov::Mov(Operand *dOpd, Operand *sOpd, MachineInst *inst) :
        IMI(Tag::IMov, inst) {
    defOpds.push_back(dOpd);
    useOpds.push_back(sOpd);
}

I::Mov::Mov(Arm::Cond cond, Operand *dOpd, Operand *sOpd, McBlock *insertAtEnd) :
        IMI(Tag::IMov, insertAtEnd) {
    this->cond = cond;
    defOpds.push_back(dOpd);
    useOpds.push_back(sOpd);
}


I::Mov::Mov(MachineInst *pInst, Operand *pOperand, Operand *pOperand1, Arm::Shift *pShift)
        : IMI(pInst, Tag::IMov) {
    defOpds.push_back(pOperand);
    useOpds.push_back(pOperand1);
    if (shift->shiftIsReg()) {
        useOpds.push_back(shift->shiftOpd);
    }
    this->shift = pShift;
}

Operand *I::Mov::getDst() const {
    return defOpds[0];
}

bool I::Mov::directColor() const {
    return getDst()->needColor(DataType::I32) && getSrc()->needColor(DataType::I32) && cond == Arm::Cond::Any &&
           shift->shiftType == Arm::ShiftType::None;
}

Operand *I::Mov::getSrc() const {
    return useOpds[0];
}

void I::Mov::setSrc(Operand *offset_opd) {
    useOpds[0] = offset_opd;
}

void I::Mov::setDst(Operand *dst) {
    defOpds[0] = dst;
}

void I::Mov::calCost() {
    Operand *dst = getDst();
    Operand *src = getSrc();
    if (dst->reg == nullptr) {
        if (src->reg != nullptr) {
            dst->setUnReCal();
        } else if (src->isImm()) {
            dst->setDefCost(this, 1);
        } else {
            int cost = src->getCost();
            if (shift->hasShift()) {
                cost++;
                if (shift->shiftIsReg()) {
                    cost += shift->shiftOpd->cost;
                }
            }
            dst->setDefCost(this, cost);
        }
    }
}

I::Mov::operator std::string() const {
    std::string stb;
    Operand *src = getSrc();
    if (src->opdType == Operand::OpdType::Immediate) {
        if (src->isGlobPtr()) {
            stb.append("movw").append(Arm::condStrList[(int) cond]).append("\t").append(std::string(*getDst())).append(
                    ",\t:lower16:").append(src->getGlob()).append("\n");
            stb.append("\tmovt").append(Arm::condStrList[(int) cond]).append("\t").append(
                    std::string(*getDst())).append(",\t:upper16:").append(src->getGlob());
            //stb.append("\tldr" + Arm::condStrList[(int)cond] + "\t" + getDst() + ",=" + src->getGlob());
        } else {
            int imm = getSrc()->value;
            if (CodeGen::immCanCode(imm)) {
                stb.append("mov").append(Arm::condStrList[(int) cond]).append("\t").append(
                        std::string(*getDst())).append(",\t#").append(std::to_string(imm));
            } else {
                unsigned int lowImm = ((unsigned int) (imm << 16)) >> 16;
                stb.append("movw").append(Arm::condStrList[(int) cond]).append("\t").append(
                        std::string(*getDst())).append(",\t#").append(std::to_string(lowImm));
                unsigned int highImm = ((unsigned int) imm) >> 16;
                if (highImm != 0) {
                    stb.append("\n\tmovt").append(Arm::condStrList[(int) cond]).append("\t").append(
                            std::string(*getDst())).append(",\t#").append(std::to_string(highImm));
                }
            }
        }
    } else {
        stb.append("mov").append(Arm::condStrList[(int) cond]).append("\t").append(std::string(*getDst())).append(
                ",\t").append((std::string(*getSrc())));
        if (useOpds.size() > 1) {
            std::string ss;
            switch (shift->shiftType) {
                case Arm::ShiftType::Asr:
                    ss = "asr";
                    break;
                case Arm::ShiftType::Lsl:
                    ss = "lsl";
                    break;
                case Arm::ShiftType::Lsr:
                    ss = "lsr";
                    break;
                case Arm::ShiftType::Ror:
                    ss = "ror";
                    break;
                case Arm::ShiftType::Rrx:
                    ss = "rrx";
                    break;
                default :
                    exit(114);
            };
            stb.append(",\t").append(ss).append("\t").append(std::string(*useOpds[1]));
        } else if (shift != Arm::Shift::NONE_SHIFT) {
            stb.append(",\t").append(std::string(*shift));
        }
    }
    return stb;
}

bool I::Mov::isNoCond() const {
    return MachineInst::isNoCond();
}

void I::Mov::remove() {
    ILinkNode::remove();
}

MachineInst *I::Mov::clone() {
    I::Mov *mv = new I::Mov();
    this->copyFieldTo(mv);
    return dynamic_cast<MachineInst *>(mv);
}

I::Binary::Binary(MachineInst *insertAfter, Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd) :
        IMI(insertAfter, tag) {
    defOpds.push_back(dOpd);
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
}

I::Binary::Binary(MachineInst *insertAfter, Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, Arm::Shift *shift) :
        IMI(insertAfter, tag) {
    defOpds.push_back(dOpd);
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
    this->shift = shift;
    if (!shift->shiftIsReg()) useOpds.push_back(shift->shiftOpd);
}

I::Binary::Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, McBlock *insertAtEnd) :
        IMI(tag, insertAtEnd) {
    defOpds.push_back(dOpd);
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
}

I::Binary::Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, Arm::Shift *shift, McBlock *insertAtEnd) :
        IMI(tag, insertAtEnd) {
    defOpds.push_back(dOpd);
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
    this->shift = shift;
    if (!shift->shiftIsReg()) useOpds.push_back(shift->shiftOpd);
}

I::Binary::Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, Arm::Shift *shift, MachineInst *firstUse) :
        IMI(tag, firstUse) {
    defOpds.push_back(dOpd);
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
    this->shift = shift;
    if (!shift->shiftIsReg()) useOpds.push_back(shift->shiftOpd);
}

I::Binary::Binary(Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, MachineInst *firstUse) :
        IMI(tag, firstUse) {
    defOpds.push_back(dOpd);
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
}

// I::Binary::Binary(Tag tag, Operand *dstAddr, Arm::Reg *rSP, Operand *offset, MachineInst *firstUse) :
//         IMI(tag, firstUse) {
//     defOpds.push_back(dstAddr);
//     useOpds.push_back(rSP);
//     useOpds.push_back(offset);
// }

Operand *I::Binary::getDst() const {
    return defOpds[0];
}

Operand *I::Binary::getLOpd() const {
    return useOpds[0];
}

Operand *I::Binary::getROpd() const {
    return useOpds[1];
}

void I::Binary::setROpd(Operand *o) {
    useOpds[1] = o;
}

std::string I::Binary::tagName(Tag tag) const {
    std::string s;
    switch (tag) {
        case MITAG(Mul): {
            s = "mul";
            break;
        }
        case MITAG(Add): {
            s = "add";
            break;
        }
        case MITAG(Sub): {
            s = "sub";
            break;
        }
        case MITAG(Rsb): {
            s = "rsb";
            break;
        }
        case MITAG(Div): {
            s = "sdiv";
            break;
        }
        case MITAG(And): {
            s = "and";
            break;
        }
        case MITAG(Bfc): {
            s = "bfc";
            break;
        }
        case MITAG(Or): {
            s = "orr";
            break;
        }
        case MITAG(LongMul): {
            s = "smmul";
            break;
        }
        case MITAG(Bic): {
            s = "bic";
            break;
        }
        case MITAG(Xor): {
            s = "eor";
            break;
        }
        case MITAG(Lsl): {
            s = "lsl";
            break;
        }
        case MITAG(Lsr): {
            s = "lsr";
            break;
        }
        case MITAG(Asr): {
            s = "asr";
            break;
        }
        case MITAG(USAT): {
            s = "usat";
            break;
        }
    }
    return s + (setCSPR ? "s" : "");
}

I::Binary::operator std::string() const {
    std::string s;
    s.append(tagName(tag)).append(Arm::condStrList[(int) cond]).append("\t").append(std::string(*getDst())).append(
                    ",\t")
            .append(std::string(*getLOpd())).append(",\t").append(std::string(*getROpd()));
    if (shift->shiftType != Arm::ShiftType::None) {
        s.append(",\t").append(std::string(*shift));
    }
    return s;
}

void I::Binary::calCost() {
    Operand *dst = getDst();
    Operand *l = getLOpd();
    Operand *r = getROpd();
    if (!dst->hasReg()) {
        int cost = 1;
        if (l->hasReg()) {
            if (!(*l == (*Arm::Reg::getRreg(Arm::GPR::sp)) && isOf(Tag::Add) && l != dst)) {
                dst->setUnReCal();
            }
        } else {
            cost += l->cost;
        }
        if (!r->isImm()) {
            if (r->hasReg()) {
                dst->setUnReCal();
            } else if (*l != *r) {
                cost += r->cost;
            }
        }
        if (shift->hasShift()) cost++;
        if (tag == Tag::LongMul) cost += 3;
        dst->setDefCost(this, cost);
    }
}

MachineInst *I::Binary::clone() {
    I::Binary *bino = new I::Binary();
    bino->setCSPR = this->setCSPR;
    this->copyFieldTo(bino);
    return dynamic_cast<MachineInst *>(bino);
}

I::Cmp::Cmp(Arm::Cond cond, Operand *lOpd, Operand *rOpd, McBlock *insertAtEnd) :
        IMI(Tag::ICmp, insertAtEnd) {
    this->cond = cond;
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
}

Operand *I::Cmp::getLOpd() const {
    return useOpds[0];
}

Operand *I::Cmp::getROpd() const {
    return useOpds[1];
}

void I::Binary::setDst(Operand *pOperand) {
    defOpds[0] = pOperand;
}

void I::Cmp::setROpd(Operand *src) {
    useOpds[1] = src;
}

I::Cmp::operator std::string() const {
    return std::string(cmn ? "cmn" : "cmp") + "\t" + std::string(*getLOpd()) + ",\t" + std::string(*getROpd());
}

MachineInst *I::Cmp::clone() {
    I::Cmp *cmp = new I::Cmp();
    cmp->cmn = this->cmn;
    this->copyFieldTo(cmp);
    return dynamic_cast<MachineInst *>(cmp);
}

Operand *I::Fma::getDst() const {
    return defOpds[0];
}

Operand *I::Fma::getlOpd() const {
    return useOpds[0];
}

Operand *I::Fma::getrOpd() const {
    return useOpds[1];
}

Operand *I::Fma::getAcc() const {
    return useOpds[2];
}

I::Fma::operator std::string() const {
    std::string res = "";
    if (sign) {
        res += "sm";
    }
    std::string op;
    if (add) {
        op = "mla";
    } else {
        op = "mls";
    }
    res += op + Arm::condStrList[(int) (cond)] + "\t" + std::string(*getDst()) + ",\t" + std::string(*getlOpd()) +
           ",\t" + std::string(*getrOpd()) + ",\t" + std::string(*getAcc());
    return res;
}
// bool I::Fma::add() const {
//     return add;
// }

bool I::Fma::isSign() const {
    return sign;
}

Operand *I::Fma::getDef() const {
    return defOpds[0];
}

void I::Fma::calCost() {
    if (getDst()->isVirtual(DataType::I32)) {
        int cost = 0;
        for (Operand *use: useOpds) {
            cost += use->cost;
        }
        getDst()->setDefCost(this, cost);
    }
}

MachineInst *I::Fma::clone() {
    I::Fma *fma = new Fma;
    fma->sign = this->sign;
    fma->add = this->add;
    fma->cloned = true;
    this->copyFieldTo(fma);
    return fma;
}

I::Swi::Swi(McBlock *insertAtEnd) :
        IMI(Tag::Swi, insertAtEnd) {
}

I::Swi::operator std::string() const {
    return "swi\t#0";
}

I::Wait::Wait(McBlock *insertAtEnd)
        : IMI(Tag::Wait, insertAtEnd) {
}

I::Wait::operator std::string() const {
    return "bl\twait";
}
