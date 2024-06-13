//
// Created by XuRuiyuan on 2023/7/31.
//

#include "lir/MachineInst.h"
#include "backend/MC.h"
#include "lir/V.h"
#include "mir/Function.h"
#include "backend/CodeGen.h"
#include "util/util.h"
#include "sstream"

// #define ER 100

std::string VMI::getMvSuffixTypeSimply(Operand *dst) {
    switch (dst->dataType) {
        case DataType::F32 :
            return ".f32";
        case DataType::I32 :
            return "";
        case DataType::I1 :
            exit(100);
    };
    return ""; // eliminate warning
}

VMI::VMI(MachineInst::Tag tag, McBlock *insertAtEnd) : MachineInst(tag, insertAtEnd) {
}

VMI::VMI(MachineInst::Tag tag, MachineInst *insertBefore) : MachineInst(tag, insertBefore) {
}

VMI::VMI(MachineInst *insertAfter, MachineInst::Tag tag) : MachineInst(insertAfter, tag) {
}


V::Ldr::Ldr(Operand *data, Operand *addr, McBlock *insertAtEnd) : VMI(MITAG(VLdr), insertAtEnd) {
    defOpds.push_back(data);
    useOpds.push_back(addr);
}

V::Ldr::Ldr(Operand *data, Operand *addr, Operand *offset, MachineInst *insertBefore) : VMI(MITAG(VLdr), insertBefore) {
    defOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
}

V::Ldr::Ldr(Operand *svr, Operand *dstAddr, MachineInst *firstUse) : VMI(MITAG(VLdr), firstUse) {
    defOpds.push_back(svr);
    useOpds.push_back(dstAddr);
}

Operand *V::Ldr::getData() const {
    return defOpds[0];
}

Operand *V::Ldr::getAddr() const {
    return useOpds[0];
}

Operand *V::Ldr::getOffset() const {
    if (useOpds.size() < 2) return new Operand(DataType::I32, 0);
    return useOpds[1];
}

void V::Ldr::setAddr(Operand *lOpd) {
    useOpds[0] = lOpd;
}

void V::Ldr::setOffSet(Operand *offSet) {
    if (useOpds.size() < 2) useOpds.push_back(offSet);
    useOpds[1] = offSet;
}

void V::Ldr::remove() {
    ILinkNode::remove();
}

bool V::Ldr::isNoCond() const {
    return MachineInst::isNoCond();
}

Arm::Shift *V::Ldr::getShift() const {
    return MachineInst::getShift();
}

void V::Ldr::addShift(Arm::Shift *shift) {
    MachineInst::addShift(shift);
}

Arm::Cond V::Ldr::getCond() const {
    return MachineInst::getCond();
}

V::Ldr::operator std::string() const {
    std::stringstream ss;
    ss << "vldr" << Arm::condStrList[(int) cond] + ".f32\t" << std::string(*getData());
    ss << ",\t[" << std::string(*getAddr());
    Operand *vOff = getOffset();
    if (*vOff != *Operand::I_ZERO) {
        ss << ",\t" << std::string(*vOff);
        if (shift->shiftType != Arm::ShiftType::None) {
            ss << "\t, " << std::string(*shift);
        }
    }
    ss << "]";
    return ss.str();
}

MachineInst *V::Ldr::clone() {
    V::Ldr *vldr = new V::Ldr;
    this->copyFieldTo(vldr);
    return dynamic_cast<MachineInst *>(vldr);
}


V::Str::Str(Operand *data, Operand *addr, Operand *offset, McBlock *insertAtEnd) : VMI(MITAG(VStr), insertAtEnd) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
}

V::Str::Str(Operand *data, Operand *addr, McBlock *insertAtEnd) : VMI(MITAG(VStr), insertAtEnd) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
}

V::Str::Str(MachineInst *insertAfter, Operand *data, Operand *addr, Operand *offset) : VMI(insertAfter, MITAG(VStr)) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
    useOpds.push_back(offset);
}

V::Str::Str(MachineInst *insertAfter, Operand *data, Operand *addr) : VMI(insertAfter, MITAG(VStr)) {
    useOpds.push_back(data);
    useOpds.push_back(addr);
}

Operand *V::Str::getData() const {
    return useOpds[0];
}

Operand *V::Str::getAddr() const {
    return useOpds[1];
}

Operand *V::Str::getOffset() const {
    if (useOpds.size() < 3) return nullptr;
    return useOpds[2];
}

void V::Str::setAddr(Operand *lOpd) {
    useOpds[1] = lOpd;
}

