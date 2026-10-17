#pragma once
#include "cps/AST.h"
#include "cps/FunctionAST.h"
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace cps {

// Only an assignment may own this expression.
class NewExprAST : public ExprAST {
    std::string ClassName;
    std::vector<std::unique_ptr<ExprAST>> Args;
public:
    NewExprAST(std::string ClassName, std::vector<std::unique_ptr<ExprAST>> Args, int Line)
        : ExprAST(Line), ClassName(std::move(ClassName)), Args(std::move(Args)) {}
    const std::string &getClassName() const { return ClassName; }
    const auto &getArgs() const { return Args; }
};

class CastExprAST : public ExprAST {
    std::string TypeName;
    std::unique_ptr<ExprAST> Operand;
public:
    CastExprAST(std::string TypeName, std::unique_ptr<ExprAST> Operand)
        : TypeName(std::move(TypeName)), Operand(std::move(Operand)) {}
    const std::string &getTypeName() const { return TypeName; }
    ExprAST *getOperand() const { return Operand.get(); }
};

// TYPE <name> = C_TYPE("<C type expression>")
class CTypeDeclAST : public StmtAST {
    std::string Name;
    std::string Expression;
public:
    CTypeDeclAST(std::string Name, std::string Expression)
        : Name(std::move(Name)), Expression(std::move(Expression)) {}
    const std::string &getName() const { return Name; }
    const std::string &getExpression() const { return Expression; }
};

// ---- User-defined type declarations (CIE 9618 §4.1) ----

// One field of a record TYPE body. Plain data, not a statement node, so the
// emitStmt chain can never execute a field by accident.
struct RecordFieldDecl {
    std::string Name;
    std::string TypeName;
    bool IsPublic = true; // TYPE fields are always public.
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

// CLASS <name> with default-public or explicit PUBLIC/PRIVATE members, ending in ENDCLASS.
// Storage and value semantics are inherited from record declarations.
class ClassTypeDeclAST : public RecordTypeDeclAST {
    std::vector<std::unique_ptr<FunctionDefAST>> Methods;
public:
    ClassTypeDeclAST(const std::string &Name, std::vector<RecordFieldDecl> Fields,
                     std::vector<std::unique_ptr<FunctionDefAST>> Methods)
        : RecordTypeDeclAST(Name, std::move(Fields)), Methods(std::move(Methods)) {}
    const auto &getMethods() const { return Methods; }
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

public:
    DesignatorExprAST(const std::string &BaseName,
                      std::vector<DesignatorAccess> Accesses,
                      int Line)
        : ExprAST(Line), BaseName(BaseName), Accesses(std::move(Accesses)) {}

    const std::string &getBaseName() const { return BaseName; }
    const std::vector<DesignatorAccess> &getAccesses() const { return Accesses; }
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
