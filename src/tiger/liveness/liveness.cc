#include "tiger/liveness/liveness.h"
#include <fstream>

extern frame::RegManager *reg_manager;

namespace live {
// moveList clear
void MoveList::Clear(){
  move_list_.clear();
}

// moveList assign
void MoveList::assign(std::list<std::pair<INodePtr, INodePtr>> list){
  move_list_.clear();
  move_list_ = list;
}

// moveList empty
bool MoveList::isEmpty(){
  return move_list_.empty();
}

// 一些辅助函数
bool Contain(temp::Temp *tmp, temp::TempList *list){
  if (list){
    for (auto t : list->GetList()){
      if (t == tmp){
        return true;
      }
    }
  }
  return false;
}

temp::TempList *Union(temp::TempList *left, temp::TempList *right){
  temp::TempList *res = new temp::TempList();
  if (left){
    for (auto tmp : left->GetList()){
      if (!Contain(tmp, res)){
        res->Append(tmp);
      }
    }
  }
  if (right){
    for (auto tmp : right->GetList()){
      if (!Contain(tmp, res)){
        res->Append(tmp);
      }
    }
  }
  return res;
}

// res = left - right
temp::TempList *Sub(temp::TempList *left, temp::TempList *right){
  temp::TempList *res = new temp::TempList();
  if (left){
    for (auto tmp : left->GetList()){
      if (!Contain(tmp, right)){
        res->Append(tmp);
      }
    }
  }
  return res;
}

bool LiveGraphFactory::isSame(temp::TempList *left, temp::TempList *right){
  // 如果元素个数都不一致则直接返回false
  if (left->GetList().size() != right->GetList().size()){
    return false;
  }
  auto left_it = left->GetList().begin();
  for (;left_it != left->GetList().end(); ++left_it){
    auto right_it = right->GetList().begin();
    for (;right_it != right->GetList().begin(); ++right_it){
      if ((*left_it)->Int() == (*right_it)->Int()){
        break;
      }
    }
    if (right_it == right->GetList().end()){
      return false;
    }
  }
  return true;
}

// 上面是写的一些辅助函数
bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

void LiveGraphFactory::LiveMap() {
  /* TODO: Put your lab6 code here */
  // 初始化
  const auto &fnode_list = flowgraph_->Nodes()->GetList();
  for (auto node : fnode_list){
    // 每个控制流节点都有一个入口活跃表和出口活跃表
    in_->Enter(node, new temp::TempList());
    out_->Enter(node, new temp::TempList());
  }

  // 活跃分析
  bool loop = true;
  while (loop)
  {
    loop = false;

    /* 
     *  迭代公式
     *  in[s] = use[s] ∪ (out[s] - def[s])
     *  out[s] = ∪(n∈succ[s])in[n]
     * 
    */
    // 从后往前遍历 
    for (auto it = fnode_list.rbegin(); it != fnode_list.rend(); ++it){
      auto node = *it;
      
      // 上一轮的入口活跃
      temp::TempList *old_in = in_->Look(node);
      // 上一轮的出口活跃
      temp::TempList *old_out = out_->Look(node); 

      assert(old_in);
      assert(old_out);

      // 更新out链表，遍历该节点的所有后继
      const auto &succ_list = node->Succ()->GetList();
      for (fg::FNodePtr succ : succ_list){
        // 后继节点上一轮的入口活跃
        auto in_nodes = in_->Look(succ);
        // 因为每次都会更新当前节点的out所以需要重复获取
        auto out_nodes = out_->Look(node);

        assert(in_nodes);
        assert(out_nodes);
        
        // 不断地将后继节点的入口活跃表并入当前节点的出口活跃表
        out_->Set(node, Union(out_nodes, in_nodes));
      }

      // 更新in链表
      temp::TempList *new_in = Union(node->NodeInfo()->Use(), Sub(out_->Look(node), node->NodeInfo()->Def()));
      in_->Set(node, new_in);
     
      // 判断是否与上一轮迭代相同
      if ((isSame(in_->Look(node), old_in)==false)||(isSame(out_->Look(node), old_out)==false)){
        loop = true;
      }
    }
  }
  
  // // 检查结果是否正确
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
  // int i=0; 
  // for (auto iter=fnode_list.begin(); iter !=fnode_list.end(); ++iter){
  //   auto node = *iter;

  //   temp::TempList *in = in_->Look(*iter);
  //   temp::TempList *out = out_->Look(*iter); 
  //   temp::TempList *Def = node->NodeInfo()->Def();
  //   temp::TempList *Use = node->NodeInfo()->Use();

  //   fout << std::endl;
  //   fout << "assem instr " << i << ": " << std::endl;

  //   // in
  //   fout << "in_: ";
  //   for (auto tmp : in->GetList()){
  //     fout << tmp->Int() << " ";
  //   }  
  //   fout << std::endl;

  //   // out
  //   fout << "out_: ";
  //   for (auto tmp : out->GetList()){
  //     fout << tmp->Int() << " ";
  //   }
  //   fout << std::endl;
    
  //   // def
  //   fout << "def_: ";
  //   for (auto tmp : Def->GetList()){
  //     fout << tmp->Int() << " ";
  //   }
  //   fout << std::endl;

  //   // use
  //   fout << "use_: ";
  //   for (auto tmp : Use->GetList()){
  //     fout << tmp->Int() << " ";
  //   }
  //   fout << std::endl;
  //   ++i;
  //   if(i>100){
  //     break;
  //   }
  // }
  // fout.close();
}

void LiveGraphFactory::InterfGraph() {
  /* TODO: Put your lab6 code here */
  // 初始化
  auto &interf_graph = live_graph_.interf_graph;
  auto &moves = live_graph_.moves;
  interf_graph = new IGraph();
  moves = new MoveList();

  // 把实寄存器存入冲突图，预着色
  // 不含栈指针
  temp::TempList *registers = reg_manager->Registers();
  const auto &reg_list = registers->GetList();
  for (temp::Temp *reg : reg_list){
    INodePtr node = interf_graph->NewNode(reg);
    temp_node_map_->Enter(reg, node);
  }

  // 实寄存器两两冲突，每两个之间都有边
  for (auto it = reg_list.begin(); it!=reg_list.end(); ++it){
    for (auto it2 = std::next(it); it2!=reg_list.end(); ++it2){
      auto first_node = temp_node_map_->Look(*it);
      auto second_node = temp_node_map_->Look(*it2);

      assert(first_node);
      assert(second_node);

      // 无向图
      interf_graph->AddEdge(first_node, second_node);
      interf_graph->AddEdge(second_node, first_node);
    }
  }

  // 把控制流中的所有寄存器全部添加到冲突图中
  const auto &fnode_list = flowgraph_->Nodes()->GetList();
  for (fg::FNodePtr node : fnode_list){
    auto instr = node->NodeInfo();
    const auto &use_list = instr->Use()->GetList();
    const auto &def_list = instr->Def()->GetList();

    // 把使用的加入
    for (auto t : use_list){
      // 跳过栈指针
      if (t == reg_manager->StackPointer()){
        continue;
      }
      if (!temp_node_map_->Look(t)) {
        INodePtr inode = interf_graph->NewNode(t);
        temp_node_map_->Enter(t, inode);
      }
    }
    // 把赋值(定义)的加入
    for (auto t : def_list){
      // 跳过栈指针p
      if (t == reg_manager->StackPointer()){
        continue;
      }
      if (!temp_node_map_->Look(t)){
        INodePtr inode = interf_graph->NewNode(t);
        temp_node_map_->Enter(t, inode);
      }
    }
  }

  /*
   * 添加冲突边原则
   * 1.任何对变量a赋值的非传送指令,对该指令出口活跃的任意变量b[i],添加冲突边(a,b[i])
   * 2.对于传送指令 a<--c,对该指令出口活跃的任意不同于c的变量b[i],添加冲突边(a,b[i]),并在movelist中保存传送指令
   *  
  */

  // // for debug
  // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::trunc);
  // fout << "begin" << std::endl;
  // fout << std::endl;
  // fout.close();

  // 栈指针
  temp::Temp *rsp = reg_manager->StackPointer();
  // 遍历所有的指令语句
  for (fg::FNodePtr node : fnode_list){
    auto instr = node->NodeInfo();
    temp::TempList *def = node->NodeInfo()->Def();
    temp::TempList *use = node->NodeInfo()->Use();
    
    // 如果该条指令是传送指令
    if (typeid(*(node->NodeInfo())) == typeid(assem::MoveInstr)){
      // // for debug
      // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
      // fout << std::endl;
      // fout << "move instr" << std::endl;
      // fout.close();

      for (auto d : def->GetList()){
        // 跳过栈指针
        if (d == rsp){
          continue;
        }
        // 该条语句的出口活跃寄存器
        auto out_reg_list = out_->Look(node)->GetList();
        // 左值节点
        auto def_node = temp_node_map_->Look(d);
        assert(def_node);

        for (auto temp : out_reg_list){
          // 对于每一个出口活跃寄存器,如果不在use中即与def添加冲突边
          // 该出口活跃寄存器是否在use中
          if (Contain(temp, use)){
            continue;
          }
          // 跳过栈指针
          if (temp == rsp){
            continue;
          }
          // 加冲突边
          auto out_node = temp_node_map_->Look(temp);
          interf_graph->AddEdge(def_node, out_node);
          interf_graph->AddEdge(out_node, def_node);
        }

        // 保存传送语句
        for (auto u : use->GetList()){
          // // for debug
          // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
          // fout << "保存传送语句" << std::endl;
          // fout.close();
          
          if (u == rsp){
            continue;
          }
          // use node
          auto use_node = temp_node_map_->Look(u);
          assert(use_node);

          // // for debug
          // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
          // fout << "Contain: " << moves->Contain(use_node, def_node) << std::endl;
          // fout << "moves size is: " << moves->GetList().size() << std::endl;
          // fout << "use: " << use_node->NodeInfo() <<  " def: "<< def_node->NodeInfo() << std::endl;
          // fout.close();

          if (moves->Contain(use_node, def_node)==false){
            // // for debug
            // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
            // fout << "append successfully" << std::endl;
            // fout.close();
            
            moves->Append(use_node, def_node);
          }
        }
      }
    }
    else{
      // // for debug
      // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
      // fout << std::endl;
      // fout << "other instr" << std::endl;
      // fout.close();

      // 处理非传送指令
      for (auto d : def->GetList()){
        if (d == rsp){
          continue;
        }
        // 该条语句的出口活跃寄存器
        auto out_reg_list = out_->Look(node)->GetList();
        // 左值节点
        auto def_node = temp_node_map_->Look(d);
        assert(def_node);

        for (auto temp : out_reg_list){
          if (temp == rsp){
            continue;
          }
          // 加冲突边
          auto out_node = temp_node_map_->Look(temp);
          interf_graph->AddEdge(def_node, out_node);
          interf_graph->AddEdge(out_node, def_node);
        }        
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

} // namespace live
