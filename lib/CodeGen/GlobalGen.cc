#include "cps/CodeGen.h"
#include "llvm/IR/GlobalVariable.h"

using namespace cps;
using namespace llvm;

namespace {
bool isGlobalLiteral(ExprAST *Expr) {
    if (dynamic_cast<IntegerExprAST*>(Expr) || dynamic_cast<RealExprAST*>(Expr) ||
        dynamic_cast<CharExprAST*>(Expr) || dynamic_cast<StringExprAST*>(Expr) ||
        dynamic_cast<BooleanExprAST*>(Expr)) return true;
    auto *Unary = dynamic_cast<UnaryExprAST*>(Expr);
    return Unary && Unary->getOp() == '-' &&
        (dynamic_cast<IntegerExprAST*>(Unary->getOperand()) ||
         dynamic_cast<RealExprAST*>(Unary->getOperand()));
}
}

void CodeGen::runGlobalPrePass(const SourceUnit &Unit, int Phase) {
    for (const auto &Stmt : Unit.Statements) {
        auto *Block = dynamic_cast<GlobalBlockAST*>(Stmt.get());
        if (!Block) continue;
        for (const auto &S : Block->getBody()) {
            SaveAndRestore<int> Location(DiagnosticLine, S->getLine());
            if (auto *Decl = dynamic_cast<DeclareStmtAST*>(S.get())) {
                if (Phase != 0) continue;
                const auto &Name = Decl->getName();
                const auto &TypeName = Decl->getType();
                if (Decl->hasParens() ||
                    !(sameIdentifier(TypeName, "INTEGER") || sameIdentifier(TypeName, "REAL") ||
                      sameIdentifier(TypeName, "CHAR") || sameIdentifier(TypeName, "STRING") ||
                      sameIdentifier(TypeName, "BOOLEAN"))) {
                    reportError("GLOBAL declarations require a primitive scalar type without constructor arguments");
                    continue;
                }
                if (Exports[CurrentUnit].count(Name) || Types->resolve(Name) ||
                    Types->lookupEnumConstant(Name) || FuncGen->getSignature(Name) ||
                    isBuiltinName(Name)) {
                    reportError("Global %s conflicts with an existing definition", Name.c_str());
                    continue;
                }
                auto *Storage = new GlobalVariable(*TheModule, Types->getLLVMType(TypeName), false,
                    GlobalValue::InternalLinkage, Types->getZeroValue(TypeName), "__cps_global." + Name);
                SymbolInfo Info{Storage, TypeName, false, nullptr, 0, false, true};
                Exports[CurrentUnit].emplace(Name, Info);
            } else if (auto *Assign = dynamic_cast<AssignStmtAST*>(S.get())) {
                if (Phase != 1) continue;
                const SymbolInfo *Info = getSymbolInfo(Assign->getName());
                if (!Info || !Info->IsGlobal) {
                    reportError("GLOBAL initializer requires a declared global variable %s",
                                Assign->getName().c_str());
                    continue;
                }
                if (!isGlobalLiteral(Assign->getExpr())) {
                    reportError("GLOBAL initializer must be a literal");
                    continue;
                }
                auto *Value = dyn_cast_or_null<Constant>(
                    emitCoercedExpr(Assign->getExpr(), resolveType(Info->TypeName)));
                if (!Value) {
                    reportError("GLOBAL initializer must be a constant scalar literal");
                    continue;
                }
                cast<GlobalVariable>(Info->Storage)->setInitializer(Value);
            } else if (Phase == 0) {
                reportError("GLOBAL blocks allow only primitive DECLARE statements and literal initializers");
            }
        }
    }
}
