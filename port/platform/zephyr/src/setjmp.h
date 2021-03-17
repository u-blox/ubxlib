/* setjmp.h.
 */

#ifndef _SETJMP_H_
#define _SETJMP_H_

/*
 * All callee preserved registers:
 * v1 - v7, fp, ip, sp, lr, f4, f5, f6, f7
 */
#define _JBLEN 23

typedef int jmp_buf[_JBLEN];

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__
void longjmp(jmp_buf __jmpb, int __retval)
__attribute__ ((__noreturn__));
#else
void longjmp(jmp_buf __jmpb, int __retval);
#endif

int setjmp(jmp_buf __jmpb);

#ifdef __cplusplus
}
#endif

#endif // _SETJMP_H_