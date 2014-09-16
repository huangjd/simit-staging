#include "llvm_codegen.h"

#include <iostream>
#include <utility>

#include "llvm/IR/Constants.h"

using namespace std;
using namespace simit::internal;

namespace simit {
namespace internal {

namespace {
//llvm::ConstantInt* getInt32(int val) {
//  return llvm::ConstantInt::get(LLVM_CONTEXT, llvm::APInt(32, val, false));
//}
} // unnamed namespace

llvm::Type *llvmType(const simit::ComponentType type) {
  assert(isValidComponentType(type));
  switch (type) {
    case simit::ComponentType::INT:
      return LLVM_INT;
    case simit::ComponentType::FLOAT:
      return LLVM_DOUBLE;
  }
}

llvm::Type *llvmType(const ir::TensorType *type){
  assert(isValidComponentType(type->getComponentType()));
  switch (type->getComponentType()) {
    case simit::ComponentType::INT:
      return LLVM_INTPTR;
    case simit::ComponentType::FLOAT:
      return LLVM_DOUBLEPTR;
  }
}

llvm::Constant *llvmPtr(const simit::ir::Literal *literal) {
  assert(literal->getType()->isTensor());
  llvm::Constant *c = (sizeof(void*) == 4)
      ? llvm::ConstantInt::get(llvm::Type::getInt32Ty(LLVM_CONTEXT),
                               (int)(intptr_t)literal->getData())
      : llvm::ConstantInt::get(llvm::Type::getInt64Ty(LLVM_CONTEXT),
                               (intptr_t)literal->getData());
  llvm::Type *ctype = llvmType(tensorTypePtr(literal->getType()));
  llvm::Constant *cptr = llvm::ConstantExpr::getIntToPtr(c, ctype);
  return cptr;
}

simit::ComponentType simitType(const llvm::Type *type) {
  if (type->isPointerTy()) {
    type = type->getPointerElementType();
  }

  if (type->isDoubleTy()) {
    return simit::FLOAT;
  }
  else if (type->isIntegerTy()) {
    return simit::INT;
  }
  else {
    UNREACHABLE;
  }
}

namespace {
void llvmArgument(const std::shared_ptr<ir::Argument> &arg,
                  std::vector<std::string> *names,
                  std::vector<llvm::Type*> *types) {
  switch (arg->getType()->getKind()) {
    case ir::Type::Tensor:
      names->push_back(arg->getName());
      types->push_back(llvmType(tensorTypePtr(arg->getType())));
      break;
    case ir::Type::Set:
      ir::SetType *type = setTypePtr(arg->getType());

      // Emit one function argument per set field
      for (auto &field : type->getElementType()->getFields()) {
        names->push_back(arg->getName() + "." + field.first);
        types->push_back(llvmType(field.second.get()));
      }

      names->push_back("num_" + arg->getName());
      types->push_back(LLVM_INT32);
      break;
  }
}

void llvmArguments(const std::vector<std::shared_ptr<ir::Argument>> &arguments,
                   const std::vector<std::shared_ptr<ir::Result>> &results,
                   std::vector<std::string> *llvmArgNames,
                   std::vector<llvm::Type*> *llvmArgTypes) {

  // We don't need two llvm arguments for aliased simit argument/results
  std::set<std::string> argNames;

  for (auto &arg : arguments) {
    argNames.insert(arg->getName());
    llvmArgument(arg, llvmArgNames, llvmArgTypes);
  }

  for (auto &res : results) {
    if (argNames.find(res->getName()) != argNames.end()) {
      continue;
    }
    llvmArgument(res, llvmArgNames, llvmArgTypes);
  }
}

} // unnamed namespace


llvm::Function *
createPrototype(const std::string &name,
                const std::vector<std::shared_ptr<ir::Argument>> &arguments,
                const std::vector<std::shared_ptr<ir::Result>> &results,
                llvm::GlobalValue::LinkageTypes linkage,
                llvm::Module *module) {
  std::vector<std::string> llvmArgNames;
  std::vector<llvm::Type*> llvmArgTypes;
  llvmArguments(arguments, results, &llvmArgNames, &llvmArgTypes);
  assert(llvmArgNames.size() == llvmArgTypes.size());

  llvm::FunctionType *ft = llvm::FunctionType::get(LLVM_VOID, llvmArgTypes,
                                                   false);

  llvm::Function *f = llvm::Function::Create(ft, linkage, name, module);
  f->setDoesNotThrow();
  for (size_t i=0; i<f->getArgumentList().size(); ++i) {
    f->setDoesNotCapture(i+1);
  }

  llvm::Function::arg_iterator llvmArgIt = f->arg_begin();
  for (auto &llvmArgName : llvmArgNames) {
    llvmArgIt->setName(llvmArgName);
    ++llvmArgIt;
  }

  assert(llvmArgIt == f->arg_end());

  return f;
}

}} // namespace simit::internal