void V::Str::setOffSet(Operand *rOpd) {
    if (useOpds.size() < 3) useOpds.push_back(rOpd);
    else useOpds[2] = rOpd;
}

void V::Str::remove() {
    ILinkNode::remove();
}

bool V::Str::isNoCond() const {
    return MachineInst::isNoCond();
}

Arm::Shift *V::Str::getShift() const {
    return MachineInst::getShift();
}

void V::Str::addShift(Arm::Shift *shift) {
    MachineInst::addShift(shift);
}

Arm::Cond V::Str::getCond() const {
    return MachineInst::getCond();
}

V::Str::operator std::string() const {
    std::string stb;
    if (getOffset() == nullptr) {
        stb.append("vstr").append(std::string(Arm::condStrList[(int) cond])).append(".f32\t").append(
                std::string(*getData())).append(",\t[").append(std::string(*getAddr())).append("]");
    } else if (getOffset()->opdType == Operand::OpdType::Immediate) {
        int shift = (this->shift->shiftType == Arm::ShiftType::None) ? 0 : this->shift->shiftOpd->value;
        int offset = this->getOffset()->value << shift;
        if (offset != 0) {
            stb.append("vstr").append(Arm::condStrList[(int) cond]).append(".32\t").append(
                    std::string(*getData())).append(",\t[").append(
                    std::string(*getAddr())).append(",\t#").append(std::to_string(offset)).append("]");
        } else {
            stb.append("vstr").append(Arm::condStrList[(int) cond]).append(".32\t").append(
                    std::string(*getData())).append(",\t[").append(std::string(*getAddr())).append("]");
        }
    } else {
        if (this->shift->shiftType == Arm::ShiftType::None) {
            stb.append("vstr").append(Arm::condStrList[(int) cond]).append(
                    ".32\t").append(std::string(*getData())).append(",\t[").append(std::string(*getAddr())).append(
                    ",\t").append(
                    std::string(*getOffset())).append("]");
        } else {
            stb.append("vstr").append(Arm::condStrList[(int) cond]).append(
                    ".32\t").append(std::string(*getData())).append(",\t[").append(
                    std::string(*getAddr())).append(",\t").append(
                    std::string(*getOffset())).append(",\tLSL #").append(
                    std::string(*this->shift->shiftOpd)).append("]");
        }
    }
    return stb;
}

MachineInst *V::Str::clone() {
    V::Str *vstr = new V::Str;
    this->copyFieldTo(vstr);
    return dynamic_cast<MachineInst *>(vstr);
}


V::Mov::Mov(Operand *dst, Operand *src, McBlock *insertAtEnd) : VMI(MITAG(VMov), insertAtEnd) {
    defOpds.push_back(dst);
    useOpds.push_back(src);
}

V::Mov::Mov(Operand *dst, Operand *src, MachineInst *insertBefore) : VMI(MITAG(VMov), insertBefore) {
    defOpds.push_back(dst);
    useOpds.push_back(src);
}

Operand *V::Mov::getDst() const {
    return defOpds[0];
}

Operand *V::Mov::getSrc() const {
    return useOpds[0];
}

void V::Mov::setSrc(Operand *offset_opd) {
    useOpds[0] = offset_opd;
}

void V::Mov::setDst(Operand *dst) {
    defOpds[0] = dst;
}

V::Mov::operator std::string() const {
    return "vmov" + getMvSuffixTypeSimply(getDst()) + "\t" + std::string(*getDst()) + ",\t" + std::string(*getSrc());
}

bool V::Mov::directColor() const {
    return getDst()->needColor(DataType::F32) && getSrc()->needColor(DataType::F32) &&
           cond == Arm::Cond::Any &&
           shift->shiftType == Arm::ShiftType::None;
}

bool V::Mov::isNoCond() const {
    return MachineInst::isNoCond();
}

void V::Mov::remove() {
    ILinkNode::remove();
}

MachineInst *V::Mov::clone() {
    V::Mov *vmov = new V::Mov;
    this->copyFieldTo(vmov);
    return dynamic_cast<MachineInst *>(vmov);
}

V::Cvt::Cvt(CvtType cvtType, Operand *dst, Operand *src, McBlock *insertAtEnd) : VMI(MachineInst::Tag::VCvt,
                                                                                     insertAtEnd) {
    this->cvtType = cvtType;
    defOpds.push_back(dst);
    useOpds.push_back(src);
}

Operand *V::Cvt::getDst() const {
    return defOpds[0];
}

Operand *V::Cvt::getSrc() const {
    return useOpds[0];
}

