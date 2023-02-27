#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"
#include <fstream>

extern frame::RegManager *reg_manager;

namespace ra {
/* TODO: Put your lab6 code here */
  Result::~Result(){
    delete il_;
    delete coloring_;
  }

  std::unique_ptr<ra::Result> RegAllocator::TransferResult(){
    return std::move(result_);
  }

  void RegAllocator::RegAlloc(){
    // // for debug
    // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::trunc);
    // fout << "RegAlloc begin" << std::endl;
    // fout.close();

    while (true){
        
        // // for debug
        // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
        // fout << "------------------------------------new iter-----------------------------"<< std::endl;
        // fout.close();

        // 清空数据结构
        init();
        // 活跃分析
        live::LiveGraph liveness = LivenessAnalysis();
        Build(liveness);
        MakeWorkList(liveness);

        // // for debug
        // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
        // fout << "all number is: " << liveness.interf_graph->Nodes()->GetList().size() <<std::endl;
        // fout << "simplifyWorkList: " << simplifyWorklist.GetList().size() << std::endl;
        // fout << "worklistMoves: " << worklistMoves.GetList().size() << std::endl;
        // fout << "freezeWorklist: " << freezeWorklist.GetList().size() << std::endl;
        // fout << "spillWorklist: " << spillWorklist.GetList().size() << std::endl;
        // fout << std::endl;
        // fout.close();

        // 简化、合并、冻结、溢出
        do {
          if (!simplifyWorklist.GetList().empty()){
            // // for debug
            // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
            // fout << "Simplify" << std::endl;
            // fout.close();
            
            Simplify();
          }
          else if (!worklistMoves.isEmpty()){
            // // for debug
            // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
            // fout << "Coalesce" << std::endl;
            // fout.close();

            Coalesce();
          }
          else if (!freezeWorklist.GetList().empty()){
            // // for debug
            // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
            // fout << "Freeze" << std::endl;
            // fout.close();

            Freeze();
          }
          else if (!spillWorklist.GetList().empty()){
            // // for debug
            // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
            // fout << "SelectSpill" << std::endl;
            // fout.close();

            SelectSpill();
          }
        }
        while (!simplifyWorklist.GetList().empty() || !worklistMoves.isEmpty()
            || !freezeWorklist.GetList().empty() || !spillWorklist.GetList().empty());
      
        // // for debug
        // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
        // fout << "Assign color" << std::endl;
        // fout.close();

        // 染色
        AssignColors();
        // 若spillNodes为空,说明分配成功,跳出循环
        if (spillNodes.GetList().empty()){          
          break;
        }
        else{

          // // for debug
          // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
          // fout << "Rewrite Program" << std::endl;
          // fout.close();

          // 否则需要重写程序
          RewriteProgram();
        }
    }

      // // for debug
      // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
      // fout << "transfer result" << std::endl;
      // fout.close();

    // 结果转化到result数据结构中
    temp::Map *coloring = temp::Map::Empty();
    for (auto tmp : color_map) {
      temp::Temp *t = tmp.first;
      std::string nm = tmp.second;
      std::string *name = new std::string(nm);
      coloring->Enter(t, name);
    }
    
    // // for debug
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "simplifyInstrList" << std::endl;
    // fout.close();

    simplifyInstrList();
    result_ = std::make_unique<ra::Result>(coloring, instr_list_);

    // // for debug
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "end" << std::endl;
    // fout.close();
  }

  void RegAllocator::init(){
    // 不含栈指针的可用寄存器的数量
    K = reg_manager->Registers()->GetList().size();

    // 数据结构的清空
    precolored.clear();
    
    simplifyWorklist.Clear();
    freezeWorklist.Clear();
    spillWorklist.Clear();

    spillNodes.Clear();
    coloredNodes.Clear();
    coalescedNodes.Clear();

    worklistMoves.Clear();
    activeMoves.Clear();
    coalescedMoves.Clear();
    constrainedMoves.Clear();
    frozenMoves.Clear();

    degrees.clear();
    adjSet.clear();
    adjList.clear();

    selectStack.Clear();

    alias.clear();
    move_list_.clear();
    color_map.clear();
    
    // 初始化预着色寄存器集合
    const auto &pc_regs_list = reg_manager->Registers()->GetList();
    for (auto t : pc_regs_list){
      auto name = reg_manager->temp_map_->Look(t);
      assert(name);
      precolored.insert(std::make_pair(t, *name));
    }
    return;
  }

