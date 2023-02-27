#include "tiger/liveness/flowgraph.h"

namespace fg {

// 生成控制流图
void FlowGraphFactory::AssemFlowGraph() {
  /* TODO: Put your lab6 code here */
  
  // 每一条汇编语句对应一个节点
  // instr_list_是作为参数初始化时就传进来的，可以直接使用
  std::list<assem::Instr *> il = instr_list_->GetList();
  FNodePtr prev = nullptr;

  // 为所有的语句生成对应的节点
  for (auto instr : il){
    // 在图中添加一个新的node并返回指向这个node的指针
    FNodePtr current = flowgraph_->NewNode(instr);

    // 有向图从上一条语句指向可能的下一条语句
    if (prev){
      flowgraph_->AddEdge(prev, current);
    }

    // 直接跳转语句的下一条语句与它不可能顺序执行
    if (typeid(*instr) == typeid(assem::OperInstr)){
      if (((assem::OperInstr*)instr)->assem_.find("jmp")==0){
        prev = nullptr;
        continue;
      }
    }

    // 将所有的标签指令加入map，产生一个标签与节点的对应关系，为后面连接jmp与target label做准备
    if (typeid(*instr) == typeid(assem::LabelInstr)){
      if (((assem::LabelInstr *)instr)->label_ != nullptr){
        label_map_->Enter(((assem::LabelInstr *)instr)->label_, current);
      }
    }

    prev = current;
  }

  // 把所有跳转指令连接的两个语句的节点连接起来
  FNodeListPtr nodes = flowgraph_->Nodes();
  std::list<FNodePtr> node_list = nodes->GetList();
  for (auto node: node_list){
    if (typeid(*(node->NodeInfo())) == typeid(assem::OperInstr)){
      assem::Targets *jumps = ((assem::OperInstr *)(node->NodeInfo()))->jumps_;

      if (!jumps){
        continue;
      }

      int size = jumps->labels_->size();
      for (int i = 0; i<size; ++i){
        // 在label_map中查找jmp label中的label对应的node
        FNode *label_node = label_map_->Look(jumps->labels_->at(i));
        // 添加一条边
        flowgraph_->AddEdge(node, label_node);
      }
    }
  }
}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();   // 标签不使用寄存器，直接返回空表
}

temp::TempList *MoveInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ ? dst_ : new temp::TempList();   // def[node]返回的是在该语句中被赋值（定义）的寄存器，即dst_
}

temp::TempList *OperInstr::Def() const {
  /* TODO: Put your lab6 code here */
  return dst_ ? dst_ : new temp::TempList();
}

temp::TempList *LabelInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return new temp::TempList();   // 标签不使用寄存器，直接返回空表
}

temp::TempList *MoveInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ ? src_ : new temp::TempList();   // use[node]返回的是在该语句中被使用的寄存器，即src_
}

temp::TempList *OperInstr::Use() const {
  /* TODO: Put your lab6 code here */
  return src_ ? src_ : new temp::TempList();
}
} // namespace assem
