#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>
#include <iostream>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;

namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  /* TODO: Put your lab5 code here */
  return new tr::Access(level, level->frame_->AllocLocal(escape));
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override { 
    /* TODO: Put your lab5 code here */
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    return new tree::ExpStm(exp_); //把exp转为stm后返回
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    tree::CjumpStm *stm = new tree::CjumpStm(tree::NE_OP, exp_, new::tree::ConstExp(0), nullptr, nullptr);
    
    PatchList *trues = new PatchList({&((*stm).true_label_)});
    PatchList *falses = new PatchList({&((*stm).false_label_)});

    return Cx(*trues, *falses, stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    return new tree::EseqExp(stm_, new tree::ConstExp(0));
  }
  [[nodiscard]] tree::Stm *UnNx() override { 
    /* TODO: Put your lab5 code here */
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    /* TODO: Put your lab5 code here */
    std::list<temp::Label **> *trues_ = new std::list<temp::Label **>();
    std::list<temp::Label **> *falses_ = new std::list<temp::Label **>();
    PatchList *trues = new PatchList(*trues_);
    PatchList *falses = new PatchList(*falses_);
    return Cx(*trues, *falses, nullptr);
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  [[nodiscard]] tree::Exp *UnEx() override {
    /* TODO: Put your lab5 code here */
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();

    this->cx_.trues_.DoPatch(t);
    this->cx_.falses_.DoPatch(f);

    return new tree::EseqExp(
      new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
      new tree::EseqExp(
        this->cx_.stm_,
        new tree::EseqExp(
          new tree::LabelStm(f),
          new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0)),
            new tree::EseqExp(new tree::LabelStm(t), new tree::TempExp(r)))
        )
      )
    );
  }

  [[nodiscard]] tree::Stm *UnNx() override {
    /* TODO: Put your lab5 code here */
    temp::Label *label = temp::LabelFactory::NewLabel();
    this->cx_.trues_.DoPatch(label);
    this->cx_.falses_.DoPatch(label);
    
    return new tree::SeqStm(this->cx_.stm_, new tree::LabelStm(label));
  }

  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override { 
    /* TODO: Put your lab5 code here */
    return cx_;
  }
};

void ProgTr::Translate() {
  /* TODO: Put your lab5 code here */
  temp::Label *main_label_ = temp::LabelFactory::NamedLabel("tigermain");
  std::list<bool> escapes;
  tr::Level *level = new tr::Level(frame::newFrame(main_label_, std::list<bool>()), nullptr);
  main_level_.reset(level);
  tenv_ = std::make_unique<env::TEnv>();
  venv_ = std::make_unique<env::VEnv>();

  // fill after main level init
  FillBaseVEnv();
  FillBaseTEnv();

  // main函数的标签
  temp::Label *main_label = temp::LabelFactory::NewLabel();
  tr::ExpAndTy *root_info = absyn_tree_->Translate(venv_.get(), tenv_.get(), main_level_.get(), main_label, errormsg_.get());
  
  // 保存整个程序的片段
  frags->PushBack(new frame::ProcFrag(root_info->exp_->UnNx(), main_level_->frame_));

}

