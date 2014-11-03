/*
 *  Basic Testcase Set 1
 */

/**********************************************/
// case 1
/**********************************************/
#if +1 + (+2 - 3 * +6) && -1 + 1 // It is an error if you see this line
"It is error for case 1"
#else // It is an error if you see this line
"It is okay for case 1"
#endif // It is an error if you see this line

#define _RWSTD_NO_BASIC_STRING_DEFINITION       0

# define _RWSTD_DEFINE_TEMPLATE(name)     !!!(_RWSTD_NO_ ## name ## _DEFINITION)

#if  _RWSTD_DEFINE_TEMPLATE (BASIC_STRING)
 "OKAY"
#else
 "ERROR"
#endif

#define  U_FILE_SEP_CHAR           '/'
#define  U_FILE_ALT_SEP_CHAR       '/'

//#define  U_FILE_SEP_CHAR           8
//#define  U_FILE_ALT_SEP_CHAR       8

#if (U_FILE_SEP_CHAR != U_FILE_ALT_SEP_CHAR)
 "ERROR"
#else
 "OKAY"
#endif

#define VER_Z   2
#define VER_Y  10

#define VERSION(z,y) \
 ((VER_Z == (z) && VER_Y >= (y)) || \
  (VER_Z > (z)))

#if !VERSION(1,0)
"ERROR"
#elif VERSION(2,10)
"OK"
#endif

#undef VER_Z
#undef VER_Y
#undef VERSION

#define VER_Z 1
#define VER_Y 7

#define VERSION( z, y )                       \
      ( VER_Z == ( z ) && VER_Y >= ( y ) ) || \
      ( VER_Z > ( z ) )

#if VERSION(1,4)
"OKAY"
#else
"ERROR"
#endif


