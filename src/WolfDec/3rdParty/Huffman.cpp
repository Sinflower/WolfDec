// ============================================================================
//
//	Huffman圧縮ライブラリ
//
//	Huffman.cpp
//
//	Creator			: 山田　巧
//	Creation Date	: 2018/12/16
//
// ============================================================================

// include --------------------------------------
#include "Huffman.h"
#include <stdio.h>
#include <malloc.h>
#include <string.h>

// data type ------------------------------------

// 数値ごとの出現数や算出されたエンコード後のビット列や、結合部分の情報等の構造体
struct HUFFMAN_NODE
{
    u64 Weight ;                  // 出現数( 結合データでは出現数を足したモノ )
    int BitNum ;                  // 圧縮後のビット列のビット数( 結合データでは使わない )
    unsigned char BitArray[32] ;  // 圧縮後のビット列( 結合データでは使わない )
    int Index ;                   // 結合データに割り当てられた参照インデックス( 0 or 1 )

    int ParentNode ;              // このデータを従えている結合データの要素配列のインデックス
    int ChildNode[2] ;            // このデータが結合させた２要素の要素配列インデックス( 結合データではない場合はどちらも -1 )
} ;

// ビット単位入出力用データ構造体
struct BIT_STREAM
{
	u8 *Buffer ;
	u64 Bytes ;
	u8 Bits ;
} ;

// 圧縮データの情報
//   6bit      圧縮前のデータのサイズのビット数(A) - 1( 0=1ビット 63=64ビット )
//   (A)bit    圧縮前のデータのサイズ
//   6bit      圧縮後のデータのサイズのビット数(B) - 1( ヘッダ部分含まない )( 0=1ビット 63=64ビット )
//   (B)bit    圧縮後のデータのサイズ
//
//   3bit      数値ごとの出現頻度の差分値のビット数(C) / 2 - 1( 0=2ビット 7=16ビット )
//   1bit      符号ビット( 0=プラス  1=マイナス )
//   (C)bit    数値ごとの出現頻度の差分値
//   ↑これが256個ある

// proto type -----------------------------------

static void BitStream_Init(  BIT_STREAM *BitStream, void *Buffer, bool IsRead ) ;		// ビット単位入出力の初期化
static void BitStream_Write( BIT_STREAM *BitStream, u8 BitNum, u64 OutputData ) ;		// ビット単位の数値の書き込みを行う
static u64  BitStream_Read(  BIT_STREAM *BitStream, u8 BitNum ) ;						// ビット単位の数値の読み込みを行う
static u8   BitStream_GetBitNum( u64 Data ) ;											// 指定の数値のビット数を取得する
static u64  BitStream_GetBytes( BIT_STREAM *BitStream ) ;								// ビット単位の入出力データのサイズ( バイト数 )を取得する

// code -----------------------------------------

// ビット単位入出力の初期化
void BitStream_Init( BIT_STREAM *BitStream, void *Buffer, bool IsRead )
{
	BitStream->Buffer = ( u8 * )Buffer ;
	BitStream->Bytes = 0 ;
	BitStream->Bits = 0 ;
	if( IsRead == false )
	{
		BitStream->Buffer[ 0 ] = 0 ;
	}
}

// ビット単位の数値の書き込みを行う
void BitStream_Write( BIT_STREAM *BitStream, u8 BitNum, u64 OutputData )
{
	u32 i ;
	for( i = 0 ; i < BitNum ; i ++ )
	{
		BitStream->Buffer[ BitStream->Bytes ] |= ( ( OutputData >> ( BitNum - 1 - i ) ) & 1 ) << ( 7 - BitStream->Bits ) ;
		BitStream->Bits ++ ;
		if( BitStream->Bits == 8 )
		{
			BitStream->Bytes ++ ;
			BitStream->Bits = 0 ;
			BitStream->Buffer[ BitStream->Bytes ] = 0 ;
		}
	}
}

// ビット単位の数値の読み込みを行う
u64 BitStream_Read( BIT_STREAM *BitStream, u8 BitNum )
{
	u64 Result = 0 ;
	u32 i ;
	for( i = 0 ; i < BitNum ; i ++ )
	{
		Result |= ( ( u64 )( ( BitStream->Buffer[ BitStream->Bytes ] >> ( 7 - BitStream->Bits ) ) & 1 ) ) << ( BitNum - 1 - i ) ;
		BitStream->Bits ++ ;
		if( BitStream->Bits == 8 )
		{
			BitStream->Bytes ++ ;
			BitStream->Bits = 0 ;
		}
	}

	return Result ;
}

