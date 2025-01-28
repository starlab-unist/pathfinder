#include "exectree.h"

#include <iomanip>
#include <sstream>

#include "input_signature.h"
#include "options.h"

namespace pathfinder {

const PCID Node::EpsilonPCID = 0;
const ExecPath Node::EPSILON = ExecPath({Node::EpsilonPCID});

Node::Node(ExecTree* exectree_) : exectree(exectree_) {}
const size_t Node::MAX_INPUT_PER_PATH;
ExecPath Node::get_path_log(bool squeeze) {
  ExecPath prefix_ = squeeze && prefix == EPSILON ? ExecPath() : prefix;

  if (is_root()) return prefix_;
  return vec_concat(parent->get_path_log(squeeze), std::move(prefix_));
}
bool Node::is_root() const { return parent == nullptr; }
const ExecPath& Node::get_prefix() const { return prefix; }
const EnumArgBitVecArray& Node::get_enum_bvs() const { return enum_bvs; }
std::pair<std::vector<EnumCondition*>, std::vector<NumericCondition*>>
Node::get_path_cond() {
  ExecPath epath = get_path_log();
  std::vector<Node*> nodes = exectree->get_nodes(epath);

  std::vector<EnumCondition*> enum_conditions;
  std::vector<NumericCondition*> numeric_conditions;
  for (auto& node : nodes) {
    if (EnumCondition* enum_condition =
            dynamic_cast<EnumCondition*>(node->cond.get()))
      enum_conditions.push_back(enum_condition);
    else if (NumericCondition* numeric_condition =
                 dynamic_cast<NumericCondition*>(node->cond.get()))
      numeric_conditions.push_back(numeric_condition);
  }

  return std::make_pair(enum_conditions, numeric_conditions);
}
size_t Node::get_depth() const { return depth; }
std::pair<std::set<Input>, std::set<Input>> Node::get_examples() {
  std::set<Input> pos_examples, neg_examples;
  pos_examples = get_inputset();
  for (auto& sibling : get_siblings(false)) {
    std::set<Input> neg_examples_ = sibling->get_inputset();
    neg_examples.insert(neg_examples_.begin(), neg_examples_.end());
  }
  return std::make_pair(pos_examples, neg_examples);
}
std::optional<Node*> Node::get_sibling() const {
  if (is_root()) return std::nullopt;

  auto& siblings = parent->children;
  if (siblings.size() != 2) return std::nullopt;

  for (auto& node : siblings)
    if (node.get() != this) return node.get();

  throw Unreachable();
}
std::vector<Node*> Node::get_siblings(bool include_self) const {
  if (is_root()) {
    if (include_self)
      return {const_cast<Node*>(this)};
    else
      return {};
  }

  std::vector<Node*> sibling;
  for (size_t i = 0; i < parent->children.size(); i++) {
    Node* node = parent->children[i].get();
    if (!include_self && node == this) continue;
    sibling.push_back(node);
  }
  return sibling;
}
EnumArgBitVecArray Node::get_sibling_enum_bvs() const {
  EnumArgBitVecArray sibling_enum_bvs = initial_enum_bvs(false);
  for (auto node : get_siblings(false)) sibling_enum_bvs.bit_or(node->enum_bvs);
  return sibling_enum_bvs;
}
std::set<Node*> Node::evaluate_condition(Input input) {
  if (is_root()) return {};

  std::vector<Node*> evaluation_targets = get_siblings(true);

  std::set<Node*> incorrect_nodes;
  for (auto& node : evaluation_targets) {
    if (node->cond->invalid()) {
      incorrect_nodes.insert(node);
      continue;
    }

    bool is_this = node == this;
    if (!node->cond->eval_and_update(input, is_this))
      incorrect_nodes.insert(node);
  }
  return incorrect_nodes;
}
void Node::promote_cond() {
  assert(cond != nullptr);
  CondType condtype = cond->get_condtype();
  if (condtype == CT_ENUM) {
    cond = std::make_unique<NumericCondition>();
  } else if (condtype == CT_NUMERIC) {
    cond = std::make_unique<NeglectCondition>();
  }

  if (auto sibling = get_sibling()) {
    assert(sibling.value()->cond != nullptr);
    assert(sibling.value()->cond->get_condtype() == condtype);
    sibling.value()->cond = copy(cond);
  }
}
Node* Node::lowest_common_ancestor(Node* other) {
  // returns the lowest common ancestor of two nodes.

  Node* ancestor1 = this;
  Node* ancestor2 = other;

  // lift a lower node to be same with higher node.
  while (ancestor1->depth > ancestor2->depth) ancestor1 = ancestor1->parent;
  while (ancestor1->depth < ancestor2->depth) ancestor2 = ancestor2->parent;

  while (ancestor1 != ancestor2) {
    ancestor1 = ancestor1->parent;
    ancestor2 = ancestor2->parent;
  }

  return ancestor1;
}
std::string Node::to_string(bool print_prefix) const {
  std::string str;
  if (print_prefix)
    add_str(VERBOSE_LOW, str,
            indent(depth) + "prefix: " + epath_to_string(prefix) + "\n");

  add_str(VERBOSE_HIGH, str,
          indent(depth) + "depth: " + std::to_string(depth) + "\n");

  std::vector<std::string> bv_str_all = enum_bvs.to_string();
  std::string enum_str;
  for (size_t i = 0; i < bv_str_all.size(); i++) {
    std::string prefix =
        i == 0 ? indent(depth) + "enum: " : indent(depth) + "      ";
    enum_str += prefix + bv_str_all[i];
    if (i != bv_str_all.size() - 1) enum_str += ",";
    enum_str += "\n";
  }
  add_str(VERBOSE_HIGH, str, enum_str);

  if (cond != nullptr)
    add_str(VERBOSE_MID, str,
            indent(depth) + "cond: " + cond->to_string() + "\n");

  if (COLORIZE_OUTPUT && exception_path && str != "")
    str = std::string("\033[33m") + str + "\033[m";
  return str;
}

LeafNode::LeafNode(ExecTree* exectree_) : Node(exectree_) {}
bool LeafNode::struct_eq(const Node& other) const {
  if (!other.is_leaf()) return false;

  return prefix == other.prefix;
}
void LeafNode::insert_inputset(ExecPath epath_tail, std::set<Input> inputset_,
                               int run_status) {
  if (is_full()) {
    Input evicted = evict_random();
    exectree->all_input.erase(evicted);
  }

  for (auto& input : inputset_) {
    inputset.insert(input);
    exectree->all_input[input] = this;
  }
  update_enum_bvs();

  if (run_status == 0)
    exception_path = false;
  else if (run_status == PATHFINDER_EXPECTED_EXCEPTION)
    exception_path = true;
  if (!is_root()) parent->mark_exception();
  tail = epath_tail;
}
void LeafNode::merge_inputset(std::set<Input> other) {
  if (inputset.size() + other.size() <= MAX_INPUT_PER_PATH) {
    for (auto&& input : other) {
      exectree->all_input[input] = this;
    }
    inputset.insert(other.begin(), other.end());
  } else {
    size_t num_left = MAX_INPUT_PER_PATH - inputset.size();
    for (auto& input : other) {
      if (num_left == 0) {
        exectree->all_input.erase(input);
      } else {
        exectree->all_input[input] = this;
        inputset.insert(input);
        num_left--;
      }
    }
  }
  update_enum_bvs();
}
bool LeafNode::is_full() const { return inputset.size() >= MAX_INPUT_PER_PATH; }

std::pair<Node*, ExecPath> LeafNode::find(ExecPath epath) {
  assert(epath.size() > 0);
  if (epath == prefix) return std::make_pair(this, ExecPath());
  size_t common_len = common_prefix_length(prefix, epath);
  ExecPath common = subvec(prefix, 0, common_len);
  ExecPath epath_rem = subvec(epath, common_len);
  if (common != prefix) {
    if (is_root()) {
      return std::make_pair(nullptr, epath);
    } else {
      return std::make_pair(parent, epath);
    }
  }
  assert(epath_rem.size() > 0);
  return std::make_pair(this, epath_rem);
}
bool LeafNode::is_internal() const { return false; }
bool LeafNode::is_leaf() const { return true; }
void LeafNode::update_enum_bvs() {
  if (default_condtype() != CT_ENUM) return;

  EnumArgBitVecArray enum_bvs_old = EnumArgBitVecArray(enum_bvs);
  EnumArgBitVecArray enum_bvs_new = initial_enum_bvs(false);
  for (auto&& input : inputset) enum_bvs_new.set(input.get_enum_args());
  if (enum_bvs_new != enum_bvs_old) {
    enum_bvs = std::move(enum_bvs_new);
    if (!is_root()) parent->update_enum_bvs();
  }
}
void LeafNode::update_depth() {
  if (is_root()) {
    depth = 0;
  } else {
    depth = parent->get_depth() + 1;
  }

  if (depth > exectree->height) exectree->height = depth;
}
void LeafNode::filter_nd_pcid(TracePC* tpc, size_t prefix_len_so_far,
                              std::set<Node*>& filtered_nodes) {
  assert(tpc != nullptr);
  size_t prefix_before, prefix_after;
  if (prefix == Node::EPSILON) {
    prefix_before = 0;
    prefix_after = 0;
  } else {
    prefix_before = prefix.size();
    prefix = tpc->Prune(prefix);
    prefix_after = prefix.size();
  }

  size_t tail_before = tail.size();
  tail = tpc->Prune(tail);
  size_t tail_after = tail.size();

  bool tail_moved = false;
  if (prefix_len_so_far + prefix_after < tpc->ExecPathSignificantMax() &&
      tail_after > 0) {
    tail_moved = true;
    size_t len_to_be_moved = std::min(
        tpc->ExecPathSignificantMax() - (prefix_len_so_far + prefix_after),
        tail_after);
    prefix = prefix == Node::EPSILON
                 ? subvec(tail, 0, len_to_be_moved)
                 : vec_concat(prefix, subvec(tail, 0, len_to_be_moved));
    tail = subvec(tail, len_to_be_moved);
  } else if (prefix.size() == 0) {
    prefix = Node::EPSILON;
  }

  bool filtered =
      prefix_before != prefix_after || tail_before != tail_after || tail_moved;

  if (filtered) filtered_nodes.insert(this);
}
std::string LeafNode::to_string(bool print_prefix) const {
  std::string str = Node::to_string(print_prefix);

  if (inputset.size() > 0) {
    std::string inputset_str;
    size_t PRINT_ARG_MAX = 5;
    size_t i = 0;
    for (auto it = inputset.begin(); it != inputset.end(); it++) {
      std::string prefix =
          i == 0 ? indent(depth) + "input: {" : indent(depth) + "        ";
      if (i >= PRINT_ARG_MAX) {
        inputset_str += prefix + "... +" +
                        std::to_string(inputset.size() - PRINT_ARG_MAX) +
                        " inputs";
        break;
      }
      inputset_str += prefix + input_to_string(*it);
      if (i != inputset.size() - 1) {
        inputset_str += ",\n";
      }
      i++;
    }
    inputset_str += "}\n";

    if (exception_path && inputset_str != "")
      inputset_str = std::string("\033[33m") + inputset_str + "\033[m";

    add_str(VERBOSE_MID, str, inputset_str);
  }

  return str;
}
Input LeafNode::evict_random() {
  assert(is_full());
  size_t pos = (std::rand() % inputset.size());
  auto it = inputset.begin();
  std::advance(it, pos);
  Input evicted = *it;
  inputset.erase(it);
  return evicted;
}
std::set<Input> LeafNode::get_inputset() { return inputset; }

InternalNode::InternalNode(ExecTree* exectree_) : Node(exectree_) {}
bool InternalNode::struct_eq(const Node& other) const {
  if (!other.is_internal()) return false;

  if (prefix != other.prefix) return false;

  const InternalNode* other_ = as_internal(&other);

  if (children.size() != other_->children.size()) return false;

  for (size_t i = 0; i < children.size(); i++)
    if (!children[i]->struct_eq(*other_->children[i])) return false;

  return true;
}
Node* InternalNode::add_child(std::unique_ptr<Node> node) {
  Node* node_raw = node.get();

  node->parent = this;

  auto it = children.begin();
  while (it != children.end()) {
    if (it->get() == nullptr) {
      it = children.erase(it);
      continue;
    }
    if ((*it)->get_prefix()[0] > node->get_prefix()[0]) break;
    ++it;
  }
  it = children.insert(it, std::move(node));

  update_enum_bvs();

  return node_raw;
}
Node* InternalNode::lookup_child(PCID pcid) {
  size_t match_idx;
  for (match_idx = 0; match_idx < children.size(); match_idx++)
    if (children[match_idx]->get_prefix()[0] == pcid) break;

  if (match_idx == children.size()) {
    return nullptr;
  } else {
    return children[match_idx].get();
  }
}
Node* InternalNode::lookup_child(ExecPath prefix_) {
  size_t match_idx;
  for (match_idx = 0; match_idx < children.size(); match_idx++)
    if (children[match_idx]->get_prefix() == prefix_) break;

  if (match_idx == children.size()) {
    return nullptr;
  } else {
    return children[match_idx].get();
  }
}
void InternalNode::squeeze_children() {
  auto it = children.begin();
  while (it != children.end()) {
    if (it->get() == nullptr) {
      it = children.erase(it);
      continue;
    }
    ++it;
  }
}
void InternalNode::mark_exception() {
  bool children_all_exception = true;
  for (auto&& child : children)
    if (!child->exception_path) {
      children_all_exception = false;
      break;
    }

  bool inconsistent = exception_path != children_all_exception;
  if (inconsistent || cond == nullptr ||
      cond->invalid()) {  // need update or is a new node
    exception_path = children_all_exception;
    if (!is_root()) parent->mark_exception();
  }
}
std::optional<CondType> InternalNode::get_children_condtype() const {
  assert(!is_leaf());
  CondType children_condtype = children[0]->cond->get_condtype();
  for (size_t i = 1; i < children.size(); i++)
    if (children[i]->cond->get_condtype() != children_condtype)
      return std::nullopt;
  return children_condtype;
}
void InternalNode::initialize_children_cond(CondType condtype) {
  assert(!is_leaf());

  for (auto&& child : children) {
    if (condtype == CT_ENUM)
      child->cond = std::make_unique<EnumCondition>();
    else
      child->cond = std::make_unique<NumericCondition>();
  }
}
std::pair<Node*, ExecPath> InternalNode::find(ExecPath epath) {
  assert(epath.size() > 0);
  assert(!prefix.empty());
  if (is_root() && prefix == EPSILON) {
    Node* matched_child = lookup_child(epath[0]);
    if (matched_child != nullptr) {
      return matched_child->find(epath);
    } else {
      return std::make_pair(nullptr, epath);
      ;
    }
  }
  if (epath == prefix) {
    Node* epsilon_child = lookup_child(EPSILON);
    if (epsilon_child != nullptr) {
      return epsilon_child->find(EPSILON);
    } else {
      return std::make_pair(this, ExecPath());
    }
  }
  size_t common_len = common_prefix_length(prefix, epath);
  ExecPath common = subvec(prefix, 0, common_len);
  ExecPath epath_rem = subvec(epath, common_len);
  if (common != prefix) {
    if (is_root()) {
      return std::make_pair(nullptr, epath);
    } else {
      return std::make_pair(parent, epath);
    }
  }
  assert(epath_rem.size() > 0);

  Node* matched_child = lookup_child(epath_rem[0]);
  if (matched_child != nullptr) {
    return matched_child->find(epath_rem);
  } else {
    return std::make_pair(this, epath_rem);
    ;
  }
}
bool InternalNode::is_internal() const { return true; }
bool InternalNode::is_leaf() const { return false; }
void InternalNode::update_enum_bvs() {
  if (default_condtype() != CT_ENUM) return;

  EnumArgBitVecArray enum_bvs_old = EnumArgBitVecArray(enum_bvs);
  EnumArgBitVecArray enum_bvs_new = initial_enum_bvs(false);
  for (auto&& child : children) enum_bvs_new.bit_or(child->get_enum_bvs());
  if (enum_bvs_new != enum_bvs_old) {
    enum_bvs = std::move(enum_bvs_new);
    if (!is_root()) parent->update_enum_bvs();
  }
}
void InternalNode::update_depth() {
  if (is_root()) {
    depth = 0;
  } else {
    depth = parent->depth + 1;
  }

  for (auto&& child : children)
    if (child->get_depth() != depth + 1) child->update_depth();
};
void InternalNode::filter_nd_pcid(TracePC* tpc, size_t prefix_len_so_far,
                                  std::set<Node*>& filtered_nodes) {
  assert(tpc != nullptr);
  size_t prefix_len;
  if (prefix == Node::EPSILON) {
    assert(is_root());
    prefix_len = 0;
  } else {
    size_t size_before = prefix.size();
    prefix = tpc->Prune(prefix);
    if (prefix.size() != size_before) {
      filtered_nodes.insert(this);
      if (prefix.size() == 0) {
        prefix = Node::EPSILON;
        prefix_len = 0;
      } else {
        prefix_len = prefix.size();
      }
    }
  }

  for (auto& child : children)
    child->filter_nd_pcid(tpc, prefix_len_so_far + prefix_len, filtered_nodes);
}
bool InternalNode::children_sorted() const {
  for (size_t i = 0; i < children.size() - 1; i++)
    if (!(children[i]->get_prefix()[0] < children[i + 1]->get_prefix()[0]))
      return false;

  return true;
}
bool InternalNode::has_two_or_more_children(bool recursive) const {
  if (children.size() < 2) return false;

  if (recursive)
    for (auto& child : children)
      if (child->is_internal() &&
          !as_internal(child.get())->has_two_or_more_children(recursive))
        return false;

  return true;
}
void InternalNode::init_cond_of_paired_siblings() {
  if (children.size() == 2) {
    if (*children[0]->cond == *default_branch_condition()) {
      children[1]->cond = default_branch_condition();
    } else if (*children[1]->cond == *default_branch_condition()) {
      children[0]->cond = default_branch_condition();
    }
  }

  for (auto& child : children)
    if (child->is_internal())
      as_internal(child.get())->init_cond_of_paired_siblings();
}
std::string InternalNode::to_string(bool print_prefix) const {
  std::string str = Node::to_string(print_prefix);

  for (size_t i = 0; i < children.size(); i++) {
    str += children[i]->to_string(print_prefix);
  }

  return str;
}
std::set<Input> InternalNode::get_inputset() {
  std::set<Input> gathered;
  for (auto&& child : children) {
    assert(child != nullptr);
    std::set<Input> inputset_ = child->get_inputset();
    gathered.insert(inputset_.begin(), inputset_.end());
  }
  return gathered;
}

InternalNode* as_internal(Node* node) {
  assert(node->is_internal());
  InternalNode* internal = dynamic_cast<InternalNode*>(node);
  assert(internal != nullptr);
  return internal;
}
const InternalNode* as_internal(const Node* node) {
  assert(node->is_internal());
  const InternalNode* internal = dynamic_cast<const InternalNode*>(node);
  assert(internal != nullptr);
  return internal;
}
LeafNode* as_leaf(Node* node) {
  assert(node->is_leaf());
  LeafNode* leaf = dynamic_cast<LeafNode*>(node);
  assert(leaf != nullptr);
  return leaf;
}
const LeafNode* as_leaf(const Node* node) {
  assert(node->is_leaf());
  const LeafNode* leaf = dynamic_cast<const LeafNode*>(node);
  assert(leaf != nullptr);
  return leaf;
}

ExecTree::ExecTree(TracePC* tpc_) : tpc(tpc_) {}
bool ExecTree::is_empty() const { return root == nullptr; }
void ExecTree::set_root(std::unique_ptr<Node> root_) {
  root = std::move(root_);
  root->depth = 0;
}
Node* ExecTree::get_root() const { return root.get(); }

std::unique_ptr<LeafNode> ExecTree::create_leaf(ExecPath prefix) {
  std::unique_ptr<LeafNode> leaf = std::make_unique<LeafNode>(this);
  leaf->prefix = prefix;
  leaf->enum_bvs = initial_enum_bvs(false);

  return std::move(leaf);
}
std::unique_ptr<InternalNode> ExecTree::create_internal(ExecPath prefix) {
  std::unique_ptr<InternalNode> internal = std::make_unique<InternalNode>(this);
  internal->prefix = prefix;
  internal->enum_bvs = initial_enum_bvs(false);

  return std::move(internal);
}
Node* ExecTree::add_node(std::unique_ptr<Node> node, InternalNode* parent,
                         std::unique_ptr<BranchCondition> cond) {
  assert(!node->prefix.empty());

  Node* node_raw = node.get();

  if (node->is_internal()) {
    internals.insert(as_internal(node_raw));
  } else {
    LeafNode* node_raw_ = as_leaf(node_raw);
    leaves.insert(node_raw_);
    for (auto&& input : node_raw_->inputset) all_input[input] = node_raw_;
  }

  if (parent == nullptr) {
    // Root node
    node->cond = std::make_unique<NeglectCondition>();
    assert(root == nullptr);  // previous root must be moved already
    root = std::move(node);
    root->parent = nullptr;
  } else {
    node->cond = cond != nullptr ? std::move(cond) : default_branch_condition();
    parent->add_child(std::move(node));
    parent->mark_exception();
  }
  node_raw->update_depth();
  return node_raw;
}
void ExecTree::add_nodes(std::vector<std::unique_ptr<Node>> nodes,
                         InternalNode* parent) {
  for (auto& node : nodes) add_node(std::move(node), parent);
}
std::unique_ptr<Node> ExecTree::pull_node(Node* node) {
  assert(node != nullptr);

  if (node->is_internal()) {
    assert(internals.find(as_internal(node)) != internals.end());
    internals.erase(as_internal(node));
  } else {
    assert(leaves.find(as_leaf(node)) != leaves.end());
    leaves.erase(as_leaf(node));
    for (auto&& input : as_leaf(node)->inputset) all_input.erase(input);
  }

  if (node->is_root()) return std::move(root);

  InternalNode* parent = node->parent;

  size_t match_idx;
  for (match_idx = 0; match_idx < parent->children.size(); match_idx++)
    if (parent->children[match_idx].get() == node) break;

  assert(match_idx < parent->children.size());

  std::unique_ptr<Node> pulled = std::move(parent->children[match_idx]);
  parent->squeeze_children();

  assert(pulled != nullptr);

  return pulled;
}
std::vector<std::unique_ptr<Node>> ExecTree::pull_children(Node* node) {
  assert(node->is_internal());

  std::vector<Node*> children_raw;
  for (auto& child : as_internal(node)->children)
    children_raw.push_back(child.get());

  std::vector<std::unique_ptr<Node>> pulled;
  for (auto& child : children_raw) pulled.push_back(pull_node(child));

  return std::move(pulled);
}
Node* ExecTree::insert(ExecPath epath, Input input, int run_status) {
  return insert(epath, std::set<Input>({input}), run_status);
}
Node* ExecTree::insert(ExecPath epath, std::set<Input> inputset,
                       int run_status) {
  ExecPath epath_significant = tpc->significant(epath);
  ExecPath epath_tail = tpc->tail_of(epath);
  if (is_empty()) {
    // Case 1: First insertion.
    //         Init with a leaf node.

    std::unique_ptr<LeafNode> new_root = create_leaf(epath_significant);
    new_root->insert_inputset(epath_tail, inputset, run_status);
    return add_node(std::move(new_root), nullptr);
  }

  Node* nearest;
  ExecPath epath_rem;
  std::tie(nearest, epath_rem) = root->find(epath_significant);

  if (nearest == nullptr) {
    // Case 2: `epath` does not go through current root.
    //         Add an internal node as a new root, and add a new leaf node.

    if (root->prefix == Node::EPSILON) {
      assert(root->is_internal() &&
             as_internal(root.get())->lookup_child(epath_rem[0]) == nullptr);
      assert(epath_rem == epath);
      std::unique_ptr<LeafNode> new_leaf = create_leaf(epath_rem);
      new_leaf->insert_inputset(epath_tail, inputset, run_status);
      Node* new_leaf_ = add_node(std::move(new_leaf), as_internal(root.get()));
      as_internal(root.get())->initialize_children_cond(default_condtype());
      return new_leaf_;
    }

    size_t common_len = common_prefix_length(root->prefix, epath_rem);
    ExecPath common =
        common_len == 0 ? Node::EPSILON : subvec(root->prefix, 0, common_len);

    std::unique_ptr<InternalNode> new_root = create_internal(common);
    std::unique_ptr<Node> old_root = pull_node(root.get());
    assert(old_root->prefix.size() > common_len);
    old_root->prefix = subvec(old_root->prefix, common_len);
    add_node(std::move(old_root), new_root.get());
    Node* new_root_ = add_node(std::move(new_root), nullptr);

    epath_rem = epath_rem.size() == common_len ? Node::EPSILON
                                               : subvec(epath_rem, common_len);
    std::unique_ptr<LeafNode> new_leaf = create_leaf(epath_rem);
    new_leaf->insert_inputset(epath_tail, inputset, run_status);
    Node* new_leaf_ = add_node(std::move(new_leaf), as_internal(new_root_));
    as_internal(new_root_)->initialize_children_cond(default_condtype());
    return new_leaf_;
  }

  if (epath_rem.size() == 0) {
    if (nearest->is_internal()) {
      // Case 3: `epath` stops at an internal node.
      //         Add a new leaf with epsilon prefix.

      std::unique_ptr<LeafNode> new_leaf = create_leaf(Node::EPSILON);
      new_leaf->insert_inputset(epath_tail, inputset, run_status);
      return add_node(std::move(new_leaf), as_internal(nearest));
    } else {
      // Case 4: Found an existing leaf.
      //         Insert `input` to it.

      Node* leaf = nearest;
      as_leaf(leaf)->insert_inputset(epath_tail, inputset, run_status);
      return leaf;
    }
  }

  if (nearest->is_internal()) {
    InternalNode* nearest_ = as_internal(nearest);
    Node* matched_child = nearest_->lookup_child(epath_rem[0]);
    if (matched_child == nullptr) {
      // Case 5: Add a new leaf to exising internal node `nearest`.

      std::unique_ptr<LeafNode> leaf = create_leaf(epath_rem);
      leaf->insert_inputset(epath_tail, inputset, run_status);
      return add_node(std::move(leaf), nearest_);
    } else {
      // Case 6: Add a new internal node between `nearest` and one of its
      // children,
      //         and add a new leaf to it.

      std::unique_ptr<Node> pulled = pull_node(matched_child);

      size_t common_len = common_prefix_length(pulled->prefix, epath_rem);
      assert(0 < common_len && common_len < pulled->prefix.size());
      ExecPath common = subvec(pulled->prefix, 0, common_len);

      std::unique_ptr<InternalNode> internal = create_internal(common);
      std::unique_ptr<BranchCondition> internal_cond = std::move(pulled->cond);
      pulled->prefix = subvec(pulled->prefix, common_len);
      add_node(std::move(pulled), internal.get());
      Node* internal_ =
          add_node(std::move(internal), nearest_, std::move(internal_cond));

      epath_rem = epath_rem.size() == common_len
                      ? Node::EPSILON
                      : subvec(epath_rem, common_len);
      std::unique_ptr<LeafNode> leaf = create_leaf(epath_rem);
      leaf->insert_inputset(epath_tail, inputset, run_status);
      return add_node(std::move(leaf), as_internal(internal_));
    }
  }

  else {
    // Case 7: `nearest` is a leaf.
    //         Add a new internal node, and make `nearest` to its epsilon child.
    //         And a new leaf to the internal node.

    std::unique_ptr<Node> pulled = pull_node(nearest);
    std::unique_ptr<InternalNode> internal = create_internal(pulled->prefix);
    InternalNode* internal_parent = pulled->parent;
    std::unique_ptr<BranchCondition> internal_cond = std::move(pulled->cond);
    pulled->prefix = Node::EPSILON;
    add_node(std::move(pulled), internal.get());

    std::unique_ptr<LeafNode> leaf = create_leaf(epath_rem);
    leaf->insert_inputset(epath_tail, inputset, run_status);
    Node* leaf_ = add_node(std::move(leaf), internal.get());

    add_node(std::move(internal), internal_parent, std::move(internal_cond));
    return leaf_;
  }
}
std::unique_ptr<Node> ExecTree::purge_leaf(ExecPath epath) {
  Node* leaf_raw = find(epath);
  assert(leaf_raw != nullptr);
  assert(leaf_raw->is_leaf());

  InternalNode* parent = leaf_raw->parent;
  std::unique_ptr<Node> leaf = pull_node(leaf_raw);

  if (parent != nullptr) rm_internal_with_only_child(parent);

  return std::move(leaf);
}
void ExecTree::purge_and_reinsert(ExecPath epath_old, ExecPath epath_new) {
  std::unique_ptr<Node> leaf_old = purge_leaf(epath_old);
  std::set<Input> inputset = as_leaf(leaf_old.get())->inputset;
  bool exception_path = leaf_old->exception_path;
  Node* leaf_new = insert(epath_new, inputset, exception_path);
}
const std::set<InternalNode*>& ExecTree::get_internals() const {
  return internals;
}
const std::set<LeafNode*>& ExecTree::get_leaves() const { return leaves; }
LeafNode* ExecTree::get_leaf(Input input) {
  assert(has(input));
  return all_input[input];
}
Node* ExecTree::find(ExecPath epath) {
  Node* nearest;
  ExecPath epath_rem;
  std::tie(nearest, epath_rem) = root->find(tpc->significant(epath));

  if (epath_rem.size() > 0) return nullptr;
  return nearest;
}
bool ExecTree::has(ExecPath epath) {
  if (is_empty()) return false;

  return find(epath) != nullptr;
}
bool ExecTree::has(Input input) {
  return all_input.find(input) != all_input.end();
}
ExecPath ExecTree::get_path(Input input) {
  LeafNode* leaf = all_input.find(input)->second;
  assert(leaf != nullptr);
  return vec_concat(leaf->get_path_log(true), leaf->tail);
}
std::vector<Node*> ExecTree::get_nodes(ExecPath epath) {
  // gather nodes along epath.
  // `epath` may involve epsilon child.

  assert(epath.size() > 0);

  std::vector<Node*> nodes;
  assert(!is_empty());
  Node* current = root.get();

  while (true) {
    nodes.push_back(current);

    size_t common_len = common_prefix_length(current->prefix, epath);
    ExecPath common = subvec(current->prefix, 0, common_len);
    epath = subvec(epath, common_len);

    if (common != current->prefix) {
      assert(current == root.get() && root->prefix == Node::EPSILON);
    }

    if (current->is_leaf()) {
      assert(epath.empty());
      break;
    }

    InternalNode* current_ = as_internal(current);

    if (epath.empty()) {
      assert(current_->children[0]->is_leaf() &&
             current_->children[0]->prefix == Node::EPSILON);
      nodes.push_back(current_->children[0].get());
      break;
    }

    Node* matched_child = current_->lookup_child(epath[0]);
    assert(matched_child != nullptr);
    current = matched_child;
  }
  return nodes;
}
std::set<Node*> ExecTree::evaluate_conditions(Input input, ExecPath epath) {
  // returns nodes whose condition is not consistent with input
  std::vector<Node*> nodes = get_nodes(tpc->significant(epath));
  std::set<Node*> incorrect_nodes;
  for (auto& node : nodes) {
    auto incorrect_nodes_ = node->evaluate_condition(input);
    incorrect_nodes.insert(incorrect_nodes_.begin(), incorrect_nodes_.end());
  }
  return incorrect_nodes;
}
std::set<Node*> ExecTree::get_all_nodes() const {
  std::set<Node*> all_nodes;
  all_nodes.insert(internals.begin(), internals.end());
  all_nodes.insert(leaves.begin(), leaves.end());
  return all_nodes;
}
bool ExecTree::has(Node* node) const {
  if (internals.find(static_cast<InternalNode*>(node)) != internals.end()) {
    return true;
  } else if (leaves.find(static_cast<LeafNode*>(node)) != leaves.end()) {
    return true;
  }
  return false;
}
void ExecTree::rm_internal_epsilon_node(InternalNode* internal) {
  assert(has(internal));
  assert(!internal->is_root());

  if (internal->prefix != Node::EPSILON) return;

  InternalNode* parent = internal->parent;
  std::unique_ptr<Node> internal_epsilon_node = pull_node(internal);
  std::vector<std::unique_ptr<Node>> children = pull_children(internal);
  add_nodes(std::move(children), parent);
}
std::set<Node*> ExecTree::sort(InternalNode* internal) {
  if (internal->children_sorted()) return {};

  std::set<Node*> merged_nodes;

  std::vector<std::unique_ptr<Node>> nodes = pull_children(internal);
  for (auto& node : nodes) {
    Node* conflict = internal->lookup_child(node->prefix[0]);

    if (conflict == nullptr) {
      add_node(std::move(node), internal);
      continue;
    }

    std::unique_ptr<Node> conflicting_node = pull_node(conflict);
    std::unique_ptr<Node> merged_node =
        merge(std::move(conflicting_node), std::move(node));
    merged_nodes.insert(merged_node.get());
    add_node(std::move(merged_node), internal);
  }

  return merged_nodes;
}
std::unique_ptr<Node> ExecTree::merge(std::unique_ptr<Node> left,
                                      std::unique_ptr<Node> right) {
  assert(left != nullptr && right != nullptr);
  assert(!left->is_root() && !right->is_root());
  assert(left->parent == right->parent);

  size_t common_len = common_prefix_length(left->prefix, right->prefix);
  assert(common_len > 0);
  ExecPath common = subvec(left->prefix, 0, common_len);

  if (left->prefix == right->prefix) {
    ExecPath prefix = common;
    assert(prefix == left->prefix);

    if (left->is_leaf() && right->is_leaf()) {
      LeafNode* left_ = as_leaf(left.get());
      LeafNode* right_ = as_leaf(right.get());

      std::unique_ptr<LeafNode> new_leaf = create_leaf(prefix);
      new_leaf->merge_inputset(std::move(left_->inputset));
      new_leaf->merge_inputset(std::move(right_->inputset));
      new_leaf->tail = left_->tail;
      new_leaf->exception_path = left_->exception_path;
      return std::move(new_leaf);
    }

    assert(prefix != Node::EPSILON);

    if (left->is_leaf() && right->is_internal()) {
      left->prefix = Node::EPSILON;
      add_node(std::move(left), as_internal(right.get()));
      return std::move(right);
    }

    if (left->is_internal() && right->is_leaf()) {
      right->prefix = Node::EPSILON;
      add_node(std::move(right), as_internal(left.get()));
      return std::move(left);
    }

    assert(left->is_internal() && right->is_internal());

    std::unique_ptr<InternalNode> new_internal = create_internal(prefix);
    add_nodes(std::move(as_internal(left.get())->children), new_internal.get());
    add_nodes(std::move(as_internal(right.get())->children),
              new_internal.get());
    return std::move(new_internal);
  }

  if (left->prefix.size() == common_len) {
    assert(right->prefix.size() > common_len);
    std::unique_ptr<InternalNode> new_internal = create_internal(common);

    if (left->is_leaf()) {
      left->prefix = Node::EPSILON;
      add_node(std::move(left), new_internal.get());
    } else {
      add_nodes(std::move(as_internal(left.get())->children),
                new_internal.get());
    }

    right->prefix = subvec(right->prefix, common_len);
    add_node(std::move(right), new_internal.get());

    return std::move(new_internal);
  }

  if (right->prefix.size() == common_len) {
    assert(left->prefix.size() > common_len);
    std::unique_ptr<InternalNode> new_internal = create_internal(common);

    left->prefix = subvec(left->prefix, common_len);
    add_node(std::move(left), new_internal.get());

    if (right->is_leaf()) {
      right->prefix = Node::EPSILON;
      add_node(std::move(right), new_internal.get());
    } else {
      add_nodes(std::move(as_internal(right.get())->children),
                new_internal.get());
    }

    return std::move(new_internal);
  }

  assert(left->prefix.size() > common_len && right->prefix.size() > common_len);

  std::unique_ptr<InternalNode> new_internal = create_internal(common);

  left->prefix = subvec(left->prefix, common_len);
  add_node(std::move(left), new_internal.get());

  right->prefix = subvec(right->prefix, common_len);
  add_node(std::move(right), new_internal.get());

  return std::move(new_internal);
}
void ExecTree::rm_internal_with_only_child(InternalNode* internal) {
  assert(has(internal));

  if (internal->has_two_or_more_children()) return;

  InternalNode* parent = internal->parent;
  std::unique_ptr<Node> internal_with_only_child = pull_node(internal);
  assert(as_internal(internal_with_only_child.get())->children.size() == 1);
  std::unique_ptr<Node> only_child =
      pull_node(as_internal(internal_with_only_child.get())->children[0].get());

  if (internal_with_only_child->prefix == Node::EPSILON) {
    assert(internal_with_only_child->is_root());
    only_child->prefix = only_child->prefix;
  } else if (only_child->prefix == Node::EPSILON) {
    assert(only_child->is_leaf());
    only_child->prefix = internal_with_only_child->prefix;
  } else {
    only_child->prefix =
        vec_concat(internal_with_only_child->prefix, only_child->prefix);
  }

  add_node(std::move(only_child), parent);
}
std::set<Node*> ExecTree::invalid_condition_nodes() const {
  std::set<Node*> invalid_nodes;
  for (auto& node : get_all_nodes()) {
    if (auto sibling = node->get_sibling())
      if (invalid_nodes.find(sibling.value()) != invalid_nodes.end()) continue;

    if (node->cond->invalid()) {
      invalid_nodes.insert(node);
      continue;
    }
  }
  return invalid_nodes;
}
void ExecTree::prune() {
  if (is_empty()) return;

  bool recursive = true;

  std::set<Node*> filtered_nodes;
  root->filter_nd_pcid(tpc, 0, filtered_nodes);

  std::set<InternalNode*> may_need_sort;
  for (auto& filtered_node : filtered_nodes)
    if (!filtered_node->is_root()) may_need_sort.insert(filtered_node->parent);

  assert(no_empty_prefixed_node());

  for (auto& filtered_node : filtered_nodes)
    if (filtered_node->is_internal() && !filtered_node->is_root())
      rm_internal_epsilon_node(as_internal(filtered_node));

  assert(no_empty_prefixed_node());
  assert(no_epsilon_internal_node());

  std::set<InternalNode*> may_have_only_child;
  while (true) {
    if (may_need_sort.empty()) break;

    auto it = may_need_sort.begin();
    InternalNode* target = *it;
    may_need_sort.erase(it);

    if (!has(target)) continue;

    std::set<Node*> merged_nodes = sort(target);
    if (!merged_nodes.empty()) {
      may_have_only_child.insert(target);
      for (auto& merged_node : merged_nodes)
        if (has(merged_node) && merged_node->is_internal())
          may_need_sort.insert(as_internal(merged_node));
    }
  }

  assert(no_empty_prefixed_node());
  assert(no_epsilon_internal_node());
  assert(sorted());

  for (auto& internal : may_have_only_child)
    if (has(internal)) rm_internal_with_only_child(internal);

  assert(no_empty_prefixed_node());
  assert(no_epsilon_internal_node());
  assert(sorted());
}

bool ExecTree::no_empty_prefixed_node() const {
  for (auto& node : get_all_nodes()) {
    assert(has(node));
    if (node->prefix.empty()) return false;
  }

  return true;
}
bool ExecTree::no_epsilon_internal_node() const {
  for (auto& internal : internals) {
    assert(has(internal));
    if (internal->prefix == Node::EPSILON && !internal->is_root()) return false;
  }

  return true;
}
bool ExecTree::sorted() const {
  for (auto& internal : internals) {
    assert(has(internal));
    if (!internal->children_sorted()) return false;
  }

  return true;
}
bool ExecTree::no_only_child_internal_node() const {
  for (auto& internal : internals) {
    assert(has(internal));
    if (!internal->has_two_or_more_children()) return false;
  }

  return true;
}

bool ExecTree::is_sorted() const {
  return no_empty_prefixed_node() && no_epsilon_internal_node() && sorted() &&
         no_only_child_internal_node();
}
size_t ExecTree::total_prefix_length() const {
  size_t total_length = 0;
  for (auto& node : get_all_nodes())
    if (node->prefix != Node::EPSILON) total_length += node->prefix.size();
  return total_length;
}
size_t ExecTree::num_total_input() const {
  return all_input.size();
  ;
}
std::string ExecTree::to_string(bool print_epath) const {
  if (is_empty()) {
    return "";
  } else {
    return root->to_string(print_epath);
  }
}

bool struct_eq(const ExecTree& left, const ExecTree& right) {
  if (left.is_empty() && right.is_empty()) return true;

  if (left.is_empty() || right.is_empty()) return true;

  return left.root->struct_eq(*right.root);
}

}  // namespace pathfinder
