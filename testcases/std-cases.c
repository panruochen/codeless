/*
 *  Standard testcases
 *
 */

/**********************************************/
// case 1
/**********************************************/
#if 1 // It is an error if you see this line
"It is okay for case 1"
#else // It is an error if you see this line
"It is error for case 1"
#endif // It is an error if you see this line

/**********************************************/
// case 2
/**********************************************/
#if 0 // It is an error if you see this line
"It is error for case 2"
#else // It is an error if you see this line
"It is okay for case 2"
#endif // It is an error if you see this line

/**********************************************/
// case 3
/**********************************************/
#if 0 // It is an error if you see this line
"It is error for case 3"
#elif X
"It is okay for case 3"
#endif

/**********************************************/
// case 4
/**********************************************/
#if 0 // It is an error if you see this line
"It is error for case 4"
#elif 1 // It is an error if you see this line
"It is okay for case 4"
#endif // It is an error if you see this line

/**********************************************/
// case 5
/**********************************************/
#if 1 // It is an error if you see this line
"It is okay for case 5"
#elif 1 // It is an error if you see this line
"It is error for case 5"
#endif // It is an error if you see this line

/**********************************************/
// case 6
/**********************************************/
#if 0 // It is an error if you see this line
"It is error for case 6"
#elif 1 // It is an error if you see this line
"It is okay for case 6"
#endif // It is an error if you see this line

/**********************************************/
// case 7
/**********************************************/
#if 0 // It is an error if you see this line
"It is error for case 7"
#elif X
"It is okay for case 7 (branch 1)"
#elif 1
"It is okay for case 7 (branch 2)"

/**********************************************/
// case 8
/**********************************************/
#if 0 // It is an error if you see this line
"It is error for case 8"
#elif X
"It is okay for case 8 (branch 1)"
#elif 1 /* Comment Line 1 -- okay
		   Comment Line 2 -- okay
		   Comment Line 3 -- okay
*/
"It is okay for case 8 (branch 2)"
#else
"It is error for case 8"
#endif

/**********************************************/
// case 9
/**********************************************/
#define CASE9_X1      (2 * 6)
#define CASE9_X2      (4 + 5)
#define CASE9_Z       CASE9_X1 - CASE9_X2 + 1 - 2 * 2 

#if CASE9_Z // It is an error if you see this line
"It is error for case 9"
#else // It is an error if you see this line
"It is okay for case 9"
#endif // It is an error if you see this line

/**********************************************/
// case 10
/**********************************************/
#define FALSE     0
#define TRUE      1
#define SUM(a,b)  ((a) + (b))
#define DIF(a,b)  ((a) - (b))
#if SUM(DIF(9,1), SUM(3,4)) - 5 * 3 == FALSE
"It is okay for case 10"
#else // It is an error if you see this line
"It is error for case 10"
#endif // It is an error if you see this line


/**********************************************/
// case 11
/**********************************************/
#define CASE11_A
#if defined CASE11_A // It is an error if you see this line
"It is okay for case 11.A"
#else // It is an error if you see this line
"It is error for case 11.A"
#endif // It is an error if you see this line

#define CASE11_B
#if !!defined(CASE11_B) // It is an error if you see this line
"It is okay for case 11.B"
#else // It is an error if you see this line
"It is error for case 11.B"
#endif // It is an error if you see this line

#define CASE11_C
#ifdef CASE11_C // It is an error if you see this line
"It is okay for case 11.C"
#else // It is an error if you see this line
"It is error for case 11.C"
#endif // It is an error if you see this line

#define CASE11_D
#ifdef CASE11_D // It is an error if you see this line
"It is okay for case 11.D"
#else // It is an error if you see this line
"It is error for case 11.D"
#endif // It is an error if you see this line

/**********************************************/
// case 12
/**********************************************/
#undef CASE12_A
#if !defined CASE12_A // It is an error if you see this line
"It is okay for case 12.A"
#else // It is an error if you see this line
"It is error for case 12.A"
#endif // It is an error if you see this line

#undef CASE12_B
#if !!defined(CASE12_B) // It is an error if you see this line
"It is error for case 12.B"
#else // It is an error if you see this line
"It is okay for case 12.B"
#endif // It is an error if you see this line

#undef  CASE12_C
#ifndef CASE12_C // It is an error if you see this line
"It is okay for case 12.C"
#else // It is an error if you see this line
"It is error for case 12.C"
#endif // It is an error if you see this line

#undef CASE12_D
#ifdef CASE12_D // It is an error if you see this line
"It is error for case 12.D"
#else // It is an error if you see this line
"It is okay for case 12.D"
#endif // It is an error if you see this line

#define AND           &&
#define CASE13_A1     1
#define CASE13_A2     2

#if ((CASE13_A1 == 1 ) AND ( CASE13_A2 != 1 ) )
"It is okay for case 13.A"
#else
"It is error for case 13.A"
#endif