V::Cvt::operator std::string() const {
    std::string stb;
    switch (cvtType) {
        case f2i :
            return "vcvt.s32.f32\t" + std::string(*getDst()) + ",\t" + std::string(*getSrc());
        case i2f:
            return "vcvt.f32.s32\t" + std::string(*getDst()) + ",\t" + std::string(*getSrc());
        default:
            exit(101);
    }
}

MachineInst *V::Cvt::clone() {
    V::Cvt *vcvt = new V::Cvt;
    vcvt->cvtType = this->cvtType;
    this->copyFieldTo(vcvt);
    return dynamic_cast<MachineInst *>(vcvt);
}

V::Binary::Binary(MachineInst::Tag tag, Operand *dOpd, Operand *lOpd, Operand *rOpd, McBlock *insertAtEnd) : VMI(tag,
                                                                                                                 insertAtEnd) {
    defOpds.push_back(dOpd);
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
}

Operand *V::Binary::getDst() const {
    return defOpds[0];
}

Operand *V::Binary::getLOpd() const {
    return useOpds[0];
}

Operand *V::Binary::getROpd() const {
    return useOpds[1];
}

V::Binary::operator std::string() const {
    std::string stb;
    switch (tag) {
        case MachineInst::Tag::FAdd:
            stb = "vadd.f32";
            break;
        case MachineInst::Tag::FSub:
            stb = "vsub.f32";
            break;
        case MachineInst::Tag::FDiv:
            stb = "vdiv.f32";
            break;
        case MachineInst::Tag::FMul:
            stb = "vmul.f32";
            break;
        default :
            exit(102);
    }
    stb.append("\t").append(std::string(*getDst())).append(",\t").append(std::string(*getLOpd())).append(",\t").append(
            std::string(*getROpd()));
    if (shift->shiftType != Arm::ShiftType::None) {
        stb.append(",\t");
        stb.append(std::string(*shift));
    }
    return stb;
}

MachineInst *V::Binary::clone() {
    V::Binary *vbino = new V::Binary;
    this->copyFieldTo(vbino);
    return dynamic_cast<MachineInst *>(vbino);
}

V::Neg::Neg(Operand *dst, Operand *src, McBlock *insertAtEnd) : VMI(MachineInst::Tag::VNeg, insertAtEnd) {
    defOpds.push_back(dst);
    useOpds.push_back(src);
}

Operand *V::Neg::getDst() const {
    return defOpds[0];
}

Operand *V::Neg::getSrc() const {
    return useOpds[0];
}

V::Neg::operator std::string() const {
    return "vneg.f32" + std::string(*getDst()) + ",\t" + std::string(*getSrc());
}

MachineInst *V::Neg::clone() {
    V::Neg *neg = new V::Neg;
    this->copyFieldTo(neg);
    return dynamic_cast<MachineInst *>(neg);
}


V::Cmp::Cmp(Arm::Cond cond, Operand *lOpd, Operand *rOpd, McBlock *insertAtEnd) : VMI(MITAG(VCmp), insertAtEnd) {
    this->cond = cond;
    useOpds.push_back(lOpd);
    useOpds.push_back(rOpd);
}

Operand *V::Cmp::getLOpd() const {
    return useOpds[0];
}

Operand *V::Cmp::getROpd() const {
    return useOpds[1];
}

V::Cmp::operator std::string() const {
    return "vcmpe.f32\t" + std::string(*getLOpd()) + ",\t" + std::string(*getROpd()) + "\n\tvmrs\tAPSR_nzcv,\tFPSCR";
}

MachineInst *V::Cmp::clone() {
    V::Cmp *vcmp = new V::Cmp;
    this->copyFieldTo(vcmp);
    return dynamic_cast<MachineInst *>(vcmp);
}

V::Ret::Ret(McFunction *mf, Arm::Reg *s0, McBlock *curMB) : VMI(MITAG(VRet), curMB) {
    savedRegsMf = mf;
    useOpds.push_back(s0);
}

V::Ret::operator std::string() const {
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
                    while (end > -1 && ((fprBit & (1u << end)) == 0))
                        end--;
                    if (end == -1)
                        break;
                    int start = end;
                    while (start > -1 && (fprBit & (1u << start)))
                        start--;
                    start++;
                    if (start == end) {
                        exit(103);
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

MachineInst *V::Ret::clone() {
    V::Ret *vret = new V::Ret;
    vret->savedRegsMf = this->savedRegsMf;
    this->copyFieldTo(vret);
    return dynamic_cast<V::Ret *>(vret);
}
