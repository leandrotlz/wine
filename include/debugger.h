/*
 * Debugger definitions
 *
 * Copyright 1995 Alexandre Julliard
 */

#ifndef __WINE_DEBUGGER_H
#define __WINE_DEBUGGER_H

#include <sys/types.h> /* u_long ... */
#include "windef.h"
#include "miscemu.h"

#define STEP_FLAG 0x100 /* single step flag */

#define SYM_FUNC	 0x0
#define SYM_DATA	 0x1
#define SYM_WIN32	 0x2
#define SYM_WINE	 0x4
#define SYM_INVALID	 0x8
#define SYM_TRAMPOLINE	 0x10
#define SYM_STEP_THROUGH 0x20

enum	debug_type {DT_BASIC, DT_CONST, DT_POINTER, DT_ARRAY, DT_STRUCT, DT_ENUM, DT_TYPEDEF, DT_FUNC, DT_BITFIELD};


/*
 * Return values for DEBUG_CheckLinenoStatus.  Used to determine
 * what to do when the 'step' command is given.
 */
#define FUNC_HAS_NO_LINES	(0)
#define NOT_ON_LINENUMBER	(1)
#define AT_LINENUMBER		(2)
#define FUNC_IS_TRAMPOLINE	(3)

/*
 * For constants generated by the parser, we use this datatype
 */
extern struct datatype * DEBUG_TypeInt;
extern struct datatype * DEBUG_TypeIntConst;
extern struct datatype * DEBUG_TypeUSInt;
extern struct datatype * DEBUG_TypeString;

typedef struct
{
    struct datatype * type;
    DWORD seg;  /* 0xffffffff means current default segment (cs or ds) */
    DWORD off;
} DBG_ADDR;

struct list_id
{
    char * sourcefile;
    int    line;
};

struct  wine_lines {
  unsigned long		line_number;
  DBG_ADDR		pc_offset;
};

struct symbol_info
{
  struct name_hash * sym;
  struct list_id     list;
};

typedef struct wine_lines WineLineNo;

/*
 * This structure holds information about stack variables, function
 * parameters, and register variables, which are all local to this
 * function.
 */
struct  wine_locals {
  unsigned int		regno:8;	/* For register symbols */
  signed int		offset:24;	/* offset from esp/ebp to symbol */
  unsigned int		pc_start;	/* For RBRAC/LBRAC */
  unsigned int		pc_end;		/* For RBRAC/LBRAC */
  char		      * name;		/* Name of symbol */
  struct datatype     * type;		/* Datatype of symbol */
};

typedef struct wine_locals WineLocals;


#ifdef __i386__

#define DBG_V86_MODULE(seg) ((seg)>>16)
#define IS_SELECTOR_V86(seg) DBG_V86_MODULE(seg)

#define DBG_FIX_ADDR_SEG(addr,default) { \
      if ((addr)->seg == 0xffffffff) (addr)->seg = (default); \
      if (!IS_SELECTOR_V86((addr)->seg)) \
      if (IS_SELECTOR_SYSTEM((addr)->seg)) (addr)->seg = 0; }

#define DBG_ADDR_TO_LIN(addr) \
    (IS_SELECTOR_V86((addr)->seg) \
      ? (char*)(DOSMEM_MemoryBase(DBG_V86_MODULE((addr)->seg)) + \
         ((((addr)->seg)&0xFFFF)<<4)+(addr)->off) : \
    (IS_SELECTOR_SYSTEM((addr)->seg) ? (char *)(addr)->off \
      : (char *)PTR_SEG_OFF_TO_LIN((addr)->seg,(addr)->off)))

#else /* __i386__ */

#define DBG_FIX_ADDR_SEG(addr,default)
#define DBG_ADDR_TO_LIN(addr) ((char *)(addr)->off)

#endif /* __386__ */

#define DBG_CHECK_READ_PTR(addr,len) \
    (!DEBUG_IsBadReadPtr((addr),(len)) || \
     (fprintf(stderr,"*** Invalid address "), \
      DEBUG_PrintAddress((addr),dbg_mode, FALSE), \
      fprintf(stderr,"\n"),0))

