#ifndef ALLOCA_MOVE_H_
#define ALLOCA_MOVE_H_

#include <vector>
#include "Function.h"

class AllocaMove {
public:
    std::vector<Function*>* functions;
    AllocaMove(std::vector<Function*>* functions);
    void Run();
};

#endif