// 指定の数値のビット数を取得する
u8 BitStream_GetBitNum( u64 Data )
{
	u32 i ;
	for( i = 1 ; i < 64 ; i ++ )
	{
		if( Data < ( 1ULL << i ) )
		{
			return i ;
		}
	}

	return i ;
}

// ビット単位の入出力データのサイズ( バイト数 )を取得する
u64 BitStream_GetBytes( BIT_STREAM *BitStream )
{
	return BitStream->Bytes + ( BitStream->Bits != 0 ? 1 : 0 ) ;
}

// データを圧縮
//
// 戻り値:圧縮後のサイズ  0 はエラー  Dest に NULL を入れると圧縮データ格納に必要なサイズが返る
u64 Huffman_Encode( void *Src, u64 SrcSize, void *Dest )
{
    // 結合データと数値データ、０～２５５までが数値データ
    // (結合データの数と圧縮するデータの種類の数を足すと必ず『種類の数＋(種類の数－１)』になる。
    // 『ホントか？』と思われる方はハフマン圧縮の説明で出てきたＡ,Ｂ,Ｃ,Ｄ,Ｅの結合部分の数を
    // 数えてみて下さい、種類が５つに対して結合部分は一つ少ない４つになっているはずです。
    // 種類が６つの時は結合部分は５つに、そして種類が２５６この時は結合部分は２５５個になります)
    HUFFMAN_NODE Node[256 + 255] ;

    unsigned char *SrcPoint ;
    u64 PressBitCounter, PressSizeCounter, SrcSizeCounter ;
    u64 i ;

    // void 型のポインタではアドレスの操作が出来ないので unsigned char 型のポインタにする
    SrcPoint = ( unsigned char * )Src ;

    // 各数値の圧縮後のビット列を算出する
    {
        int NodeIndex, MinNode1, MinNode2 ;
        int NodeNum, DataNum ;

        // 数値データを初期化する
        for( i = 0 ; i < 256 ; i ++ )
        {
            Node[i].Weight = 0 ;           // 出現数はこれから算出するので０に初期化
            Node[i].ChildNode[0] = -1 ;    // 数値データが終点なので -1 をセットする
            Node[i].ChildNode[1] = -1 ;    // 数値データが終点なので -1 をセットする
            Node[i].ParentNode = -1 ;      // まだどの要素とも結合されていないので -1 をセットする
        }

        // 各数値の出現数をカウント
        for( i = 0 ; i < SrcSize ; i ++ )
        {
            Node[ SrcPoint[i] ].Weight ++ ;
        }

		// 出現数を 0～65535 の比率に変換する
		for( i = 0 ; i < 256 ; i ++ )
		{
			Node[ i ].Weight = Node[ i ].Weight * 0xffff / SrcSize ;
		}

        // 出現数の少ない数値データ or 結合データを繋いで
        // 新しい結合データを作成、全ての要素を繋いで残り１個になるまで繰り返す
        DataNum = 256 ; // 残り要素数
        NodeNum = 256 ; // 次に新しく作る結合データの要素配列のインデックス
        while( DataNum > 1 )
        {
            // 出現数値の低い要素二つを探す
            {
                MinNode1 = -1 ;
                MinNode2 = -1 ;
                
                // 残っている要素全てを調べるまでループ
                NodeIndex = 0 ;
                for( i = 0 ; i < DataNum ; NodeIndex ++ )
                {
                    // もう既に何処かの要素と結合されている場合は対象外
                    if( Node[NodeIndex].ParentNode != -1 ) continue ;
                    
                    i ++ ;
                    
                    // まだ有効な要素をセットしていないか、より出現数値の
                    // 少ない要素が見つかったら更新
                    if( MinNode1 == -1 || Node[MinNode1].Weight > Node[NodeIndex].Weight )
                    {
                        // 今まで一番出現数値が少なかったと思われた
                        // 要素は二番目に降格
                        MinNode2 = MinNode1 ;

                        // 新しい一番の要素の要素配列のインデックスを保存
                        MinNode1 = NodeIndex ;
                    }
                    else
                    {
                        // 一番よりは出現数値が多くても、二番目よりは出現数値が
                        // 少ないかもしれないので一応チェック(又は二番目に出現数値の
                        // 少ない要素がセットされていなかった場合もセット)
                        if( MinNode2 == -1 || Node[MinNode2].Weight > Node[NodeIndex].Weight )
                        {
                            MinNode2 = NodeIndex ;
                        }
                    }
                }
            }
            
            // 二つの要素を繋いで新しい要素(結合データ)を作る
            Node[NodeNum].ParentNode = -1 ;  // 新しいデータは当然まだ何処とも繋がっていないので -1 
            Node[NodeNum].Weight = Node[MinNode1].Weight + Node[MinNode2].Weight ;    // 出現数値は二つの数値を足したものをセットする
            Node[NodeNum].ChildNode[0] = MinNode1 ;    // この結合部で 0 を選んだら出現数値が一番少ない要素に繋がる
            Node[NodeNum].ChildNode[1] = MinNode2 ;    // この結合部で 1 を選んだら出現数値が二番目に少ない要素に繋がる

            // 結合された要素二つに、自分達に何の値が割り当てられたかをセットする
            Node[MinNode1].Index = 0 ;    // 一番出現数値が少ない要素は 0 番
            Node[MinNode2].Index = 1 ;    // 二番目に出現数値が少ない要素は 1 番

            // 結合された要素二つに、自分達を結合した結合データの要素配列インデックスをセットする
            Node[MinNode1].ParentNode = NodeNum ;
            Node[MinNode2].ParentNode = NodeNum ;

            // 要素の数を一個増やす
            NodeNum ++ ;

            // 残り要素の数は、一つ要素が新しく追加された代わりに
            // 二つの要素が結合されて検索の対象から外れたので
            // 結果 1 - 2 で -1 
            DataNum -- ;
        }
        
        // 各数値の圧縮後のビット列を割り出す
        {
            unsigned char TempBitArray[32] ;
            int TempBitIndex, TempBitCount, BitIndex, BitCount ;
        
            // 数値データの種類の数だけ繰り返す
            for( i = 0 ; i < 256 ; i ++ )
            {
                // 数値データから結合データを上へ上へと辿ってビット数を数える
                {
                    // ビット数を初期化しておく
                    Node[i].BitNum = 0 ;
                    
                    // 一時的に数値データから遡っていったときのビット列を保存する処理の準備
                    TempBitIndex = 0 ;
                    TempBitCount = 0 ;
                    TempBitArray[TempBitIndex] = 0 ;
                    
                    // 何処かと結合されている限りカウントし続ける(天辺は何処とも結合されていないので終点だと分かる)
                    for( NodeIndex = ( int )i ; Node[NodeIndex].ParentNode != -1 ; NodeIndex = Node[NodeIndex].ParentNode )
                    {
                        // 配列要素一つに入るビットデータは８個なので、同じ配列要素に
                        // 既に８個保存していたら次の配列要素に保存先を変更する
                        if( TempBitCount == 8 )
                        {
                            TempBitCount = 0 ;
                            TempBitIndex ++ ;
                            TempBitArray[TempBitIndex] = 0 ;
                        }
                        
                        // 新しく書き込む情報で今までのデータを上書きしてしまわないように１ビット左にシフトする
                        TempBitArray[TempBitIndex] <<= 1 ;

                        // 結合データに割り振られたインデックスを最下位ビット(一番右側のビット)に書き込む
                        TempBitArray[TempBitIndex] |= (unsigned char)Node[NodeIndex].Index ;

                        // 保存したビット数を増やす
                        TempBitCount ++ ;

                        // ビット数を増やす
                        Node[i].BitNum ++ ;
                    }
                }
				
                // TempBitArray に溜まったデータは数値データから結合データを天辺に向かって
                // 上へ上へと遡っていった時のビット列なので、逆さまにしないと圧縮後のビット
                // 配列として使えない(展開時に天辺の結合データから数値データまで辿ることが
                // 出来ない)ので、順序を逆さまにしたものを数値データ内のビット列バッファに保存する
                {
                    BitCount = 0 ;
                    BitIndex = 0 ;
                    
                    // 最初のバッファを初期化しておく
                    // (全部 論理和(or)演算 で書き込むので、最初から１になっている
                    // ビットに０を書き込んでも１のままになってしまうため)
                    Node[i].BitArray[BitIndex] = 0 ;
                    
                    // 一時的に保存しておいたビット列の最初まで遡る
                    while( TempBitIndex >= 0 )
                    {
                        // 書き込んだビット数が一つの配列要素に入る８ビットに
                        // 達してしまったら次の配列要素に移る
                        if( BitCount == 8 )
                        {
                            BitCount = 0 ;
                            BitIndex ++ ;
                            Node[i].BitArray[BitIndex] = 0 ;
                        }

                        // まだ何も書き込まれていないビットアドレスに１ビット書き込む
                        Node[i].BitArray[BitIndex] |= (unsigned char)( ( TempBitArray[TempBitIndex] & 1 ) << BitCount ) ;
						
                        // 書き込み終わったビットはもういらないので次のビットを
                        // 書き込めるように１ビット右にシフトする
                        TempBitArray[TempBitIndex] >>= 1 ;
                        
                        // １ビット書き込んだので残りビット数を１個減らす
                        TempBitCount -- ;
                        
                        // もし現在書き込み元となっている配列要素に書き込んでいない
                        // ビット情報が無くなったら次の配列要素に移る
                        if( TempBitCount == 0 )
                        {
                            TempBitIndex -- ;
                            TempBitCount = 8 ;
                        }
                        
                        // 書き込んだビット数を増やす
                        BitCount ++ ;
                    }
                }
            }
        }
    }

    // 変換処理
    {
        unsigned char *PressData ;
        int BitData, BitCounter, BitIndex, BitNum, NodeIndex ;
        
        // 圧縮データを格納するアドレスをセット
        // (圧縮データ本体は元のサイズ、圧縮後のサイズ、各数値の出現数等を
        // 格納するデータ領域の後に格納する)
        PressData = ( unsigned char * )Dest ;
        
        // 圧縮するデータの参照アドレスを初期化
        SrcSizeCounter = 0 ;
        
        // 圧縮したデータの参照アドレスを初期化
        PressSizeCounter = 0 ;
        
        // 圧縮したビットデータのカウンタを初期化
        PressBitCounter = 0 ;
        
        // 圧縮データの最初のバイトを初期化しておく
        if( Dest != NULL ) PressData[PressSizeCounter] = 0 ;

        // 圧縮対照のデータを全て圧縮後のビット列に変換するまでループ
        for( SrcSizeCounter = 0 ; SrcSizeCounter < SrcSize ; SrcSizeCounter ++ )
        {
            // 保存する数値データのインデックスを取得
            NodeIndex = SrcPoint[SrcSizeCounter] ;
            
            // 指定の数値データの圧縮後のビット列を出力
            {
                // 参照する配列のインデックスを初期化
                BitIndex = 0 ;
                
                // 配列要素中の出力したビット数の初期化
                BitNum = 0 ;
                
                // 最初に書き込むビット列の配列要素をセット
                BitData = Node[NodeIndex].BitArray[0] ;

                // 全てのビットを出力するまでループ
                for( BitCounter = 0 ; BitCounter < Node[NodeIndex].BitNum ; BitCounter ++ )
                {
                    // もし書き込んだビット数が８個になっていたら次の配列要素に移る
                    if( PressBitCounter == 8 )
                    {
                        PressSizeCounter ++ ;
                        if( Dest != NULL ) PressData[PressSizeCounter] = 0 ;
                        PressBitCounter = 0 ;
                    }
                    
                    // もし書き出したビット数が８個になっていたら次の配列要素に移る
                    if( BitNum == 8 )
                    {
                        BitIndex ++ ;
                        BitData = Node[NodeIndex].BitArray[BitIndex] ;
                        BitNum = 0 ;
                    }
                    
                    // まだ何も書き込まれていないビットアドレスに１ビット書き込む
                    if( Dest != NULL ) PressData[PressSizeCounter] |= (unsigned char)( ( BitData & 1 ) << PressBitCounter ) ;

                    // 書き込んだビット数を増やす
                    PressBitCounter ++ ;

                    // 次に書き出すビットを最下位ビット(一番右のビット)にする為に
                    // １ビット右シフトする
                    BitData >>= 1 ;
                    
                    // 書き出したビット数を増やす
                    BitNum ++ ;
                }
            }
        }
        
        // 最後の１バイト分のサイズを足す
        PressSizeCounter ++ ;
    }
    
    // 圧縮データの情報を保存する
    {
		BIT_STREAM BitStream ;
		u8 HeadBuffer[ 256 * 2 + 32 ] ;
		u8 BitNum ;
		u64 HeadSize ;
		s32 WeightSaveData[ 256 ] ;

		BitStream_Init( &BitStream, HeadBuffer, false ) ;

        // 元のデータのサイズをセット
		BitNum = BitStream_GetBitNum( SrcSize ) ;
		if( BitNum > 0 )
		{
			BitNum -- ;
		}
        BitStream_Write( &BitStream, 6, BitNum ) ;
		BitStream_Write( &BitStream, BitNum + 1, SrcSize ) ;
        
        // 圧縮後のデータのサイズをセット
		BitNum = BitStream_GetBitNum( PressSizeCounter ) ;
        BitStream_Write( &BitStream, 6, BitNum ) ;
		BitStream_Write( &BitStream, BitNum + 1, PressSizeCounter ) ;
        
        // 各数値の出現率の差分値を保存する
		WeightSaveData[ 0 ] = ( s32 )Node[ 0 ].Weight ;
        for( i = 1 ; i < 256 ; i ++ )
        {
			WeightSaveData[ i ] = ( s32 )Node[ i ].Weight - ( s32 )Node[ i - 1 ].Weight ;
        }
        for( i = 0 ; i < 256 ; i ++ )
        {
			u64 OutputNum ;
			bool Minus ;

			if( WeightSaveData[ i ] < 0 )
			{
				OutputNum = ( u64 )( -WeightSaveData[ i ] ) ;
				Minus = true ;
			}
			else
			{
				OutputNum = ( u64 )WeightSaveData[ i ] ;
				Minus = false ;
			}

			BitNum = ( BitStream_GetBitNum( OutputNum ) + 1 ) / 2 ;
			if( BitNum > 0 )
			{
				BitNum -- ;
			}
	        BitStream_Write( &BitStream, 3, BitNum ) ;
			BitStream_Write( &BitStream, 1, Minus ? 1 : 0 ) ;
			BitStream_Write( &BitStream, ( BitNum + 1 ) * 2, OutputNum ) ;
        }
		
		// ヘッダサイズを取得
		HeadSize = BitStream_GetBytes( &BitStream ) ;

		// 圧縮データの情報を圧縮データにコピーする
		if( Dest != NULL )
		{
			u64 j ;

			// ヘッダの分だけ移動
			for( j = PressSizeCounter - 1 ; j >= 0 ; j -- )
			{
				( ( u8 * )Dest )[ HeadSize + j ] = ( ( u8 * )Dest )[ j ] ;
				if( j == 0 )
				{
					break ;
				}
			}

			// ヘッダを書き込み
			memcpy( Dest, HeadBuffer, ( size_t )HeadSize ) ;
		}

		// 圧縮後のサイズを返す
		return PressSizeCounter + HeadSize ;
    }
}

