#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>
#include <iostream>
#include <fstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;


} // namespace

namespace cg {

void CodeGen::Codegen() {
  /* TODO: Put your lab5 code here */
  // 栈帧尺寸标签，此处与ProcEntryExit3设置的栈帧尺寸名保持一致
  fs_ = frame_->name_->Name() + "_framesize";
  assem::InstrList *instr_list = new assem::InstrList();
  std::list<tree::Stm *> stm_list = traces_->GetStmList()->GetList();
  std::vector<temp::Temp *> tmp_store;
  int i = -1;
 
  // 保存被调用者保存寄存器，存到虚寄存器中
  auto Callee_saves = reg_manager->CalleeSaves()->GetList();
  for (auto reg = Callee_saves.begin(); reg!=Callee_saves.end(); ++reg){
    temp::Temp *tmp = temp::TempFactory::NewTemp();
    tmp_store.push_back(tmp);
    ++i;
    instr_list->Append(
      new assem::MoveInstr(
        std::string("movq `s0, `d0"),
        new temp::TempList(tmp), new temp::TempList((*reg))
      ));
  }

  // // for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "w");
  // fprintf(fout, "\ncodegen begin\n");
  // fprintf(fout, fs_.c_str());
  // fflush(fout);
  
  int count = 0;

  // 指令选择，对所有块进行指令选择
  for (auto stm = stm_list.begin(); stm != stm_list.end(); ++stm){
    // for debug
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "statement Munch count: %d\n", count);
    // (*stm)->Print(fout, 1);
    // fprintf(fout, "\n");
    // fflush(fout);
    ++count;

    (*stm)->Munch(*instr_list,fs_); 
  }
  
  // // for debug
  // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "begin recover register\n");
  // fflush(fout);

  // 恢复被调用者保存寄存器，反着恢复
  for (auto reg = Callee_saves.rbegin(); reg!=Callee_saves.rend(); ++reg){
    
    // // for debug
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "recover register count: %d\n", i);
    // fflush(fout);

    instr_list->Append(
      new assem::MoveInstr(std::string("movq `s0, `d0"), 
      new temp::TempList((*reg)), new temp::TempList(tmp_store[i])
      ));
    --i;
  }

  // // for debug
  // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "end register recover\n");
  // fflush(fout);

  // 指定出口活跃，并转变为cg::AssemInstr
  assem_instr_ = std::make_unique<AssemInstr>(instr_list);
  frame::procEntryExit2(assem_instr_->GetInstrList());

  // // for debug
  // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "end codegen\n");
  // fflush(fout); 
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
  fprintf(out, "\n");
}
} // namespace cg

namespace tree {
/* TODO: Put your lab5 code here */

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */

  // // for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "SeqStm Munch\n");
  // fflush(fout);

  left_->Munch(instr_list, fs);
  right_->Munch(instr_list, fs);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // // for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "LabelStm Munch: %s\n", label_->Name().c_str());
  // fflush(fout);

  // 新建标签并加入到指令序列中
  assem::LabelInstr *instr = new assem::LabelInstr(label_->Name(), label_);
  instr_list.Append(instr);

  // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "end LabelStm Munch: %s\n", label_->Name().c_str());
  // fflush(fout);
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "JumpStm Munch: %s\n", this->exp_->name_->Name().c_str());
  // fflush(fout);
  
  // 无条件跳转
  temp::Label *jump_label = exp_->name_;
  std::string instr = std::string("jmp ") + temp::LabelFactory::LabelString(jump_label);
  instr_list.Append(new assem::OperInstr(instr, nullptr, nullptr, new assem::Targets(jumps_)));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */

  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "CJumpStm Munch\n");
  // fflush(fout);

  // 条件式左右两边分别翻译，得到两个存有结果的寄存器
  temp::Temp *left_temp = left_->Munch(instr_list, fs);
  temp::Temp *right_temp = right_->Munch(instr_list, fs);
  
  // 判断关系符是什么
  std::string op;

  switch (op_)
  {
  case tree::RelOp::EQ_OP:
    op = std::string("je");
    break;
  case tree::RelOp::NE_OP:
    op = std::string("jne");
    break;
  case tree::RelOp::LT_OP:
    op = std::string("jl");
    break;
  case tree::RelOp::GT_OP:
    op = std::string("jg");
    break;
  case tree::RelOp::LE_OP:
    op = std::string("jle");
    break;
  case tree::RelOp::GE_OP:
    op = std::string("jge");
    break;
  case tree::RelOp::ULT_OP:
    op = std::string("jb");
    break;
  case tree::RelOp::ULE_OP:
    op = std::string("jbe");
    break;
  case tree::RelOp::UGT_OP:
    op = std::string("ja");
    break;
  case tree::RelOp::UGE_OP:
    op = std::string("jae");
    break;
  default:
    break;
  }
  
  // 条件跳转先判断再跳转
  auto labelList = new std::vector<temp::Label *>();
  labelList->push_back(true_label_);
  instr_list.Append(new assem::OperInstr("cmpq `s0, `s1", nullptr, new temp::TempList({right_temp, left_temp}), nullptr));
  instr_list.Append(new assem::OperInstr(op+" `j0", nullptr, nullptr, new assem::Targets(labelList)));
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "MoveStm Munch\n");
  // fflush(fout);

  // 如果目标是寄存器，只有LOAD一种情况
  if (typeid(*dst_) == typeid(tree::TempExp)){

    // //for debug
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "MoveStm Munch load\n");
    // fflush(fout);

    temp::TempList *src = new temp::TempList(src_->Munch(instr_list, fs));
    temp::TempList *dst = new temp::TempList(dst_->Munch(instr_list, fs));
    instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"), dst, src));
  }
  else if (typeid(*dst_) == typeid(tree::MemExp)){
    // 如果目标是内存，有一下几种解决方案
    tree::MemExp *mem_dst = static_cast<tree::MemExp *>(dst_);
    tree::Exp *mem_dst_exp = mem_dst->exp_;

    // 模式 1：MOVE(MEM(BINOP(PLUS, CONST(i), e1), e2)
    // 模式 2：MOVE(MEM(BINOP(PLUS, e1, CONST(i)), e2)
    bool success = false;
    if (typeid(*mem_dst_exp) == typeid(tree::BinopExp)){

      // //for debug
      // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
      // fprintf(fout, "MoveStm Munch 1or2\n");
      // fflush(fout);

      // 如果内存表达式是二元运算
      tree::BinopExp *binop_exp = static_cast<tree::BinopExp *>(mem_dst_exp);
      if (binop_exp->op_ == tree::BinOp::PLUS_OP){
        // 如果目标表达式是加法
        tree::Exp *left_exp = binop_exp->left_;
        tree::Exp *right_exp = binop_exp->right_;

        tree::Exp *e1 = nullptr;
        int imm = -1;

        if (typeid(*left_exp) == typeid(tree::ConstExp)){
          // 如果加法左边是常数，那么直接取出这个常数
          tree::ConstExp *const_exp = static_cast<tree::ConstExp *>(left_exp);
          imm = const_exp->consti_;
          e1 = binop_exp->right_;
        }
        else if (typeid(*right_exp) == typeid(tree::ConstExp)){
          // 如果加法右边是常数，那么直接取出这个常数
          tree::ConstExp *const_exp = static_cast<tree::ConstExp *>(right_exp);
          imm = const_exp->consti_;
          e1 = binop_exp->left_;
        }

        if (e1) {
          // 对源操作数与二元运算的其中一个操作数执行指令选择，并存在两个临时寄存器中
          temp::Temp *e1_reg = e1->Munch(instr_list, fs);
          temp::Temp *e2_reg = src_->Munch(instr_list, fs);
          
          // movq e2,imm(e1)
          temp::TempList *templist = new temp::TempList({e2_reg, e1_reg});
          instr_list.Append(new assem::OperInstr(
            std::string("movq `s0, ") + std::to_string(imm) + std::string("(`s1)"), nullptr,
            templist, nullptr
          ));

          success = true;
        }
      }
    }

    // 模式 3：MOVE(MEM(e1), MEM(e2))
    if (!success && typeid(*src_) == typeid(tree::MemExp)){

      // //for debug
      // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
      // fprintf(fout, "MoveStm Munch 3\n");
      // fflush(fout);

      tree::MemExp *mem_src = static_cast<tree::MemExp *>(src_);
      tree::Exp *mem_src_exp = mem_src->exp_;

      // 需要两条语句，因为x86_64规定不能两个操作数都是内存，所以需要一个中间寄存器
      temp::Temp *t = temp::TempFactory::NewTemp();

      temp::Temp *e1_reg = mem_dst_exp->Munch(instr_list, fs);
      temp::Temp *e2_reg = mem_src_exp->Munch(instr_list, fs);

      // 把源操作数转移到中间寄存器
      instr_list.Append(new assem::OperInstr(std::string("movq (`s0), `d0"),
        new temp::TempList(t), new temp::TempList(e2_reg), nullptr));
      // 把中间寄存器的值转移到目标操作数
      temp::TempList *templist = new temp::TempList({t, e1_reg});
      instr_list.Append(new assem::OperInstr(std::string("movq `s0, (`s1)"),
        nullptr, templist, nullptr));

      success = true;
    }

    // 模式4：MOVE(MEM(e1), e2)
    if (success!=true){

      // //for debug
      // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
      // fprintf(fout, "MoveStm Munch 4\n");
      // fflush(fout);

      temp::Temp *e1_reg = mem_dst_exp->Munch(instr_list, fs);
      temp::Temp *e2_reg = src_->Munch(instr_list, fs);

      temp::TempList *templist = new temp::TempList({e2_reg, e1_reg});

      instr_list.Append(new assem::OperInstr(std::string("movq `s0, (`s1)"),
        nullptr, templist, nullptr));
      
      success = true;
    }
  }
  else {
    assert(false);
  }

}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "ExpStm Munch\n");
  // fflush(fout);

  exp_->Munch(instr_list, fs);  
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "BinopExp Munch\n");
  // fflush(fout);

  temp::Temp *reg = temp::TempFactory::NewTemp();
  temp::Temp *left = left_->Munch(instr_list, fs);
  temp::Temp *right = right_->Munch(instr_list, fs);
  temp::Temp *rdx = reg_manager->Registers()->NthTemp(3);
  temp::Temp *rax = reg_manager->ReturnValue();

  temp::TempList *dst;
  temp::TempList *src;

  switch (op_)
  {
  case tree::BinOp::PLUS_OP:
    // // for debug
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "PLUS\n");
    // fflush(fout);

    dst = new temp::TempList(reg);
    instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"), dst, new temp::TempList(left)));
    src = new temp::TempList(right);
    // reg是隐藏的src, 计算完后reg中存的是加的和
    src->Append(reg);
    instr_list.Append(new assem::OperInstr(std::string("addq `s0, `d0"), dst, src, nullptr));
    break;
  case tree::BinOp::MINUS_OP:
    // // for debug
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "MINUS\n");
    // fflush(fout);

    dst = new temp::TempList(reg);
    instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"), dst, new temp::TempList(left)));
    src = new temp::TempList(right);
    // reg是隐藏的src, 计算完后reg中存的是减的差
    instr_list.Append(new assem::OperInstr(std::string("subq `s0, `d0"), dst, src, nullptr));
    break;
  case tree::BinOp::MUL_OP:
    // // for debug
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "MUL\n");
    // fflush(fout);

    // R[%rdx] : R[%rax] <- S × R[%rax] 全符号乘法
    // 将左值移到rax中
    instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"), new temp::TempList(rax), new temp::TempList(left)));
    dst = new temp::TempList(rax);
    dst->Append(rdx);
    // cqto
    instr_list.Append(new assem::OperInstr(std::string("cqto"), dst, new temp::TempList(rax), nullptr));
    src = new temp::TempList(right);
    src->Append(rax);
    src->Append(rdx);
    // imulq
    // 将右值作为imulq的参数，它会与%rax做乘法，并将结果存放在%rax和%rdx中
    instr_list.Append(new assem::OperInstr(std::string("imulq `s0"), dst, src, nullptr));
    // 把%rax移动到reg中
    instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"), new temp::TempList(reg), new temp::TempList(rax)));
    break;
  case tree::BinOp::DIV_OP:
    // // for debug
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "DIV\n");
    // fflush(fout);

    // R[%rax] <- R[%rax]  ÷  S 全符号除法
    // R[%rdx] <- R[%rax] mod S
    // 将左值移动到%rax里
    instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"), new temp::TempList(rax), new temp::TempList(left)));
    dst = new temp::TempList(rax);
    // cqto
    // cqto读取%rax的符号位，并将它复制到%rdx的所有位，执行符号扩展
    instr_list.Append(new assem::OperInstr(std::string("cqto"), dst, new temp::TempList(rax), nullptr));
    src = new temp::TempList(right);
    src->Append(rax);
    src->Append(rdx);
    // 将右值作为idivq的参数，它会与%rax做除法，并将商和余数存放在%rax和%rdx中
    instr_list.Append(new assem::OperInstr(std::string("idivq `s0"), dst, src, nullptr));
    // 最后把%rax移动到reg中
    instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"), new temp::TempList(reg), new temp::TempList(rax)));
    break;
  default:
    break;
  }
  return reg;
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "MemExp Munch\n");
  // fflush(fout);

  temp::Temp *reg = temp::TempFactory::NewTemp();
  temp::TempList *dst = new temp::TempList(reg);
  temp::Temp *exp = exp_->Munch(instr_list, fs);
  temp::TempList *src = new temp::TempList(exp);
  instr_list.Append(new assem::OperInstr("movq (`s0), `d0", dst, src, nullptr));
  return reg;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "TempExp Munch\n");
  // fflush(fout);

  if (temp_ != reg_manager->FramePointer()) {
    return temp_;
  }

  // else replace it by rsp and framesize
  temp::Temp *reg = temp::TempFactory::NewTemp();
  std::string instr_str = std::string("leaq ") + std::string(fs) + std::string("(`s0), `d0");
  instr_list.Append(new assem::OperInstr(instr_str, new temp::TempList(reg), new temp::TempList(reg_manager->StackPointer()), nullptr));
  return reg;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "EseqExp Munch\n");
  // fflush(fout);

  stm_->Munch(instr_list, fs);
  return exp_->Munch(instr_list, fs);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "NameExp Munch: %s\n", this->name_->Name().c_str());
  // fflush(fout);

  temp::Temp *address = temp::TempFactory::NewTemp();
  std::string instr_str = std::string("leaq ") + name_->Name() + std::string("(%rip), `d0");
  instr_list.Append(new assem::OperInstr(instr_str, new temp::TempList(address), nullptr, nullptr));

  return address;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE * = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "ConstExp Munch: %d\n", consti_);
  // fflush(fout);

  temp::Temp *reg = temp::TempFactory::NewTemp();
  std::string instr_str = std::string("movq $") + std::to_string(consti_) + std::string(", `d0");
  instr_list.Append(new assem::OperInstr(instr_str, new temp::TempList(reg), nullptr, nullptr));

  return reg;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "CallExp Munch: %s\n", ((tree::NameExp*)fun_)->name_->Name().c_str());
  // fflush(fout);

  // 最后存储返回值的寄存器
  temp::Temp *ret = temp::TempFactory::NewTemp();

  // 计算参数并存到虚寄存器中
  temp::TempList *args = args_->MunchArgs(instr_list, fs);

  // 获取参数的个数以及传参寄存器的个数
  int args_num = args->GetList().size();
  int reg_num = reg_manager->ArgRegs()->GetList().size();

  if (typeid(*fun_) != typeid(tree::NameExp)){
    // // for debug
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "Funtype error: %s\n", ((tree::NameExp*)fun_)->name_->Name().c_str());
    // fflush(fout);  
    return nullptr;
  }
  // "callq fun_name"
  std::string instr_str = "callq " + temp::LabelFactory::LabelString(((tree::NameExp*)fun_)->name_);
  
  // 处理参数  
  instr_list.Append(new assem::OperInstr(instr_str, reg_manager->CallerSaves(), args, nullptr));

  // pop the args on the stack
  if(reg_num < args_num){
    std::string instr = std::string("addq $") + std::to_string((args_num - reg_num) * reg_manager->WordSize()) + std::string(", `d0");
    instr_list.Append(new assem::OperInstr(instr, new temp::TempList(reg_manager->StackPointer()), nullptr, nullptr));
  }

  // parse the return value
  instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"), 
    new temp::TempList(ret), new temp::TempList(reg_manager->ReturnValue())));

  return ret;  
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  /* TODO: Put your lab5 code here */
  // //for debug
  // FILE *fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "ExpList Munch\n");
  // fflush(fout);

  std::list<tree::Exp *> args_list = this->GetList();
  std::list<temp::Temp *> regs = reg_manager->ArgRegs()->GetList();
  temp::TempList *args = new temp::TempList();
  
  int regs_num = regs.size();
  int count = 0;
  int ws = reg_manager->WordSize();
  
  // for (auto arg = args_list.begin(); arg!=args_list.end(); ++arg){
    
  //   // for debug
  //   fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  //   fprintf(fout, "munch args-count: %d\n", count);
  //   fflush(fout);

  //   temp::Temp *tmp = (*arg)->Munch(instr_list, fs);
  //   if(count < regs_num){
  //     instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"),
  //       new temp::TempList(reg_manager->ArgRegs()->NthTemp(count)), new temp::TempList(tmp)));
  //   }
  //   else {
  //     instr_list.Append(new assem::OperInstr(std::string("subq $8, %rsp"),
  //       new temp::TempList(reg_manager->StackPointer()), nullptr, nullptr));
  //     instr_list.Append(new assem::OperInstr(std::string("movq `s0, (%rsp)"),
  //       new temp::TempList(reg_manager->StackPointer()), new temp::TempList(tmp), nullptr));

  //   }
  //   count++;
  //   args->Append(tmp); 
  // }
  for (auto arg = args_list.begin(); arg!=args_list.end(); ++arg){
    if (count < regs_num){
      temp::Temp *tmp = (*arg)->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(std::string("movq `s0, `d0"),
        new temp::TempList(reg_manager->ArgRegs()->NthTemp(count)), new temp::TempList(tmp)));      
      count++;
      args->Append(tmp);
      continue;
    }
    else{
      count++;
      break;
    }
  }

  // 从右向左放到栈上，第七个参数在最下面
  if (count > regs_num){
    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "before count is: %d\n", count);
    // fflush(fout);
    
    count = args_list.size() - regs_num;

    // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
    // fprintf(fout, "after count is: %d\n", count);
    // fflush(fout);

    for (auto arg = args_list.rbegin(); arg!=args_list.rend(); ++arg){
      if (count > 0){
        temp::Temp *tmp = (*arg)->Munch(instr_list, fs);
        instr_list.Append(new assem::OperInstr(std::string("subq $8, %rsp"),
          new temp::TempList(reg_manager->StackPointer()), nullptr, nullptr));
        instr_list.Append(new assem::OperInstr(std::string("movq `s0, (%rsp)"),
          new temp::TempList(reg_manager->StackPointer()), new temp::TempList(tmp), nullptr));      
        count--;
        args->Append(tmp);
        continue;
      }
      else{
        break;
      }
    }
  }


  // fout = fopen("/home/stu/tiger-compiler/codegen_log.txt", "a");
  // fprintf(fout, "end munch args\n");
  // fflush(fout);

  return args;
}

} // namespace tree
