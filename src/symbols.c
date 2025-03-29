#include <vslc.h>

// Declaration of global symbol table
symbol_table_t* global_symbols;

// Declarations of helper functions defined further down in this file
static void find_globals(void);
static void bind_names(symbol_table_t* local_symbols, node_t* root);
static void print_symbol_table(symbol_table_t* table, int nesting);
static void destroy_symbol_tables(void);

static size_t add_string(char* string);
static void print_string_list(void);
static void destroy_string_list(void);

/* External interface */

// Creates a global symbol table, and local symbol tables for each function.
// All usages of symbols are bound to their symbol table entries.
// All strings are entered into the string_list
void create_tables(void)
{
  // Create a global symbol table, and make symbols for all globals
  find_globals();

  // For all functions, we want to fill their local symbol tables,
  // and bind all names found in the function body
  for (size_t i = 0; i < global_symbols->n_symbols; i++)
  {
    symbol_t* symbol = global_symbols->symbols[i];
    if (symbol->type == SYMBOL_FUNCTION)
      bind_names(symbol->function_symtable, symbol->node->children[2]);
  }
}

// Prints the global symbol table, and the local symbol tables for each function.
// Also prints the global string list.
// Finally prints out the AST again, with bound symbols.
void print_tables(void)
{
  print_symbol_table(global_symbols, 0);
  printf("\n == STRING LIST == \n");
  print_string_list();
  printf("\n == BOUND SYNTAX TREE == \n");
  print_syntax_tree();
}

// Cleans up all memory owned by symbol tables and the global string list
void destroy_tables(void)
{
  destroy_symbol_tables();
  destroy_string_list();
}

/* Internal matters */

#define CREATE_AND_INSERT_SYMBOL(table, ...)                                 \
  do                                                                         \
  {                                                                          \
    symbol_t* symbol = malloc(sizeof(symbol_t));                             \
    *symbol = (symbol_t){__VA_ARGS__};                                       \
    if (symbol_table_insert((table), symbol) == INSERT_COLLISION)            \
    {                                                                        \
      fprintf(stderr, "error: symbol '%s' already defined\n", symbol->name); \
      exit(EXIT_FAILURE);                                                    \
    }                                                                        \
  } while (false)

// Goes through all global declarations, adding them to the global symbol table.
// When adding functions, a local symbol table with symbols for its parameters are created.
static void find_globals(void)
{
  global_symbols = symbol_table_init();

  for (size_t i = 0; i < root->n_children; i++)
  {
    node_t* node = root->children[i];
    if (node->type == GLOBAL_DECLARATION)
    {
      node_t* global_variable_list = node->children[0];
      for (size_t j = 0; j < global_variable_list->n_children; j++)
      {
        node_t* var = global_variable_list->children[j];
        char* name;
        symtype_t symtype;

        // The global variable list can both contain arrays and normal variables.
        if (var->type == ARRAY_INDEXING)
        {
          name = var->children[0]->data.identifier;
          symtype = SYMBOL_GLOBAL_ARRAY;
        }
        else
        {
          assert(var->type == IDENTIFIER);
          name = var->data.identifier;
          symtype = SYMBOL_GLOBAL_VAR;
        }

        CREATE_AND_INSERT_SYMBOL(
            global_symbols,
            .name = name,
            .type = symtype,
            .node = var,
            .function_symtable = NULL);
      }
    }
    else if (node->type == FUNCTION)
    {
      // Functions have their own local symbol table. We make it now, and add the function
      // parameters
      symbol_table_t* function_symtable = symbol_table_init();
      // We let the global hashmap be the backup of the local scope
      function_symtable->hashmap->backup = global_symbols->hashmap;

      node_t* parameters = node->children[1];
      for (int j = 0; j < parameters->n_children; j++)
      {
        CREATE_AND_INSERT_SYMBOL(
            function_symtable,
            .name = parameters->children[j]->data.identifier,
            .type = SYMBOL_PARAMETER,
            .node = parameters->children[j],
            .function_symtable = NULL);
      }

      CREATE_AND_INSERT_SYMBOL(
          global_symbols,
          .name = node->children[0]->data.identifier,
          .type = SYMBOL_FUNCTION,
          .node = node,
          .function_symtable = function_symtable);
    }
    else
    {
      assert(false && "Unknown global node type");
    }
  }
}

// Creates a new empty hashmap for the symbol table, using the outer scope's hashmap as backup
static void push_local_scope(symbol_table_t* table)
{
  symbol_hashmap_t* hashmap = symbol_hashmap_init();
  hashmap->backup = table->hashmap;
  table->hashmap = hashmap;
}

// Destroys the hashmap, and replaces it with the outer scope's hashmap
static void pop_local_scope(symbol_table_t* table)
{
  symbol_hashmap_t* hashmap = table->hashmap;
  table->hashmap = hashmap->backup;
  symbol_hashmap_destroy(hashmap);
}

