#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
    int leftMaxArgs = this->stm1->MaxArgs();
    int rightMaxArgs = this->stm2->MaxArgs();
    if (leftMaxArgs >= rightMaxArgs){
        return leftMaxArgs;
    }
    else{
        return rightMaxArgs;
    }
}

Table *A::CompoundStm::Interp(Table *t) const {
    // TODO: put your code here (lab1).
    t = this->stm1->Interp(t);
    t = this->stm2->Interp(t);
    return t;
}

int A::AssignStm::MaxArgs() const {
    // TODO: put your code here (lab1).
    return this->exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
    // TODO: put your code here (lab1).
    IntAndTable* result = this->exp->Interp(t);
//    Table* newNode = new Table(this->id,result->i,t);
    t = t->Update(this->id,result->i);
    return t;
}

int A::PrintStm::MaxArgs() const {
    // TODO: put your code here (lab1).
    return this->exps->MaxArgs();
}

Table *A::PrintStm::Interp(Table *t) const {
    // TODO: put your code here (lab1).
    this->exps->Interp(t);
    return t;
}

int A::NumExp::MaxArgs() const {
    return 0;
}

IntAndTable* A::NumExp::Interp(Table* t) const {
    return new IntAndTable(this->num,t);
}

int A::IdExp::MaxArgs() const {
    return 0;
}

IntAndTable* A::IdExp::Interp(Table* t) const {
    int result = t->Lookup(this->id);
    return new IntAndTable(result,t);
}

int A::OpExp::MaxArgs() const {
    return 0;
}

IntAndTable* A::OpExp::Interp(Table* t) const {
   IntAndTable* arg1 = this->left->Interp(t);
   IntAndTable* arg2 = this->right->Interp(t);
   int result;
   switch (this->oper) {
   case 0:
       result = arg1->i + arg2->i;
       break;
   case 1:
       result = arg1->i - arg2->i;
       break;
   case 2:
       result = arg1->i * arg2->i;
       break;
   case 3:
       result = arg1->i / arg2->i;
   default:
       assert(false);
   }
   return new IntAndTable(result,t);
}

int A::EseqExp::MaxArgs() const {
    return this->stm->MaxArgs();
}

IntAndTable* A::EseqExp::Interp(Table* t) const {
  t = this->stm->Interp(t);
  IntAndTable* result = this->exp->Interp(t);
  return new IntAndTable(result->i,t);
}

int A::LastExpList::MaxArgs() const {
    return 1;
}

IntAndTable* A::LastExpList::Interp(Table* t) const {
    IntAndTable* result = this->exp->Interp(t);
    std::cout << result->i << std::endl;
    return new IntAndTable(result->i,t);
}

int A::PairExpList::MaxArgs() const {
    return 1 + this->tail->MaxArgs();
}

IntAndTable* A::PairExpList::Interp(Table* t) const {
    IntAndTable* result = this->exp->Interp(t);
    std::cout << result->i << " ";
    this->tail->Interp(t);
    return new IntAndTable(result->i,t);
}

int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}
}  // namespace A
