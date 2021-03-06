// Copyright (c) 2012, Rasmus Andersson. All rights reserved. Use of this source
// code is governed by a MIT-style license that can be found in the LICENSE file.
#include <llvm/Support/CallSite.h>
#include <llvm/Analysis/Verifier.h>

#include "_VisitorImplHeader.h"

Value *Visitor::codegenFunction(ast::Function *node,
                                const Text& symbol,
                                std::string name,  // = ""
                                Type* returnType,  // = 0
                                Value* returnValue // = 0
                                )
{
  DEBUG_TRACE_LLVM_VISITOR;

  bool inferredReturnType = false;
  
  // Figure out return type (unless it's been overridden by returnType) if
  // the interface declares the return type.
  ast::FunctionType* astFT = node->functionType();
  if (returnType != 0) {
    const ast::Type* astReturnType = ASTTypeForIRType(returnType);
    if (astReturnType == 0) return 0;
    astFT->setResultType(astReturnType);
  } else {
    returnType = returnTypeForFunctionType(astFT);
    if (returnType == 0) return 0;
    inferredReturnType = returnType->isVoidTy();
  }
  // } else if (!astFT->resultTypeIsUnknown()) {
  //   returnType = IRTypeForASTType(astFT->resultType());
  //   if (returnType == 0)
  //     return error("Unable to transcode return type from AST to IR");
  // } else {
  //   inferredReturnType = true;
  //   returnType = builder_.getVoidTy();
  //   //returnType = Type::getLabelTy(getGlobalContext());
  // }
  
  // Generate interface
  Function* F = codegenFunctionType(astFT, name, returnType);
  if (F == 0) return 0;

  // If the return type is inferred, register us in the current BlockScope
  //BlockScope* bs = inferredReturnType->blockScope();

  // Setup function body
  BasicBlock *BB = BasicBlock::Create(getGlobalContext(), "", F);
  assert(BB != 0);
  BlockScope bs(*this, BB);
  // BB->getParent() -> Function

  // Export the current function into the bs
  if (!symbol.empty())
    bs.setFunctionSymbolTarget(symbol, node->functionType(), F->getFunctionType(), F);
  
  // setSymbol for arguments, and alloca+store if mutable
  ast::VariableList *args = node->functionType()->args();
  if (args) {
    unsigned i = 0;
    for (Function::arg_iterator AI = F->arg_begin(); i != args->size(); ++AI, ++i) {
      Argument& arg = *AI;
      ast::Variable* var = (*args)[i];
      
      if (var->isMutable()) {
        // Store mutable value using a STORE instruction (SSA w/ pointers)
      
        // Create an alloca for this variable.
        IRBuilder<> TmpB(&F->getEntryBlock(), F->getEntryBlock().begin());
        AllocaInst *Alloca = TmpB.CreateAlloca(arg.getType(), 0, arg.getName());

        // Store the initial value into the alloca.
        builder_.CreateStore(&arg, Alloca);
      
        // Register or override the value ref with the alloca inst
        bs.setSymbolTarget(var->name(), var->type(), Alloca, /* isMutable = */true);

      } else {
        // Register the symbol
        bs.setSymbolTarget(var->name(), var->type(), &arg, /* isMutable = */false);
      }
      
    }
  }
  
  // Generate block code
  Value *lastValue = codegenBlock(node->body());
  if (lastValue == 0) {
    F->eraseFromParent();
    return 0;
  }
  
  // Unless the return value has been overridden, set it from the last value
  if (returnValue == 0)
    returnValue = lastValue;

  // Emit block terminator (return)
  if (builder_.CreateRet(returnValue) == 0) {
    F->eraseFromParent();
    return error("Failed to build terminating return instruction");
  }

  // if (!inferredReturnType) {
  //   if (returnValue->getType() != returnType) {
  //     // Oops. The parser failed to predict the correct result type.
  //     inferredReturnType = true;
  //   }
  // }
  
  // Return value changed -- generate func interface
  if (inferredReturnType) {
    returnType = returnValue->getType();

    //F->eraseFromParent();
    //return error("Not implemented: Function return type inference");

    // From: http://llvm.org/docs/doxygen/html/DeadArgumentElimination_8cpp_source.html
    
    // Derive new function type with different return type but same parameters
    // TODO: We could use the same code here to update inferred parameter types
    FunctionType *FTy = F->getFunctionType();
    std::vector<Type*> params(FTy->param_begin(), FTy->param_end());
    FunctionType *NFTy = FunctionType::get(returnType, params, false);
    
    // Create the new function body and insert it into the module
    Function *NF = Function::Create(NFTy, F->getLinkage());
    NF->copyAttributesFrom(F);
    F->getParent()->getFunctionList().insert(F, NF);
    NF->takeName(F);

    // Loop over all of the callers of the function, transforming the call sites
    // to pass in a smaller number of arguments into the new function.
    //
    /*std::vector<Value*> Args;
    size_t NumArgs = params.size();
    while (!F->use_empty()) {
      std::cerr << "MOS" << std::endl;
      CallSite CS(F->use_back());
      Instruction *Call = CS.getInstruction();

      // Pass all the same arguments.
      Args.assign(CS.arg_begin(), CS.arg_begin() + NumArgs);

      // Drop any attributes that were on the vararg arguments.
      AttrListPtr PAL = CS.getAttributes();
      if (!PAL.isEmpty() && PAL.getSlot(PAL.getNumSlots() - 1).Index > NumArgs) {
        SmallVector<AttributeWithIndex, 8> AttributesVec;
        for (unsigned i = 0; PAL.getSlot(i).Index <= NumArgs; ++i)
          AttributesVec.push_back(PAL.getSlot(i));
        if (Attributes FnAttrs = PAL.getFnAttributes())
          AttributesVec.push_back(AttributeWithIndex::get(~0, FnAttrs));
        PAL = AttrListPtr::get(AttributesVec.begin(), AttributesVec.end());
      }

      Instruction *New;
      if (InvokeInst *II = dyn_cast<InvokeInst>(Call)) {
        New = InvokeInst::Create(NF, II->getNormalDest(), II->getUnwindDest(), Args, "", Call);
        cast<InvokeInst>(New)->setCallingConv(CS.getCallingConv());
        cast<InvokeInst>(New)->setAttributes(PAL);
      } else {
        New = CallInst::Create(NF, Args, "", Call);
        cast<CallInst>(New)->setCallingConv(CS.getCallingConv());
        cast<CallInst>(New)->setAttributes(PAL);
        if (cast<CallInst>(Call)->isTailCall())
          cast<CallInst>(New)->setTailCall();
      }
      New->setDebugLoc(Call->getDebugLoc());

      Args.clear();

      if (!Call->use_empty())
        Call->replaceAllUsesWith(New);

      New->takeName(Call);

      // Finally, remove the old call from the program, reducing the use-count of
      // F.
      Call->eraseFromParent();
    }*/
    
    // Since we have now created the new function, splice the body of the old
    // function right into the new function, leaving the old rotting hulk of the
    // function empty.
    NF->getBasicBlockList().splice(NF->begin(), F->getBasicBlockList());

    // Loop over the argument list, transferring uses of the old arguments over to
    // the new arguments, also transferring over the names as well.  While we're at
    // it, remove the dead arguments from the DeadArguments list.
    for (Function::arg_iterator I = F->arg_begin(), E = F->arg_end(),
         I2 = NF->arg_begin(); I != E; ++I, ++I2) {
      // Move the name and users over to the new version.
      I->replaceAllUsesWith(I2);
      I2->takeName(I);
    }
    
    // Swap F and erase the old function.
    Function *OF = F;
    F = NF;
    OF->eraseFromParent();

  } else if (returnValue->getType() != returnType) {
    // Return type should match the actual type, but it doesn't.
    return error(std::string("Function return type (")
                 + llvmTypeToString(*returnValue->getType())
                 + ") is incompatible with the actual result type ("
                 + llvmTypeToString(*returnType)
                 + ")");
  }

  // Update the AST node if the return type is unknown
  if (   node->functionType()->resultType() == 0
      || node->functionType()->resultType()->isUnknown()) {
    const ast::Type* astReturnType = ASTTypeForIRType(returnType);
    if (astReturnType == 0) {
      //returnType->dump();
      return error(R_FMT("Unable to transcode return type from IR "
                         << llvmTypeToString(*returnType) << " to AST"));
    }
    node->functionType()->setResultType(astReturnType);
  }
  
  // Check return type against function's declared return type
  // if (F->getFunctionType()->isValidReturnType(returnType) == false) {
  //   F->eraseFromParent();
    
  //   std::cerr << "F " << name << std::endl;
  //   std::cerr << "LT: "; if (!returnValue->getType()) std::cerr << "<nil>" << std::endl; else {
  //     returnValue->getType()->dump(); std::cerr << std::endl; }
  //   std::cerr << "RT: "; if (!returnType) std::cerr << "<nil>" << std::endl; else {
  //     returnType->dump(); std::cerr << std::endl; }
    
  //   return error("Function return type mismatch (does not match declared return type)");
  // }

  // Validate the generated code, checking for consistency.
  verifyFunction(*F);

  return F;
}

#include "_VisitorImplFooter.h"
