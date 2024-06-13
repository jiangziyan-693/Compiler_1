#include "../../include/midend/GlobalValueLocalize.h"
#include "../../include/util/CenterControl.h"
#include "HashMap.h"




	GlobalValueLocalize::GlobalValueLocalize(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues) {
        this->functions = functions;
        this->globalValues = globalValues;
//        this->removedGlobal = new std::set<>();
    }

    void GlobalValueLocalize::Run() {
        for (Value* value: KeySet(*globalValues)) {
            if (!(*globalValues)[(GlobalValue*) value]->type->is_int32_type() &&
                !(*globalValues)[(GlobalValue*) value]->type->is_float_type()) {
                continue;
            }
            localizeSingleValue(value);
        }
        for (Value* value: *removedGlobal) {
            globalValues->erase((GlobalValue*) value);
            value->remove();
        }
    }

    void GlobalValueLocalize::localizeSingleValue(Value* value) {
        bool hasStore = false;
        std::set<Function*>* useFunctions = new std::set<Function*>();
        Use* use = value->getBeginUse();
        std::set<Instr*>* useInstrs = new std::set<Instr*>();
        while (use->next != nullptr) {
        if (dynamic_cast<INSTR::Store*>(use->user) != nullptr) {
                hasStore = true;
            }
            useInstrs->insert(use->user);
            useFunctions->insert(use->user->bb->function);
            use = (Use*) use->next;
        }


        // 没有被store的全局变量,
        if (!hasStore) {
            for (Instr* instr : *useInstrs) {
                Constant* constant = nullptr;
                ValueInit* initValue = (ValueInit*) (*globalValues)[(GlobalValue*) value];
// dynamic_cast<Constant*>(assert initValue->getValue()) != nullptr;
                if (initValue->type->is_float_type()) {
                    constant = new ConstantFloat((float) ((ConstantFloat*) initValue->value)->get_const_val());
                } else if ((*globalValues)[(GlobalValue*) value]->type->is_int32_type()) {
                    constant = new ConstantInt((int) ((ConstantInt*) initValue->value)->get_const_val());
                } else {
                    //System->err.println("error");
                }
                instr->modifyAllUseThisToUseA(constant);
                instr->remove();
            }
            removedGlobal->insert(value);
            return;
        }

        //只在main中调用的
        if (useFunctions->size() == 1) {
            Function* function = nullptr;
            for (Function* tmp: *useFunctions) {
                function = tmp;
            }
            if (function->name != ("main")) {
                return;
            }
            BasicBlock* entry = function->getBeginBB();

            ValueInit* initValue = (ValueInit*) (*globalValues)[(GlobalValue*) value];
// dynamic_cast<Constant*>(assert initValue->getValue()) != nullptr;

            if (initValue->type->is_float_type()) {
                INSTR::Alloc* alloc = new INSTR::Alloc(BasicType::F32_TYPE, entry, true);
                INSTR::Store* store = new INSTR::Store(initValue->value, alloc, entry);
                value->modifyAllUseThisToUseA(alloc);
                entry->insertAtHead(store);
                entry->insertAtHead(alloc);
            } else if ((*globalValues)[(GlobalValue*) value]->type->is_int32_type()) {
                INSTR::Alloc* alloc = new INSTR::Alloc(BasicType::I32_TYPE, entry, true);
                INSTR::Store* store = new INSTR::Store(initValue->value, alloc, entry);
                value->modifyAllUseThisToUseA(alloc);
                entry->insertAtHead(store);
                entry->insertAtHead(alloc);
            } else {
                //System->err.println("error");
            }
            removedGlobal->insert(value);
        }

        // 只在一个函数中被load,store
        // 只要有store,就不能直接替换为初始值,
        // 即使一个函数内没有store,但是因为另外的函数存在store,不明函数的调用逻辑,仍不能在没有store 的函数内替换为初始值
        // 即使只在一个函数内被load,store,仍然不能局部化,因为局部化后,每次调用函数对变量的修改是无法保留的
        // 如:
        // int a = 0;
        // int func() {
        //     a = a + 1;
        // }
        //TODO:获取函数被调用的次数,次数为1则可局部化:只在非递归函数的循环深度为一的位置被调用一次?
        //fixme:上述条件是否正确
        //TODO:main不可能被递归调用,纳入考虑
    }



