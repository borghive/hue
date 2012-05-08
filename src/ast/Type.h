#ifndef RSMS_AST_TYPE_DECLARATION_H
#define RSMS_AST_TYPE_DECLARATION_H
#include "../Text.h"
#include <string>
#include <vector>

namespace rsms { namespace ast {

class ArrayType;

// Declares a type, e.g. Float (double precision number) or [Int] (list of integer numbers)
class Type {
public:
  enum TypeID {
    Unknown = 0,
    Named,
    Float,
    Int,
    Char,
    Byte,
    Bool,
    Func,
    Array,
  };
  
  Type(TypeID typeID) : typeID_(typeID) {}
  Type(const Text& name) : typeID_(Named), name_(name) {}
  virtual ~Type() {}
  
  inline const TypeID& typeID() const { return typeID_; }
  inline const Text& name() const { return name_; }
  
  virtual std::string toString() const {
    switch (typeID_) {
      case Named: return name_.UTF8String();
      case Float: return "Float";
      case Int: return "Int";
      case Char: return "Char";
      case Byte: return "Byte";
      case Bool: return "Bool";
      case Func: return "func";
      default: return "?";
    }
  }
private:
  TypeID typeID_;
  Text name_;
};


class ArrayType : public Type {
public:
  ArrayType(Type* type) : Type(Array), type_(type) {}
  inline const Type* type() const { return type_; }
  
  virtual std::string toString() const {
    std::string s("[");
    s += (type_ ? type_->toString() : "<nil>");
    s += "]";
    return s;
  }
private:
  Type* type_;
};


typedef std::vector<Type*> TypeList;

}} // namespace rsms::ast
#endif  // RSMS_AST_TYPE_DECLARATION_H