  // 活跃分析,得到冲突图
  live::LiveGraph RegAllocator::LivenessAnalysis(){

    // // for debug
    // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::trunc);
    // fout << "LivenessAnalysis begin" << std::endl;
    // fout.close();

    fg::FlowGraphFactory flow_graph_fac(instr_list_);
    flow_graph_fac.AssemFlowGraph();  // 构造控制流图

    fg::FGraphPtr flow_graph = flow_graph_fac.GetFlowGraph();

    live::LiveGraphFactory live_graph_fac(flow_graph);
    live_graph_fac.Liveness();  // 构造活跃性图(冲突图)
    
    // // for debug
    // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "Livness move size is: " <<  live_graph_fac.GetLiveGraph().moves->GetList().size() << std::endl;
    // fout << std::endl;
    // fout.close();

    return live_graph_fac.GetLiveGraph();
  }

  void RegAllocator::Build(const live::LiveGraph &liveness_){
    // 初始化
    const auto &org_move_list = liveness_.moves->GetList();

    // 构造moveList,初始化所有传送指令的源与目标节点对应的列表
    for (auto move : org_move_list){
        auto src = move.first;
        auto dst = move.second;
        move_list_[src].Append(src, dst);
        move_list_[dst].Append(src, dst);
    }

    worklistMoves = *liveness_.moves;

    // 度数的初始化
    for (auto node : liveness_.interf_graph->Nodes()->GetList()){
      degrees[node] = 0;
    }
    
    // 构建邻接矩阵和邻接表,并获取节点的degree
    for (auto u : liveness_.interf_graph->Nodes()->GetList()){
      for (auto v : u->Adj()->GetList()){
        AddEdge(u, v);
      }
    }

    // 给机器寄存器预着色
    for (auto &it : precolored) {
      color_map.insert(std::make_pair(it.first, it.second));
    }
  }

  void RegAllocator::AddEdge(live::INodePtr u, live::INodePtr v){
    if (adjSet.find(std::make_pair(u, v)) == adjSet.end() && u != v){
      adjSet.insert(std::make_pair(u, v));
      adjSet.insert(std::make_pair(v, u));
      
      //不用给预着色寄存器添加度数，因为默认是无限大
      if (!isPrecolored(u->NodeInfo())) {
        adjList[u].Append(v);
        degrees[u]++;
      }
      if (!isPrecolored(v->NodeInfo())) {
        adjList[v].Append(u);
        degrees[v]++;
      }
    }
  }

  void RegAllocator::MakeWorkList(const live::LiveGraph &liveness_) {
    const auto &interf_node_list = liveness_.interf_graph->Nodes()->GetList();
    for (auto node : interf_node_list){
      temp::Temp *tmp = node->NodeInfo();
      // 跳过预着色寄存器
      if (isPrecolored(tmp)) {
        continue;
      }
      if (degrees[node] >= K){
        spillWorklist.Append(node);   // 高度数节点
      }
      else if (MoveRelated(node)){
        freezeWorklist.Append(node);  // 低度数、传送相关节点
      }
      else{
        simplifyWorklist.Append(node);   // 低度数、传送无关节点
      }
    }
  }

  // 简化
  void RegAllocator::Simplify() {
    auto node = simplifyWorklist.GetList().front();   // 选择第一个
    simplifyWorklist.DeleteNode(node);

    // 加入到删除的栈中
    selectStack.Append(node);

    // 更新度数和相关数据结构
    std::list<live::INodePtr> adj_list = Adjacent(node).GetList();
    for (auto adj_node : adj_list){
      DecrementDegree(adj_node);
    }
  }

  void RegAllocator::DecrementDegree(live::INodePtr node){
    if (isPrecolored(node->NodeInfo())){
      return;
    }
    
    int d = degrees[node];
    degrees[node] = d-1;

    // 当它的度数从K变成K-1时,根据该节点是否是传送相关的重新进行节点分类
    if (d == K){
      // EnableMoves({m} ∪ Adjacent(m))
      live::INodeList list = Adjacent(node);
      list.Append(node);

      EnableMoves(list);

      spillWorklist.DeleteNode(node);
      if (MoveRelated(node)){
        freezeWorklist.Append(node);
      }
      else{
        simplifyWorklist.Append(node);
      }
    }
  }

