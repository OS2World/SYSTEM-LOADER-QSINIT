//
// QSINIT
// le/lx exe module initializaion
// must be included to any .c file with main() function
//
#ifndef _LXMOD_STARTDEFS
#define _LXMOD_STARTDEFS

#ifdef __cplusplus
extern "C" {
#endif

#pragma disable_message (914);
#pragma aux main "_*" parm caller [] value [eax] modify [eax ecx edx];
#pragma aux LibMain "_*" parm caller [] value [eax] modify [eax ecx edx];
#pragma enable_message (914);

#ifdef __cplusplus
}
#endif

#endif // _LXMOD_STARTDEFS
