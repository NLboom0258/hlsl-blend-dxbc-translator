// intrinsics.h - intrinsic return-type resolution
#pragma once

#include <string>
#include <vector>

#include "ast.h"
#include "symbols.h"
#include "types.h"

namespace hb {

// Return dimension of an intrinsic call (for type inference).
int intrinsic_return_dim(const std::string& name, const std::vector<Expr*>& args,
                         SymbolTable& sym);
// Full return type.
Type intrinsic_return_type(const std::string& name, const std::vector<Expr*>& args,
                           SymbolTable& sym);

} // namespace hb
