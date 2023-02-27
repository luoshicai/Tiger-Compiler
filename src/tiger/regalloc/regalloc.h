#ifndef TIGER_REGALLOC_REGALLOC_H_
#define TIGER_REGALLOC_REGALLOC_H_

#include "tiger/codegen/assem.h"
#include "tiger/codegen/codegen.h"
#include "tiger/frame/frame.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/regalloc/color.h"
#include "tiger/util/graph.h"

#include <map>
#include <set>

namespace ra {

class Result {
public:
  temp::Map *coloring_;
  assem::InstrList *il_;

  Result() : coloring_(nullptr), il_(nullptr) {}
  Result(temp::Map *coloring, assem::InstrList *il)
      : coloring_(coloring), il_(il) {}
  Result(const Result &result) = delete;
  Result(Result &&result) = delete;
  Result &operator=(const Result &result) = delete;
  Result &operator=(Result &&result) = delete;
  ~Result();
};

class RegAllocator {
  /* TODO: Put your lab6 code here */
  public:
    RegAllocator(frame::Frame *frame, std::unique_ptr<cg::AssemInstr> assem_instr)
    : frame_(frame), instr_list_(assem_instr->GetInstrList()), result_(std::make_unique<ra::Result>()){};
    
    void RegAlloc();
    std::unique_ptr<ra::Result> TransferResult();
    
  // 一些最根本的属性
  private:
    frame::Frame *frame_;                   // 当前函数或过程的栈帧
    assem::InstrList *instr_list_;          // 汇编指令列表
    std::unique_ptr<ra::Result> result_;    // 虚寄存器(Temp)与实际寄存器的对应关系
  
  public:
    live::LiveGraph LivenessAnalysis();     //活跃分析
    void init();                            // 数据结构的清空
    void Build(const live::LiveGraph &liveness_);
    void AddEdge(live::INodePtr u, live::INodePtr v);  // 添加边到adjSet和adjList
    void MakeWorkList(const live::LiveGraph &liveness_);
    void Simplify();   // 执行简化
    void DecrementDegree(live::INodePtr node);
    void EnableMoves(const live::INodeList &nodes);   // 是给定节点准备好合并
    void Coalesce();   // 执行合并
    void AddWorkList(live::INodePtr node);   // 看看能不能将合并后的代表元放到simplifyWorkList中去
    void Combine(live::INodePtr u, live::INodePtr v);   
    void Freeze();   // 执行冻结
    void FreezeMoves(live::INodePtr u);
    void SelectSpill();   // 挑选度数最大的节点进行溢出
    void AssignColors();   // 着色
    void RewriteProgram();   // 重写程序
    void simplifyInstrList();   // 删掉汇编指令列表中无意义的传送指令
    
  // 一些功能函数
  public:
    bool isPrecolored(temp::Temp *t);
    bool MoveRelated(live::INodePtr node);
    live::MoveList NodeMoves(live::INodePtr node);  
    live::INodeList Adjacent(live::INodePtr node);   // 与node相邻的所有节点表
    live::INodePtr GetAlias(live::INodePtr node);   // 递归函数,查找node的代表元
    
    // 合并预着色寄存器的启发式函数(George),对于其每个邻居t,或者其度数小于K,或者与待合并的另一个节点已有冲突
    bool OK(const live::INodeList &nodes, live::INodePtr r);   //批量处理
    bool OK(live::INodePtr t, live::INodePtr r);    // 对每一个
    // 合并普通寄存器的启发式函数(Briggs), 合并产生的节点的高度数邻接点的个数小于K
    bool Conservative(const live::INodeList &nodes);
  
  // 对集合的操作函数
  public:
    live::MoveList Intersect(const live::MoveList &left, const live::MoveList &right);
    live::MoveList Union(const live::MoveList &left, const live::MoveList &right);
    bool Contain(std::pair<live::INodePtr, live::INodePtr> node, const live::MoveList &list);
    
    live::INodeList Sub(const live::INodeList &left, const live::INodeList &right);
    live::INodeList Union(const live::INodeList &left, const live::INodeList &right);
    live::INodeList Intersect(const live::INodeList &left, const live::INodeList &right);
    bool Contain(live::INodePtr node, const live::INodeList &list);
    
    bool Contain(temp::Temp *temp, temp::TempList *tempList);

  private:
    std::map<temp::Temp *, std::string> precolored;  // 机器寄存器集合

    live::INodeList simplifyWorklist;  // 低度数、传送无关的节点表
    live::INodeList freezeWorklist;    // 低度数、传送有关的节点表
    live::INodeList spillWorklist;     // 高度数的节点表
    
    live::INodeList spillNodes;        // 确定真的不可着色,要溢出的节点集合
    live::INodeList coloredNodes;      // 已成功着色的节点集合
    live::INodeList coalescedNodes;    // 已合并的节点集合

    live::MoveList worklistMoves;      // 可以执行合并操作的move指令集合
    live::MoveList activeMoves;        // 由于某个节点度数太高,还没有做好合并准备的传送指令集合
    live::MoveList coalescedMoves;     // 已经合并的传送指令集合
    live::MoveList constrainedMoves;   // 不能被合并的传送指令集合
    live::MoveList frozenMoves;        // 不再被考虑合并的传送指令集合(一般为合并和简化都失效时新增)

    //由于不更改原冲突图,所以在寄存器分配过程中要用下面这些数据结构来记录图的最新状态
    std::map<live::INodePtr, int> degrees;  // 节点的度数
    std::set<std::pair<live::INodePtr, live::INodePtr>> adjSet;  // 冲突边集合 
    std::map<live::INodePtr, live::INodeList> adjList;  // 冲突邻接表

    live::INodeList selectStack;  // 包含从图中删除的临时变量的栈

    std::map<live::INodePtr, live::INodePtr> alias;   // 已合并节点的代表元,如:(u,v)被合并后,alias[v]=u

    std::map<live::INodePtr, live::MoveList> move_list_;   // 节点-->与该节点传送相关的节点列表
    std::map<temp::Temp *, std::string> color_map;

    int K;   // 可用寄存器的数量

    // 因为溢出,在RewriteProgram时增加的寄存器,一般是不能再被选为溢出的
    std::set<temp::Temp *> added_temps;
};

} // namespace ra

#endif