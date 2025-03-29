#include "vslc.h"

// Global root for abstract syntax tree
node_t* root;

// Declarations of helper functions defined further down in this file
static void node_print(node_t* node, int nesting);
static node_t* constant_fold_subtree(node_t* node);
static bool remove_unreachable_code(node_t* node);
static void destroy_subtree(node_t* discard);

// Initialize a node with the given type and children
node_t* node_create(node_type_t type, size_t n_children, ...)
{
  node_t* result = malloc(sizeof(node_t));

  // Initialize every field in the struct
  *result = (node_t){
      .type = type,
      .n_children = n_children,
      .children = malloc(n_children * sizeof(node_t*)),
      .symbol = NULL,
  };

  // Read each child node from the va_list
  va_list child_list;
  va_start(child_list, n_children);
  for (size_t i = 0; i < n_children; i++)
  {
    result->children[i] = va_arg(child_list, node_t*);
  }
  va_end(child_list);

  return result;
}

// Append an element to the given LIST node, returns the list node
node_t* append_to_list_node(node_t* list_node, node_t* element)
{
  assert(list_node->type == LIST);

  // Calculate the minimum size of the new allocation
  size_t min_allocation_size = list_node->n_children + 1;

  // Round up to the next power of two
  size_t new_allocation_size = 1;
  while (new_allocation_size < min_allocation_size)
    new_allocation_size *= 2;

  // Resize the allocation
  list_node->children = realloc(list_node->children, new_allocation_size * sizeof(node_t*));

  // Insert the new element and increase child count by 1
  list_node->children[list_node->n_children] = element;
  list_node->n_children++;

  return list_node;
}

// Outputs the entire syntax tree to the terminal
void print_syntax_tree(void)
{
  // If the environment variable GRAPHVIZ_OUTPUT is set, print a GraphViz graph in the dot format
  if (getenv("GRAPHVIZ_OUTPUT") != NULL)
    graphviz_node_print(root);
  else
    node_print(root, 0);
}

// Performs constant folding and removes unconditional conditional branches
void constant_fold_syntax_tree(void)
{
  root = constant_fold_subtree(root);
}

// Removes code that is never reached due to return and break statements.
// Also ensures execution never reaches the end of a function without reaching a return statement.
void remove_unreachable_code_syntax_tree(void)
{
  for (size_t i = 0; i < root->n_children; i++)
  {
    node_t* child = root->children[i];
    if (child->type != FUNCTION)
      continue;

    node_t* function_body = child->children[2];

    bool has_return = remove_unreachable_code(function_body);

    // If the function body is not guaranteed to call return, we wrap it in a BLOCK like so:
    // {
    //   original_function_body
    //   return 0
    // }
    if (!has_return)
    {
      node_t* zero_node = node_create(NUMBER_LITERAL, 0);
      zero_node->data.number_literal = 0;
      node_t* return_node = node_create(RETURN_STATEMENT, 1, zero_node);
      node_t* statement_list = node_create(LIST, 2, function_body, return_node);
      node_t* new_function_body = node_create(BLOCK, 1, statement_list);
      child->children[2] = new_function_body;
    }
  }
}

// Frees all memory held by the syntax tree
void destroy_syntax_tree(void)
{
  destroy_subtree(root);
  root = NULL;
}

// The rest of this file contains private helper functions used by the above functions

