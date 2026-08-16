// function_lib.h - HLSL function library parser (HLSLFunctionImport / lib.txt)
#pragma once

#include <map>
#include <string>

#include "ast.h"

namespace hb {

// Parse a function library source string into a name -> FunctionDef map.
// Skips malformed definitions. Comments must be stripped by the caller.
bool parse_function_library(const std::string& source, std::map<std::string, FunctionDef>& out);

} // namespace hb
