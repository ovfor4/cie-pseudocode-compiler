#include "cps/CodeGen.h"
#include "cps/Operator.h"

using namespace cps;

// Probing candidates must not evaluate operands or diagnose absent overloads:
// both static typing and emission use this same selection.
CodeGen::OperatorResolution CodeGen::resolveOperator(
    int Op, const TypeInfo *Left, const TypeInfo *Right) {
    OperatorResolution Result;
    if (!Left) return Result;
    for (const FuncSig *Sig : FuncGen->getOverloads(operatorName(Op))) {
        if (Sig->Prototype->getOperator() != Op || Sig->Params.size() != (Right ? 2u : 1u)) continue;
        if (!Left->sameType(resolveType(Sig->Params[0].TypeName))) continue;
        if (Right && !Right->sameType(resolveType(Sig->Params[1].TypeName))) continue;
        if (Result.Signature) Result.Ambiguous = true;
        else Result.Signature = Sig;
    }
    return Result;
}