  void RegAllocator::EnableMoves(const live::INodeList &nodes){
    const auto &node_list = nodes.GetList();
    for (auto node : node_list){
      auto node_moves = NodeMoves(node);
      const auto &move_list = node_moves.GetList();
        
      for (auto move : move_list){
        if (activeMoves.Contain(move.first, move.second)){
          // activeMoves <- activeMoves \ m
          activeMoves.Delete(move.first, move.second);
          // worklistMoves <- worklistMoves ∪ m
          worklistMoves.Append(move.first, move.second);
        }
      }
    }
  }

  void RegAllocator::Coalesce(){
    auto move = worklistMoves.GetList().front();   // 选择第一个
    live::INodePtr x = move.first;   // src
    live::INodePtr y = move.second;  // dst

    // 找代表元
    x = GetAlias(x);
    y = GetAlias(y);

    // 如果u和v中只有一个是预着色寄存器,那一定是u
    live::INodePtr u, v;
    if (isPrecolored(y->NodeInfo())) {
      u = y;
      v = x;
    }
    else {
      u = x;
      v = y;
    }

    worklistMoves.Delete(move.first, move.second);
    
    // 合并的四种情况
    if (u == v){
      // // for debug
      // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
      // fout << "u==v" << std::endl;
      // fout.close();

      // 如果src和dst已经是一个节点了，那么不需要做什么额外的事情
      coalescedMoves.Append(move.first, move.second);
      AddWorkList(u);
    }
    else if (isPrecolored(v->NodeInfo()) || adjSet.find(std::make_pair(u,v)) != adjSet.end()) {
      // // for debug
      // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
      // fout << "2" << std::endl;
      // fout.close();

      // 如果u和v除了是传送相关之外,还是相互冲突的(u和v同时是预着色寄存器也是这种情况),则一定不可着色
      constrainedMoves.Append(move.first, move.second);
      AddWorkList(u);
      AddWorkList(v);
    }
    else if (
      // George
      (isPrecolored(u->NodeInfo()) && OK(Adjacent(v), u)) ||
      // Briggs
      (!isPrecolored(u->NodeInfo()) && Conservative(Union(Adjacent(u), Adjacent(v))))
     ) {
      constrainedMoves.Append(move.first, move.second);
      Combine(u, v);
      AddWorkList(u);     
    }
    else {
      // // for debug
      // fout.open("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
      // fout << "4" << std::endl;
      // fout.close();
      
      // 否则加入没准备好合并的集合,等日后再使用EnableMoves恢复到workListMoves中
      activeMoves.Append(move.first, move.second);
    }
  }

  void RegAllocator::AddWorkList(live::INodePtr node){
    int d = degrees[node];
    // 是低度数节点且非传送相关
    if (!isPrecolored(node->NodeInfo()) && !MoveRelated(node) && d < K){
      freezeWorklist.DeleteNode(node);
      simplifyWorklist.Append(node);
    }
  }

  void RegAllocator::Combine(live::INodePtr u, live::INodePtr v){
    // 能进合并阶段则SimplifyWorklist一定为空,所以u和v要么在freezeWorklist中要么在spillWorklist中
    if (freezeWorklist.Contain(v)){
      freezeWorklist.DeleteNode(v);
    }
    else {
      spillWorklist.DeleteNode(v);
    }
    coalescedNodes.Append(v);

    // 更新代表元
    alias[v] = u;

    // 合并MoveList,因为u和v合成了一个节点,所以与v相关的传送指令也与u相关了
    move_list_[u] = Union(move_list_[u], move_list_[v]);
    
    live::INodeList v_list;
    v_list.Append(v);
    EnableMoves(v_list);

    auto adjacent = Adjacent(v);
    
    for (auto adj_node : adjacent.GetList()){
      AddEdge(adj_node, u);

      DecrementDegree(adj_node);
    }

    // 同理, u一开始也只可能在freezeWorkList或spillWorkList中,
    // 如果原来在freezeWorkList中,但合并导致u的度数增大,则将u放到spillWorkList中去
    if (degrees[u] >= K && freezeWorklist.Contain(u)){
      freezeWorklist.DeleteNode(u);
      spillWorklist.Append(u);
    }
  }

  void RegAllocator::Freeze(){
    auto node = freezeWorklist.GetList().front();   // 找第一个节点进行冻结
    freezeWorklist.DeleteNode(node);
    simplifyWorklist.Append(node);

    // 把node相关的move指令放到frozenMove中去,这一操作是不可逆的
    FreezeMoves(node);
  }

