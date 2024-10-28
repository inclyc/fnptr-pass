#pragma once

#include <llvm/IR/Module.h>

#include <set>
#include <vector>

using FunctionSet = std::set<const llvm::Function *>;
using FunctionSetMap = std::map<const llvm::Value *, FunctionSet>;

/// Stores all possible function for each value.
FunctionSetMap analyzeFunctions(const llvm::Module &M);