// A recursive function that traverses the body of a function, and:
//  - Adds variable declarations to the function's local symbol table.
//  - Pushes and pops local variable scopes when entering and leaving blocks.
//  - Binds all IDENTIFIER nodes that are not declarations, to the symbol it references.
//  - Moves STRING_LITERAL nodes' data into the global string list,
//    and replaces the node with a STRING_LIST_REFERENCE node.
//    Overwrites the node's data.string_list_index field with with string list index
static void bind_names(symbol_table_t* local_symbols, node_t* node)
{
  if (node == NULL)
    return;

  switch (node->type)
  {
  // Can either be a variable in an expression, or the name of a function in a function call
  // Either way, we wish to associate it with its symbol
  case IDENTIFIER:
  {
    symbol_t* symbol = symbol_hashmap_lookup(local_symbols->hashmap, node->data.identifier);
    if (symbol == NULL)
    {
      fprintf(stderr, "error: unrecognized symbol '%s'\n", (char*)node->data.identifier);
      exit(EXIT_FAILURE);
    }
    node->symbol = symbol;
    break;
  }

  // Blocks may contain a list of declarations. In such cases, a scope gets pushed, the declarations
  // get added, and the name binding continues in the body
  case BLOCK:
    if (node->n_children == 2)
    {
      push_local_scope(local_symbols);
      // Iterate through all declarations in the delcaration list
      node_t* decl_list = node->children[0];
      for (int i = 0; i < decl_list->n_children; i++)
      {
        // Each declaration can have one or more IDENTIFIER nodes
        node_t* declaration = decl_list->children[i];
        for (int j = 0; j < declaration->n_children; j++)
        {
          CREATE_AND_INSERT_SYMBOL(
              local_symbols,
              .name = declaration->children[j]->data.identifier,
              .type = SYMBOL_LOCAL_VAR,
              .node = declaration->children[j],
              .function_symtable = local_symbols);
        }
      }
      bind_names(local_symbols, node->children[1]);
      pop_local_scope(local_symbols);
    }
    else
    {
      // If the block only contains statements, and no declaration list, no need to push a scope
      bind_names(local_symbols, node->children[0]);
    }
    break;

  // Strings get inserted into the global string list
  // The STRING_LITERAL node gets replaced by a STRING_LIST_REFERENCE node
  case STRING_LITERAL:
  {
    size_t position = add_string(node->data.string_literal);
    node->type = STRING_LIST_REFERENCE;
    node->data.string_list_index = position;
    break;
  }

  // For all other nodes, recurse through its children
  default:
    for (int i = 0; i < node->n_children; i++)
      bind_names(local_symbols, node->children[i]);
    break;
  }
}

// Prints the given symbol table, with sequence number, symbol names and types.
// When printing function symbols, its local symbol table is recursively printed, with indentation.
static void print_symbol_table(symbol_table_t* table, int nesting)
{
  for (size_t i = 0; i < table->n_symbols; i++)
  {
    symbol_t* symbol = table->symbols[i];

    printf(
        "%*s%ld: %s(%s)\n",
        nesting * 4,
        "",
        symbol->sequence_number,
        SYMBOL_TYPE_NAMES[symbol->type],
        symbol->name);

    // If the symbol is a function, print its local symbol table as well
    if (symbol->type == SYMBOL_FUNCTION)
      print_symbol_table(symbol->function_symtable, nesting + 1);
  }
}

// Frees up the memory used by the global symbol table, all local symbol tables, and their symbols
static void destroy_symbol_tables(void)
{
  // First destory all local symbol tables, by looking for functions among the globals
  for (int i = 0; i < global_symbols->n_symbols; i++)
  {
    if (global_symbols->symbols[i]->type == SYMBOL_FUNCTION)
      symbol_table_destroy(global_symbols->symbols[i]->function_symtable);
  }
  // Then destroy the global symbol table
  symbol_table_destroy(global_symbols);
}

// Declaration of global string list
char** string_list;
size_t string_list_len;
static size_t string_list_capacity;

// Adds the given string to the global string list, resizing if needed.
// Takes ownership of the string, and returns its position in the string list.
static size_t add_string(char* string)
{
  if (string_list_len + 1 >= string_list_capacity)
  {
    string_list_capacity = string_list_capacity * 2 + 8;
    string_list = realloc(string_list, string_list_capacity * sizeof(char*));
  }
  string_list[string_list_len] = string;
  return string_list_len++;
}

// Prints all strings added to the global string list
static void print_string_list(void)
{
  for (size_t i = 0; i < string_list_len; i++)
    printf("%ld: %s\n", i, string_list[i]);
}

// Frees all strings in the global string list, and the string list itself
static void destroy_string_list(void)
{
  for (int i = 0; i < string_list_len; i++)
    free(string_list[i]);
  free(string_list);
}
