#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"


#define DEFAULT_TYPE type::IntTy::Instance()

namespace absyn {

void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  root_->SemAnalyze(venv, tenv, 0, errormsg);
  return;
}

type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry)== typeid(env::VarEntry)){
    return (static_cast<env::VarEntry *>(entry))->ty_->ActualTy();
  }
  else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
  }

  return DEFAULT_TYPE;
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*ty) != typeid(type::RecordTy)){
    errormsg->Error(pos_, "not a record type");
  }
  else {
    type::RecordTy *rec = static_cast<type::RecordTy *>(ty);
    type::FieldList *fields = rec->fields_;
    std::list<type::Field *>* list = &fields->GetList();

    for (auto iter = list->begin(); iter != list->end(); ++iter){
      if ((*iter)->name_ == sym_){
        return (*iter)->ty_->ActualTy();
      }
    }

    errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
    return DEFAULT_TYPE;
  }
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   int labelcount,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *var_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *exp_ty = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*var_ty) != typeid(type::ArrayTy)){
    errormsg->Error(pos_, "array type required");
    return DEFAULT_TYPE;
  }
  else if (typeid(*exp_ty) != typeid(type::IntTy)){
    errormsg->Error(pos_, "array index must be integer");
    return DEFAULT_TYPE;
  }

  return ((type::ArrayTy*) var_ty)->ty_->ActualTy();
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return var_->SemAnalyze(venv, tenv, labelcount, errormsg);
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  env::EnvEntry *entry = venv->Look(func_);
  if (!entry || typeid(*entry)!=typeid(env::FunEntry)){
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
    return type::VoidTy::Instance();
  }

  type::TyList *formals = ((env::FunEntry*) entry)->formals_;
  const std::list<type::Ty *> formallist = formals->GetList();
  const std::list<Exp *> arglist = args_->GetList();

  auto iter1 = formallist.begin();
  auto iter2 = arglist.begin();

  for (; iter1!=formallist.end() && iter2!=arglist.end(); ++iter1,++iter2){
    type::Ty *ty = (*iter2)->SemAnalyze(venv, tenv, labelcount, errormsg);

    if ((*iter1)->ActualTy()!=ty->ActualTy()){
      errormsg->Error((*iter2)->pos_, "para type mismatch");
      return type::VoidTy::Instance();
    }
  }

  if (iter1 != formallist.end()){
    errormsg->Error(pos_, "too few params in function %s", func_->Name().c_str());
    return type::VoidTy::Instance();
  }
  else if (iter2 != arglist.end()){
    errormsg->Error(pos_, "too many params in function %s", func_->Name().c_str());
    return type::VoidTy::Instance();
  }

  return ((env::FunEntry*) entry)->result_->ActualTy();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *left_ty = left_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *right_ty = right_->SemAnalyze(venv, tenv, labelcount, errormsg);

  switch (oper_)
  {
    case Oper::PLUS_OP:
    case Oper::MINUS_OP:
    case Oper::TIMES_OP:
    case Oper::DIVIDE_OP:
    case Oper::AND_OP:
    case Oper::OR_OP:{
      if (typeid(*left_ty) != typeid(type::IntTy)) {
        errormsg->Error(left_->pos_, "integer required");
      }
      else if (typeid(*right_ty) != typeid(type::IntTy)) {
        errormsg->Error(right_->pos_, "integer required");
      }
      break;
    }
    default: {
      if (!left_ty->IsSameType(right_ty)) {
        errormsg->Error(pos_, "same type required");
      }
      break;
    }
  }
  return DEFAULT_TYPE;
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
    return DEFAULT_TYPE;
  }
  else {
    ty = ty->ActualTy();
    if (typeid(*ty) != typeid(type::RecordTy)) {
      errormsg->Error(pos_, "not a record type");
      return DEFAULT_TYPE;
    }
    else {
      type::RecordTy *rec = static_cast<type::RecordTy *>(ty);
      type::FieldList *fields = rec->fields_;

      std::list<absyn::EField *> recordExpList = fields_->GetList();
      std::list<type::Field *> recordDecList = fields->GetList();

      auto iter1 = recordExpList.begin();
      for (;iter1 != recordExpList.end(); ++iter1){
        type::Field *field = nullptr;
        for (auto iter2 = recordDecList.begin(); iter2!=recordDecList.end(); ++iter2){
          if ((*iter2)->name_ == (*iter1)->name_){
            field = *iter2;
            break;
          }
        }

        if (!field){
          errormsg->Error((*iter1)->exp_->pos_, "field name doesn't exist");
          return DEFAULT_TYPE;
        }

        type::Ty * oneFieldTy = (*iter1)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
        if (!oneFieldTy->IsSameType(field->ty_)) {
          errormsg->Error((*iter1)->exp_->pos_, "unmatched assign exp");
          return DEFAULT_TYPE;
        }
      }
      return ty;
    }
  }
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  const std::list<Exp *> seqlist = seq_->GetList();

  type::Ty *ty = type::VoidTy::Instance();
  for (auto iter1 = seqlist.begin(); iter1!= seqlist.end(); ++iter1){
    ty = (*iter1)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  return ty;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *left_ty = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *right_ty = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (typeid(*var_) == typeid(SimpleVar)){
    SimpleVar *svar = static_cast<SimpleVar *>(var_);
    env::EnvEntry *entry = venv->Look(svar->sym_);
    if (entry->readonly_){
      errormsg->Error(svar->pos_, "loop variable can't be assigned");
    }
  }

  if (!left_ty->IsSameType(right_ty)) {
    errormsg->Error(pos_, "unmatched assign exp");
  }

  return type::VoidTy::Instance();
}