// Prints out the given node and all its children recursively
static void node_print(node_t* node, int nesting)
{
  // Indent the line based on how deep the node is in the syntax tree
  printf("%*s", nesting, "");

  if (node == NULL)
  {
    printf("(NULL)\n");
    return;
  }

  printf("%s", NODE_TYPE_NAMES[node->type]);

  // For nodes with extra data, include it in the printout
  switch (node->type)
  {
  case OPERATOR:
    printf(" (%s)", node->data.operator);
    break;
  case IDENTIFIER:
    printf(" (%s)", node->data.identifier);
    break;
  case NUMBER_LITERAL:
    printf(" (%ld)", node->data.number_literal);
    break;
  case STRING_LITERAL:
    printf(" (%s)", node->data.string_literal);
    break;
  case STRING_LIST_REFERENCE:
    printf(" (%zu)", node->data.string_list_index);
    break;
  default:
    break;
  }

  // If the node is a reference to a symbol, print its type and number
  if (node->symbol)
  {
    printf(" %s(%zu)", SYMBOL_TYPE_NAMES[node->symbol->type], node->symbol->sequence_number);
  }

  putchar('\n');

  // Recursively print children, with some more indentation
  for (size_t i = 0; i < node->n_children; i++)
    node_print(node->children[i], nesting + 1);
}

// Constant folds the given OPERATOR node, if all children are NUMBER_LITERAL
static node_t* constant_fold_operator(node_t* node)
{
  assert(node->type == OPERATOR);

  // Check that all operands are NUMBER_LITERALs
  for (size_t i = 0; i < node->n_children; i++)
    if (node->children[i]->type != NUMBER_LITERAL)
      return node;

  const char* op = node->data.operator;

  // This is where we store the result of the constant fold
  int64_t result;

  if (node->n_children == 1)
  {
    int64_t operand = node->children[0]->data.number_literal;
    if (strcmp(op, "-") == 0)
      result = -operand;
    else if (strcmp(op, "!") == 0)
      result = !operand;
    else
      assert(false && "Unknown unary operator");
  }
  else if (node->n_children == 2)
  {
    int64_t lhs = node->children[0]->data.number_literal;
    int64_t rhs = node->children[1]->data.number_literal;
    if (strcmp(op, "==") == 0)
      result = lhs == rhs;
    else if (strcmp(op, "!=") == 0)
      result = lhs != rhs;
    else if (strcmp(op, "<") == 0)
      result = lhs < rhs;
    else if (strcmp(op, "<=") == 0)
      result = lhs <= rhs;
    else if (strcmp(op, ">") == 0)
      result = lhs > rhs;
    else if (strcmp(op, ">=") == 0)
      result = lhs >= rhs;
    else if (strcmp(op, "+") == 0)
      result = lhs + rhs;
    else if (strcmp(op, "-") == 0)
      result = lhs - rhs;
    else if (strcmp(op, "*") == 0)
      result = lhs * rhs;
    else if (strcmp(op, "/") == 0)
      result = lhs / rhs;
    else
      assert(false && "Unknown unary operator");
  }

  // Free all children, turn the node into a NUMBER_LITERAL
  for (size_t i = 0; i < node->n_children; i++)
    destroy_subtree(node->children[i]);

  node->type = NUMBER_LITERAL;
  node->data.number_literal = result;
  node->n_children = 0;
  return node;
}

// If the condition of the given if node is a NUMBER_LITERAL, the if is replaced by the taken
// branch. If the if condition is false, and the if has no else-body, NULL is returned.
static node_t* constant_fold_if(node_t* node)
{
  assert(node->type == IF_STATEMENT);

  if (node->children[0]->type != NUMBER_LITERAL)
    return node;
  bool condition = node->children[0]->data.number_literal;

  // Detatch the node we want to return from the IF_STATEMENT-node
  node_t* result = NULL;

  if (condition)
  {
    result = node->children[1];
    node->children[1] = NULL;
  }
  else if (node->n_children == 3)
  {
    result = node->children[2];
    node->children[2] = NULL;
  }
  // If condition is false and the if has no else-body, we just let result be NULL

  // Destory everything that is still attached to the IF_STATEMENT-node
  destroy_subtree(node);
  return result;
}

// If the condition of the given while node is a NUMBER_LITERAL, and it is false (0),
// we remove the entire while node and return NULL instead.
// Loops that look like while(true) { ... } are kept as is. They may have a break inside
static node_t* constant_fold_while(node_t* node)
{
  assert(node->type == WHILE_STATEMENT);

  if (node->children[0]->type != NUMBER_LITERAL)
    return node;

  bool condition = node->children[0]->data.number_literal;
  if (condition)
    return node;

  destroy_subtree(node);
  return NULL;
}

