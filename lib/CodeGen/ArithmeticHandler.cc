#include "cps/ArithmeticHandler.h"
#include "cps/Lexer.h"
#include <cstdio>

using namespace llvm;
using namespace cps;

Value *ArithmeticHandler::emitBinaryOp(int Op, Value *LHS, Value *RHS, int Line) {
    bool LIsInt = LHS->getType()->isIntegerTy(64);
    bool RIsInt = RHS->getType()->isIntegerTy(64);
    bool LIsDouble = LHS->getType()->isDoubleTy();
    bool RIsDouble = RHS->getType()->isDoubleTy();

    if (Op == '/') {
        Value *LVal = LHS;
        Value *RVal = RHS;
        if (LIsInt) LVal = Builder.CreateSIToFP(LHS, Type::getDoubleTy(Context));
        if (RIsInt) RVal = Builder.CreateSIToFP(RHS, Type::getDoubleTy(Context));
        
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
        switch (Op) {
            case '+': return Builder.CreateAdd(LHS, RHS, "addtmp");
            case '-': return Builder.CreateSub(LHS, RHS, "subtmp");
            case '*': return Builder.CreateMul(LHS, RHS, "multmp");
            
            case tok_eq: return Builder.CreateICmpEQ(LHS, RHS, "eqtmp");
            case tok_ne: return Builder.CreateICmpNE(LHS, RHS, "netmp");
            case '<':    return Builder.CreateICmpSLT(LHS, RHS, "slttmp");
            case '>':    return Builder.CreateICmpSGT(LHS, RHS, "sgttmp");
            case tok_le: return Builder.CreateICmpSLE(LHS, RHS, "sletmp");
            case tok_ge: return Builder.CreateICmpSGE(LHS, RHS, "sgetmp");
        }
    }

    fprintf(stderr, "Error: Unsupported operand types for binary operator (line %d).\n", Line);
    HadError = true;
    return nullptr;
}
