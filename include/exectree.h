#ifndef PATHFINDER_EXECTREE
#define PATHFINDER_EXECTREE

#include "branch_condition.h"
#include "pathfinder_defs.h"
#include "sygus_ast.h"
#include "trace_pc.h"
#include "utils.h"

namespace pathfinder {

class PathConflict : public std::exception {};

// Radix tree based path tree
class ExecTree;

class LeafNode;
class InternalNode;

class Node {
 public:
  static const size_t MAX_INPUT_PER_PATH = 100;
  static const PCID EpsilonPCID;
  static const ExecPath EPSILON;

  Node(ExecTree* exectree_);
  virtual ~Node() = default;
  virtual bool struct_eq(const Node& other) const = 0;
  virtual std::pair<Node*, ExecPath> find(ExecPath epath) = 0;
  ExecPath get_path_log(bool squeeze = false);
  bool is_root() const;
  virtual bool is_internal() const = 0;
  virtual bool is_leaf() const = 0;
  const ExecPath& get_prefix() const;
  const EnumArgBitVecArray& get_enum_bvs() const;
  std::pair<std::vector<EnumCondition*>, std::vector<NumericCondition*>>
  get_path_cond();
  std::set<Node*> evaluate_condition(Input input);

  void promote_cond();
  Node* lowest_common_ancestor(Node* other);
  virtual void filter_nd_pcid(TracePC* tpc, size_t prefix_len_so_far,
                              std::set<Node*>& filtered_nodes) = 0;
  virtual void update_enum_bvs() = 0;
  virtual void update_depth() = 0;
  size_t get_depth() const;

  virtual std::string to_string(bool print_prefix = false) const;

 protected:
  ExecTree* exectree = nullptr;
  InternalNode* parent = nullptr;
  ExecPath prefix;
  EnumArgBitVecArray enum_bvs;
  std::unique_ptr<BranchCondition> cond;
  size_t depth = 0;
  bool exception_path = false;

 private:
  virtual std::set<Input> get_inputset() = 0;
  std::optional<Node*> get_sibling() const;
  std::vector<Node*> get_siblings(bool include_self) const;
  EnumArgBitVecArray get_sibling_enum_bvs() const;
  std::pair<std::set<Input>, std::set<Input>> get_examples();

  // TODO: remove this workaround
  friend class LeafNode;
  friend class InternalNode;
  friend class ExecTree;
  friend class Engine;
};

class LeafNode : public Node {
 public:
  LeafNode(ExecTree* exectree_);
  virtual bool struct_eq(const Node& other) const override;

  void insert_inputset(ExecPath epath_tail, std::set<Input> inputset_,
                       int run_status);
  void merge_inputset(std::set<Input> inputset);
  bool is_full() const;

  virtual std::pair<Node*, ExecPath> find(ExecPath epath) override;
  virtual bool is_internal() const override;
  virtual bool is_leaf() const override;
  virtual void update_enum_bvs() override;
  virtual void update_depth() override;

  virtual void filter_nd_pcid(TracePC* tpc, size_t prefix_len_so_far,
                              std::set<Node*>& filtered_nodes) override;

  virtual std::string to_string(bool print_prefix = false) const override;

 private:
  Input evict_random();
  virtual std::set<Input> get_inputset() override;

  std::set<Input> inputset;
  ExecPath tail;

  // TODO: remove this workaround
  friend class Node;
  friend class ExecTree;
  friend class Engine;
};

class InternalNode : public Node {
 public:
  InternalNode(ExecTree* exectree_);
  virtual bool struct_eq(const Node& other) const override;

  Node* add_child(std::unique_ptr<Node> node);
  void squeeze_children();
  void mark_exception();
  std::optional<CondType> get_children_condtype() const;
  void initialize_children_cond(CondType condtype);

  virtual std::pair<Node*, ExecPath> find(ExecPath epath) override;
  virtual bool is_internal() const override;
  virtual bool is_leaf() const override;
  virtual void update_enum_bvs() override;
  virtual void update_depth() override;

  virtual void filter_nd_pcid(TracePC* tpc, size_t prefix_len_so_far,
                              std::set<Node*>& filtered_nodes) override;

  bool children_sorted() const;
  bool has_two_or_more_children(bool recursive = false) const;
  void init_cond_of_paired_siblings();

  virtual std::string to_string(bool print_prefix = false) const override;

 private:
  virtual std::set<Input> get_inputset() override;
  Node* lookup_child(PCID pcid);
  Node* lookup_child(ExecPath prefix_);

  std::vector<std::unique_ptr<Node>> children;

  // TODO: remove this workaround
  friend class Node;
  friend class ExecTree;
  friend class Engine;
};

InternalNode* as_internal(Node* node);
const InternalNode* as_internal(const Node* node);

LeafNode* as_leaf(Node* node);
const LeafNode* as_leaf(const Node* node);

class ExecTree {
 public:
  ExecTree(TracePC* tpc_);
  bool is_empty() const;
  void set_root(std::unique_ptr<Node> root_);
  Node* get_root() const;
  Node* insert(ExecPath epath, Input input, int run_status);
  Node* insert(ExecPath epath, std::set<Input> inputset, int run_status);
  void purge_and_reinsert(ExecPath epath_old, ExecPath epath_new);
  const std::set<InternalNode*>& get_internals() const;
  const std::set<LeafNode*>& get_leaves() const;
  Node* find(ExecPath epath);
  bool has(ExecPath epath);
  bool has(Input input);
  LeafNode* get_leaf(Input input);
  ExecPath get_path(Input input);
  std::vector<Node*> get_nodes(ExecPath epath);
  std::set<Node*> evaluate_conditions(Input input, ExecPath epath);
  std::set<Node*> invalid_condition_nodes() const;
  void prune();
  bool is_sorted() const;
  size_t total_prefix_length() const;
  size_t num_total_input() const;
  std::string to_string(bool print_epath = false) const;

 private:
  std::unique_ptr<LeafNode> create_leaf(ExecPath prefix);
  std::unique_ptr<InternalNode> create_internal(ExecPath prefix);
  Node* add_node(std::unique_ptr<Node> node, InternalNode* parent,
                 std::unique_ptr<BranchCondition> cond = nullptr);
  void add_nodes(std::vector<std::unique_ptr<Node>> nodes,
                 InternalNode* parent);
  std::unique_ptr<Node> pull_node(Node* node);
  std::vector<std::unique_ptr<Node>> pull_children(Node* node);
  std::unique_ptr<Node> purge_leaf(ExecPath epath);

  std::set<Node*> get_all_nodes() const;
  bool has(Node* node) const;

  void rm_internal_epsilon_node(InternalNode* internal);
  std::set<Node*> sort(InternalNode* internal);
  std::unique_ptr<Node> merge(std::unique_ptr<Node> left,
                              std::unique_ptr<Node> right);
  void rm_internal_with_only_child(InternalNode* internal);

  bool no_empty_prefixed_node() const;
  bool no_epsilon_internal_node() const;
  bool sorted() const;
  bool no_only_child_internal_node() const;

  TracePC* tpc;

  std::unique_ptr<Node> root;
  std::set<InternalNode*> internals;
  std::set<LeafNode*> leaves;
  size_t height = 0;  // = max depth

  // Set of all inputs.
  // Used for checking conflict.
  std::map<Input, LeafNode*> all_input;

  friend class Node;
  friend class LeafNode;
  friend class InternalNode;
  friend class Engine;
  friend bool struct_eq(const ExecTree& left, const ExecTree& right);
};

// Used in unit tests
bool struct_eq(const ExecTree& left, const ExecTree& right);

}  // namespace pathfinder

#endif
