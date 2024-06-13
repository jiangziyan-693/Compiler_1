//
// Created by XuRuiyuan on 2023/7/22.
//

#include "lir/StackCtrl.h"
#include "Function.h"
#include "Arm.h"
#include "CodeGen.h"

StackCtl::Push::operator std::string() const {
    if (savedRegsMf->mFunc->isExternal) {
        // throw new AssertionError("push in external func");
        // return "\tpush\t{r0,r1,r2,r3}";
        return "push\t{r2,r3}";
    } else {
        std::string sb;
        if (savedRegsMf->usedCalleeSavedGPRs != 0) {
            sb.append("push\t{");
            unsigned int num = savedRegsMf->usedCalleeSavedGPRs;
            assert(num <= (1U << GPR_NUM));
            int i = 0;
            bool first = true;
            while (num) {
                if ((num & 1u) != 0) {
                    if (!first) {
                        sb.append(",");
                    } else {
                        first = false;
                    }
                    assert(0 <= i && i < GPR_NUM);
                    sb.append(Arm::gprStrList[i]);
                }
                i++;
                num >>= 1;
            }
            sb.append("}");
            // todo: for bf fixup
            if(CenterControl::_PREC_MB_TOO_MUCH) sb.append("\n\tnop");
        }
        return sb;
    }
}


StackCtl::Pop::operator std::string() const {
    if (savedRegsMf->mFunc->isExternal) {
        // throw new AssertionError("push in external func");
        // return "\tpush\t{r0,r1,r2,r3}";
        return "pop\t{r2,r3}";
    } else {
        std::string sb;
        if (savedRegsMf->usedCalleeSavedGPRs != 0) {
            sb.append("pop\t{");
            unsigned int num = savedRegsMf->usedCalleeSavedGPRs;
            int i = 0;
            bool first = true;
            while (num) {
                if ((num & 1u) != 0) {
                    if (!first) {
                        sb.append(" ,");
                    } else {
                        first = false;
                    }
                    sb.append(Arm::gprStrList[i]);
                }
                i++;
                num >>= 1;
            }
            sb.append("}");
        }
        return sb;
    }
}

StackCtl::VPush::operator std::string() const {
    std::string sb;
    if (savedRegsMf->mFunc->isExternal) {
        sb.append("\tvpush\t{s2-s").append(std::to_string(CodeGen::sParamCnt - 1)).append("}");
    } else {
        if (savedRegsMf->usedCalleeSavedFPRs != 0) {
            unsigned int fprBit = savedRegsMf->usedCalleeSavedFPRs;
            int start = 0;
            while (start < FPR_NUM) {
                while (start < FPR_NUM && ((fprBit & (1u << start)) == 0))
                    start++;
                if (start == FPR_NUM)
                    break;
                int end = start;
                while (end < FPR_NUM && fprBit & (1u << end))
                    end++;
                end--;
                if (end == start) {
                    exit(119);
                } else if (end > start) {
                    if (end - start > 15) {
                        end = start + 15;
                    }
                    sb.append("\tvpush\t{s").append(std::to_string(start)).append("-s").append(std::to_string(end)).append("}\n");
                }
                start = end + 1;
            }
        } else {
            sb.append("@ ! empty VPush !");
        }
    }
    return sb;
}


StackCtl::VPop::operator std::string() const {
    exit(102);
}


