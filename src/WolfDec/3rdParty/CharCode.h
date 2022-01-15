// ============================================================================
//
//		キャラクタコード関係のライブラリ
//
//		Creator			: 山田　巧
//		Creation Data	: 09/17/2004
//
// ============================================================================

#ifndef CHARCODE_H
#define CHARCODE_H

// include --------------------------------------

#include <tchar.h>
#include "DataType.h"


// define ---------------------------------------

#define CHARCODEFORMAT_SHIFTJIS				(932)
#define CHARCODEFORMAT_GB2312				(936)
#define CHARCODEFORMAT_UHC					(949)
#define CHARCODEFORMAT_BIG5					(950)
#define CHARCODEFORMAT_UTF16LE				(1200)
#define CHARCODEFORMAT_UTF16BE				(1201)
#define CHARCODEFORMAT_WINDOWS_1252			(1252)			// 欧文(ラテン文字の文字コード)
#define CHARCODEFORMAT_ISO_IEC_8859_15		(32764)			// 欧文(ラテン文字の文字コード)
#define CHARCODEFORMAT_UTF8					(65001)
#define CHARCODEFORMAT_ASCII				(32765)			// アスキー文字コード
#define CHARCODEFORMAT_UTF32LE				(32766)			// 正式なコードが無かったので勝手に定義
#define CHARCODEFORMAT_UTF32BE				(32767)			// 正式なコードが無かったので勝手に定義

// シフトJIS２バイト文字判定
#define CHECK_SHIFTJIS_2BYTE( x )			( ( u8 )( ( ( ( u8 )(x) ) ^ 0x20) - ( u8 )0xa1 ) < 0x3c )

// UTF16LEサロゲートペア判定( リトルエンディアン環境用 )
#define CHECK_CPU_LE_UTF16LE_4BYTE( x )		( ( ( x ) & 0xfc00 ) == 0xd800 )

// UTF16LEサロゲートペア判定( リトルエンディアン環境用 )
#define CHECK_CPU_LE_UTF16LE_4BYTE( x )		( ( ( x ) & 0xfc00 ) == 0xd800 )

// UTF16LEサロゲートペア判定( ビッグエンディアン環境用 )
#define CHECK_CPU_BE_UTF16LE_4BYTE( x )		( ( ( ( ( ( ( u16 )( x ) ) >> 8 ) & 0xff ) | ( ( ( u16 )( x ) << 8 ) & 0xff00 ) ) & 0xfc00 ) == 0xd800 )

// UTF16BEサロゲートペア判定( リトルエンディアン環境用 )
#define CHECK_CPU_LE_UTF16BE_4BYTE( x )		CHECK_CPU_BE_UTF16LE_4BYTE( x )

// UTF16BEサロゲートペア判定( ビッグエンディアン環境用 )
#define CHECK_CPU_BE_UTF16BE_4BYTE( x )		CHECK_CPU_LE_UTF16LE_4BYTE( x )

#define WCHAR_T_CODEPAGE				( sizeof( wchar_t ) > 2 ? DX_CHARCODEFORMAT_UTF32LE : DX_CHARCODEFORMAT_UTF16LE )

// wchar_t サロゲートペア判定( UTF-32 or UTF-16 想定 )
#ifdef WCHAR_T_BE
	#define CHECK_WCHAR_T_DOUBLE( x )	( sizeof( wchar_t ) == 2 && ( ( ( u16 )( x ) & 0x00fc ) == 0x00d8 ) )
#else
	#define CHECK_WCHAR_T_DOUBLE( x )	( sizeof( wchar_t ) == 2 && ( ( ( u16 )( x ) & 0xfc00 ) == 0xd800 ) )
#endif

// function proto type --------------------------

extern	int				InitCharCode( void ) ;																	// 文字コードライブラリの初期化

