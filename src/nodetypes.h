// This is a special file that is not intended to be #include-d normally.
// Instead, it is included by "tree.h" and "tree.c" to provide both an enum of node types,
// and an array of strings containing the node names.

// clang-format off

#ifndef NODE_TYPE
#error The file nodetypes.h should only be included after defining the NODE_TYPE macro
#endif

NODE_TYPE(LIST),
NODE_TYPE(GLOBAL_DECLARATION),
NODE_TYPE(ARRAY_INDEXING),
NODE_TYPE(VARIABLE),
NODE_TYPE(FUNCTION),
NODE_TYPE(BLOCK),
NODE_TYPE(ASSIGNMENT_STATEMENT),
NODE_TYPE(RETURN_STATEMENT),
NODE_TYPE(PRINT_STATEMENT),
NODE_TYPE(IF_STATEMENT),
NODE_TYPE(WHILE_STATEMENT),
NODE_TYPE(BREAK_STATEMENT),
NODE_TYPE(OPERATOR),              // uses the data field "operator"
NODE_TYPE(FUNCTION_CALL),
NODE_TYPE(IDENTIFIER),            // uses and owns the data field "identifer"
NODE_TYPE(NUMBER_LITERAL),        // uses the data field "number_literal"
NODE_TYPE(STRING_LITERAL),        // uses and owns the data field "string_literal"
NODE_TYPE(STRING_LIST_REFERENCE), // uses the data field "string_list_index"

#undef NODE_TYPE
