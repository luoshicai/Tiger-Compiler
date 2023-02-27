#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {
/* TODO: Put your lab5 code here */
class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *toExp(tree::Exp *framePtr) const override{
    tree::Exp *binopExp = new tree::BinopExp(tree::BinOp::PLUS_OP, framePtr, new tree::ConstExp(offset));
    tree::Exp *exp = new tree::MemExp(binopExp);
    return exp;
  }
};


class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  /* TODO: Put your lab5 code here */
  tree::Exp *toExp(tree::Exp *framePtr) const override{
    return new tree::TempExp(reg);
  }
};

class X64Frame : public Frame {
  /* TODO: Put your lab5 code here */
public:
  void setFormals(const std::list<bool> &escapes){
    // 首先判断函数的所有参数应该放在栈上还是放在寄存器上
    for (auto iter = escapes.begin(); iter!=escapes.end(); ++iter){
      frame::Access *escape_access = AllocLocal(*iter);
      formals_.push_back(escape_access);   
    }
    
    tree::Exp *src, *dst;
    tree::Stm *stm;

    // 参数寄存器
    std::list<temp::Temp *> argRegList = reg_manager->ArgRegs()->GetList();
    auto iter = argRegList.begin();

    int i=0;
    const int ws = reg_manager->WordSize();

    // 执行view shift，把所有参数都转移到该函数的虚寄存器中
    for (auto formal = formals_.begin(); formal!=formals_.end(); ++formal){
      // 目标：虚寄存器或栈帧
      dst = (*formal)->toExp(new tree::TempExp(reg_manager->FramePointer()));   
      
      if (iter!=argRegList.end()){
        src = new tree::TempExp((*iter));
        iter++;
      }
      else {
        // 执行call的时候会自动把返回地址压入栈中，然后才是帧指针，因此获取栈上的参数需要多加一个字长
        src = new tree::MemExp(
          new tree::BinopExp(tree::BinOp::PLUS_OP,
          new tree::TempExp(reg_manager->FramePointer()),
          new tree::ConstExp((i+1)*ws))
        );
        ++i;
      }
      stm = new tree::MoveStm(dst, src);

      // view shift 中的语句负责把参数转移到自己的Access中
      if (view_shift_==nullptr){
        view_shift_ = stm;
      }
      else{
        view_shift_ = new tree::SeqStm(view_shift_, stm);
      }
    }
  }

  frame::Access *AllocLocal(bool escape){
    frame::Access *access;
    if (escape){
      // 如果是逃逸的，那么栈帧向低地址方向移动一个字长
      offset = offset - reg_manager->WordSize(); 
      access = new InFrameAccess(offset);
    }
    else{
      // 否则使用寄存器来保存变量
      access = new InRegAccess(temp::TempFactory::NewTemp());
    }
    return access;
  }
};

/* TODO: Put your lab5 code here */
tree::Exp *ExternalCall(std::string s, tree::ExpList *args) {
  return new tree::CallExp(new tree::NameExp(temp::LabelFactory::NamedLabel(s)), args);
}

frame::Frame *newFrame(temp::Label *name, std::list<bool> formals){
  // 初始化frame
  frame::X64Frame *newFrame = new frame::X64Frame();
  newFrame->name_ = name;
  newFrame->offset = 0;
  newFrame->maxArgs = 0;
  
  newFrame->setFormals(formals);
  return newFrame;
  }

tree::Stm *procEntryExit1(frame::Frame *frame, tree::Stm *stm){
  return new tree::SeqStm(frame->view_shift_, stm);
}

assem::InstrList *procEntryExit2(assem::InstrList *body){
  body->Append(new assem::OperInstr("", nullptr, reg_manager->ReturnSink(), nullptr));

  return body;
}

assem::Proc *procEntryExit3(frame::Frame *frame, assem::InstrList *body){
  static char buf[256];
  std::string prolog;
  std::string epilog;
  

  // prolog part
  // sprintf(buf, ".set %s_framesize, %d\n", frame->name_->Name().c_str(), std::to_string(-(frame->offset)));
  // prolog = std::string(buf);
  prolog = ".set " + frame->name_->Name() + "_framesize, " + std::to_string(-(frame->offset)) + "\n";  

  sprintf(buf, "%s:\n", frame->name_->Name().c_str());
  prolog.append(std::string(buf));

  // 当参数超过6个时需要额外的栈空间存这些参数
  sprintf(buf, "subq $%d, %%rsp\n", -frame->offset);
  prolog.append(std::string(buf));

  // epilog part
  sprintf(buf, "addq $%d, %%rsp\n", -frame->offset);
  epilog.append(buf);

  epilog.append(std::string("retq\n"));

  return new assem::Proc(prolog, body, epilog);
}
} // namespace frame
