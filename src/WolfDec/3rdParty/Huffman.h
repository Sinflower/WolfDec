// ============================================================================
//
//	Huffman圧縮ライブラリ
//
//	Huffman.h
//
//	Creator			: 山田　巧
//	Creation Date	: 2018/12/16
//
// ============================================================================

#ifndef HUFFMAN_H
#define HUFFMAN_H

// define ---------------------------------------

#ifndef u64
#define u64		unsigned long long
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

#ifndef s32
#define s32		signed int
#endif

#ifndef f32
#define f32		float
#endif

#ifndef NULL
#define NULL	(0)
#endif

// proto type -----------------------------------

// データを圧縮
// 戻り値:圧縮後のサイズ  0 はエラー  Dest に NULL を入れると圧縮データ格納に必要なサイズが返る
extern u64 Huffman_Encode( void *Src, u64 SrcSize, void *Dest ) ;

// 圧縮データを解凍
// 戻り値:解凍後のサイズ  0 はエラー  Dest に NULL を入れると解凍データ格納に必要なサイズが返る
extern u64 Huffman_Decode( void *Press, void *Dest ) ;

#endif // HUFFMAN_H