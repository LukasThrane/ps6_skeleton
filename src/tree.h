#ifndef TREE_H
#define TREE_H

#include <stdint.h>
#include <stdlib.h>

// Create the node_type_t enum containing all node types defined in nodetypes.h
typedef enum
{

#define NODE_TYPE(node_type) node_type
#include "nodetypes.h"
  NODE_TYPE_COUNT
} node_type_t;

// Array containing human-readable names for all node types
extern const char* NODE_TYPE_NAMES[NODE_TYPE_COUNT];

// This is the tree node structure for the abstract syntax tree
typedef struct node
{
  node_type_t type;
  struct node** children; // An owned list of pointers to child nodes
  size_t n_children;      // The length of the list of child nodes

  // At most one of the data fields can be used at once.
  // The node's type decides which field is active, if any
  union
  {
    const char* operator;     // pointer to constant string, such as "+". Not owned
    char* identifier;         // owned heap allocation. The identifier as a string
    int64_t number_literal;   // the literal integer value
    char* string_literal;     // owned heap allocation. Includes the surrounding "quotation marks"
    size_t string_list_index; // position in global string list
  } data;

  // A pointer to the symbol this node references. Not owned.
  // Only used by IDENTIFIER nodes that reference symbols defined elsewhere.
  struct symbol* symbol;
} node_t;

// Global root for parse tree and abstract syntax tree
extern node_t* root;

// The node creation function, used by the parser
node_t* node_create(node_type_t type, size_t n_children, ...);

// Append an element to the given LIST node, returns the list node
node_t* append_to_list_node(node_t* list_node, node_t* element);

// Outputs the entire syntax tree to the terminal
void print_syntax_tree(void);

// Performs constant folding and removes unconditional conditional branches
void constant_fold_syntax_tree(void);

// Removes code that is never reached due to return and break statement.
// Also ensures all functions return
void remove_unreachable_code_syntax_tree(void);

// Cleans up the entire syntax tree
void destroy_syntax_tree(void);

// Special function used when syntax trees are output as graphviz graphs.
// Implemented in graphviz_output.c
void graphviz_node_print(node_t* root);

#endif // TREE_H