  void RegAllocator::FreezeMoves(live::INodePtr u){
    // 找出所有与u相关的,还可以合并的传送指令
    auto node_moves = NodeMoves(u);
    const auto &move_list = node_moves.GetList();
    for (auto move : move_list){
      live::INodePtr x = move.first;
      live::INodePtr y = move.second;
      live::INodePtr v = nullptr;
      // 传送指令有src和dst,u已经处理过了,找出非u的那一个进行处理
      if (GetAlias(y) == GetAlias(u)){
        v = GetAlias(x);
      }
      else{
        v = GetAlias(y);
      }

      activeMoves.Delete(move.first, move.second);
      frozenMoves.Append(move.first, move.second);

      // 如果冻结节点u,删除了与u相关的传送指令导致了v不再传送相关,则将v放入simplifyWorklist中去
      if ((!isPrecolored(v->NodeInfo())) && NodeMoves(v).isEmpty() && degrees[v] < K){
        freezeWorklist.DeleteNode(v);
        simplifyWorklist.Append(v);
      }
    }
  }

  void RegAllocator::SelectSpill(){
    // 找度数最大的节点溢出,如果没有就选第一个
    live::INodePtr m = *(spillWorklist.GetList().begin());
    int max_degree = degrees[m];
    for (auto tmp : spillWorklist.GetList()){
      // 不要溢出增加的寄存器
      if (added_temps.find(tmp->NodeInfo()) != added_temps.end()){
        continue;
      }

      // 这个判断可能没有必要,但为了保险起见还是判断一下
      if (!spillNodes.Contain(tmp) && !isPrecolored(tmp->NodeInfo())){
        int d = degrees[tmp];
        if (d > max_degree){
          m = tmp;
          max_degree = d;
        }
      }
    }

    // 在这里只需将m放入smplifyWorkList中而不需要更新与m有边的节点的度数之类的
    // 因为在这里m还没有实际上被简化,在下一轮被simplify时这些都会更新的
    spillWorklist.DeleteNode(m);
    simplifyWorklist.Append(m);
    FreezeMoves(m);
  }

  void RegAllocator::AssignColors(){
    while (selectStack.GetList().size() != 0){
      // 从后往前模拟出栈,进行着色
      auto node = selectStack.GetList().back();
      selectStack.DeleteNode(node);

      // 所有可以分配的颜色
      std::set<std::string> okColors;
      for (auto &it : precolored){
        okColors.insert(it.second);
      }

      // 去掉与该节点冲突的节点的颜色
      const auto &adj_list = adjList[node].GetList();
      for (auto adj_node : adj_list){
        auto root = GetAlias(adj_node);
        auto tmp = root->NodeInfo();
        if (coloredNodes.Contain(root) || isPrecolored(tmp)){
          // 找到这个已被用过的颜色并把它从okColor中移除
          for (auto iter = okColors.begin(); iter!=okColors.end(); ++iter){
            if (*iter == color_map[tmp]){
               okColors.erase(iter);
               break;
            }
          }
        }
      }

      if (okColors.empty()){
        // 如果没有颜色可以用了,那么它必须被溢出
        spillNodes.Append(node);
      }
      else {
        // 随机挑选
        int size = okColors.size();
        int select = rand()%size;
        std::string selected_colors;
        int i=0;
        for (auto iter = okColors.begin(); iter != okColors.end(); ++iter){
          if (select == i){
            selected_colors = *iter;
          }
          ++i;
        }

        coloredNodes.Append(node);
        //color_map.insert(std::make_pair(node->NodeInfo(), *okColors.begin()));
        color_map.insert(std::make_pair(node->NodeInfo(), selected_colors));
      }
    }
    // 把合并的节点也加入color_map
    for (auto node : coalescedNodes.GetList()){
      color_map[node->NodeInfo()] = color_map[GetAlias(node)->NodeInfo()];
    }
    return;
  }