#define DBG_CHECK_WRITE_PTR(addr,len) \
    (!DEBUG_IsBadWritePtr((addr),(len)) || \
     (fprintf(stderr,"*** Invalid address "), \
      DEBUG_PrintAddress(addr,dbg_mode, FALSE), \
      fprintf(stderr,"\n"),0))

#ifdef REG_SP  /* Some Sun includes define this */
#undef REG_SP
#endif

enum debug_regs
{
    REG_EAX, REG_EBX, REG_ECX, REG_EDX, REG_ESI,
    REG_EDI, REG_EBP, REG_EFL, REG_EIP, REG_ESP,
    REG_AX, REG_BX, REG_CX, REG_DX, REG_SI,
    REG_DI, REG_BP, REG_FL, REG_IP, REG_SP,
    REG_CS, REG_DS, REG_ES, REG_SS, REG_FS, REG_GS
};


enum exec_mode
{
    EXEC_CONT,       /* Continuous execution */
    EXEC_PASS,       /* Continue, passing exception to app */
    EXEC_STEP_OVER,  /* Stepping over a call to next source line */
    EXEC_STEP_INSTR,  /* Step to next source line, stepping in if needed */
    EXEC_STEPI_OVER,  /* Stepping over a call */
    EXEC_STEPI_INSTR,  /* Single-stepping an instruction */
    EXEC_FINISH,		/* Step until we exit current frame */
    EXEC_STEP_OVER_TRAMPOLINE  /* Step over trampoline.  Requires that
				* we dig the real return value off the stack
				* and set breakpoint there - not at the
				* instr just after the call.
				*/
};

extern CONTEXT DEBUG_context;  /* debugger/registers.c */
extern unsigned int dbg_mode;
extern HANDLE dbg_heap;

  /* debugger/break.c */
extern void DEBUG_SetBreakpoints( BOOL set );
extern int DEBUG_FindBreakpoint( const DBG_ADDR *addr );
extern void DEBUG_AddBreakpoint( const DBG_ADDR *addr );
extern void DEBUG_DelBreakpoint( int num );
extern void DEBUG_EnableBreakpoint( int num, BOOL enable );
extern void DEBUG_InfoBreakpoints(void);
extern void DEBUG_AddTaskEntryBreakpoint( HTASK16 hTask );
extern BOOL DEBUG_HandleTrap(void);
extern BOOL DEBUG_ShouldContinue( enum exec_mode mode, int * count );
extern enum exec_mode DEBUG_RestartExecution( enum exec_mode mode, int count );
extern BOOL DEBUG_IsFctReturn(void);

  /* debugger/db_disasm.c */
extern void DEBUG_Disasm( DBG_ADDR *addr, int display );

  /* debugger/expr.c */
extern void DEBUG_FreeExprMem(void);
struct expr * DEBUG_RegisterExpr(enum debug_regs);
struct expr * DEBUG_SymbolExpr(const char * name);
struct expr * DEBUG_ConstExpr(int val);
struct expr * DEBUG_StringExpr(const char * str);
struct expr * DEBUG_SegAddr(struct expr *, struct expr *);
struct expr * DEBUG_USConstExpr(unsigned int val);
struct expr * DEBUG_BinopExpr(int oper, struct expr *, struct expr *);
struct expr * DEBUG_UnopExpr(int oper, struct expr *);
struct expr * DEBUG_StructPExpr(struct expr *, const char * element);
struct expr * DEBUG_StructExpr(struct expr *, const char * element);
struct expr * DEBUG_ArrayExpr(struct expr *, struct expr * index);
struct expr * DEBUG_CallExpr(const char *, int nargs, ...);
struct expr * DEBUG_TypeCastExpr(struct datatype *, struct expr *);
extern   int  DEBUG_ExprValue(DBG_ADDR *, unsigned int *);
DBG_ADDR DEBUG_EvalExpr(struct expr *);
extern int DEBUG_DelDisplay(int displaynum);
extern struct expr * DEBUG_CloneExpr(struct expr * exp);
extern int DEBUG_FreeExpr(struct expr * exp);
extern int DEBUG_DisplayExpr(struct expr * exp);

  /* more debugger/break.c */
extern int DEBUG_AddBPCondition(int bpnum, struct expr * exp);

  /* debugger/display.c */
