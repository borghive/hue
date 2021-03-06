// Copyright (c) 2012, Rasmus Andersson. All rights reserved. Use of this source
// code is governed by a MIT-style license that can be found in the LICENSE file.
#include "_VisitorImplHeader.h"



Function *Visitor::codegenFunctionType(ast::FunctionType *astFT,
                                       std::string name, // = "",
                                       Type *returnType  // = 0
                                       ) {
  DEBUG_TRACE_LLVM_VISITOR;
  
  // No return type means void
  if (returnType == 0) {
    returnType = builder_.getVoidTy();
  }
  
  // Get LLVM function type
  FunctionType *FT = getLLVMFuncTypeForASTFuncType(astFT, returnType);
  if (FT == 0) return 0;
  
  if (!FT) return (Function*)error("Failed to create function type");

  // Create function
  GlobalValue::LinkageTypes linkageType = GlobalValue::PrivateLinkage;
  if (astFT->isPublic()) {
    linkageType = GlobalValue::ExternalLinkage;
  }
  Function *F = Function::Create(FT, linkageType, name, module_);
  if (!F) return (Function*)error("Failed to create function");
  
  // No unwinding
  F->setDoesNotThrow();

  ast::VariableList *argVars = astFT->args();

  // If F conflicted, there was already something named 'Name'.  If it has a
  // body, don't allow redefinition or reextern.
  if (F->getName() != name) {
    // Delete the one we just made and get the existing one.
    F->eraseFromParent();
    F = module_->getFunction(name);
  
    // If F already has a body, reject this.
    if (!F->empty()) {
      return (Function*)error("redefinition of a concrete function");
      return 0;
    }
  
    // If F took a different number of args, reject.
    if ( (argVars == 0 && F->arg_size() != 0) || (argVars != 0 && F->arg_size() != argVars->size()) ) {
      return (Function*)error("redefinition of a function with different arguments");
    }
  }

  // Set names for all arguments.
  if (argVars) {
    unsigned i = 0;
    for (Function::arg_iterator AI = F->arg_begin(); i != argVars->size(); ++AI, ++i) {
      const Text& argName = (*argVars)[i]->name();
      AI->setName(argName.UTF8String());
    }
  }

  return F;
}

#include "_VisitorImplFooter.h"
