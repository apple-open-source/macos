#ifndef __EMBEDDED_SYMBOL_H__
#define __EMBEDDED_SYMBOL_H__

struct embedded_symbol
{
  char *name;
  enum language	language;
};

typedef struct embedded_symbol embedded_symbol;

embedded_symbol *search_for_embedded_symbol PARAMS ((CORE_ADDR pc));

#endif /* __EMBEDDED_SYMBOL_H__ */