// Does constant folding on the subtreee rooted at the given node.
// Returns the root of the new subtree.
// Any node that is detached from the tree by this operation must be freed, to avoid memory leaks.
static node_t* constant_fold_subtree(node_t* node)
{
  if (node == NULL)
    return node;

  // First do constant folding on all child nodes
  for (size_t i = 0; i < node->n_children; i++)
    node->children[i] = constant_fold_subtree(node->children[i]);

  switch (node->type)
  {
  case OPERATOR:
    return constant_fold_operator(node);
  case IF_STATEMENT:
    return constant_fold_if(node);
  case WHILE_STATEMENT:
    return constant_fold_while(node);
  default:
    return node;
  }
}

// Operates on the statement given as node, and any sub-statements it may have.
// Returns true if execution of the given statement is guaranteed to interrupt execution
// through either a return statement or a break statement.
// When node is a BLOCK, any statements that come after such an interrupting statement are removed.
static bool remove_unreachable_code(node_t* node)
{
  if (node == NULL)
    return false;

  switch (node->type)
  {
  case RETURN_STATEMENT:
  case BREAK_STATEMENT:
    return true;
  case IF_STATEMENT:
  {
    if (node->n_children == 2)
    {
      // If the if only has a then-statement, it can not terminate execution
      remove_unreachable_code(node->children[1]);
      return false;
    }
    else
    {
      // If both the then-statement and the else-statement are interrupted
      // we know that the if itself is interrupting as well
      bool then_interrupts = remove_unreachable_code(node->children[1]);
      bool else_interrupts = remove_unreachable_code(node->children[2]);
      return then_interrupts && else_interrupts;
    }
  }
  case WHILE_STATEMENT:
  {
    // Even if the body of the while contains interrupting statements,
    // that is not a guarantee that the code after the while is unreachable.
    // The while may never be entered, for example, or the interrupting statement may be BREAK.
    remove_unreachable_code(node->children[1]);
    return false;
  }
  case BLOCK:
  {
    // The list of statements in a BLOCK is always the last child node
    node_t* statement_list = node->children[node->n_children - 1];

    for (size_t i = 0; i < statement_list->n_children; i++)
    {
      node_t* child_statement = statement_list->children[i];
      bool interrupting = remove_unreachable_code(child_statement);

      // If we have an interrupting statement, the rest of the statement list should be freed
      if (interrupting)
      {

        // free all statements that come after i
        for (size_t j = i + 1; j < statement_list->n_children; j++)
          destroy_subtree(statement_list->children[j]);

        // Truncate the list of statements
        statement_list->n_children = i + 1;
        return true;
      }
    }

    // If we get here, none of the statements in the block are interrupting.
    return false;
  }
  default:
    return false;
  }
}

// Frees the memory owned by the given node, but does not touch its children
static void node_finalize(node_t* discard)
{
  if (discard == NULL)
    return;

  // Only free data if the data field is owned by the node
  switch (discard->type)
  {
  case IDENTIFIER:
    free(discard->data.identifier);
    break;
  case STRING_LITERAL:
    free(discard->data.string_literal);
    break;
  default:
    break;
  }
  free(discard->children);
  free(discard);
}

// Recursively frees the memory owned by the given node, and all its children
static void destroy_subtree(node_t* discard)
{
  if (discard == NULL)
    return;

  for (size_t i = 0; i < discard->n_children; i++)
    destroy_subtree(discard->children[i]);
  node_finalize(discard);
}

// Definition of the global string array NODE_TYPE_NAMES
const char* NODE_TYPE_NAMES[NODE_TYPE_COUNT] = {
#define NODE_TYPE(node_type) #node_type
#include "nodetypes.h"
};
