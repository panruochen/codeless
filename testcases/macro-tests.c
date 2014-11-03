#if defined(__linux__) and !defined(__ANDROID__)
#endif

#define _AC(x,y)  x ## y
_AC(1, UL)

#define ___config_enabled(__ignored, val, ...) val

#if ___config_enabled(1,0) || ___config_enabled(0,1,0) // || ___config_enabled(0)
#endif

/*#define MA0(a,b)   ##a##b
MA0(7,8)
#define MA1(a, b		...)   (a + f( | b | ))
MA1(0,1,2)
MA1(3,			4 ,		5		)
MA1(3,	4    ,5    )
#define ADD(a,b) ((a)+(b))
#define SUB(a,b) ((a)-(b))
#define MUL(a,b) ((a)*(b))
MA1(0, ADD(1,2), SUB(3,4), MUL(5,6))
#define MA2(a,b...)  # a |  # b
MA2(Fri,5,Five)*/

#define CALL_FN1(a,b,c,args...)  a  ##		_ ## b ##	_## c\
	##(args)
CALL_FN1(set,switch,off,7,8,9)
#define eprintf(args...) printf(args)
#define CALL_FN2(a,b,c,...)  a  ##		_ ## b ##	_## c \
	(__VA_ARGS__)
#define __S(x)    #x
#define _S(x,y)   x ## y
#define S(x,y)    __S( _S( x, y ) )
void set_switch_off(int,int,int);
int main()
{
//CALL_FN2(set,switch,off,07,08,09)
	eprintf("%s:%s:%u",__FILE__,__func__,__LINE__);
	CALL_FN2(set,switch,off,7,8,9);
	CALL_FN2(set,switch,off,  0x7,  0x8    	,  0x9);
	S( 7 8 , 9		10 );
	return 0;
}
