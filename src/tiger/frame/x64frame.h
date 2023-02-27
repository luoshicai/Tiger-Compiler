//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {
  /* TODO: Put your lab5 code here */
private:
  temp::Temp *rax;
  temp::Temp *rbx;
  temp::Temp *rcx;
  temp::Temp *rdx;
  temp::Temp *rsi;
  temp::Temp *rdi;
  temp::Temp *rbp;
  temp::Temp *rsp;
  temp::Temp *r8;
  temp::Temp *r9;
  temp::Temp *r10;
  temp::Temp *r11;
  temp::Temp *r12;
  temp::Temp *r13;
  temp::Temp *r14;
  temp::Temp *r15;

public:
  X64RegManager(){
    rax = temp::TempFactory::NewTemp();
    rbx = temp::TempFactory::NewTemp();
    rcx = temp::TempFactory::NewTemp();
    rdx = temp::TempFactory::NewTemp();
    rsi = temp::TempFactory::NewTemp();
    rdi = temp::TempFactory::NewTemp();
    rbp = temp::TempFactory::NewTemp();
    rsp = temp::TempFactory::NewTemp();
    r8 = temp::TempFactory::NewTemp();
    r9 = temp::TempFactory::NewTemp();
    r10 = temp::TempFactory::NewTemp();
    r11 = temp::TempFactory::NewTemp();
    r12 = temp::TempFactory::NewTemp();
    r13 = temp::TempFactory::NewTemp();
    r14 = temp::TempFactory::NewTemp();
    r15 = temp::TempFactory::NewTemp();

    regs_=std::vector<temp::Temp *>{
      rax,rbx,rcx,rdx,rsi,rdi,rbp,rsp,r8,r9,r10,r11,r12,r13,r14,r15
    };

    temp_map_->Enter(rax, new std::string("%rax"));
    temp_map_->Enter(rbx, new std::string("%rbx"));
    temp_map_->Enter(rcx, new std::string("%rcx"));
    temp_map_->Enter(rdx, new std::string("%rdx"));
    temp_map_->Enter(rsi, new std::string("%rsi"));
    temp_map_->Enter(rdi, new std::string("%rdi"));
    temp_map_->Enter(rbp, new std::string("%rbp"));
    temp_map_->Enter(rsp, new std::string("%rsp"));
    temp_map_->Enter(r8, new std::string("%r8"));
    temp_map_->Enter(r9, new std::string("%r9"));
    temp_map_->Enter(r10, new std::string("%r10"));
    temp_map_->Enter(r11, new std::string("%r11"));
    temp_map_->Enter(r12, new std::string("%r12"));
    temp_map_->Enter(r13, new std::string("%r13"));
    temp_map_->Enter(r14, new std::string("%r14"));
    temp_map_->Enter(r15, new std::string("%r15"));
  }

  temp::Temp *GetRegister(int reg_num){
    return regs_[reg_num];
  }

  // lab5-part1 2022/11/11 写IR树时为了编译能跑后面要改
  // 2022/11/19已修改
  /**
   * Get general-purpose registers except RSI
   * NOTE: returned temp list should be in the order of calling convention
   * @return general-purpose registers
   */
  temp::TempList *Registers(){
    temp::TempList *tempList = new temp::TempList({rax, rbx, rcx, rdx,
      rsi, rdi, rbp, r8, r9, r10, r11, r12, r13, r14, r15});

    return tempList;
  }

  /**
   * Get registers which can be used to hold arguments
   * NOTE: returned temp list must be in the order of calling convention
   * @return argument registers
   */
  temp::TempList *ArgRegs(){
    temp::TempList *tempList = new temp::TempList({rdi, rsi, rdx, rcx, r8, r9});
    return tempList;
  }

  /**
   * Get caller-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return caller-saved registers
   */
  temp::TempList *CallerSaves(){
    temp::TempList *tempList = new temp::TempList({rax, rdi, rsi, rdx, rcx, r8, r9, r10, r11});
    return tempList;
  }

  /**
   * Get callee-saved registers
   * NOTE: returned registers must be in the order of calling convention
   * @return callee-saved registers
   */
  temp::TempList *CalleeSaves(){
    temp::TempList *tempList = new temp::TempList({rbx, rbp, r12, r13, r14, r15});
    return tempList;
  }

  /**
   * Get return-sink registers
   * @return return-sink registers
   */
  temp::TempList *ReturnSink(){
    // 出口活跃寄存器
    temp::TempList *temp_list = CalleeSaves();
    temp_list->Append(StackPointer()); //%rsp
    temp_list->Append(ReturnValue());  //%rax
    return temp_list;
  }

  /**
   * Get word size
   */
  int WordSize(){
    return 8;
  }

  temp::Temp *FramePointer(){
    return rbp;
  }

  temp::Temp *StackPointer(){
    return rsp;
  }

  temp::Temp *ReturnValue(){
    return rax;
  }  
};


} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
