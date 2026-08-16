// symbols.cpp
#include "symbols.h"

#include <algorithm>
#include <cstdlib>

namespace hb {

SymbolTable::SymbolTable(int min_reg) : min_reg_(min_reg), next_reg_(min_reg) {
    scopes_.emplace_back();  // global scope
}

Symbol* SymbolTable::lookup(const std::string& name) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto f = it->find(name);
        if (f != it->end()) return &f->second;
    }
    return nullptr;
}

bool SymbolTable::exists(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it)
        if (it->count(name)) return true;
    return false;
}

Symbol* SymbolTable::lookup_current_scope(const std::string& name) {
    auto f = scopes_.back().find(name);
    return f == scopes_.back().end() ? nullptr : &f->second;
}

Symbol* SymbolTable::find_by_reg(const std::string& reg_base) {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        for (auto& kv : *it) {
            if (kv.second.reg == reg_base) return &kv.second;
        }
    }
    return nullptr;
}

std::string SymbolTable::alloc_temp(const std::string& mask) {
    std::string reg_name;
    if (!free_list_.empty()) {
        reg_name = "r" + std::to_string(free_list_.back());
        free_list_.pop_back();
    } else {
        reg_name = "r" + std::to_string(next_reg_++);
    }
    allocated_bases_.insert(reg_name);
    all_bases_ever_.insert(reg_name);
    return reg_name + "." + mask;
}

void SymbolTable::free_temp(const std::string& reg_str) {
    size_t dot = reg_str.find('.');
    std::string base = (dot == std::string::npos) ? reg_str : reg_str.substr(0, dot);
    if (!base.empty() && base[0] == 'r' && allocated_bases_.count(base)) {
        allocated_bases_.erase(base);
        int n = std::atoi(base.c_str() + 1);
        if (std::find(free_list_.begin(), free_list_.end(), n) == free_list_.end())
            free_list_.push_back(n);
    }
}

void SymbolTable::declare(const std::string& name, const std::string& reg, const std::string& mask,
                          const Type& type, bool is_temp) {
    Symbol sym;
    sym.reg = reg;
    sym.mask = mask;
    sym.type = type;
    sym.is_temp = is_temp;
    scopes_.back()[name] = sym;
}

void SymbolTable::erase(const std::string& name) {
    scopes_.back().erase(name);
}

void SymbolTable::enter_scope() {
    scopes_.emplace_back();
}

void SymbolTable::exit_scope() {
    if (scopes_.size() <= 1) return;  // never pop the global scope
    for (const auto& kv : scopes_.back()) {
        if (kv.second.is_temp)
            free_temp(kv.second.reg);
    }
    scopes_.pop_back();
}

std::vector<std::string> SymbolTable::all_allocated_bases() const {
    std::vector<std::string> bases(all_bases_ever_.begin(), all_bases_ever_.end());
    std::sort(bases.begin(), bases.end());
    return bases;
}

} // namespace hb