  void RegAllocator::RewriteProgram(){
    // std::list<temp::Temp *> newTempList;
    auto newInstrList = new assem::InstrList();

    int ws = reg_manager->WordSize();
    std::string label = frame_->GetLabel();
    temp::Temp *rsp = reg_manager->StackPointer();

    const auto &spill_node_list = spillNodes.GetList();
    const auto &old_instr_list = instr_list_->GetList();

    // 用来存放上一次更新的汇编指令序列,第一次它就是旧指令序列
    std::list<assem::Instr *> prev_instr_list;
    for (auto instr : old_instr_list){
      prev_instr_list.push_back(instr);
    }

    // 遍历所有溢出的节点
    for (auto node : spill_node_list){
      auto spilledTemp = node->NodeInfo();
      frame_->offset -= ws;   // 在帧中开辟新的空间
      std::list<assem::Instr *> cur_instr_list;
      
      // 遍历所有的汇编指令,在需要的地方插入语句
      for (auto instr : prev_instr_list){
        auto use_regs = instr->Use();   // 使用的寄存器集合
        auto def_regs = instr->Def();   // 定义的寄存器集合

        // 如果溢出的寄存器被这条语句使用了,那么需要在其之前插入一条load指令
        if (Contain(spilledTemp, use_regs)){
          auto newTemp = temp::TempFactory::NewTemp();

          // movq (fun_framesize-offset)(src), dst
          std::string assem = "movq (" + label + "_framesize-" + std::to_string(std::abs(frame_->offset)) + ")(`s0), `d0";

          // 目标:新临时寄存器
          // 源: 帧指针(通过栈指针计算出)-偏移
          auto pre_instr = new assem::OperInstr(assem, new temp::TempList({newTemp}), new temp::TempList({rsp}), nullptr);

          cur_instr_list.push_back(pre_instr);

          // 替换原指令成员变量src_保存的溢出节点
          use_regs->replaceTemp(spilledTemp, newTemp);

          // 加入列表
          added_temps.insert(newTemp);
        }

        // 保存当前汇编指令
        cur_instr_list.push_back(instr);

        // 如果溢出的寄存器被这条语句定义了,那么需要在其之后插入一条store指令
        if (Contain(spilledTemp, def_regs)){
          auto newTemp = temp::TempFactory::NewTemp();

          // movq src, (fun_framesize-offset)(dst)
          std::string assem = "movq `s0, (" + label + "_framesize-" + std::to_string(std::abs(frame_->offset)) + ")(`d0)";

          // 源:新临时寄存器
          // 目标: 帧指针(通过栈指针计算出)-偏移
          assem::Instr *back_instr = new assem::OperInstr(assem, new temp::TempList({rsp}), new temp::TempList({newTemp}), nullptr);

          cur_instr_list.push_back(back_instr);

          // 替换原指令成员变量dst_保存的溢出节点
          def_regs->replaceTemp(spilledTemp, newTemp);

          // 加入列表
          added_temps.insert(newTemp);
        }
      }
      
      // 保存这一轮的指令列表
      prev_instr_list = cur_instr_list;
    }

    for (auto instr : prev_instr_list){
      newInstrList->Append(instr);
    }
    instr_list_ = newInstrList;
    return;
  }

  void RegAllocator::simplifyInstrList(){
    assem::InstrList* ret = new assem::InstrList();

    for (auto instr : instr_list_->GetList()){
      // 如果move指令的源与目标相同,则删除它
      if (typeid(*instr) == typeid(assem::MoveInstr)){
        if (!instr->Def()->GetList().empty() && !instr->Use()->GetList().empty()
        && color_map[instr->Def()->GetList().front()] == color_map[instr->Use()->GetList().front()]){          
          continue;
        }
      }
      // 否则需要把指令添加到返回列表中
      ret->Append(instr);
    }
    instr_list_ = ret;
  }

  /**
   * 功能函数
  */
  bool RegAllocator::isPrecolored(temp::Temp *t){
    return precolored.find(t) != precolored.end();
  }

  bool RegAllocator::MoveRelated(live::INodePtr node){
    if (NodeMoves(node).isEmpty()){
      return false;
    }
    return true;
  }
  
  live::MoveList RegAllocator::NodeMoves(live::INodePtr node){
    if (move_list_.find(node) == move_list_.end()) {
      return live::MoveList();
    }
    
    // // for debug
    live::MoveList new_move_list = Intersect(move_list_[node], Union(activeMoves, worklistMoves));
    // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "d == K in NodeMoves, move_list_[node] size is: " << move_list_[node].GetList().size() << std::endl;
    // fout << "d == K in NodeMoves, activeMoves size is: " << activeMoves.GetList().size() << std::endl;
    // fout << "d == K in NodeMoves, worklistMoves size is: " << worklistMoves.GetList().size() << std::endl;
    // for (auto move : move_list_[node].GetList()){
    //   fout << "move.first: " << move.first->NodeInfo()->Int() << " move.second: " << move.second->NodeInfo()->Int() << std::endl;
    // }
    // for (auto move : new_move_list.GetList()){
    //   fout << "new move.first: " << move.first->NodeInfo()->Int() << " move.second: " << move.second->NodeInfo()->Int() << std::endl;
    // }
    // fout.close();

    // moveList[n] ∩ (activeMoves ∪ worklistMoves)  只有这两个集合中的move指令还是可以合并的
    return new_move_list;
  }

