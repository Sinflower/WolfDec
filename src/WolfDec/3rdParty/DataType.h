// ============================================================================
//
//		データタイプ定義
//
//
//	Creator			: 山田　巧
//	Creation Date	: 11/02/2004
//
// ============================================================================

#ifndef DATA_TYPE_H
#define DATA_TYPE_H

#ifndef u64
#define u64		unsigned __int64
#endif

#ifndef u32
#define u32		unsigned int
#endif

#ifndef u16
#define u16		unsigned short
#endif

#ifndef u8
#define u8		unsigned char
#endif


#ifndef s64
#define s64		signed __int64
#endif

#ifndef s32
#define s32		signed int
#endif

#ifndef s16
#define s16		signed short
#endif

#ifndef s8
#define s8		signed char
#endif

#ifndef f32
#define f32		float
#endif

#ifndef f64
#define f64		double
#endif


#ifndef TRUE
#define TRUE	(1)
#endif

#ifndef FALSE
#define FALSE	(0)
#endif


#ifndef NULL
#define NULL	(0)
#endif

typedef struct tagFRECT
{
	f32 x[2], y[2] ;
} FRECT ;

#define PIF		(3.1415926535897932384F)
#define PIF_2	(2*PIF)

#endif
