#include <Library/BaseLib.h>

/* https://gcc.gnu.org/onlinedocs/gccint/Integer-library-routines.html */
__attribute__ ((__used__))
long long
__divmoddi4(
  long long A,
  long long B,
  long long *R
  )
{
  return (INT64) DivS64x64Remainder ((INT64)A, (INT64)B, (INT64 *)R);
}