extern int DEBUG_DoDisplay(void);
extern int DEBUG_AddDisplay(struct expr * exp, int count, char format);
extern int DEBUG_DoDisplay(void);
extern int DEBUG_DelDisplay(int displaynum);
extern int DEBUG_InfoDisplay(void);

  /* debugger/hash.c */
extern struct name_hash * DEBUG_AddSymbol( const char *name, 
					   const DBG_ADDR *addr,
					   const char * sourcefile,
					   int flags);
extern struct name_hash * DEBUG_AddInvSymbol( const char *name, 
					   const DBG_ADDR *addr,
					   const char * sourcefile);
extern BOOL DEBUG_GetSymbolValue( const char * name, const int lineno,
				    DBG_ADDR *addr, int );
extern BOOL DEBUG_SetSymbolValue( const char * name, const DBG_ADDR *addr );
extern const char * DEBUG_FindNearestSymbol( const DBG_ADDR *addr, int flag,
					     struct name_hash ** rtn,
					     unsigned int ebp,
					     struct list_id * source);
extern void DEBUG_ReadSymbolTable( const char * filename );
extern int  DEBUG_LoadEntryPoints( const char * prefix );
extern void DEBUG_AddLineNumber( struct name_hash * func, int line_num, 
		     unsigned long offset );
extern struct wine_locals *
            DEBUG_AddLocal( struct name_hash * func, int regno, 
			    int offset,
			    int pc_start,
			    int pc_end,
			    char * name);
extern int DEBUG_CheckLinenoStatus(const DBG_ADDR *addr);
extern void DEBUG_GetFuncInfo(struct list_id * ret, const char * file, 
			      const char * func);
extern int DEBUG_SetSymbolSize(struct name_hash * sym, unsigned int len);
extern int DEBUG_SetSymbolBPOff(struct name_hash * sym, unsigned int len);
extern int DEBUG_GetSymbolAddr(struct name_hash * sym, DBG_ADDR * addr);
extern int DEBUG_cmp_sym(const void * p1, const void * p2);
extern BOOL DEBUG_GetLineNumberAddr( struct name_hash *, const int lineno, 
				DBG_ADDR *addr, int bp_flag );

extern int DEBUG_SetLocalSymbolType(struct wine_locals * sym, 
				    struct datatype * type);
BOOL DEBUG_Normalize(struct name_hash * nh );

  /* debugger/info.c */
extern void DEBUG_PrintBasic( const DBG_ADDR *addr, int count, char format );
extern struct symbol_info DEBUG_PrintAddress( const DBG_ADDR *addr, 
					      int addrlen, int flag );
extern void DEBUG_Help(void);
extern void DEBUG_HelpInfo(void);
extern struct symbol_info DEBUG_PrintAddressAndArgs( const DBG_ADDR *addr, 
						     int addrlen, 
						     unsigned int ebp, 
						     int flag );

  /* debugger/memory.c */
extern BOOL DEBUG_IsBadReadPtr( const DBG_ADDR *address, int size );
extern BOOL DEBUG_IsBadWritePtr( const DBG_ADDR *address, int size );
extern int DEBUG_ReadMemory( const DBG_ADDR *address );
extern void DEBUG_WriteMemory( const DBG_ADDR *address, int value );
extern void DEBUG_ExamineMemory( const DBG_ADDR *addr, int count, char format);

  /* debugger/registers.c */
extern void DEBUG_SetRegister( enum debug_regs reg, int val );
extern int DEBUG_GetRegister( enum debug_regs reg );
extern void DEBUG_InfoRegisters(void);
extern BOOL DEBUG_ValidateRegisters(void);
extern int DEBUG_PrintRegister(enum debug_regs reg);

  /* debugger/stack.c */
extern void DEBUG_InfoStack(void);
extern void DEBUG_BackTrace(void);
extern void DEBUG_SilentBackTrace(void);
extern int  DEBUG_InfoLocals(void);
extern int  DEBUG_SetFrame(int newframe);
extern int  DEBUG_GetCurrentFrame(struct name_hash ** name, 
				  unsigned int * eip,
				  unsigned int * ebp);

  /* debugger/stabs.c */
