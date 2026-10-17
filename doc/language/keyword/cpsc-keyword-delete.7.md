% CPSC-KEYWORD-DELETE(7)

# NAME

cpsc-keyword-delete - destroy objects and release heap allocations

# SYNOPSIS

```text
CLASS ClassName
    PROCEDURE DELETE()
        statements
    ENDPROCEDURE
ENDCLASS
DELETE Object
DELETE Pointer^
DELETE Array[Index]
DELETE this
Object.DELETE()
CALL Object.DELETE()
```

# DESCRIPTION

Destructors, keyword DELETE and automatic destruction are compiler extensions; the Cambridge 2026 guide defines NEW constructors but no destructors. A destructor is a parameterless PROCEDURE named DELETE inside a CLASS. It may be PUBLIC or PRIVATE. Member names are case insensitive; only one DELETE is allowed per class. FUNCTION DELETE, global PROCEDURE DELETE and destructor parameters are errors.

An explicit `Object.DELETE()`, `CALL Object.DELETE()` or `this.DELETE()` is an ordinary method call. It calls only the method, without automatically destroying fields or releasing memory. It does not end the object's automatic lifetime.

The statement `DELETE Object` requires an addressable class object. It first checks that the address is the start of an active tracked heap allocation, including allocations created by keyword NEW. Stack objects, internal array storage, interior addresses and previously released allocations fail with a source-line runtime diagnostic and an anonymous exception before any destructor runs. Address reuse can make a stale alias indistinguishable from a current allocation.

After validation, DELETE calls the object's DELETE method if present, then recursively destroys its class-valued fields in reverse declaration order, including through record fields, and releases the allocation through FREE. The variable and all aliases remain dangling. If destruction throws, memory release is not reached. Heap objects are never automatically destroyed.

Automatic destruction (RAII) applies to original storage created by DECLARE, including main-level declarations, arrays and records containing class values, and to BYVAL copies containing class values. Destruction occurs in reverse declaration order on RETURN, at the end of a function or procedure, and at normal program end. Array elements are destroyed in reverse flat element order. Block exit alone does not destroy these function-scoped lifetimes. A declaration executed again first destroys its previous live incarnation, then zeroes and constructs the original storage again.

A completed construction or BYVAL copy marks the object alive. Destruction clears this state before calling the method, so a throwing destructor is never retried automatically. BYVAL copies are shallow. NEW rebinding changes the variable's current address, while automatic destruction still targets its original storage; the heap object requires explicit DELETE.

Exception unwinding destroys live objects in each function frame being left before the caller's CATCH runs. A CATCH inside the same function retains its objects until function exit. An uncaught exception reaching main's final handler skips destruction of main-level objects. If a destructor throws during unwinding, its exception propagates and interrupts the remaining cleanup instead of terminating as C++ would.

Ordinary method access rules apply to explicit, keyword and automatic destruction. A private DELETE makes destruction outside the owning class a compile error.

# EXAMPLE

```text
CLASS Item
    PROCEDURE DELETE()
        OUTPUT "destroyed"
    ENDPROCEDURE
ENDCLASS
DECLARE Object : Item
Object <- NEW Item()
DELETE Object
// Normal program end also destroys Object's original stack storage.
```

# SEE ALSO

[cpsc-keyword-class(7)](cpsc-keyword-class.7.md), [cpsc-keyword-declare(7)](cpsc-keyword-declare.7.md), [cpsc-keyword-try(7)](cpsc-keyword-try.7.md), [cpsc-function-free(7)](../function/cpsc-function-free.7.md).