tr::Level *newLevel(Level *parent, temp::Label *name, std::list<bool> formals){
  // 隐式在头部添加静链，并且它一定是逃逸的
  formals.push_front(true);
  frame::Frame *newFrame = frame::newFrame(name, formals);
  Level *level = new Level(newFrame, parent);
  
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "new level's name is:  " << name->Name() << std::endl;
  // fout.close(); 
  
  return level;
}
} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt");
  // fout << "root translate" << std::endl;
  // fout.close();
  
  return root_->Translate(venv,tenv,level,label,errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "SimpleVar translate" << std::endl;
  // fout << "find sym_: " << sym_->Name() << std::endl;
  // fout.close();

  env::EnvEntry *entry = venv->Look(sym_);
  if (entry && typeid(*entry) == typeid(env::VarEntry)){
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "if SimpleVar translate" << std::endl;
    // fout.close();

    env::VarEntry *ventry = (env::VarEntry*)entry;
    tr::Level *dec_level = ventry->access_->level_;
    tree::Exp *fp = new tree::TempExp(reg_manager->FramePointer());

    //根据当前栈帧和目标栈帧推导计算的表达式
    while (level != dec_level)
    {
      // // for debug
      // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
      // fout << "level: " << level << std::endl;
      // fout << "dec_level: " << dec_level << std::endl;
      // fout << "level->parent_: " << level->parent_ << std::endl; 
      // fout.close();

      // 静态链应当是栈帧的第一个参数
      frame::Access *access = level->frame_->formals_.front();
      // 得到一个内存表达式，其运算结果是上一个栈指针
      fp = access->toExp(fp);
      level = level->parent_;
    }

    tr::ExExp *exp = new tr::ExExp(ventry->access_->access_->toExp(fp));
    
    return new tr::ExpAndTy(exp, ventry->ty_->ActualTy());
  }
  else{
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "else SimpleVar translate" << std::endl;
    // fout.close();
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().data());
    return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
  }
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "FieldVar translate" << std::endl;
  // fout.close();

  // 翻译结构名
  tr::ExpAndTy* var = var_->Translate(venv, tenv, level, label, errormsg);
  type::RecordTy *record_ty = static_cast<type::RecordTy *>(var->ty_->ActualTy());

  // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "end FieldVar name translate" << std::endl;
  // fout.close();

  std::list<type::Field*> field_list = record_ty->fields_->GetList();
  int offset=0;
  for (auto iter = field_list.begin(); iter!=field_list.end(); ++iter){
    // // for debug
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "Field var offset: " << offset << std::endl;
    // fout.close();

    if ((*iter)->name_->Name() == sym_->Name()){
      tr::Exp *fieldVar = new tr::ExExp(new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP, var->exp_->UnEx(), new tree::ConstExp(offset * reg_manager->WordSize()))
      ));
      return new tr::ExpAndTy(fieldVar, (*iter)->ty_->ActualTy());
    }

    // // for debug
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "end Field var offset: " << offset << std::endl;
    // fout.close();
    
    offset++;
  }
  return new tr::ExpAndTy(nullptr, type::IntTy::Instance());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "SubscriptVar translate" << std::endl;
  // fout.close();

  // 先翻译数组的名字
  tr::ExpAndTy *var_info = var_->Translate(venv, tenv, level, label, errormsg);

  // 再翻译数组的下标
  tr::ExpAndTy *subscript_info = subscript_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *ty = var_info->ty_->ActualTy();

  tr::Exp *SubscriptVar = new tr::ExExp(new tree::MemExp(
    new tree::BinopExp(tree::BinOp::PLUS_OP, var_info->exp_->UnEx(), 
      new tree::BinopExp(tree::MUL_OP, subscript_info->exp_->UnEx(), new tree::ConstExp(reg_manager->WordSize())))
  ));

  // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "end SubscriptVar translate" << std::endl;
  // fout.close();

  return new tr::ExpAndTy(SubscriptVar, static_cast<type::ArrayTy *>(ty)->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "VarExp translate" << std::endl;
  // fout.close();

  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "NilExp translate" << std::endl;
  // fout.close();

  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(0)), type::NilTy::Instance());
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "IntExp translate" << std::endl;
  // fout.close();
  return new tr::ExpAndTy(new tr::ExExp(new tree::ConstExp(val_)), type::IntTy::Instance());
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "String translate" << std::endl;
  // fout.close();
  temp::Label *label_tmp = temp::LabelFactory::NewLabel();
  frame::StringFrag *str_frag = new frame::StringFrag(label_tmp, str_);

  frags->PushBack(str_frag);
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(label_tmp)), type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "CallExp translate" << std::endl;
  // fout.close();
  // 从类型环境中找该函数
  env::FunEntry *entry = (env::FunEntry *)venv->Look(func_);
  tr::Exp *exp;

  // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << func_->Name() << "  " << entry->label_ << std::endl;
  // fout.close();

  // 传进来的函数参数
  std::list<Exp *> args = args_->GetList();
  tree::ExpList *expList = new tree::ExpList();
  
  // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "end args translate" << std::endl;
  // fout.close();

  // 对每个参数进行翻译
  for (auto iter = args.begin(); iter!=args.end(); ++iter){
    tr::ExpAndTy *arg_info = (*iter)->Translate(venv, tenv, level, label, errormsg);
    expList->Append((*arg_info).exp_->UnEx());
  }

  // 如果这个函数是最外层的函数
  if (entry->level_->parent_ == nullptr){
    
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "in if CallExp translate" << std::endl;
    // fout.close();
    
    exp = new tr::ExExp(new tree::CallExp(new tree::NameExp(
      temp::LabelFactory::NamedLabel(temp::LabelFactory::LabelString(func_))),expList));
  }
  else{

    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "in else CallExp translate" << std::endl;
    // fout.close();

    // 计算静态链,函数定义的层级是函数自身层级的父层级
    tree::Exp *sl = new tree::TempExp(reg_manager->FramePointer());
    while (level != entry->level_->parent_)
    {
      sl = level->frame_->formals_.front()->toExp(sl);
      level = level->parent_;
    }

    // 参数列表的第一个为静态链
    expList->Insert(sl);
    exp = new tr::ExExp(new tree::CallExp(new tree::NameExp(entry->label_), expList));

    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "after calculate static link: " << sl << std::endl;
    // fout.close();
  }

  // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "end CallExp translate" << std::endl;
  // fout.close();
  
  return new tr::ExpAndTy(exp, entry->result_->ActualTy());
} 

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "OpExp translate" << std::endl;
  // fout.close();

  tr::ExpAndTy *left = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right = right_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *exp = nullptr;
  tree::CjumpStm *cjstm = nullptr;
  tr::PatchList *trues = nullptr;
  tr::PatchList *falses = nullptr;

  switch (oper_)
  {
  case Oper::PLUS_OP:
    exp = new tr::ExExp(new tree::BinopExp(tree::BinOp::PLUS_OP, left->exp_->UnEx(), right->exp_->UnEx()));
    break;
  
  case Oper::MINUS_OP:
    exp = new tr::ExExp(new tree::BinopExp(tree::BinOp::MINUS_OP, left->exp_->UnEx(), right->exp_->UnEx()));
    break;

  case Oper::TIMES_OP:
    exp = new tr::ExExp(new tree::BinopExp(tree::BinOp::MUL_OP, left->exp_->UnEx(), right->exp_->UnEx()));
    break;

  case Oper::DIVIDE_OP:
    exp = new tr::ExExp(new tree::BinopExp(tree::BinOp::DIV_OP, left->exp_->UnEx(), right->exp_->UnEx()));
    break;
  
  case Oper::AND_OP:{
    tr::Cx left_cx = left->exp_->UnCx(errormsg);
    tr::Cx right_cx = right->exp_->UnCx(errormsg);
    trues = new tr::PatchList();
    falses = new tr::PatchList();
    // short cut, 两个语句false的Label相同
    tr::PatchList false_list = falses->JoinPatch(left_cx.falses_, right_cx.falses_);
    // 第一个语句为true则跳到第二个语句
    temp::Label *z = temp::LabelFactory::NewLabel();
    left_cx.trues_.DoPatch(z);

    // 把两个语句组合起来
    tree::Stm *newStm = new tree::SeqStm(left_cx.stm_, new tree::SeqStm(new tree::LabelStm(z), right_cx.stm_));

    exp = new tr::CxExp(right_cx.trues_, false_list, newStm);
    break;
  }

  case Oper::OR_OP:{
    tr::Cx left_cx = left->exp_->UnCx(errormsg);
    tr::Cx right_cx = right->exp_->UnCx(errormsg);
    trues = new tr::PatchList(); 
    falses = new tr::PatchList();
    // short cut, 两个语句true的Label相同
    tr::PatchList true_list = trues->JoinPatch(left_cx.trues_, right_cx.trues_);
    // 第一个语句为false则跳到第二个语句
    temp::Label *z = temp::LabelFactory::NewLabel();
    left_cx.falses_.DoPatch(z);

    // 把两个语句组合起来
    tree::Stm *newStm = new tree::SeqStm(left_cx.stm_, new tree::SeqStm(new tree::LabelStm(z), right_cx.stm_));

    exp = new tr::CxExp(true_list, right_cx.falses_, newStm);
    break;
  }
  case Oper::LT_OP:
    cjstm = new tree::CjumpStm(tree::RelOp::LT_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    
    trues = new tr::PatchList({&cjstm->true_label_});
    falses = new tr::PatchList({&cjstm->false_label_});

    exp = new tr::CxExp(*trues, *falses, cjstm);
    break;
  
  case Oper::LE_OP:
    cjstm = new tree::CjumpStm(tree::RelOp::LE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);

    trues = new tr::PatchList({&cjstm->true_label_});
    falses = new tr::PatchList({&cjstm->false_label_});

    exp = new tr::CxExp(*trues, *falses, cjstm);
    break;
  
  case Oper::GT_OP:
    cjstm = new tree::CjumpStm(tree::RelOp::GT_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);

    trues = new tr::PatchList({&cjstm->true_label_});
    falses = new tr::PatchList({&cjstm->false_label_});

    exp = new tr::CxExp(*trues, *falses, cjstm);
    break;

  case Oper::GE_OP:
    cjstm = new tree::CjumpStm(tree::RelOp::GE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);

    trues = new tr::PatchList({&cjstm->true_label_});
    falses = new tr::PatchList({&cjstm->false_label_});

    exp = new tr::CxExp(*trues, *falses, cjstm);
    break;
  
  case Oper::EQ_OP:
    //字符串比较
    if (left->ty_->IsSameType(type::StringTy::Instance()) &&
        right->ty_->IsSameType(type::StringTy::Instance())){
          auto *exp_list = new tree::ExpList();
          exp_list->Append(left->exp_->UnEx());
          exp_list->Append(right->exp_->UnEx());
          cjstm = new tree::CjumpStm(tree::EQ_OP, frame::ExternalCall("string_equal", exp_list), new tree::ConstExp(1), nullptr, nullptr);
        }
    else{
      cjstm = new tree::CjumpStm(tree::RelOp::EQ_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    }
    trues = new tr::PatchList({&cjstm->true_label_});
    falses = new tr::PatchList({&cjstm->false_label_});

    exp = new tr::CxExp(*trues, *falses, cjstm);
    break;

  case Oper::NEQ_OP:
    cjstm = new tree::CjumpStm(tree::NE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    
    trues = new tr::PatchList({&cjstm->true_label_});
    falses = new tr::PatchList({&cjstm->false_label_});
    
    exp = new tr::CxExp(*trues, *falses, cjstm);
    break;

  default:
    assert(0);
  }
  
  return new tr::ExpAndTy(exp, type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "RecordExp translate" << std::endl;
  // fout.close();

  type::Ty *ty = tenv->Look(typ_)->ActualTy();
  tr::ExExp *exp = nullptr;

  // 翻译RecordExp中的每一个表达式
  tree::ExpList *expList = new tree::ExpList();
  std::list<absyn::EField *> eFieldList = fields_->GetList();
  for (auto iter = eFieldList.begin(); iter!=eFieldList.end(); ++iter){
    tr::ExpAndTy *res = (*iter)->exp_->Translate(venv,tenv,level,label,errormsg);
    expList->Append(res->exp_->UnEx());
  }

  temp::Temp *reg = temp::TempFactory::NewTemp();
  // 分配空间返回指针
  int size = expList->GetList().size();
  tree::ExpList *exps = new tree::ExpList();
  exps->Append(new tree::ConstExp(size * reg_manager->WordSize()));
  tree::Stm *stm = new tree::MoveStm(new tree::TempExp(reg), frame::ExternalCall(std::string("alloc_record"), exps));

  // 将之前翻译出来的RecordExp一个一个的move到分配出来的空间中
  int count = 0;
  for (auto iter = expList->GetList().begin(); iter!=expList->GetList().end(); ++iter){
    stm = new tree::SeqStm(stm, new tree::MoveStm(
      new tree::MemExp(new tree::BinopExp(tree::PLUS_OP, new tree::TempExp(reg),
        new tree::ConstExp(count * reg_manager->WordSize()))), (*iter)));
    count++;
  }

  return new tr::ExpAndTy(new tr::ExExp(new tree::EseqExp(stm, new tree::TempExp(reg))), ty);
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "SeqExp translate" << std::endl;
  // fout.close();

  if (!seq_) {
    return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
  }
  type::Ty *ty = type::VoidTy::Instance();
  tr::Exp *exp = nullptr;

  // 对SeqExp中的每一个表达式分别进行翻译
  for (auto iter = seq_->GetList().begin(); iter != seq_->GetList().end(); ++iter) {
    tr::ExpAndTy *tmp = (*iter)->Translate(venv, tenv, level, label, errormsg);
    if (!exp){
      exp = new tr::ExExp(tmp->exp_->UnEx());
    }
    else{
      exp = new tr::ExExp(new tree::EseqExp(exp->UnNx(), tmp->exp_->UnEx()));
    }
    ty = tmp->ty_->ActualTy();
  }
  // 然后连起来返回
  return new tr::ExpAndTy(exp, ty);
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "AssignExp translate" << std::endl;
  // fout.close();
  // 翻译左值
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
  // 翻译右值
  tr::ExpAndTy *exp = exp_->Translate(venv, tenv, level, label, errormsg);
  // 执行move语句
  tr::Exp *assignExp = new tr::NxExp(new tree::MoveStm(var->exp_->UnEx(), exp->exp_->UnEx()));

  return new tr::ExpAndTy(assignExp, type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "IfExp translate" << std::endl;
  // fout.close();

  // 翻译条件表达式
  tr::ExpAndTy *test = test_->Translate(venv, tenv, level, label, errormsg);
  // 翻译条件表达式为True的情况
  tr::ExpAndTy *then = then_->Translate(venv, tenv, level, label, errormsg);
  // 翻译条件表达式为false的情况，可能没有
  tr::Exp *exp;
  if (elsee_ != nullptr){
    tr::ExpAndTy *else_info = elsee_->Translate(venv, tenv, level, label, errormsg);
    
    tr::Cx test_cx = test->exp_->UnCx(errormsg);
    // 作为返回值寄存器
    temp::Temp *r = temp::TempFactory::NewTemp();
    // 真标签
    temp::Label *trues = temp::LabelFactory::NewLabel();
    // 假标签
    temp::Label *falses = temp::LabelFactory::NewLabel();
    // t和f做完后跳转到后面的公共位置
    temp::Label *flag = temp::LabelFactory::NewLabel();
    std::vector<temp::Label *> *arr = new std::vector<temp::Label *>();
    arr->push_back(flag);
    
    // 执行DoPatch
    test_cx.trues_.DoPatch(trues);
    test_cx.falses_.DoPatch(falses);
    
    exp = new tr::ExExp(
      // 整个是一个表达式序列
      new tree::EseqExp(
        // if (test)语句，如果是真直接往下，如果是假跳转到f标签
        test_cx.stm_,
        new tree::EseqExp(
          // 如果是真，那么将then的值给返回值寄存器r
          new tree::LabelStm(trues),
          new tree::EseqExp(
            new tree::MoveStm(new tree::TempExp(r), then->exp_->UnEx()),
            new tree::EseqExp(
              // true执行完毕后，不执行下面的f部分，直接跳转到done位置
              new tree::JumpStm(new tree::NameExp(flag), arr),
              // --上下互不干扰--
              new tree::EseqExp(
                // 如果是假，再把else的值传给r
                new tree::LabelStm(falses),
                new tree::EseqExp(
                  new tree::MoveStm(new tree::TempExp(r), else_info->exp_->UnEx()),
                  new tree::EseqExp(
                    new tree::JumpStm(new tree::NameExp(flag), arr),
                    // 这里设置done标签，并把r当作返回值
                    new tree::EseqExp(new tree::LabelStm(flag),new tree::TempExp(r))
                  )
                )
              )
            )
          )
        )
      )
    );
  }
  else{
    tr::Cx test_cx = test->exp_->UnCx(errormsg);
    // 作为返回值寄存器
    temp::Temp *r = temp::TempFactory::NewTemp();
    // 真标签
    temp::Label *trues = temp::LabelFactory::NewLabel();
    // 假标签
    temp::Label *falses = temp::LabelFactory::NewLabel();
    // done
    temp::Label *flag = temp::LabelFactory::NewLabel();
    // 执行DoPatch
    test_cx.trues_.DoPatch(trues);
    test_cx.falses_.DoPatch(falses);

    exp = new tr::NxExp(
      new tree::SeqStm(test_cx.stm_, new tree::SeqStm(
        // 如果为真
        new tree::LabelStm(trues),
        new tree::SeqStm(then->exp_->UnNx(),
        // 如果为假则相当于跳过了then语句
        new tree::LabelStm(falses))
      ))
    );
  }
  
  return new tr::ExpAndTy(exp, then->ty_->ActualTy());
}

/* test label:
 *            if not(condition) goto done
 * body label：
 *            body
 *            goto test
 * done label:
 */
tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "WhileExp translate" << std::endl;
  // fout.close();

  // done_label
  temp::Label *done_label = temp::LabelFactory::NewLabel();
  // test_label
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  // body_label
  temp::Label *body_label = temp::LabelFactory::NewLabel();

  // 翻译循环条件
  tr::ExpAndTy *test_info = test_->Translate(venv, tenv, level, label, errormsg);
  // 翻译循环体
  tr::ExpAndTy *body_info = body_->Translate(venv, tenv, level, done_label, errormsg); 
  
  // 执行 Do patch
  tr::Cx test_cx = test_info->exp_->UnCx(errormsg);
  test_cx.trues_.DoPatch(body_label);
  test_cx.falses_.DoPatch(done_label);
  
  // 翻译goto test时会用到
  std::vector<temp::Label *> *test_label_list = new std::vector<temp::Label *>();
  test_label_list->push_back(test_label);

  tr::Exp *exp = new tr::NxExp(
    new tree::SeqStm(
      // 放置label标签
      new tree::LabelStm(test_label),
      new tree::SeqStm(
        // 循环语句如果为真，跳转到body，如果为假，跳转到done
        test_cx.stm_,
        new tree::SeqStm(
          // 放置body标签
          new tree::LabelStm(body_label),
          new tree::SeqStm(
            // body语句
            body_info->exp_->UnNx(),
            new tree::SeqStm(
              // body执行完毕后跳转到test位置
              new tree::JumpStm(new tree::NameExp(test_label),test_label_list),
              // 放置done标签
              new tree::LabelStm(done_label)
            )
          )
        )
      )
    )
  );
  
  return new tr::ExpAndTy(exp, type::VoidTy::Instance());
}

/* i=lo
 * test:
 *   if i<hi then body else done
 * body:
 *   循环体
 *   i = i+1
 *   goto test
 * done:
 */ 
tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "ForExp translate" << std::endl;
  // fout.close();

  venv->BeginScope();
  // done_label
  temp::Label *done_label = temp::LabelFactory::NewLabel();
  // test_label
  temp::Label *test_label = temp::LabelFactory::NewLabel();
  // body_label
  temp::Label *body_label = temp::LabelFactory::NewLabel();
  
  // 翻译lo_
  tr::ExpAndTy *lo = lo_->Translate(venv, tenv, level, label, errormsg);
  // 翻译hi_
  tr::ExpAndTy *hi = hi_->Translate(venv, tenv, level, label, errormsg);
  
  // 给循环计数器开辟空间，可能是逃逸的，逃逸就要在栈上开辟
  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, lo->ty_, true));
  tree::Exp *i = access->access_->toExp(new tree::TempExp(reg_manager->FramePointer()));

  // 翻译循环体,可能会用到循环变量，所以一定要在venv->Enter之后
  tr::ExpAndTy *body_info = body_->Translate(venv, tenv, level, done_label, errormsg);

  // 跳转到test_label时用
  std::vector<temp::Label *> *arr = new std::vector<temp::Label *>();
  arr->push_back(test_label);

  tr::Exp *exp = new tr::NxExp(new tree::SeqStm(
    // 把lo赋值给i
    new tree::MoveStm(i, lo->exp_->UnEx()),
    new tree::SeqStm(
      // 放置test标签
      new tree::LabelStm(test_label),
      new tree::SeqStm(
        // 如果满足循环条件则跳转到body位置，否则跳转到done
        new tree::CjumpStm(tree::RelOp::LE_OP, i, hi->exp_->UnEx(), body_label, done_label),
        new tree::SeqStm(
          new tree::LabelStm(body_label),
          new tree::SeqStm(
            // 执行循环体
            body_info->exp_->UnNx(),
            new tree::SeqStm(
              // 将i的值加1
              new tree::MoveStm(i, new tree::BinopExp(tree::BinOp::PLUS_OP, i, new tree::ConstExp(1))),
              new tree::SeqStm(
                //跳转到test_label的位置
                new tree::JumpStm(new tree::NameExp(test_label),arr),
                // 放置done标签
                new tree::LabelStm(done_label)
              )
            )
          )
        )
      )
    )
  ));
  venv->EndScope();

  return new tr::ExpAndTy(exp, type::VoidTy::Instance());

}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "BreakExp translate: " << label->Name() << std::endl;
  // fout.close();

  // 直接跳到done_label
  std::vector<temp::Label *> *to_done = new std::vector<temp::Label *>();
  to_done->push_back(label);
  
  tr::Exp *exp = new tr::NxExp(
    new tree::JumpStm(new tree::NameExp(label), to_done));
  
  return new tr::ExpAndTy(exp, type::VoidTy::Instance());
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "LetExp translate" << std::endl;
  // fout.close();

  std::list<Dec *> decs = decs_->GetList();
  tree::Stm *stm = nullptr;
  tree::Exp *exp = nullptr;

  // 翻译DecList
  for (auto iter = decs.begin(); iter!=decs.end(); ++iter){
    if (!stm){
      stm = (*iter)->Translate(venv, tenv, level, label, errormsg)->UnNx();
    }
    else{
      stm = new tree::SeqStm(stm, (*iter)->Translate(venv, tenv, level, label, errormsg)->UnNx());
    }
  } 

  // 翻译body
  tr::ExpAndTy *body_info = body_->Translate(venv, tenv, level, label, errormsg);
  
  // 如果stm为空，说明没有定义什么
  if (!stm){
    exp = body_info->exp_->UnEx();
  }
  else{
    exp = new tree::EseqExp(stm, body_info->exp_->UnEx());
  }

  return new tr::ExpAndTy(new tr::ExExp(exp), body_info->ty_->ActualTy());
}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "ArrayExp translate" << std::endl;
  // fout.close();

  type::Ty *typ_ty = tenv->Look(typ_)->ActualTy();
  // 翻译数组大小
  tr::ExpAndTy *size = size_->Translate(venv, tenv, level, label, errormsg);
  // 翻译数组初始值
  tr::ExpAndTy *init = init_->Translate(venv, tenv, level, label, errormsg);
  // 调用库函数为数组初始化空间
  tr::Exp *exp = new tr::ExExp(frame::ExternalCall("init_array", new tree::ExpList({size->exp_->UnEx(), init->exp_->UnEx()})));
  
  return new tr::ExpAndTy(exp,typ_ty);
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "VoidExp translate" << std::endl;
  // fout.close();

  return new tr::ExpAndTy(nullptr, type::VoidTy::Instance());
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "FunctionDec translate" << std::endl;
  // fout.close();

  std::list<FunDec *> functions = functions_->GetList();

  // 首先翻译所有函数声明
  for(auto iter = functions.begin(); iter!=functions.end(); ++iter){
    type::TyList *formals = (*iter)->params_->MakeFormalTyList(tenv, errormsg);
    // 创建一个以函数名为名字的标签
    temp::Label *function_label = temp::LabelFactory::NamedLabel((*iter)->name_->Name());
    
    // // for debug
    // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "translate function name: " << (*iter)->name_->Name() << std::endl;
    // fout << "function level: " << level << std::endl;
    // fout.close();

    // 逃逸表
    std::list<bool> formal_escape;
    std::list<Field *> fieldList = (*iter)->params_->GetList();
    for (auto param = fieldList.begin(); param!=fieldList.end(); ++param){
      formal_escape.push_back((*param)->escape_);
    }


    tr::Level *new_level = tr::newLevel(level, function_label, formal_escape);
    
    // // for debug
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "function parent: " << new_level->parent_ << std::endl;
    // fout.close();

    // 如果函数存在返回值
    if ((*iter)->result_){
      type::Ty *result_ty = tenv->Look((*iter)->result_);
      venv->Enter((*iter)->name_, new env::FunEntry(new_level, function_label, formals, result_ty));
    }
    else{
      venv->Enter((*iter)->name_, new env::FunEntry(new_level, function_label, formals, type::VoidTy::Instance()));
    }
  }

  // 再逐个翻译函数体
  for (auto iter = functions.begin(); iter!=functions.end(); ++iter){
    env::EnvEntry *fun = venv->Look((*iter)->name_);
    type::TyList *formals = ((env::FunEntry*)fun)->formals_;
    auto formal_it = formals->GetList().begin();
    auto param_it = ((*iter)->params_->GetList()).begin();
    venv->BeginScope();
    std::list<frame::Access*> accessList = ((env::FunEntry*)fun)->level_->frame_->formals_;
    std::list<frame::Access*>::iterator access_it = accessList.begin();    

    // // For debug
    // // 函数参数的level
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "function body Translate: " << std::endl;
    // fout << "function variable level: " << ((env::FunEntry*)fun)->level_ << std::endl;
    // fout << "function variable parent level: " << ((env::FunEntry*)fun)->level_->parent_ << std::endl;
    // fout.close();

    // 跳过静态链
    access_it++;
    // 把参数一个一个存入变量环境
    for(; param_it!=((*iter)->params_->GetList()).end();param_it++){
      venv->Enter((*param_it)->name_,
        new env::VarEntry(new tr::Access(((env::FunEntry*)fun)->level_, *access_it), *formal_it));
      formal_it++;
      access_it++;
    }
    
    // 翻译函数体
    tr::ExpAndTy *body_info = (*iter)->body_->Translate(venv, tenv, ((env::FunEntry*)fun)->level_, label, errormsg);
    venv->EndScope();

    // // for debug
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "function body: " << body_info->exp_ << std::endl;
    // fout.close();

    // 返回值
    tree::Stm *stm = new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), body_info->exp_->UnEx());
    
    // // for debug
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "why view_shift_ is null: " << ((env::FunEntry*)fun)->level_->frame_->view_shift_ << std::endl;
    // fout.close();

    stm = frame::procEntryExit1(((env::FunEntry*)fun)->level_->frame_, stm);    
    // 保存函数片段
    frags->PushBack(new frame::ProcFrag(stm, ((env::FunEntry*)fun)->level_->frame_));
  }
  
  return new tr::ExExp(new tree::ConstExp(0));
}

tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "VarDec translate" << std::endl;
  // fout.close();

  // 翻译赋值语句
  tr::ExpAndTy *init = init_->Translate(venv, tenv, level, label, errormsg);
  type::Ty *typ_ty;
  if (!typ_){
    typ_ty = init->ty_->ActualTy();
  }
  else{
    typ_ty = tenv->Look(typ_);
  }
  // 根据变量是否是逃逸变量来决定变量是放在寄存器中还是放在栈上
  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, typ_ty));
  tree::Exp *dst = access->access_->toExp(new tree::TempExp(reg_manager->FramePointer()));
  tree::Exp *src = init->exp_->UnEx();
  
  return new tr::NxExp(new tree::MoveStm(dst, src));
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "TypeDec translate" << std::endl;
  // fout.close();

  // 首先将所有的声明加入环境避免前面的声明使用后面声明的变量声明却找不到
  for (auto iter = types_->GetList().begin(); iter!=types_->GetList().end(); ++iter){
    tenv->Enter((*iter)->name_, new type::NameTy((*iter)->name_,nullptr));
  }

  // 再逐个翻译
  for (auto iter = types_->GetList().begin(); iter!=types_->GetList().end(); ++iter){
    type::NameTy *nameTy = (type::NameTy *)tenv->Look((*iter)->name_);
    nameTy->ty_ = (*iter)->ty_->Translate(tenv, errormsg);
  }

  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "NameTy translate" << std::endl;
  // fout.close();
  type::Ty *name_ty = tenv->Look(name_);
  return new type::NameTy(name_, name_ty);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "RecordTy translate" << std::endl;
  // fout.close();

  type::FieldList *fields = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(fields);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  /* TODO: Put your lab5 code here */
  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // fout << "ArrayTy translate" << std::endl;
  // fout.close();

  type::Ty *array_ty = tenv->Look(array_);
  return new type::ArrayTy(array_ty);
}

} // namespace absyn
