#pragma once
#include "cps/AST.h"
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cps {

// ---- User-defined type declarations (CIE 9618 §4.1) ----

// One field of a record TYPE body. Plain data, not a statement node, so the
// emitStmt chain can never execute a field by accident.
struct RecordFieldDecl {
    std::string Name;
    std::string TypeName;
};

// TYPE <name> = (value1, value2, ...)
class EnumTypeDeclAST : public StmtAST {
    std::string Name;
    std::vector<std::string> ValueNames;

public:
    EnumTypeDeclAST(const std::string &Name, std::vector<std::string> ValueNames)
        : Name(Name), ValueNames(std::move(ValueNames)) {}

    const std::string &getName() const { return Name; }
    const std::vector<std::string> &getValueNames() const { return ValueNames; }
};

// TYPE <name> = ^<pointee type>
class PointerTypeDeclAST : public StmtAST {
    std::string Name;
    std::string PointeeTypeName;

public:
    PointerTypeDeclAST(const std::string &Name, const std::string &PointeeTypeName)
        : Name(Name), PointeeTypeName(PointeeTypeName) {}

    const std::string &getName() const { return Name; }
    const std::string &getPointeeTypeName() const { return PointeeTypeName; }
};

// TYPE <name>
//    DECLARE <field> : <type>
//    ...
// ENDTYPE
class RecordTypeDeclAST : public StmtAST {
    std::string Name;
    std::vector<RecordFieldDecl> Fields;

public:
    RecordTypeDeclAST(const std::string &Name, std::vector<RecordFieldDecl> Fields)
        : Name(Name), Fields(std::move(Fields)) {}

    const std::string &getName() const { return Name; }
    const std::vector<RecordFieldDecl> &getFields() const { return Fields; }
};

// ---- Designators: chained accesses usable as lvalue or rvalue (§4.2) ----

enum class AccessKind : uint8_t { Index, Field, Deref };

// One postfix accessor: [indices], .Field, or ^ (dereference).
struct DesignatorAccess {
    AccessKind Kind;
    std::vector<std::unique_ptr<ExprAST>> Indices; // Index only
    std::string FieldName;                         // Field only
};

// <base name> followed by one or more accessors, e.g. Form[i].YearGroup, p^.
// Bare names and single-index accesses keep their legacy nodes (parser
// degrade rule); a designator node always carries at least one accessor
// beyond those shapes, except as the operand of AddrOfExprAST.
class DesignatorExprAST : public ExprAST {
    std::string BaseName;
    std::vector<DesignatorAccess> Accesses;
    int Line;

public:
    DesignatorExprAST(const std::string &BaseName,
                      std::vector<DesignatorAccess> Accesses,
                      int Line)
        : BaseName(BaseName), Accesses(std::move(Accesses)), Line(Line) {}

    const std::string &getBaseName() const { return BaseName; }
    const std::vector<DesignatorAccess> &getAccesses() const { return Accesses; }
    int getLine() const { return Line; }
};

// ^<designator> — address-of. The operand is a designator by construction,
// so an lvalue is guaranteed at parse time.
class AddrOfExprAST : public ExprAST {
    std::unique_ptr<DesignatorExprAST> Target;

public:
    explicit AddrOfExprAST(std::unique_ptr<DesignatorExprAST> Target)
        : Target(std::move(Target)) {}

    DesignatorExprAST *getTarget() const { return Target.get(); }
};

// <designator> <- <expr> for targets beyond a bare name or single indexing
// (those degrade to AssignStmtAST / ArrayAssignStmtAST in the parser).
class DesignatorAssignStmtAST : public StmtAST {
    std::unique_ptr<DesignatorExprAST> Target;
    std::unique_ptr<ExprAST> Expr;

public:
    DesignatorAssignStmtAST(std::unique_ptr<DesignatorExprAST> Target,
                            std::unique_ptr<ExprAST> Expr)
        : Target(std::move(Target)), Expr(std::move(Expr)) {}

    DesignatorExprAST *getTarget() const { return Target.get(); }
    ExprAST *getExpr() const { return Expr.get(); }
};

} // namespace cps
