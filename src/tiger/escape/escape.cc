#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"
#include<iostream>
namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  /* TODO: Put your lab5 code here */
  int depth = 0;
  root_->Traverse(env, depth);
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  esc::EscapeEntry *t = env->Look(sym_);
  if (depth > t->depth_){
    *(t->escape_) = true;
  }
  return;
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  return;
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  subscript_->Traverse(env, depth);
  return;
}


void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  const auto &args = this->args_->GetList();

  for (auto it = args.begin(); it != args.end(); ++it){
    (*it)->Traverse(env,depth);
  }
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  const auto &fields = this->fields_->GetList();

  for (auto it = fields.begin(); it!=fields.end(); ++it){
    (*it)->exp_->Traverse(env,depth);
  }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  const auto &seq = this->seq_->GetList();

  for (auto it = seq.begin(); it!=seq.end(); ++it){
    (*it)->Traverse(env, depth);
  }
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if (elsee_){
    elsee_->Traverse(env, depth);
  }
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  test_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  // add var to env
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  
  // Traverse
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);
  body_->Traverse(env, depth);
  return;
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */

  env->BeginScope();

  const auto &decs = this->decs_->GetList();
  for (auto it = decs.begin(); it!=decs.end(); ++it){
    (*it)->Traverse(env, depth);
  }

  body_->Traverse(env, depth);
  
  env->EndScope();
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  const auto &funDecList = this->functions_->GetList();

  // There are multiple function definitions
  for (auto it = funDecList.begin(); it!=funDecList.end(); ++it){
    // For each function definition, resolve its parameters and body
    env->BeginScope();
    depth++;

    auto funDec = *it;
    const auto &params = funDec->params_->GetList();
    for (auto it2 = params.begin(); it2!=params.end(); ++it2){
      Field *field = *it2;
      field->escape_ = false;
      env->Enter(field->name_, new esc::EscapeEntry(depth, &field->escape_));
    }

    funDec->body_->Traverse(env, depth);
    
    env->EndScope();
    depth--;
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
  escape_ = false;
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  init_->Traverse(env, depth);
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  /* TODO: Put your lab5 code here */
}

} // namespace absyn