type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *testTy, *thenTy, *elseTy;

  testTy = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!testTy || !testTy->IsSameType(type::IntTy::Instance())){
    errormsg->Error(test_->pos_, "if test must be integer");
  }

  thenTy = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!elsee_){
    if (typeid(*thenTy) != typeid(type::VoidTy)){
      errormsg->Error(then_->pos_, "if-then exp's body must produce no value");
    }
  }
  else {
    elseTy = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!thenTy->IsSameType(elseTy)) {
      errormsg->Error(then_->pos_, "then exp and else exp type mismatch");
    }
    return thenTy->ActualTy();
  }
  return type::VoidTy::Instance();
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *testTy = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *bodyTy = body_->SemAnalyze(venv, tenv, labelcount+1, errormsg);

  if (typeid(*testTy)!=typeid(type::IntTy)){
    errormsg->Error(test_->pos_, "integer required");
  }
  if (typeid(*bodyTy) != typeid(type::VoidTy)){
    errormsg->Error(body_->pos_, "while body must produce no value");
  }

  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *lo_Ty = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hi_Ty = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typeid(*lo_Ty)!=typeid(type::IntTy)){
    errormsg->Error(lo_->pos_, "for exp's range type is not integer");
  }
  if (typeid(*hi_Ty)!=typeid(type::IntTy)){
    errormsg->Error(hi_->pos_, "for exp's range type is not integer");
  }

  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance(), true));
  type::Ty *bodyTy = body_->SemAnalyze(venv, tenv, labelcount+1, errormsg);
  venv->EndScope();
  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  if (labelcount==0){
    errormsg->Error(pos_, "break is not inside any loop");
  }
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  venv->BeginScope();
  tenv->BeginScope();
  const std::list<Dec *> declist = decs_->GetList();
  for (auto iter = declist.begin(); iter!=declist.end(); ++iter){
    (*iter)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }

  type::Ty *ty;
  if (!body_){
    ty = type::VoidTy::Instance();
  }
  else {
    ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  venv->EndScope();
  tenv->EndScope();
  return ty;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(typ_);
  if (!ty){
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
  }
  else {
    ty = ty->ActualTy();
    if (typeid(*ty)!=typeid(type::ArrayTy)){
      errormsg->Error(pos_, "not an array type");
    }
    else {
      type::ArrayTy *array_ty = (type::ArrayTy *)ty;

      ty = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
      if (typeid(*ty)!=typeid(type::IntTy)){
        errormsg->Error(size_->pos_, "integer required");
      }

      ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
      if (!ty->IsSameType(array_ty->ty_)){
        errormsg->Error(pos_, "type mismatch");
      }

      return array_ty;
    }

  }
  return DEFAULT_TYPE;
}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             int labelcount, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  const std::list<FunDec *> function_list = functions_->GetList();

  // first pass through, check function declaration
  for (auto iter=function_list.begin(); iter!=function_list.end(); ++iter){
    FunDec *fun = *iter;
    if (venv->Look(fun->name_)) {
      errormsg->Error(pos_, "two functions have the same name");
    }

    FieldList *params = fun->params_;
    type::TyList *formals = params->MakeFormalTyList(tenv, errormsg);
    if (fun->result_){
      type::Ty *result_ty = tenv->Look(fun->result_);
      venv->Enter(fun->name_, new env::FunEntry(formals, result_ty));
    }
    else {
      venv->Enter(fun->name_, new env::FunEntry(formals, type::VoidTy::Instance()));
    }

  }

  // second pass through, check function body
  for (auto iter=function_list.begin(); iter!=function_list.end(); ++iter){
    FunDec *fun = *iter;
    env::FunEntry *funEntry = (env::FunEntry *)venv->Look(fun->name_);
    type::TyList *formals = funEntry->formals_;

    venv->BeginScope();

    std::list<Field *> paramList = fun->params_->GetList();
    std::list<type::Ty *> formalList = formals->GetList();

    auto param = paramList.begin();
    auto formal = formalList.begin();
    for (; formal != formalList.end(); ++formal, ++param){
      venv->Enter((*param)->name_, new env::VarEntry(*formal));
    }

    type::Ty *resTy = fun->body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (!resTy->IsSameType(funEntry->result_)){
      if (funEntry->result_->IsSameType(type::VoidTy::Instance())){
        errormsg->Error(pos_, "procedure returns value");
      }
      else {
        errormsg->Error(pos_, "function return type mismatch");
      }
    }
    venv->EndScope();
  }

}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                        err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // two variable have the same name
  if (venv->Look(var_)){
    return;
  }

  type::Ty *exp_ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (typ_) {
    type::Ty *var_ty = tenv->Look(typ_);
    if (!var_ty) {
      errormsg->Error(pos_, "undefined type of %s", typ_->Name().c_str());
    }
    if (!var_ty->IsSameType(exp_ty)){
      errormsg->Error(pos_, "type mismatch");
    }
    venv->Enter(var_, new env::VarEntry(var_ty));
  }
  else {
    if (exp_ty->IsSameType(type::NilTy::Instance()) && typeid(*(exp_ty->ActualTy())) != typeid(type::RecordTy)){
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
    venv->Enter(var_, new env::VarEntry(exp_ty));
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, int labelcount,
                         err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  // First scan, add type declaration
  const std::list<NameAndTy *> ty_list = types_->GetList();
  for (auto iter = ty_list.begin(); iter!=ty_list.end(); iter++){
    NameAndTy *nameandty = *iter;
    if (tenv->Look(nameandty->name_)) {
      errormsg->Error(pos_, "two types have the same name");
    }
    else {
      tenv->Enter(nameandty->name_, new type::NameTy(nameandty->name_, nullptr));
    }
  }

  // Second scan, parsing declaration expression
  for (auto iter = ty_list.begin(); iter!=ty_list.end(); ++iter){
    NameAndTy *nameandty = *iter;
    type::NameTy *nameTy = (type::NameTy *)tenv->Look(nameandty->name_);
    nameTy->ty_ = nameandty->ty_->SemAnalyze(tenv, errormsg);
  }

  // Third scan, check recursive name type dec
  bool hasCircle = false;
  for (auto iter = ty_list.begin(); iter!=ty_list.end(); ++iter){
    NameAndTy *nameandty = *iter;
    type::NameTy *nameTy = (type::NameTy *)tenv->Look(nameandty->name_);

    type::Ty *actualTy = nameTy->ty_;
    while (typeid(*actualTy)==typeid(type::NameTy))
    {
      nameTy = (type::NameTy *)actualTy;
      if (nameandty->name_ == nameTy->sym_){
        hasCircle = true;
        break;
      }
      actualTy = nameTy->ty_;
    }

    if (hasCircle){
      break;
    }
  }

    if (hasCircle){
      errormsg->Error(pos_, "illegal type cycle");
    }

}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(name_);
  if (!ty){
    errormsg->Error(pos_, "undefined type %s", name_->Name());
    return DEFAULT_TYPE;
  }
  else {
    return ty;
  }
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::FieldList *field_list = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(field_list);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab4 code here */
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().c_str());
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