  live::INodeList RegAllocator::Adjacent(live::INodePtr node){
    if (adjList.find(node)==adjList.end()){
      return live::INodeList();
    }
    // adjList[n] \ (selectStack ∪ coalescedNodes)
    return Sub(adjList[node], Union(selectStack, coalescedNodes));
  }

  live::INodePtr RegAllocator::GetAlias(live::INodePtr node){
    if (coalescedNodes.Contain(node)){
      return GetAlias(alias[node]);
    }
    else{
      return node;
    }
  }

  bool RegAllocator::OK(const live::INodeList &nodes, live::INodePtr r){
    bool res = true;
    // 批量处理,必须全部是OK才返回OK
    for (auto node : nodes.GetList()){
      if (!OK(node, r)){
        res = false;
        break;
      }
    }
    // // for debug
    // std::ofstream fout("/home/stu/tiger-compiler/tiger_log.txt", std::ios::app);
    // fout << "mache register is: " << r->NodeInfo() << std::endl;
    // fout << "George return: " << res << std::endl;
    // fout << std::endl;
    // fout.close();
    return res;
  }

  bool RegAllocator::OK(live::INodePtr t, live::INodePtr r){
    // 合并预着色寄存器的启发式函数(George),对于其每个邻居t,或者其度数小于K,或者与待合并的另一个节点已有冲突
    return ((degrees[t]<K) || (isPrecolored(t->NodeInfo())) || (adjSet.find(std::make_pair(t,r)) != adjSet.end()));
  }

  bool RegAllocator::Conservative(const live::INodeList &nodes){
    // 合并普通寄存器的启发式函数(Briggs), 合并产生的节点的高度数邻接点的个数小于K
    int k = 0;
    for (auto node : nodes.GetList()){
      if (isPrecolored(node->NodeInfo()) || degrees[node] >= K){
        k++;
      }
    }
    return (k < K);
  }
  /**
   * 一些集合的操作函数
  */
 // MoveList相关
  bool RegAllocator::Contain(std::pair<live::INodePtr, live::INodePtr> node, const live::MoveList &list){
    for (auto tmp : list.GetList()){
      if (tmp == node){
        return true;
      }
    }
    return false;
  }

  live::MoveList RegAllocator::Intersect(const live::MoveList &left, const live::MoveList &right){
    std::list<std::pair<live::INodePtr, live::INodePtr>> new_move_list;
    for (auto move : right.GetList()){
      if (Contain(move, left)){
        new_move_list.push_back(move);
      }
    }

    live::MoveList res;
    res.assign(new_move_list);
    return res;
  }

  live::MoveList RegAllocator::Union(const live::MoveList &left, const live::MoveList &right){
    std::list<std::pair<live::INodePtr, live::INodePtr>> new_move_list;
    for (auto move : left.GetList()){
      new_move_list.push_back(move);
    }
    for (auto move : right.GetList()){
      if (!Contain(move, left)){
        new_move_list.push_back(move);
      }
    }

    live::MoveList res;
    res.assign(new_move_list);
    return res;
  }

  // INodeList相关
  bool RegAllocator::Contain(live::INodePtr node, const live::INodeList &list){
    for (auto tmp : list.GetList()){
      if (tmp->NodeInfo() == node->NodeInfo()){
        return true;
      }
    }
    return false;
  }

  live::INodeList RegAllocator::Union(const live::INodeList &left, const live::INodeList &right){
    live::INodeList result;
    for (auto l : left.GetList()){
      if (!Contain(l, result)){
        result.Append(l);
      }
    }
    for (auto r : right.GetList()){
      if (!Contain(r, result)){
        result.Append(r);
      }
    }
    
    return result;
  }

  live::INodeList RegAllocator::Intersect(const live::INodeList &left, const live::INodeList &right){
    live::INodeList result;
    for (auto l :left.GetList()){
      if (Contain(l, right)){
        result.Append(l);
      }
    }

    return result;
  }

  live::INodeList RegAllocator::Sub(const live::INodeList &left, const live::INodeList &right){
    live::INodeList result;
    for (auto l : left.GetList()){
      if (!Contain(l, right)){
        result.Append(l);
      }
    }

    return result;
  }

  bool RegAllocator::Contain(temp::Temp *temp, temp::TempList *tempList){
    for (auto it : tempList->GetList()){
      if (temp == it){
        return true;
      }
    }
    return false;
  }
} // namespace ra