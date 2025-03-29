#include "vslc.h"

// Helper function for escaping special characters when printing GraphViz strings
static void print_escaped_string(char* str)
{
  for (char* c = str; *c != '\0'; c++)
  {
    switch (*c)
    {
    case '\\':
      printf("\\\\");
      break;
    case '"':
      printf("\\\"");
      break;
    case '\n':
      printf("\\\\n");
      break;
    default:
      putchar(*c);
      break;
    }
  }
}

// A recursive function for printing a node as GraphViz, and all its children
static void graphviz_node_print_internal(node_t* node)
{
  printf("node%p [label=\"%s", node, NODE_TYPE_NAMES[node->type]);
  switch (node->type)
  {
  case OPERATOR:
    printf("\\n%s", node->data.operator);
    break;
  case IDENTIFIER:
    printf("\\n%s", node->data.identifier);
    break;
  case NUMBER_LITERAL:
    printf("\\n%ld", node->data.number_literal);
    break;
  case STRING_LITERAL:
    printf("\\n");
    print_escaped_string(node->data.string_literal);
    break;
  case STRING_LIST_REFERENCE:
    printf("\\n%zu", node->data.string_list_index);
    break;
  default:
    break;
  }

  printf("\"];\n");
  for (size_t i = 0; i < node->n_children; i++)
  {
    node_t* child = node->children[i];
    if (child == NULL)
      printf("node%p -- node%pNULL%zu ;\n", node, node, i);
    else
    {
      printf("node%p -- node%p ;\n", node, child);
      graphviz_node_print_internal(child);
    }
  }
}

void graphviz_node_print(node_t* root)
{
  printf("graph \"\" {\n node[shape=box];\n");
  graphviz_node_print_internal(root);
  printf("}\n");
}