extern int DEBUG_ReadExecutableDbgInfo(void);
extern int DEBUG_ParseStabs(char * addr, unsigned int load_offset, unsigned int staboff, int stablen, unsigned int strtaboff, int strtablen);

  /* debugger/msc.c */
extern int DEBUG_RegisterDebugInfo( HMODULE, const char *);
extern int DEBUG_ProcessDeferredDebug(void);
extern int DEBUG_RegisterELFDebugInfo(int load_addr, u_long size, char * name);
extern void DEBUG_InfoShare(void);
extern void DEBUG_InitCVDataTypes(void);

  /* debugger/types.c */
extern int DEBUG_nchar;
extern void DEBUG_InitTypes(void);
extern struct datatype * DEBUG_NewDataType(enum debug_type xtype, 
					   const char * typename);
extern unsigned int 
DEBUG_TypeDerefPointer(DBG_ADDR * addr, struct datatype ** newtype);
extern int DEBUG_AddStructElement(struct datatype * dt, 
				  char * name, struct datatype * type, 
				  int offset, int size);
extern int DEBUG_SetStructSize(struct datatype * dt, int size);
extern int DEBUG_SetPointerType(struct datatype * dt, struct datatype * dt2);
extern int DEBUG_SetArrayParams(struct datatype * dt, int min, int max,
				struct datatype * dt2);
extern void DEBUG_Print( const DBG_ADDR *addr, int count, char format, int level );
extern unsigned int DEBUG_FindStructElement(DBG_ADDR * addr, 
					    const char * ele_name, int * tmpbuf);
extern struct datatype * DEBUG_GetPointerType(struct datatype * dt);
extern int DEBUG_GetObjectSize(struct datatype * dt);
extern unsigned int DEBUG_ArrayIndex(DBG_ADDR * addr, DBG_ADDR * result, int index);
extern struct datatype * DEBUG_FindOrMakePointerType(struct datatype * reftype);
extern long long int DEBUG_GetExprValue(DBG_ADDR * addr, char ** format);
extern int DEBUG_SetBitfieldParams(struct datatype * dt, int offset, 
				   int nbits, struct datatype * dt2);
extern int DEBUG_CopyFieldlist(struct datatype * dt, struct datatype * dt2);
extern enum debug_type DEBUG_GetType(struct datatype * dt);
extern struct datatype * DEBUG_TypeCast(enum debug_type, const char *);
extern int DEBUG_PrintTypeCast(struct datatype *);

  /* debugger/source.c */
extern void DEBUG_ShowDir(void);
extern void DEBUG_AddPath(const char * path);
extern void DEBUG_List(struct list_id * line1, struct list_id * line2,  
		       int delta);
extern void DEBUG_NukePath(void);
extern void DEBUG_GetCurrentAddress( DBG_ADDR * );
extern void DEBUG_Disassemble( const DBG_ADDR *, const DBG_ADDR*, int offset );

  /* debugger/dbg.y */
extern DWORD wine_debugger( EXCEPTION_RECORD *rec, CONTEXT *context, BOOL first_chance );
extern void DEBUG_Exit( DWORD exit_code );

  /* Choose your allocator! */
#if 1
/* this one is libc's fast one */
#include "xmalloc.h"
#define DBG_alloc(x) xmalloc(x)
#define DBG_realloc(x,y) xrealloc(x,y)
#define DBG_free(x) free(x)
#define DBG_strdup(x) xstrdup(x)
#else
/* this one is slow (takes 5 minutes to load the debugger on my machine),
   but is pretty crash-proof (can step through malloc() without problems,
   malloc() arena (and other heaps) can be totally wasted and it'll still
   work, etc... if someone could make optimized routines so it wouldn't
   take so long to load, it could be made default) */
#include "heap.h"
#define DBG_alloc(x) HEAP_xalloc(dbg_heap,0,x)
#define DBG_realloc(x,y) HEAP_xrealloc(dbg_heap,0,x,y)
#define DBG_free(x) HeapFree(dbg_heap,0,x)
#define DBG_strdup(x) HEAP_strdupA(dbg_heap,0,x)
#define DBG_need_heap
#endif

#endif  /* __WINE_DEBUGGER_H */
