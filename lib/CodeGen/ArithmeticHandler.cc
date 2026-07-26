#include "cps/ArithmeticHandler.h"
#include "cps/Lexer.h"
#include <cstdio>

using namespace llvm;
using namespace cps;

// Widen an i1 (BOOLEAN) or i8 (CHAR) operand to i64 with the same ZExt
// coerceValueToType uses, so a mixed pair like EOF(f) - 1 cannot reach the
// instruction builders with mismatched integer widths (invalid IR).
static Value *widenSmallInt(IRBuilder<> &Builder, LLVMContext &Context, Value *V) {
    Type *Ty = V->getType();
    if (Ty->isIntegerTy() && Ty->getIntegerBitWidth() < 64)
        return Builder.CreateZExt(V, Type::getInt64Ty(Context), "widetmp");
    return V;
}

Value *ArithmeticHandler::emitBinaryOp(int Op, Value *LHS, Value *RHS, int Line) {
    bool LIsInt = LHS->getType()->isIntegerTy(64);
    bool RIsInt = RHS->getType()->isIntegerTy(64);
    bool LIsDouble = LHS->getType()->isDoubleTy();
    bool RIsDouble = RHS->getType()->isDoubleTy();

    if (Op == '/') {
        Value *LVal = widenSmallInt(Builder, Context, LHS);
        Value *RVal = widenSmallInt(Builder, Context, RHS);
        if (LVal->getType()->isIntegerTy(64)) LVal = Builder.CreateSIToFP(LVal, Type::getDoubleTy(Context));
        if (RVal->getType()->isIntegerTy(64)) RVal = Builder.CreateSIToFP(RVal, Type::getDoubleTy(Context));
        if (!LVal->getType()->isDoubleTy() || !RVal->getType()->isDoubleTy()) {
            fprintf(stderr, "Error: '/' requires numeric operands (line %d).\n", Line);
            HadError = true;
            return nullptr;
        }

        // TODO: DIV divide by 0 runtime check
        return Builder.CreateFDiv(LVal, RVal, "divtmp");
    }

    if (Op == tok_div || Op == tok_mod) {
        if (!LIsInt || !RIsInt) {
            fprintf(stderr, "Error: DIV and MOD operators require INTEGER operands (line %d).\n", Line);
            HadError = true;
            return nullptr;
        }
        if (Op == tok_div) return Builder.CreateSDiv(LHS, RHS, "div_int_tmp");
        if (Op == tok_mod) return Builder.CreateSRem(LHS, RHS, "mod_tmp");
    }
    
    if (Op == tok_and || Op == tok_or) {
        Value *L = LHS;
        Value *R = RHS;

        if (L->getType()->isIntegerTy(64)) 
            L = Builder.CreateICmpNE(L, ConstantInt::get(Context, APInt(64, 0)), "tobool");
        if (R->getType()->isIntegerTy(64)) 
            R = Builder.CreateICmpNE(R, ConstantInt::get(Context, APInt(64, 0)), "tobool");
        
        if (L->getType()->isDoubleTy())
            L = Builder.CreateFCmpONE(L, ConstantFP::get(Context, APFloat(0.0)), "tobool");
        if (R->getType()->isDoubleTy())
            R = Builder.CreateFCmpONE(R, ConstantFP::get(Context, APFloat(0.0)), "tobool");

        if (Op == tok_and) return Builder.CreateAnd(L, R, "andtmp");
        if (Op == tok_or) return Builder.CreateOr(L, R, "ortmp");
    }

    if (LIsDouble || RIsDouble) {
        Value *LVal = LHS;
        Value *RVal = RHS;
        if (LIsInt) LVal = Builder.CreateSIToFP(LHS, Type::getDoubleTy(Context));
        if (RIsInt) RVal = Builder.CreateSIToFP(RHS, Type::getDoubleTy(Context));
        
        switch (Op) {
            case '+': return Builder.CreateFAdd(LVal, RVal, "addtmp");
            case '-': return Builder.CreateFSub(LVal, RVal, "subtmp");
            case '*': return Builder.CreateFMul(LVal, RVal, "multmp");

            case tok_eq: return Builder.CreateFCmpOEQ(LVal, RVal, "eqtmp");
            case tok_ne: return Builder.CreateFCmpONE(LVal, RVal, "netmp");
            case '<':    return Builder.CreateFCmpOLT(LVal, RVal, "slttmp");
            case '>':    return Builder.CreateFCmpOGT(LVal, RVal, "sgttmp");
            case tok_le: return Builder.CreateFCmpOLE(LVal, RVal, "sletmp");
            case tok_ge: return Builder.CreateFCmpOGE(LVal, RVal, "sgetmp");
        }
    } else {
        // Same-type pairs (i64/i64, i8/i8 CHAR, i1/i1, ptr/ptr STRING) go
        // through untouched; mixed integer widths are widened, anything else
        // mismatched is a diagnostic instead of invalid IR.
        Value *L = LHS;
        Value *R = RHS;
        if (L->getType() != R->getType() &&
            L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
            L = widenSmallInt(Builder, Context, L);
            R = widenSmallInt(Builder, Context, R);
        }
        if (L->getType() != R->getType()) {
            fprintf(stderr, "Error: Mismatched operand types for binary operator (line %d).\n", Line);
            HadError = true;
            return nullptr;
        }
        switch (Op) {
            case '+': return Builder.CreateAdd(L, R, "addtmp");
            case '-': return Builder.CreateSub(L, R, "subtmp");
            case '*': return Builder.CreateMul(L, R, "multmp");

            case tok_eq: return Builder.CreateICmpEQ(L, R, "eqtmp");
            case tok_ne: return Builder.CreateICmpNE(L, R, "netmp");
            case '<':    return Builder.CreateICmpSLT(L, R, "slttmp");
            case '>':    return Builder.CreateICmpSGT(L, R, "sgttmp");
            case tok_le: return Builder.CreateICmpSLE(L, R, "sletmp");
            case tok_ge: return Builder.CreateICmpSGE(L, R, "sgetmp");
        }
    }

    fprintf(stderr, "Error: Unsupported operand types for binary operator (line %d).\n", Line);
    HadError = true;
    return nullptr;
}