extern	int				GetCharCodeFormatUnitSize(	int CharCodeFormat ) ;													// 指定のコードページの情報最少サイズを取得する( 戻り値：バイト数 )
extern	int				GetCharBytes(			const char *CharCode, int CharCodeFormat ) ;									// １文字のバイト数を取得する( 戻り値：１文字のバイト数 )
extern	u32				GetCharCode(			const char *CharCode, int CharCodeFormat, int *CharBytes ) ;					// １文字の文字コードと文字のバイト数を取得する
extern	int				PutCharCode(			u32 CharCode, int CharCodeFormat, char *Dest ) ;								// 文字コードを通常の文字列に変換する、終端にヌル文字は書き込まない( 戻り値：書き込んだバイト数 )
extern	u32				ConvCharCode(			u32 SrcCharCode, int SrcCharCodeFormat, int DestCharCodeFormat ) ;				// 文字コードを指定のコードページの文字に変換する
extern	int				ConvCharCodeString(		const u32 *Src, int SrcCharCodeFormat, u32 *Dest, int DestCharCodeFormat ) ;	// １文字４バイトの配列を、別コードページの１文字４バイトの配列に変換する( 戻り値：変換後のサイズ、ヌル文字含む( 単位：バイト ) )
extern	int				StringToCharCodeString( const char  *Src, int SrcCharCodeFormat, u32  *Dest ) ;						// 文字列を１文字４バイトの配列に変換する( 戻り値：変換後のサイズ、ヌル文字含む( 単位：バイト ) )
extern	int				CharCodeStringToString( const u32 *Src, char *Dest, int DestCharCodeFormat ) ;						// １文字４バイトの配列を文字列に変換する( 戻り値：変換後のサイズ、ヌル文字含む( 単位：バイト ) )
extern	int				ConvString(				const char *Src, int SrcCharCodeFormat, char *Dest, int DestCharCodeFormat ) ;	// 文字列を指定のコードページの文字列に変換する( 戻り値：変換後のサイズ、ヌル文字含む( 単位：バイト ) )
extern	int				GetStringCharNum(		const char *String, int CharCodeFormat ) ;									// 文字列に含まれる文字数を取得する
extern	const char *	GetStringCharAddress(	const char *String, int CharCodeFormat, int Index ) ;							// 指定番号の文字のアドレスを取得する
extern	u32				GetStringCharCode(		const char *String, int CharCodeFormat, int Index ) ;							// 指定番号の文字のコードを取得する

extern	void			CL_strcpy(            int CharCodeFormat, char *Dest,                     const char *Src ) ;
extern	void			CL_strcpy_s(          int CharCodeFormat, char *Dest, size_t BufferBytes, const char *Src ) ;
extern	void			CL_strncpy(           int CharCodeFormat, char *Dest,                     const char *Src, int Num ) ;
extern	void			CL_strncpy_s(         int CharCodeFormat, char *Dest, size_t BufferBytes, const char *Src, int Num ) ;
extern	void			CL_strcat(            int CharCodeFormat, char *Dest,                     const char *Src ) ;
extern	void			CL_strcat_s(          int CharCodeFormat, char *Dest, size_t BufferBytes, const char *Src ) ;
extern	const char *	CL_strstr(            int CharCodeFormat, const char *Str1, const char *Str2 ) ;
extern	int				CL_strlen(            int CharCodeFormat, const char *Str ) ;
extern	int				CL_strcmp(            int CharCodeFormat, const char *Str1, const char *Str2 ) ;
extern	int				CL_stricmp(           int CharCodeFormat, const char *Str1, const char *Str2 ) ;
extern	int				CL_strcmp_str2_ascii( int CharCodeFormat, const char *Str1, const char *Str2 ) ;
extern	int				CL_strncmp(           int CharCodeFormat, const char *Str1, const char *Str2, int Size ) ;
extern	const char *	CL_strchr(            int CharCodeFormat, const char *Str, u32 CharCode ) ;
extern	const char *	CL_strrchr(           int CharCodeFormat, const char *Str, u32 CharCode ) ;
extern	char *			CL_strupr(            int CharCodeFormat, char *Str ) ;
extern	int				CL_vsprintf(          int CharCodeFormat, char *Buffer, const char *FormatString, va_list Arg ) ;
extern	int				CL_sprintf(           int CharCodeFormat, char *Buffer, const char *FormatString, ... ) ;
extern	char *			CL_itoa(              int CharCodeFormat, int Value, char *Buffer, int Radix ) ;
extern	int				CL_atoi(              int CharCodeFormat, const char *Str ) ;
extern	double			CL_atof(              int CharCodeFormat, const char *Str ) ;
extern	int				CL_vsscanf(           int CharCodeFormat, const char *String, const char *FormatString, va_list Arg ) ;
extern	int				CL_sscanf(            int CharCodeFormat, const char *String, const char *FormatString, ... ) ;

#endif // CHARCODE_H