// 圧縮データを解凍
//
// 戻り値:解凍後のサイズ  0 はエラー  Dest に NULL を入れると解凍データ格納に必要なサイズが返る
u64 Huffman_Decode( void *Press, void *Dest )
{
    // 結合データと数値データ、０～２５５までが数値データ
    HUFFMAN_NODE Node[256 + 255] ;

    u64 PressSizeCounter, DestSizeCounter, DestSize ;
    unsigned char *PressPoint, *DestPoint ;
	u64 OriginalSize ;
	u64 PressSize ;
	u64 HeadSize ;
	u16 Weight[ 256 ] ;
    int i ;

    // void 型のポインタではアドレスの操作が出来ないので unsigned char 型のポインタにする
    PressPoint = ( unsigned char * )Press ;
    DestPoint = ( unsigned char * )Dest ;

    // 圧縮データの情報を取得する
	{
		BIT_STREAM BitStream ;
		u8 BitNum ;
		u8 Minus ;
		u16 SaveData ;

		BitStream_Init( &BitStream, PressPoint, true ) ;

		OriginalSize = BitStream_Read( &BitStream, ( u8 )( BitStream_Read( &BitStream, 6 ) + 1 ) ) ;
		PressSize    = BitStream_Read( &BitStream, ( u8 )( BitStream_Read( &BitStream, 6 ) + 1 ) ) ;

		// 出現頻度のテーブルを復元する
		BitNum      = ( u8 )( BitStream_Read( &BitStream, 3 ) + 1 ) * 2 ;
		Minus       = ( u8 )BitStream_Read( &BitStream, 1 ) ;
		SaveData    = ( u16 )BitStream_Read( &BitStream, BitNum ) ;
		Weight[ 0 ] = SaveData ;
        for( i = 1 ; i < 256 ; i ++ )
        {
			BitNum      = ( u8 )( BitStream_Read( &BitStream, 3 ) + 1 ) * 2 ;
			Minus       = ( u8 )BitStream_Read( &BitStream, 1 ) ;
			SaveData    = ( u16 )BitStream_Read( &BitStream, BitNum ) ;
			Weight[ i ] = Minus == 1 ? Weight[ i - 1 ] - SaveData : Weight[ i - 1 ] + SaveData ;
        }

		HeadSize = BitStream_GetBytes( &BitStream ) ;
	}
    
    // Dest が NULL の場合は 解凍後のデータのサイズを返す
    if( Dest == NULL )
        return OriginalSize ;

    // 解凍後のデータのサイズを取得する
    DestSize = OriginalSize ;

    // 各数値の結合データを構築する
    {
        int NodeIndex, MinNode1, MinNode2 ;
        int NodeNum, DataNum ;

        // 数値データを初期化する
        for( i = 0 ; i < 256 + 255 ; i ++ )
        {
            Node[i].Weight = Weight[i] ;    // 出現数は保存しておいたデータからコピー
            Node[i].ChildNode[0] = -1 ;    // 数値データが終点なので -1 をセットする
            Node[i].ChildNode[1] = -1 ;    // 数値データが終点なので -1 をセットする
            Node[i].ParentNode = -1 ;      // まだどの要素とも結合されていないので -1 をセットする
        }

        // 出現数の少ない数値データ or 結合データを繋いで
        // 新しい結合データを作成、全ての要素を繋いで残り１個になるまで繰り返す
        // (圧縮時と同じコードです)
        DataNum = 256 ; // 残り要素数
        NodeNum = 256 ; // 次に新しく作る結合データの要素配列のインデックス
        while( DataNum > 1 )
        {
            // 出現数値の低い要素二つを探す
            {
                MinNode1 = -1 ;
                MinNode2 = -1 ;
                
                // 残っている要素全てを調べるまでループ
                NodeIndex = 0 ;
                for( i = 0 ; i < DataNum ; NodeIndex ++ )
                {
                    // もう既に何処かの要素と結合されている場合は対象外
                    if( Node[NodeIndex].ParentNode != -1 ) continue ;
                    
                    i ++ ;
                    
                    // まだ有効な要素をセットしていないか、より出現数値の
                    // 少ない要素が見つかったら更新
                    if( MinNode1 == -1 || Node[MinNode1].Weight > Node[NodeIndex].Weight )
                    {
                        // 今まで一番出現数値が少なかったと思われた
                        // 要素は二番目に降格
                        MinNode2 = MinNode1 ;

                        // 新しい一番の要素の要素配列のインデックスを保存
                        MinNode1 = NodeIndex ;
                    }
                    else
                    {
                        // 一番よりは出現数値が多くても、二番目よりは出現数値が
                        // 少ないかもしれないので一応チェック(又は二番目に出現数値の
                        // 少ない要素がセットされていなかった場合もセット)
                        if( MinNode2 == -1 || Node[MinNode2].Weight > Node[NodeIndex].Weight )
                        {
                            MinNode2 = NodeIndex ;
                        }
                    }
                }
            }
            
            // 二つの要素を繋いで新しい要素(結合データ)を作る
            Node[NodeNum].ParentNode = -1 ;  // 新しいデータは当然まだ何処とも繋がっていないので -1 
            Node[NodeNum].Weight = Node[MinNode1].Weight + Node[MinNode2].Weight ;    // 出現数値は二つの数値を足したものをセットする
            Node[NodeNum].ChildNode[0] = MinNode1 ;    // この結合部で 0 を選んだら出現数値が一番少ない要素に繋がる
            Node[NodeNum].ChildNode[1] = MinNode2 ;    // この結合部で 1 を選んだら出現数値が二番目に少ない要素に繋がる

            // 結合された要素二つに、自分達に何の値が割り当てられたかをセットする
            Node[MinNode1].Index = 0 ;    // 一番出現数値が少ない要素は 0 番
            Node[MinNode2].Index = 1 ;    // 二番目に出現数値が少ない要素は 1 番

            // 結合された要素二つに、自分達を結合した結合データの要素配列インデックスをセットする
            Node[MinNode1].ParentNode = NodeNum ;
            Node[MinNode2].ParentNode = NodeNum ;

            // 要素の数を一個増やす
            NodeNum ++ ;

            // 残り要素の数は、一つ要素が新しく追加された代わりに
            // 二つの要素が結合されて検索の対象から外れたので
            // 結果 1 - 2 で -1 
            DataNum -- ;
        }

        // 各数値の圧縮時のビット列を割り出す
        {
            unsigned char TempBitArray[32] ;
            int TempBitIndex, TempBitCount, BitIndex, BitCount ;
        
            // 数値データと結合データの数だけ繰り返す
            for( i = 0 ; i < 256 + 254 ; i ++ )
            {
                // 数値データから結合データを上へ上へと辿ってビット数を数える
                {
                    // ビット数を初期化しておく
                    Node[i].BitNum = 0 ;
                    
                    // 一時的に数値データから遡っていったときのビット列を保存する処理の準備
                    TempBitIndex = 0 ;
                    TempBitCount = 0 ;
                    TempBitArray[TempBitIndex] = 0 ;
                    
                    // 何処かと結合されている限りカウントし続ける(天辺は何処とも結合されていないので終点だと分かる)
                    for( NodeIndex = ( int )i ; Node[NodeIndex].ParentNode != -1 ; NodeIndex = Node[NodeIndex].ParentNode )
                    {
                        // 配列要素一つに入るビットデータは８個なので、同じ配列要素に
                        // 既に８個保存していたら次の配列要素に保存先を変更する
                        if( TempBitCount == 8 )
                        {
                            TempBitCount = 0 ;
                            TempBitIndex ++ ;
                            TempBitArray[TempBitIndex] = 0 ;
                        }
                        
                        // 新しく書き込む情報で今までのデータを上書きしてしまわないように１ビット左にシフトする
                        TempBitArray[TempBitIndex] <<= 1 ;

                        // 結合データに割り振られたインデックスを最下位ビット(一番右側のビット)に書き込む
                        TempBitArray[TempBitIndex] |= (unsigned char)Node[NodeIndex].Index ;

                        // 保存したビット数を増やす
                        TempBitCount ++ ;

                        // ビット数を増やす
                        Node[i].BitNum ++ ;
                    }
                }
				
                // TempBitArray に溜まったデータは数値データから結合データを天辺に向かって
                // 上へ上へと遡っていった時のビット列なので、逆さまにしないと圧縮後のビット
                // 配列として使えない(展開時に天辺の結合データから数値データまで辿ることが
                // 出来ない)ので、順序を逆さまにしたものを数値データ内のビット列バッファに保存する
                {
                    BitCount = 0 ;
                    BitIndex = 0 ;
                    
                    // 最初のバッファを初期化しておく
                    // (全部 論理和(or)演算 で書き込むので、最初から１になっている
                    // ビットに０を書き込んでも１のままになってしまうため)
                    Node[i].BitArray[BitIndex] = 0 ;
                    
                    // 一時的に保存しておいたビット列の最初まで遡る
                    while( TempBitIndex >= 0 )
                    {
                        // 書き込んだビット数が一つの配列要素に入る８ビットに
                        // 達してしまったら次の配列要素に移る
                        if( BitCount == 8 )
                        {
                            BitCount = 0 ;
                            BitIndex ++ ;
                            Node[i].BitArray[BitIndex] = 0 ;
                        }

                        // まだ何も書き込まれていないビットアドレスに１ビット書き込む
                        Node[i].BitArray[BitIndex] |= (unsigned char)( ( TempBitArray[TempBitIndex] & 1 ) << BitCount ) ;
						
                        // 書き込み終わったビットはもういらないので次のビットを
                        // 書き込めるように１ビット右にシフトする
                        TempBitArray[TempBitIndex] >>= 1 ;
                        
                        // １ビット書き込んだので残りビット数を１個減らす
                        TempBitCount -- ;
                        
                        // もし現在書き込み元となっている配列要素に書き込んでいない
                        // ビット情報が無くなったら次の配列要素に移る
                        if( TempBitCount == 0 )
                        {
                            TempBitIndex -- ;
                            TempBitCount = 8 ;
                        }
                        
                        // 書き込んだビット数を増やす
                        BitCount ++ ;
                    }
                }
            }
		}
    }

    // 解凍処理
    {
        unsigned char *PressData ;
        int PressBitCounter, PressBitData, Index, NodeIndex ;
		int NodeIndexTable[ 512 ] ;
		int j ;

		// 各ビット配列がどのノードに繋がるかのテーブルを作成する
		{
			u16 BitMask[ 9 ] ;

			for( i = 0 ; i < 9 ; i ++ )
			{
				BitMask[ i ] = ( u16 )( ( 1 << ( i + 1 ) ) - 1 ) ;
			}

			for( i = 0 ; i < 512 ; i ++ )
			{
				NodeIndexTable[ i ] = -1 ;

				// ビット列に適合したノードを探す
				for( j = 0 ; j < 256 + 254 ; j ++ )
				{
					u16 BitArray01 ;

					if( Node[ j ].BitNum > 9 )
					{
						continue ;
					}

					BitArray01 = ( u16 )Node[ j ].BitArray[ 0 ] | ( Node[ j ].BitArray[ 1 ] << 8 ) ;
					if( ( i & BitMask[ Node[ j ].BitNum - 1 ] ) == ( BitArray01 & BitMask[ Node[ j ].BitNum - 1 ] ) )
					{
						NodeIndexTable[ i ] = j ;
						break ;
					}
				}
			}
		}

        // 圧縮データ本体の先頭アドレスをセット
        // (圧縮データ本体は元のサイズ、圧縮後のサイズ、各数値の出現数等を
        // 格納するデータ領域の後にある)
        PressData = PressPoint + HeadSize ;

        // 解凍したデータの格納アドレスを初期化
        DestSizeCounter = 0 ;
        
        // 圧縮データの参照アドレスを初期化
        PressSizeCounter = 0 ;
        
        // 圧縮ビットデータのカウンタを初期化
        PressBitCounter = 0 ;
        
        // 圧縮データの１バイト目をセット
        PressBitData = PressData[PressSizeCounter] ;

        // 圧縮前のデータサイズになるまで解凍処理を繰り返す
        for( DestSizeCounter = 0 ; DestSizeCounter < DestSize ; DestSizeCounter ++ )
        {
            // ビット列から数値データを検索する
            {
				// 最後の17byte分のデータは天辺から探す( 最後の次のバイトを読み出そうとしてメモリの不正なアクセスになる可能性があるため )
				if( DestSizeCounter >= DestSize - 17 )
				{
					// 結合データの天辺は一番最後の結合データが格納される５１０番目(０番から数える)
					// 天辺から順に下に降りていく
					NodeIndex = 510 ;
				}
				else
				{
					// それ以外の場合はテーブルを使用する

                    // もし PressBitData に格納されている全ての
                    // ビットデータを使い切ってしまった場合は次の
                    // ビットデータをセットする
                    if( PressBitCounter == 8 )
                    {
                        PressSizeCounter ++ ;
                        PressBitData = PressData[PressSizeCounter] ;
                        PressBitCounter = 0 ;
                    }

					// 圧縮データを9bit分用意する
					PressBitData = ( PressBitData | ( PressData[ PressSizeCounter + 1 ] << ( 8 - PressBitCounter ) ) ) & 0x1ff ;

					// テーブルから最初の結合データを探す
					NodeIndex = NodeIndexTable[ PressBitData ] ;

					// 使った分圧縮データのアドレスを進める
					PressBitCounter += Node[ NodeIndex ].BitNum ;
					if( PressBitCounter >= 16 )
					{
						PressSizeCounter += 2 ;
						PressBitCounter -= 16 ;
						PressBitData = PressData[PressSizeCounter] >> PressBitCounter ;
					}
					else
					if( PressBitCounter >= 8 )
					{
						PressSizeCounter ++ ;
						PressBitCounter -= 8 ;
						PressBitData = PressData[PressSizeCounter] >> PressBitCounter ;
					}
					else
					{
						PressBitData >>= Node[ NodeIndex ].BitNum ;
					}
				}
                
                // 数値データに辿り着くまで結合データを下りていく
                while( NodeIndex > 255 )
                {
                    // もし PressBitData に格納されている全ての
                    // ビットデータを使い切ってしまった場合は次の
                    // ビットデータをセットする
                    if( PressBitCounter == 8 )
                    {
                        PressSizeCounter ++ ;
                        PressBitData = PressData[PressSizeCounter] ;
                        PressBitCounter = 0 ;
                    }
                    
                    // １ビット取得する
                    Index = PressBitData & 1 ;
                    
                    // 使用した１ビット分だけ右にシフトする
                    PressBitData >>= 1 ;
                    
                    // 使用したビット数を一個増やす
                    PressBitCounter ++ ;
                    
                    // 次の要素(結合データか数値データかはまだ分からない)に移る
                    NodeIndex = Node[NodeIndex].ChildNode[Index] ;
                }
            }

            // 辿り着いた数値データを出力
            DestPoint[DestSizeCounter] = (unsigned char)NodeIndex ;
        }
    }

    // 解凍後のサイズを返す
    return OriginalSize ;
}

