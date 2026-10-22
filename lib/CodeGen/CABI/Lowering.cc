// Applies a clang-arranged CABISignature with no per-architecture code. Sizes
// are never computed here: the module has no DataLayout until emission, so a
// coerced value goes through memory that holds both the value and its coerced
// form.
#include "cps/CABI.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"

using namespace llvm;
using namespace cps;

namespace {

bool isDirect(const CABIArg &A) {
    return A.K == CABIArg::Kind::Direct || A.K == CABIArg::Kind::Extend;
}

// The IR values one argument occupies: none, one, or a flattened struct's elements.
unsigned countIRArgs(const CABIArg &A) {
    if (A.K == CABIArg::Kind::Ignore) return 0;
    return A.Flatten ? cast<StructType>(A.CoerceType)->getNumElements() : 1;
}

void appendIRTypes(const CABIArg &A, std::vector<Type *> &Out) {
    if (A.K == CABIArg::Kind::Indirect) Out.push_back(PointerType::getUnqual(A.ValueType->getContext()));
    else if (A.Flatten) for (Type *Element : cast<StructType>(A.CoerceType)->elements()) Out.push_back(Element);
    else if (isDirect(A)) Out.push_back(A.CoerceType);
}

AllocaInst *entryAlloca(IRBuilder<> &B, Type *Ty, uint64_t MinAlign, const Twine &Name) {
    Function *F = B.GetInsertBlock()->getParent();
    IRBuilder<> Entry(&F->getEntryBlock(), F->getEntryBlock().begin());
    AllocaInst *Memory = Entry.CreateAlloca(Ty, nullptr, Name);
    if (MinAlign > Memory->getAlign().value()) Memory->setAlignment(Align(MinAlign));
    return Memory;
}

// Memory for the value at offset 0 and its coerced form at A.Offset; the
// struct is at least as large and as aligned as either.
struct CoercionSlot {
    AllocaInst *Value;
    llvm::Value *Coerced;
    Align CoercedAlign;
};

CoercionSlot coercionSlot(IRBuilder<> &B, const CABIArg &A) {
    LLVMContext &C = A.ValueType->getContext();
    auto *Ty = StructType::get(C, {A.ValueType, ArrayType::get(Type::getInt8Ty(C), A.Offset), A.CoerceType});
    AllocaInst *Memory = entryAlloca(B, Ty, 0, "c.coerce");
    Value *Coerced = B.CreateConstInBoundsGEP1_32(Type::getInt8Ty(C), Memory, A.Offset);
    return {Memory, Coerced, commonAlignment(Memory->getAlign(), A.Offset)};
}

AttributeSet argumentAttributes(LLVMContext &C, const CABIArg &A) {
    AttrBuilder Attrs(C);
    if (A.K == CABIArg::Kind::Extend) Attrs.addAttribute(A.Extension);
    if (A.InReg) Attrs.addAttribute(Attribute::InReg);
    if (isDirect(A) && A.DirectAlign) Attrs.addStackAlignmentAttr(A.DirectAlign);
    if (A.K == CABIArg::Kind::Indirect && A.ByVal) {
        Attrs.addByValAttr(A.ValueType);
        Attrs.addAlignmentAttr(A.Align);
    }
    return AttributeSet::get(C, Attrs);
}

} // namespace

FunctionType *cps::getCABIFunctionType(const CABISignature &Sig) {
    LLVMContext &C = Sig.Ret.ValueType->getContext();
    std::vector<Type *> Params;
    if (Sig.Ret.K == CABIArg::Kind::Indirect) Params.push_back(PointerType::getUnqual(C));
    for (size_t I = 0; I < Sig.Fixed; ++I) appendIRTypes(Sig.Args[I], Params);
    Type *Ret = isDirect(Sig.Ret) ? Sig.Ret.CoerceType : Type::getVoidTy(C);
    return FunctionType::get(Ret, Params, Sig.Variadic);
}

AttributeList cps::getCABIAttributes(LLVMContext &C, const CABISignature &Sig) {
    AttrBuilder RetAttrs(C);
    std::vector<AttributeSet> Params;
    if (Sig.Ret.K == CABIArg::Kind::Indirect) {
        AttrBuilder SRet(C);
        SRet.addStructRetAttr(Sig.Ret.ValueType);
        SRet.addAlignmentAttr(Sig.Ret.Align);
        if (Sig.Ret.InReg) SRet.addAttribute(Attribute::InReg);
        Params.push_back(AttributeSet::get(C, SRet));
    } else if (isDirect(Sig.Ret)) {
        if (Sig.Ret.K == CABIArg::Kind::Extend) RetAttrs.addAttribute(Sig.Ret.Extension);
        if (Sig.Ret.InReg) RetAttrs.addAttribute(Attribute::InReg);
    }
    for (const CABIArg &A : Sig.Args)
        Params.insert(Params.end(), countIRArgs(A), argumentAttributes(C, A));
    return AttributeList::get(C, AttributeSet(), AttributeSet::get(C, RetAttrs), Params);
}

std::vector<Value *> cps::lowerCABIArguments(IRBuilder<> &B, const CABISignature &Sig,
                                             ArrayRef<Value *> Args, Value *&SRet) {
    std::vector<Value *> Out;
    SRet = nullptr;
    if (Sig.Ret.K == CABIArg::Kind::Indirect) {
        SRet = entryAlloca(B, Sig.Ret.ValueType, Sig.Ret.Align, "c.result");
        Out.push_back(SRet);
    }
    for (size_t I = 0; I < Args.size(); ++I) {
        const CABIArg &A = Sig.Args[I];
        Value *V = Args[I];
        if (A.K == CABIArg::Kind::Ignore) continue;
        if (A.K == CABIArg::Kind::Indirect) {
            // byval or not, the callee receives memory of its own.
            AllocaInst *Copy = entryAlloca(B, A.ValueType, A.Align, "c.arg");
            B.CreateStore(V, Copy);
            Out.push_back(Copy);
            continue;
        }
        if (!A.Flatten && !A.Offset && A.CoerceType == V->getType()) {
            Out.push_back(V);
            continue;
        }
        CoercionSlot Slot = coercionSlot(B, A);
        B.CreateStore(V, Slot.Value);
        if (!A.Flatten) {
            Out.push_back(B.CreateAlignedLoad(A.CoerceType, Slot.Coerced, Slot.CoercedAlign));
            continue;
        }
        auto *Struct = cast<StructType>(A.CoerceType);
        for (unsigned E = 0; E < Struct->getNumElements(); ++E)
            Out.push_back(B.CreateLoad(Struct->getElementType(E), B.CreateStructGEP(Struct, Slot.Coerced, E)));
    }
    return Out;
}

Value *cps::lowerCABIResult(IRBuilder<> &B, const CABISignature &Sig, Value *Call, Value *SRet) {
    const CABIArg &R = Sig.Ret;
    if (R.ValueType->isVoidTy()) return nullptr;
    if (R.K == CABIArg::Kind::Ignore) return Constant::getNullValue(R.ValueType);
    if (R.K == CABIArg::Kind::Indirect) return B.CreateLoad(R.ValueType, SRet);
    if (!R.Offset && R.CoerceType == R.ValueType) return Call;
    CoercionSlot Slot = coercionSlot(B, R);
    B.CreateAlignedStore(Call, Slot.Coerced, Slot.CoercedAlign);
    return B.CreateLoad(R.ValueType, Slot.Value);
}
