#ifndef SYMBOLS
#define SYMBOLS

#include <stdarg.h>

//! Size of symbol hashtable.
/** Optimistically large. Should be a prime, says theory.
 */
#define HASHSIZE 997

enum symboltypes
{ T_UNDEF = -1, T_PROTOCOL, T_CONST, T_VAR, T_SYSCONST };

#define EOS 0

//! Symbol structure
struct symbol
{
  //! Type of symbol.
  /**
   *\sa T_UNDEF, T_PROTOCOL, T_CONST, T_VAR, T_SYSCONST
   */
  int type;
  //! Line number at which it occurred.
  int lineno;
  //! Level of occurrence in role nodes. 0 for as non-key, 1 for key only, 2 for key of key only, etc..
  int keylevel;
  //! Ascii string with name of the symbol.
  const char *text;
  //! Possible next pointer.
  struct symbol *next;
  //! Used for linking all symbol blocks, freed or in use.
  struct symbol *allocnext;
};

typedef struct symbol *Symbol;	//!< pointer to symbol structure

void symbolsInit (void);
void symbolsDone (void);

Symbol get_symb (void);
void free_symb (const Symbol s);

void insert (const Symbol s);
Symbol lookup (const char *s);
void symbolPrint (const Symbol s);
void symbolPrintAll (void);
Symbol symbolSysConst (const char *str);
void symbol_fix_keylevels (void);
Symbol symbolNextFree (Symbol prefixsymbol);

void eprintf (char *fmt, ...);
void veprintf (const char *fmt, va_list args);

extern int globalError;
extern char *globalStream;

#endif