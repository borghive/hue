#include <llvm/Instructions.h>

#include "_VisitorImplHeader.h"

Value* Visitor::createNewLocalSymbol(ast::Variable *variable, Value *rhsValue) {
  Type* rhsOriginalType = rhsValue->getType();
  Value* V = rhsValue;
  const bool storageTypeIsKnown = variable->hasUnknownType() == false;
  bool requireAlloca = false;
  
  // If storage type is explicit/known --and-- RHS is a primitive: cast if needed
  if (storageTypeIsKnown && rhsOriginalType->isPrimitiveType()) {
    // Map AST type to LLVM type
    Type *storageType = IRTypeForASTTypeDecl(*variable->type());
    if (storageType == 0) return error("No encoding for AST type");
  
    // If storage type is different than value, attempt to convert the value
    if (storageType->getTypeID() != V->getType()->getTypeID()) {
      // RHS type is different that storage type
      Type* VT = V->getType();
      
      // Require an alloca since we are replacing 
      requireAlloca = true;
    
      // Numbers: cast to storage
      if (storageType->isDoubleTy() && VT->isIntegerTy()) { // Promote Int constant to Float
        //warning("Implicit conversion of integer value to floating point storage");
        V = builder_.CreateSIToFP(V, storageType, "casttmp");
      } else if (storageType->isIntegerTy() && VT->isDoubleTy()) { // Demote Float constant to Int
        // warning("Implicit conversion of floating point value to integer storage");
        // V = builder_.CreateFPToSI(V, storageType, "casttmp");
        return error("Illegal conversion from floating point number to integer");
      } else {
        return error("Symbol/variable type is different than value type");
      }
    }
  }
  
  // Alloca+STORE if RHS is not a primitive --or-- the var is mutable
  if (V != rhsValue || variable->isMutable()) {
    rlog("Alloca+STORE variable \"" << variable->name() << "\"");
    V = createAllocaAndStoreValue(V, variable->name());
    // Note: alloca optimization passes will take care of removing any unneeded
    // allocas that are the result of constant inlining. E.g.
    //   x = 3.4
    //   y = 3
    //   z Int = y + x
    //  
    // Would result in the following IR:
    //
    //   %z = alloca i64
    //   store i64 6, i64* %z
    //
    // Which, after an alloca optimization pass, will simply reference the constant i64 6
    //
  } else {
    // Type* T = V->getType();
    rlog("REF variable \"" << variable->name() << "\"");
    // T->dump();
    // V->dump();
    // if (T->isPointerTy() && T->getNumContainedTypes() == 1 && T->getContainedType(0)->isFunctionTy()) {
    //   rlog("is a pointer to a func");
    // }
  }
  
  blockScope()->setSymbol(variable->name(), V, variable->isMutable());
  
  return V;
}


// Assignment
Value *Visitor::codegenAssignment(const ast::Assignment* node) {
  DEBUG_TRACE_LLVM_VISITOR;
  
  // TODO: Make assignments that occur in the module block
  // global. That way we can also set their linkage to external
  // in order to have them exported, if marked for export. Use GlobalValue
  // for constant symbols and GlobalVariable for variables.
  
  // Get the list of variables
  const ast::VariableList *variables = node->variables();
  assert(variables->size() == 1); // TODO: support >1 LHS variable
  
  
  // Codegen the RHS.
  Value *rhsValue;
  
  if (node->rhs()->type == ast::Node::TFunction) {
    // Generate a globally unique mangled name
    const std::string& varName = (*variables)[0]->name();
    std::string mangledName = uniqueMangledName(varName);
    
    // Generate function
    rhsValue = codegenFunction(static_cast<const ast::Function*>(node->rhs()), mangledName);

  } else if (node->rhs()->type == ast::Node::TExternalFunction) {
    rhsValue = codegenExternalFunction(static_cast<const ast::ExternalFunction*>(node->rhs()));
  } else {
    rhsValue = codegen(node->rhs());
  }
  if (rhsValue == 0) return 0;
  
  
  // For each variable
  ast::VariableList::const_iterator it = variables->begin();
  for (; it < variables->end(); it++) {
    ast::Variable *variable = (*it);
  
    // Look up the name in this direct scope.
    const Symbol& symbol = lookupSymbol(variable->name(), false);
    Value *existingValue = symbol.value;
    
    if (existingValue == 0) {  // New symbol
      rhsValue = createNewLocalSymbol(variable, rhsValue);
      if (rhsValue == 0) return 0;

    } else {  // Existing symbol
      if (symbol.isMutable) {
        if (!AllocaInst::classof(existingValue)) {
          return error("not implemented: promote mutatable ref to alloca");
        } else {
          // STORE the value
          llvm::StoreInst* si = builder_.CreateStore(rhsValue, static_cast<AllocaInst*>(existingValue));
          rhsValue = si;
        }
      } else {
        rlog(variable->name() << " is not mutable");
        return error("Trying to mutate an immutable value");
      }
    }
  
  }

  return rhsValue;
}

#include "_VisitorImplFooter.h"
