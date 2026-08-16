// symbols.h - symbol table + temp register allocator
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "types.h"

namespace hb {

struct Symbol {
    std::string reg;   // base register: "r12", "cb5[0]", "v0", "t3", "s0"
    std::string mask;  // declared default mask: "xyz"
    Type type;
    bool is_temp = false;   // allocated temp (rN managed by allocator)
    bool is_texture = false; // t-slot alias (HLSLTexture)
    bool is_sampler = false; // s-slot alias (HLSLSampler)
};

class SymbolTable {
public:
    explicit SymbolTable(int min_reg);

    Symbol* lookup(const std::string& name);
    bool exists(const std::string& name) const;
    // Lookup only in the current (innermost) scope.
    Symbol* lookup_current_scope(const std::string& name);
    // Find a variable bound to the given register base (e.g. "r5"), innermost scope.
    Symbol* find_by_reg(const std::string& reg_base);

    // Allocate a temp register with the given writemask. Returns "rN.mask".
    std::string alloc_temp(const std::string& mask);
    // Free a temp register (accepts "rN.mask" or "rN").
    void free_temp(const std::string& reg_str);

    // Declare a variable binding (temp or external register) in the current scope.
    void declare(const std::string& name, const std::string& reg, const std::string& mask,
                 const Type& type, bool is_temp);
    // Remove a variable (used for function-local cleanup).
    void erase(const std::string& name);

    // Scope management (for function expansion). enter_scope pushes a new
    // scope; exit_scope pops it and frees temp registers of variables declared
    // within that scope.
    void enter_scope();
    void exit_scope();

    const std::set<std::string>& allocated_bases() const { return allocated_bases_; }
    int next_reg() const { return next_reg_; }
    int min_reg() const { return min_reg_; }

    // All bases that have been allocated at least once (for HLSLInit of the
    // full working set).
    std::vector<std::string> all_allocated_bases() const;

private:
    int min_reg_;
    int next_reg_;
    std::vector<int> free_list_;       // reusable register numbers (LIFO)
    std::vector<std::map<std::string, Symbol>> scopes_;  // scope stack (innermost last)
    std::set<std::string> allocated_bases_;    // currently-allocated rN bases
    std::set<std::string> all_bases_ever_;     // every rN base ever allocated
};

} // namespace hb
