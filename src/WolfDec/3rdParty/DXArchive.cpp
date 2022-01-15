// -------------------------------------------------------------------------------
// 
// 		ＤＸライブラリアーカイバ
// 
//	Creator			: 山田 巧
//	Creation Date	: 2003/09/11
//	Version			: 1.02
// 
// -------------------------------------------------------------------------------

#define INLINE_ASM

// include ----------------------------
#include "DXArchive.h"
#include "CharCode.h"
#include "Huffman.h"
#include "FileLib.h"
#include <stdio.h>
#include <windows.h>
#include <string.h>

// define -----------------------------

#define MIN_COMPRESS		(4)						// 最低圧縮バイト数
#define MAX_SEARCHLISTNUM	(64)					// 最大一致長を探す為のリストを辿る最大数
#define MAX_SUBLISTNUM		(65536)					// 圧縮時間短縮のためのサブリストの最大数
#define MAX_COPYSIZE 		(0x1fff + MIN_COMPRESS)	// 参照アドレスからコピー出切る最大サイズ( 圧縮コードが表現できるコピーサイズの最大値 + 最低圧縮バイト数 )
#define MAX_ADDRESSLISTNUM	(1024 * 1024 * 1)		// スライド辞書の最大サイズ
#define MAX_POSITION		(1 << 24)				// 参照可能な最大相対アドレス( 16MB )

WCHAR sjis2utf8(const char* sjis);

// struct -----------------------------

// 圧縮時間短縮用リスト
typedef struct LZ_LIST
{
	LZ_LIST *next, *prev ;
	u32 address ;
} LZ_LIST ;

// data -------------------------------

// デフォルト鍵文字列
static char DefaultKeyString[ 9 ] = { 0x44, 0x58, 0x42, 0x44, 0x58, 0x41, 0x52, 0x43, 0x00 }; // "DXLIBARC"

// ログ文字列の長さ
size_t LogStringLength = 0 ;

// class code -------------------------

// ファイル名も一緒になっていると分かっているパス中からファイルパスとディレクトリパスを分割する
// フルパスである必要は無い
int DXArchive::GetFilePathAndDirPath(TCHAR *Src, TCHAR *FilePath, TCHAR *DirPath )
{
	int i, Last ;
	
	// ファイル名を抜き出す
	i = 0 ;
	Last = -1 ;
	while( Src[i] != '\0' )
	{
		if( CheckMultiByteChar( &Src[i] ) == FALSE )
		{
			if( Src[i] == '\\' || Src[i] == '/' || Src[i] == '\0' || Src[i] == ':' ) Last = i ;
			i ++ ;
		}
		else
		{
			i += 2 ;
		}
	}
	if( FilePath != NULL )
	{
		if( Last != -1 ) _tcscpy( FilePath, &Src[Last+1] ) ;
		else _tcscpy( FilePath, Src ) ;
	}
	
	// ディレクトリパスを抜き出す
	if( DirPath != NULL )
	{
		if( Last != -1 )
		{
			_tcsncpy( DirPath, Src, Last ) ;
			DirPath[Last] = '\0' ;
		}
		else
		{
			DirPath[0] = '\0' ;
		}
	}
	
	// 終了
	return 0 ;
}

// ファイルの情報を得る
DARC_FILEHEAD *DXArchive::GetFileInfo( const TCHAR *FilePath, DARC_DIRECTORY **DirectoryP )
{
	DARC_DIRECTORY *OldDir ;
	DARC_FILEHEAD *FileH ;
	u8 *NameData ;
	int i, j, k, Num, FileHeadSize ;
	SEARCHDATA SearchData ;

	// 元のディレクトリを保存しておく
	OldDir = this->CurrentDirectory ;

	// ファイルパスに \ が含まれている場合、ディレクトリ変更を行う
	if( _tcschr( FilePath, '\\' ) != NULL )
	{
		// カレントディレクトリを目的のファイルがあるディレクトリに変更する
		if( this->ChangeCurrentDirectoryBase( FilePath, false, &SearchData ) >= 0 )
		{
			// エラーが起きなかった場合はファイル名もディレクトリだったことになるのでエラー
			goto ERR ;
		}
	}
	else
	{
		// ファイル名を検索用データに変換する
		ConvSearchData( &SearchData, FilePath, NULL ) ;
	}

	// 同名のファイルを探す
	FileHeadSize = sizeof( DARC_FILEHEAD ) ;
	FileH = ( DARC_FILEHEAD * )( this->FileP + this->CurrentDirectory->FileHeadAddress ) ;
	Num = ( int )this->CurrentDirectory->FileHeadNum ;
	for( i = 0 ; i < Num ; i ++, FileH = (DARC_FILEHEAD *)( (u8 *)FileH + FileHeadSize ) )
	{
		// ディレクトリチェック
		if( ( FileH->Attributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ) continue ;

		// 文字列数とパリティチェック
		NameData = this->NameP + FileH->NameAddress ;
		if( SearchData.PackNum != ((u16 *)NameData)[0] || SearchData.Parity != ((u16 *)NameData)[1] ) continue ;

		// 文字列チェック
		NameData += 4 ;
		for( j = 0, k = 0 ; j < SearchData.PackNum ; j ++, k += 4 )
			if( *((u32 *)&SearchData.FileName[k]) != *((u32 *)&NameData[k]) ) break ;

		// 適合したファイルがあったらここで終了
		if( SearchData.PackNum == j ) break ;
	}

	// 無かったらエラー
	if( i == Num ) goto ERR ;

	// ディレクトリのアドレスを保存する指定があった場合は保存
	if( DirectoryP != NULL )
	{
		*DirectoryP = this->CurrentDirectory ;
	}
	
	// ディレクトリを元に戻す
	this->CurrentDirectory = OldDir ;
	
	// 目的のファイルのアドレスを返す
	return FileH ;
	
ERR :
	// ディレクトリを元に戻す
	this->CurrentDirectory = OldDir ;
	
	// エラー終了
	return NULL ;
}

// アーカイブ内のカレントディレクトリの情報を取得する
DARC_DIRECTORY *DXArchive::GetCurrentDirectoryInfo( void )
{
	return CurrentDirectory ;
}

// どちらが新しいかを比較する
DXArchive::DATE_RESULT DXArchive::DateCmp( DARC_FILETIME *date1, DARC_FILETIME *date2 )
{
	if( date1->LastWrite == date2->LastWrite ) return DATE_RESULT_DRAW ;
	else if( date1->LastWrite > date2->LastWrite ) return DATE_RESULT_LEFTNEW ;
	else return DATE_RESULT_RIGHTNEW ;
}

// 比較対照の文字列中の大文字を小文字として扱い比較する( 0:等しい  1:違う )
int DXArchive::StrICmp( const TCHAR *Str1, const TCHAR *Str2 )
{
	int c1, c2 ;
	
	while( *Str1 != '\0' && *Str2 != '\0' )
	{
		if( CheckMultiByteChar( Str1 ) == FALSE )
		{
			c1 = ( *Str1 >= 'A' && *Str1 <= 'Z' ) ? *Str1 - 'A' + 'a' : *Str1 ;
			c2 = ( *Str2 >= 'A' && *Str2 <= 'Z' ) ? *Str2 - 'A' + 'a' : *Str2 ;
			if( c1 != c2 ) return 1 ;
			Str1 ++ ;
			Str2 ++ ;
		}
		else
		{
			if( *( (unsigned short *)Str1 ) != *( (unsigned short *)Str2 ) ) return 1 ;
			Str1 += 2 ;
			Str2 += 2 ;
		}
	}
	if( *Str1 != '\0' || *Str2 != '\0' ) return 1 ;

	// 此処まで来て初めて等しい
	return 0 ;
}

// 文字列中の英字の小文字を大文字に変換
int DXArchive::ConvSearchData( SEARCHDATA *SearchData, const TCHAR *Src, int *Length )
{
	int i, StringLength ;
	u16 ParityData ;

	ParityData = 0 ;
	for( i = 0 ; Src[i] != '\0' && Src[i] != '\\' ; )
	{
		if( CheckMultiByteChar( &Src[i] ) == TRUE )
		{
			// ２バイト文字の場合はそのままコピー
			*((u16 *)&SearchData->FileName[i]) = *((u16 *)&Src[i]) ;
			ParityData += (u8)Src[i] + (u8)Src[i+1] ;
			i += 2 ;
		}
		else
		{
			// 小文字の場合は大文字に変換
			if( Src[i] >= 'a' && Src[i] <= 'z' )	SearchData->FileName[i] = (u8)Src[i] - 'a' + 'A' ;
			else									SearchData->FileName[i] = (u8)Src[i] ;
			ParityData += (u8)SearchData->FileName[i] ;
			i ++ ;
		}
	}

	// 文字列の長さを保存
	if( Length != NULL ) *Length = i ;

	// ４の倍数の位置まで０を代入
	StringLength = ( ( i + 1 ) + 3 ) / 4 * 4 ;
	memset( &SearchData->FileName[i], 0, StringLength - i ) ;

	// パリティデータの保存
	SearchData->Parity = ParityData ;

	// パックデータ数の保存
	SearchData->PackNum = StringLength / 4 ;

	// 正常終了
	return 0 ;
}

// ファイル名データを追加する( 戻り値は使用したデータバイト数 )
int DXArchive::AddFileNameData( const TCHAR *FileName, u8 *FileNameTable )
{
	int PackNum, Length, i ;
	u32 Parity ;

	// サイズをセット
	Length = ( int )_tcslen( FileName ) ;

	// 一文字も無かった場合の処理
	if( Length == 0 )
	{
		// パック数とパリティ情報のみ保存
		*((u32 *)&FileNameTable[0]) = 0 ;

		// 使用サイズを返す
		return 4 ;
	}
	Length ++ ;

	PackNum = ( Length + 3 ) / 4 ;

	// パック数を保存
	*((u16 *)&FileNameTable[0]) = PackNum ;

	// バッファの初期化
	memset( &FileNameTable[4], 0, PackNum * 4 * 2 ) ;

	// 文字列をコピー
	_tcscpy( (TCHAR *)&FileNameTable[4 + PackNum * 4], FileName ) ;

	// 英字の小文字を全て大文字に変換したファイル名を保存
	Parity = 0 ;
	for( i = 0 ; FileName[i] != '\0' ; )
	{
		// ２バイト文字かどうかで処理を分岐
		if( CheckMultiByteChar( &FileName[i] ) == TRUE )
		{
			// ２バイト文字
			*((u16 *)&FileNameTable[4 + i]) = *((u16 *)&FileName[i]) ;
			Parity += (u8)FileName[i] + (u8)FileName[i+1] ;
			i += 2 ;
		}
		else
		{
			// １バイト文字
			if( FileName[i] >= 'a' && FileName[i] <= 'z' )
			{
				// 小文字の場合は大文字に変換
				FileNameTable[4 + i] = (u8)FileName[i] - 'a' + 'A' ;
			}
			else
			{
				// そうではない場合は普通にコピー
				FileNameTable[4 + i] = (u8)FileName[i] ;
			}
			Parity += FileNameTable[4 + i] ;
			i ++ ;
		}
	}

	// パリティ情報を保存
	*((u16 *)&FileNameTable[2]) = (u16)Parity ;

	// 使用したサイズを返す
	return PackNum * 4 * 2 + 4 ;
}

// ファイル名データから元のファイル名の文字列を取得する
TCHAR *DXArchive::GetOriginalFileName( u8 *FileNameTable )
{
	const char *pName = ((char *)FileNameTable + *((u16 *)&FileNameTable[0]) * 4 + 4);

	bool isMultiByte = false;
	size_t nameLen = strlen(pName);
	TCHAR *pFileStr = new TCHAR[nameLen+1]();

	for(size_t si = 0, sf = 0; si < nameLen; si++, sf++)
	{
#ifdef _UNICODE
		if(CheckMultiByteChar((TCHAR*)&pName[si]))
		{
			isMultiByte = true;
			char ss[3];
			ss[0] = pName[si];
			ss[1] = pName[si + 1];
			ss[2] = 0;
			pFileStr[sf] = sjis2utf8(ss);
			si++;
		}
		else
#endif
			pFileStr[sf] = pName[si];
	}

	return pFileStr;
}

// 標準ストリームにデータを書き込む( 64bit版 )
void DXArchive::fwrite64( void *Data, s64 Size, FILE *fp )
{
	int WriteSize ;
	s64 TotalWriteSize ;

	TotalWriteSize = 0 ;
	while( TotalWriteSize < Size )
	{
		if( Size > 0x7fffffff )
		{
			WriteSize = 0x7fffffff ;
		}
		else
		{
			WriteSize = ( int )Size ;
		}

		fwrite( ( u8 * )Data + TotalWriteSize, 1, WriteSize, fp ) ;

		TotalWriteSize += WriteSize ;
	}
}

// 標準ストリームからデータを読み込む( 64bit版 )
void DXArchive::fread64( void *Buffer, s64 Size, FILE *fp )
{
	int ReadSize ;
	s64 TotalReadSize ;

	TotalReadSize = 0 ;
	while( TotalReadSize < Size )
	{
		if( Size > 0x7fffffff )
		{
			ReadSize = 0x7fffffff ;
		}
		else
		{
			ReadSize = ( int )Size ;
		}

		fread( ( u8 * )Buffer + TotalReadSize, 1, ReadSize, fp ) ;

		TotalReadSize += ReadSize ;
	}
}

// データを反転させる関数
void DXArchive::NotConv( void *Data , s64 Size )
{
	s64 DwordNumQ ;
	s64 ByteNum ;
	u32 *dd ;

	dd = ( u32 * )Data ;

	DwordNumQ = Size / 4 ;
	ByteNum = Size - DwordNumQ * 4 ;

	if( DwordNumQ != 0 )
	{
		if( DwordNumQ < 0x100000000 )
		{
			u32 DwordNum ;

			DwordNum = ( u32 )DwordNumQ ;
			do
			{
				*dd++ = ~*dd ;
			}while( --DwordNum ) ;
		}
		else
		{
			do
			{
				*dd++ = ~*dd ;
			}while( --DwordNumQ ) ;
		}
	}
	if( ByteNum != 0 )
	{
		do
		{
			*((BYTE *)dd) = ~*((u8 *)dd) ;
			dd = (u32 *)(((u8 *)dd) + 1) ;
		}while( --ByteNum ) ;
	}
}


// データを反転させてファイルに書き出す関数
void DXArchive::NotConvFileWrite( void *Data, s64 Size, FILE *fp )
{
	// データを反転する
	NotConv( Data, Size ) ;

	// 書き出す
	fwrite64( Data, Size, fp ) ;

	// 再び反転
	NotConv( Data, Size ) ;
}

// データを反転させてファイルから読み込む関数
void DXArchive::NotConvFileRead( void *Data, s64 Size, FILE *fp )
{
	// 読み込む
	fread64( Data, Size, fp ) ;

	// データを反転
	NotConv( Data, Size ) ;
}

// カレントディレクトリにある指定のファイルの鍵用の文字列を作成する、戻り値は文字列の長さ( 単位：Byte )( FileString は DXA_KEY_STRING_MAXLENGTH の長さが必要 )
size_t DXArchive::CreateKeyFileString( int CharCodeFormat, const char *KeyString, size_t KeyStringBytes, DARC_DIRECTORY *Directory, DARC_FILEHEAD *FileHead, u8 *FileTable, u8 *DirectoryTable, u8 *NameTable, u8 *FileString )
{
	size_t StartAddr ;

	// 最初にパスワードの文字列をセット
	if( KeyString != NULL && KeyStringBytes != 0 )
	{
		memcpy( FileString, KeyString, KeyStringBytes ) ;
		FileString[ KeyStringBytes ] = '\0' ;
		StartAddr = KeyStringBytes ;
	}
	else
	{
		FileString[ 0 ] = '\0' ;
		StartAddr = 0 ;
	}
	memset( &FileString[ DXA_KEY_STRING_MAXLENGTH - 8 ], 0, 8 ) ;

	// 次にファイル名の文字列をセット
	CL_strcat_s( CharCodeFormat, ( char * )&FileString[ StartAddr ], ( DXA_KEY_STRING_MAXLENGTH - 8 ) - StartAddr, ( char * )( NameTable + FileHead->NameAddress + 4 ) ) ;

	// その後にディレクトリの文字列をセット
	if( Directory->ParentDirectoryAddress != 0xffffffffffffffff )
	{
		do
		{
			CL_strcat_s( CharCodeFormat, ( char * )&FileString[ StartAddr ], ( DXA_KEY_STRING_MAXLENGTH - 8 ) - StartAddr, ( char * )( NameTable + ( ( DARC_FILEHEAD * )( FileTable + Directory->DirectoryAddress ) )->NameAddress + 4 ) ) ;
			Directory = ( DARC_DIRECTORY * )( DirectoryTable + Directory->ParentDirectoryAddress ) ;
		}while( Directory->ParentDirectoryAddress != 0xffffffffffffffff ) ;
	}

	return StartAddr + CL_strlen( CharCodeFormat, ( char * )&FileString[ StartAddr ] ) * GetCharCodeFormatUnitSize( CharCodeFormat ) ;
}

// 鍵文字列を作成
void DXArchive::KeyCreate( const char *Source, size_t SourceBytes, u8 *Key )
{
	char SourceTempBuffer[ 1024 ] ;
	char WorkBuffer[ 1024 ] ;
	char *UseWorkBuffer ;
	u32 i, j ;
	u32 CRC32_0 ;
	u32 CRC32_1 ;

	if( SourceBytes == 0 )
	{
		SourceBytes = CL_strlen( CHARCODEFORMAT_ASCII, Source ) ;
	}

	if( SourceBytes < 4 )
	{
		CL_strcpy( CHARCODEFORMAT_ASCII, SourceTempBuffer, Source ) ;
		CL_strcpy( CHARCODEFORMAT_ASCII, &SourceTempBuffer[ SourceBytes ], DefaultKeyString ) ;
		Source = SourceTempBuffer ;
		SourceBytes = CL_strlen( CHARCODEFORMAT_ASCII, Source ) ;
	}

	if( SourceBytes > sizeof( WorkBuffer ) )
	{
		UseWorkBuffer = ( char * )malloc( SourceBytes ) ;
	}
	else
	{
		UseWorkBuffer = WorkBuffer ;
	}

	j = 0 ;
	for( i = 0 ; i < SourceBytes ; i += 2, j++ )
	{
		UseWorkBuffer[ j ] = Source[ i ] ;
	}
	CRC32_0 = HashCRC32( UseWorkBuffer, j ) ;

	j = 0 ;
	for( i = 1 ; i < SourceBytes ; i += 2, j++ )
	{
		UseWorkBuffer[ j ] = Source[ i ] ;
	}
	CRC32_1 = HashCRC32( UseWorkBuffer, j ) ;

	Key[ 0 ] = ( u8 )( CRC32_0 >>  0 ) ;
	Key[ 1 ] = ( u8 )( CRC32_0 >>  8 ) ;
	Key[ 2 ] = ( u8 )( CRC32_0 >> 16 ) ;
	Key[ 3 ] = ( u8 )( CRC32_0 >> 24 ) ;
	Key[ 4 ] = ( u8 )( CRC32_1 >>  0 ) ;
	Key[ 5 ] = ( u8 )( CRC32_1 >>  8 ) ;
	Key[ 6 ] = ( u8 )( CRC32_1 >> 16 ) ;

	if( SourceBytes > sizeof( WorkBuffer ) )
	{
		free( UseWorkBuffer ) ;
	}
}

// 鍵文字列を使用して Xor 演算( Key は必ず DXA_KEY_BYTES の長さがなければならない )
void DXArchive::KeyConv( void *Data, s64 Size, s64 Position, unsigned char *Key )
{
	if( Key == NULL )
	{
		return ;
	}

	Position %= DXA_KEY_BYTES ;

	if( Size < 0x100000000 )
	{
		u32 i, j ;

		j = ( u32 )Position ;
		for( i = 0 ; i < Size ; i ++ )
		{
			((u8 *)Data)[i] ^= Key[j] ;

			j ++ ;
			if( j == DXA_KEY_BYTES ) j = 0 ;
		}
	}
	else
	{
		s64 i, j ;

		j = Position ;
		for( i = 0 ; i < Size ; i ++ )
		{
			((u8 *)Data)[i] ^= Key[j] ;

			j ++ ;
			if( j == DXA_KEY_BYTES ) j = 0 ;
		}
	}
}

// データを鍵文字列を使用して Xor 演算した後ファイルに書き出す関数( Key は必ず DXA_KEY_BYTES の長さがなければならない )
void DXArchive::KeyConvFileWrite( void *Data, s64 Size, FILE *fp, unsigned char *Key, s64 Position )
{
	s64 pos = 0 ;

	if( Key != NULL )
	{
		// ファイルの位置を取得しておく
		pos = Position == -1 ? _ftelli64( fp ) : Position ;

		// データを鍵文字列を使って Xor 演算する
		KeyConv( Data, Size, pos, Key ) ;
	}

	// 書き出す
	fwrite64( Data, Size, fp ) ;

	if( Key != NULL )
	{
		// 再び Xor 演算
		KeyConv( Data, Size, pos, Key ) ;
	}
}

// ファイルから読み込んだデータを鍵文字列を使用して Xor 演算する関数( Key は必ず DXA_KEY_BYTES の長さがなければならない )
void DXArchive::KeyConvFileRead( void *Data, s64 Size, FILE *fp, unsigned char *Key, s64 Position )
{
	s64 pos = 0 ;

	if( Key != NULL )
	{
		// ファイルの位置を取得しておく
		pos = Position == -1 ? _ftelli64( fp ) : Position ;
	}

	// 読み込む
	fread64( Data, Size, fp ) ;

	if( Key != NULL )
	{
		// データを鍵文字列を使って Xor 演算
		KeyConv( Data, Size, pos, Key ) ;
	}
}

// 指定のディレクトリにあるファイルをアーカイブデータに吐き出す
int DXArchive::DirectoryEncode( int CharCodeFormat, TCHAR *DirectoryName, u8 *NameP, u8 *DirP, u8 *FileP, DARC_DIRECTORY *ParentDir, SIZESAVE *Size, int DataNumber, FILE *DestFp, void *TempBuffer, bool Press, bool MaxPress, bool AlwaysHuffman, u8 HuffmanEncodeKB, const char *KeyString, size_t KeyStringBytes, bool NoKey, char *KeyStringBuffer, DARC_ENCODEINFO *EncodeInfo )
{
	TCHAR DirPath[MAX_PATH] ;
	WIN32_FIND_DATA FindData ;
	HANDLE FindHandle ;
	DARC_DIRECTORY Dir ;
	DARC_DIRECTORY *DirectoryP ;
	DARC_FILEHEAD File ;
	u8 lKey[DXA_KEY_BYTES] ;
	size_t KeyStringBufferBytes ;

	// ディレクトリの情報を得る
	FindHandle = FindFirstFile( DirectoryName, &FindData ) ;
	if( FindHandle == INVALID_HANDLE_VALUE ) return 0 ;
	
	// ディレクトリ情報を格納するファイルヘッダをセットする
	{
		File.NameAddress       = Size->NameSize ;
		File.Time.Create       = ( ( ( LONGLONG )FindData.ftCreationTime.dwHighDateTime ) << 32 ) + FindData.ftCreationTime.dwLowDateTime ;
		File.Time.LastAccess   = ( ( ( LONGLONG )FindData.ftLastAccessTime.dwHighDateTime ) << 32 ) + FindData.ftLastAccessTime.dwLowDateTime ;
		File.Time.LastWrite    = ( ( ( LONGLONG )FindData.ftLastWriteTime.dwHighDateTime ) << 32 ) + FindData.ftLastWriteTime.dwLowDateTime ;
		File.Attributes        = FindData.dwFileAttributes ;
		File.DataAddress       = Size->DirectorySize ;
		File.DataSize          = 0 ;
		File.PressDataSize	   = 0xffffffffffffffff ;
		File.HuffPressDataSize = 0xffffffffffffffff ;
	}

	// ディレクトリ名を書き出す
	Size->NameSize += AddFileNameData( FindData.cFileName, NameP + Size->NameSize ) ;

	// ディレクトリ情報が入ったファイルヘッダを書き出す
	memcpy( FileP + ParentDir->FileHeadAddress + DataNumber * sizeof( DARC_FILEHEAD ),
			&File, sizeof( DARC_FILEHEAD ) ) ;

	// Find ハンドルを閉じる
	FindClose( FindHandle ) ;

	// 指定のディレクトリにカレントディレクトリを移す
	GetCurrentDirectory( MAX_PATH, DirPath ) ;
	SetCurrentDirectory( DirectoryName ) ;

	// ディレクトリ情報のセット
	{
		Dir.DirectoryAddress = ParentDir->FileHeadAddress + DataNumber * sizeof( DARC_FILEHEAD ) ;
		Dir.FileHeadAddress  = Size->FileSize ;

		// 親ディレクトリの情報位置をセット
		if( ParentDir->DirectoryAddress != 0xffffffffffffffff && ParentDir->DirectoryAddress != 0 )
		{
			Dir.ParentDirectoryAddress = ((DARC_FILEHEAD *)( FileP + ParentDir->DirectoryAddress ))->DataAddress ;
		}
		else
		{
			Dir.ParentDirectoryAddress = 0 ;
		}

		// ディレクトリ中のファイルの数を取得する
		Dir.FileHeadNum = GetDirectoryFilePath(TEXT(""), NULL ) ;
	}

	// ディレクトリの情報を出力する
	memcpy( DirP + Size->DirectorySize, &Dir, sizeof( DARC_DIRECTORY ) ) ;	
	DirectoryP = ( DARC_DIRECTORY * )( DirP + Size->DirectorySize ) ;

	// アドレスを推移させる
	Size->DirectorySize += sizeof( DARC_DIRECTORY ) ;
	Size->FileSize      += sizeof( DARC_FILEHEAD ) * Dir.FileHeadNum ;
	
	// ファイルが何も無い場合はここで終了
	if( Dir.FileHeadNum == 0 )
	{
		// もとのディレクトリをカレントディレクトリにセット
		SetCurrentDirectory( DirPath ) ;
		return 0 ;
	}

	// ファイル情報を出力する
	{
		int i ;
		
		i = 0 ;
		
		// 列挙開始
		FindHandle = FindFirstFile(TEXT("*"), &FindData ) ;
		do
		{
			// 上のディレクトリに戻ったりするためのパスは無視する
			if( _tcscmp( FindData.cFileName, TEXT(".") ) == 0 || _tcscmp( FindData.cFileName, TEXT("..") ) == 0 ) continue ;

			// ファイルではなく、ディレクトリだった場合は再帰する
			if( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				// ディレクトリだった場合の処理
				if( DirectoryEncode( CharCodeFormat, FindData.cFileName, NameP, DirP, FileP, &Dir, Size, i, DestFp, TempBuffer, Press, MaxPress, AlwaysHuffman, HuffmanEncodeKB, KeyString, KeyStringBytes, NoKey, KeyStringBuffer, EncodeInfo ) < 0 ) return -1 ;
			}
			else
			{
				// ファイルだった場合の処理

				// ファイルのデータをセット
				File.NameAddress       = Size->NameSize ;
				File.Time.Create       = ( ( ( LONGLONG )FindData.ftCreationTime.dwHighDateTime   ) << 32 ) + FindData.ftCreationTime.dwLowDateTime   ;
				File.Time.LastAccess   = ( ( ( LONGLONG )FindData.ftLastAccessTime.dwHighDateTime ) << 32 ) + FindData.ftLastAccessTime.dwLowDateTime ;
				File.Time.LastWrite    = ( ( ( LONGLONG )FindData.ftLastWriteTime.dwHighDateTime  ) << 32 ) + FindData.ftLastWriteTime.dwLowDateTime  ;
				File.Attributes        = FindData.dwFileAttributes ;
				File.DataAddress       = Size->DataSize ;
				File.DataSize          = ( ( ( LONGLONG )FindData.nFileSizeHigh ) << 32 ) + FindData.nFileSizeLow ;
				File.PressDataSize     = 0xffffffffffffffff ;
				File.HuffPressDataSize = 0xffffffffffffffff ;

				// 進行状況出力
				if( EncodeInfo->OutputStatus )
				{
					// 処理ファイル名をセット
					wcscpy( EncodeInfo->ProcessFileName, FindData.cFileName ) ;

					// ファイル数を増やす
					EncodeInfo->CompFileNum ++ ;

					// 表示
					EncodeStatusOutput( EncodeInfo ) ;
				}

				// ファイル名を書き出す
				Size->NameSize += AddFileNameData( FindData.cFileName, NameP + Size->NameSize ) ;

				// ファイル個別の鍵を作成
				if( NoKey == false )
				{
					KeyStringBufferBytes = CreateKeyFileString( CharCodeFormat, KeyString, KeyStringBytes, DirectoryP, &File, FileP, DirP, NameP, ( BYTE * )KeyStringBuffer ) ;
					KeyCreate( KeyStringBuffer, KeyStringBufferBytes, lKey ) ;
				}
				
				// ファイルデータを書き出す
				if( File.DataSize != 0 )
				{
					FILE *SrcP ;
					u64 FileSize, WriteSize, MoveSize ;
					bool Huffman = false ;
					bool AlwaysPress = false ;

					// ファイルを開く
					SrcP = _tfopen( FindData.cFileName, TEXT("rb") ) ;
					
					// サイズを得る
					_fseeki64( SrcP, 0, SEEK_END ) ;
					FileSize = _ftelli64( SrcP ) ;
					_fseeki64( SrcP, 0, SEEK_SET ) ;

					// 圧縮の対象となるファイルフォーマットか調べる
					{
						u32 Len ;
						Len = ( u32 )_tcslen( FindData.cFileName ) ;
						if( Len > 4 )
						{
							TCHAR *sp ;
						
							sp = &FindData.cFileName[Len-3] ;
							if( StrICmp( sp, TEXT("wav") ) == 0 ||
								StrICmp( sp, TEXT("jpg") ) == 0 ||
								StrICmp( sp, TEXT("png") ) == 0 ||
								StrICmp( sp, TEXT("mpg") ) == 0 ||
								StrICmp( sp, TEXT("mp3") ) == 0 ||
								StrICmp( sp, TEXT("mp4") ) == 0 ||
								StrICmp( sp, TEXT("m4a") ) == 0 ||
								StrICmp( sp, TEXT("ogg") ) == 0 ||
								StrICmp( sp, TEXT("ogv") ) == 0 ||
								StrICmp( sp, TEXT("ops") ) == 0 ||
								StrICmp( sp, TEXT("wmv") ) == 0 ||
								StrICmp( sp, TEXT("tif") ) == 0 ||
								StrICmp( sp, TEXT("tga") ) == 0 ||
								StrICmp( sp, TEXT("bmp") ) == 0 ||
								StrICmp( sp - 1, TEXT("jpeg") ) == 0 )
							{
								Huffman = true ;
							}

							// wav や bmp の場合は必ず圧縮する
							if( StrICmp( sp, TEXT("wav") ) == 0 ||
								StrICmp( sp, TEXT("tga") ) == 0 ||
								StrICmp( sp, TEXT("bmp") ) == 0 )
							{
								AlwaysPress = true ;
							}
						}
					}

					// AlwaysHuffman が true の場合は必ずハフマン圧縮する
					if( AlwaysHuffman )
					{
						Huffman = true ;
					}

					// ハフマン圧縮するサイズが 0 の場合はハフマン圧縮を行わない
					if( HuffmanEncodeKB == 0 )
					{
						Huffman = false ;
					}
					
					// 圧縮の指定がある場合で、
					// 必ず圧縮するファイルフォーマットか、ファイルサイズが 10MB 以下の場合は圧縮を試みる
					if( Press == true && ( AlwaysPress || File.DataSize < 10 * 1024 * 1024 ) )
					{
						void *SrcBuf, *DestBuf ;
						u32 DestSize, Len ;
						
						// 一部のファイル形式の場合は予め弾く
						if( AlwaysPress == false && ( Len = ( int )_tcslen( FindData.cFileName ) ) > 4 )
						{
							TCHAR *sp ;
							
							sp = &FindData.cFileName[Len-3] ;
							if( StrICmp( sp, TEXT("wav") ) == 0 ||
								StrICmp( sp, TEXT("jpg") ) == 0 ||
								StrICmp( sp, TEXT("png") ) == 0 ||
								StrICmp( sp, TEXT("mpg") ) == 0 ||
								StrICmp( sp, TEXT("mp3") ) == 0 ||
								StrICmp( sp, TEXT("mp4") ) == 0 ||
								StrICmp( sp, TEXT("ogg") ) == 0 ||
								StrICmp( sp, TEXT("ogv") ) == 0 ||
								StrICmp( sp, TEXT("ops") ) == 0 ||
								StrICmp( sp, TEXT("wmv") ) == 0 ||
								StrICmp( sp - 1, TEXT("jpeg") ) == 0 ) goto NOPRESS ;
						}
						
						// データが丸ごと入るメモリ領域の確保
						SrcBuf  = malloc( ( size_t )( FileSize + FileSize * 2 + 64 ) ) ;
						DestBuf = (u8 *)SrcBuf + FileSize ;
						
						// ファイルを丸ごと読み込む
						fread64( SrcBuf, FileSize, SrcP ) ;
						
						// 圧縮する場合は強制的に進行状況出力を更新
						if( EncodeInfo->OutputStatus )
						{
							EncodeStatusOutput( EncodeInfo, true ) ;
						}

						// 圧縮
						DestSize = Encode( SrcBuf, ( u32 )FileSize, DestBuf, EncodeInfo->OutputStatus, MaxPress ) ;
						
						// 殆ど圧縮出来なかった場合は圧縮無しでアーカイブする
						if( AlwaysPress == false && ( (f64)DestSize / (f64)FileSize > 0.90 ) )
						{
							_fseeki64( SrcP, 0L, SEEK_SET ) ;
							free( SrcBuf ) ;
							goto NOPRESS ;
						}

						// 圧縮データのサイズを保存する
						File.PressDataSize = DestSize ;
						
						// ハフマン圧縮も行うかどうかで処理を分岐
						if( Huffman )
						{
							u8 *HuffData ;

							// ハフマン圧縮するサイズによって処理を分岐
							if( HuffmanEncodeKB == 0xff || DestSize <= ( u64 )( HuffmanEncodeKB * 1024 * 2 ) )
							{
								// ハフマン圧縮用のメモリ領域を確保
								HuffData = ( u8 * )calloc( 1, DestSize * 2 + 256 * 2 + 32 ) ;

								// ファイル全体をハフマン圧縮
								File.HuffPressDataSize = Huffman_Encode( DestBuf, DestSize, HuffData ) ;

								// 圧縮データに鍵を適用して書き出す
								WriteSize = ( File.HuffPressDataSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
								KeyConvFileWrite( HuffData, WriteSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;
							}
							else
							{
								// ハフマン圧縮用のメモリ領域を確保
								HuffData = ( u8 * )calloc( 1, HuffmanEncodeKB * 1024 * 2 * 4 + 256 * 2 + 32 ) ;

								// ファイルの前後をハフマン圧縮
								memcpy( HuffData,                                  DestBuf,                                     HuffmanEncodeKB * 1024 ) ;
								memcpy( HuffData + HuffmanEncodeKB * 1024, ( u8 * )DestBuf + DestSize - HuffmanEncodeKB * 1024, HuffmanEncodeKB * 1024 ) ;
								File.HuffPressDataSize = Huffman_Encode( HuffData, HuffmanEncodeKB * 1024 * 2, HuffData + HuffmanEncodeKB * 1024 * 2 ) ;

								// ハフマン圧縮した部分を書き出す
								KeyConvFileWrite( HuffData + HuffmanEncodeKB * 1024 * 2, File.HuffPressDataSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;

								// ハフマン圧縮していない箇所を書き出す
								WriteSize = File.HuffPressDataSize + DestSize - HuffmanEncodeKB * 1024 * 2 ;
								WriteSize = ( WriteSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
								KeyConvFileWrite( ( u8 * )DestBuf + HuffmanEncodeKB * 1024, WriteSize - File.HuffPressDataSize, DestFp, NoKey ? NULL : lKey, File.DataSize + File.HuffPressDataSize ) ;
							}

							// メモリの解放
							free( HuffData ) ;
						}
						else
						{
							// 圧縮データを反転して書き出す
							WriteSize = ( DestSize + 3 ) / 4 * 4 ;
							KeyConvFileWrite( DestBuf, WriteSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;
						}
						
						// メモリの解放
						free( SrcBuf ) ;
					}
					else
					{
NOPRESS:					
						// ハフマン圧縮も行うかどうかで処理を分岐
						if( Press && Huffman )
						{
							u8 *SrcBuf, *HuffData ;

							// データが丸ごと入るメモリ領域の確保
							SrcBuf = ( u8 * )calloc( 1, ( size_t )( FileSize + 32 ) ) ;

							// ファイルを丸ごと読み込む
							fread64( SrcBuf, FileSize, SrcP ) ;
						
							// ハフマン圧縮するサイズによって処理を分岐
							if( HuffmanEncodeKB == 0xff || FileSize <= HuffmanEncodeKB * 1024 * 2 )
							{
								// ハフマン圧縮用のメモリ領域を確保
								HuffData = ( u8 * )calloc( 1, ( size_t )( FileSize * 2 + 256 * 2 + 32 ) ) ;

								// ファイル全体をハフマン圧縮
								File.HuffPressDataSize = Huffman_Encode( SrcBuf, FileSize, HuffData ) ;

								// 圧縮データに鍵を適用して書き出す
								WriteSize = ( File.HuffPressDataSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
								KeyConvFileWrite( HuffData, WriteSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;
							}
							else
							{
								// ハフマン圧縮用のメモリ領域を確保
								HuffData = ( u8 * )calloc( 1, HuffmanEncodeKB * 1024 * 2 * 4 + 256 * 2 + 32 ) ;

								// ファイルの前後をハフマン圧縮
								memcpy( HuffData,                          SrcBuf,                                     HuffmanEncodeKB * 1024 ) ;
								memcpy( HuffData + HuffmanEncodeKB * 1024, SrcBuf + FileSize - HuffmanEncodeKB * 1024, HuffmanEncodeKB * 1024 ) ;
								File.HuffPressDataSize = Huffman_Encode( HuffData, HuffmanEncodeKB * 1024 * 2, HuffData + HuffmanEncodeKB * 1024 * 2 ) ;

								// ハフマン圧縮した部分を書き出す
								KeyConvFileWrite( HuffData + HuffmanEncodeKB * 1024 * 2, File.HuffPressDataSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;
								
								// ハフマン圧縮していない箇所を書き出す
								WriteSize = File.HuffPressDataSize + FileSize - HuffmanEncodeKB * 1024 * 2 ;
								WriteSize = ( WriteSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
								KeyConvFileWrite( SrcBuf + HuffmanEncodeKB * 1024, WriteSize - File.HuffPressDataSize, DestFp, NoKey ? NULL : lKey, File.DataSize + File.HuffPressDataSize ) ;
							}

							// メモリの解放
							free( SrcBuf ) ;
							free( HuffData ) ;
						}
						else
						{
							// 転送開始
							WriteSize = 0 ;
							while( WriteSize < FileSize )
							{
								// 転送サイズ決定
								MoveSize = DXA_BUFFERSIZE < FileSize - WriteSize ? DXA_BUFFERSIZE : FileSize - WriteSize ;
								MoveSize = ( MoveSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
							
								// ファイルの鍵適用読み込み
								memset( TempBuffer, 0, ( size_t )MoveSize ) ;
								KeyConvFileRead( TempBuffer, MoveSize, SrcP, NoKey ? NULL : lKey, File.DataSize + WriteSize ) ;

								// 書き出し
								fwrite64( TempBuffer, MoveSize, DestFp ) ;
							
								// 書き出しサイズの加算
								WriteSize += MoveSize ;
							}
						}
					}
					
					// 書き出したファイルを閉じる
					fclose( SrcP ) ;
				
					// データサイズの加算
					Size->DataSize += WriteSize ;
				}
				
				// ファイルヘッダを書き出す
				memcpy( FileP + Dir.FileHeadAddress + sizeof( DARC_FILEHEAD ) * i, &File, sizeof( DARC_FILEHEAD ) ) ;
			}
			
			i ++ ;
		}
		while( FindNextFile( FindHandle, &FindData ) != 0 ) ;
		
		// Find ハンドルを閉じる
		FindClose( FindHandle ) ;
	}
						
	// もとのディレクトリをカレントディレクトリにセット
	SetCurrentDirectory( DirPath ) ;

	// 終了
	return 0 ;
}

// 指定のディレクトリデータにあるファイルを展開する
int DXArchive::DirectoryDecode( u8 *NameP, u8 *DirP, u8 *FileP, DARC_HEAD *Head, DARC_DIRECTORY *Dir, FILE *ArcP, unsigned char *Key, const char *KeyString, size_t KeyStringBytes, bool NoKey, char *KeyStringBuffer )
{
	TCHAR DirPath[MAX_PATH] ;
	
	// 現在のカレントディレクトリを保存
	GetCurrentDirectory( MAX_PATH, DirPath ) ;

	// ディレクトリ情報がある場合は、まず展開用のディレクトリを作成する
	if( Dir->DirectoryAddress != 0xffffffffffffffff && Dir->ParentDirectoryAddress != 0xffffffffffffffff )
	{
		DARC_FILEHEAD *DirFile ;
		
		// DARC_FILEHEAD のアドレスを取得
		DirFile = ( DARC_FILEHEAD * )( FileP + Dir->DirectoryAddress ) ;
		
		// ディレクトリの作成
		TCHAR *pName = GetOriginalFileName(NameP + DirFile->NameAddress);
		CreateDirectory( pName, NULL ) ;
		delete[] pName;

		// そのディレクトリにカレントディレクトリを移す
		pName = GetOriginalFileName(NameP + DirFile->NameAddress);
		SetCurrentDirectory(pName) ;
		delete[] pName;
	}

	// 展開処理開始
	{
		u32 i, FileHeadSize ;
		DARC_FILEHEAD *File ;
		size_t KeyStringBufferBytes ;
		unsigned char lKey[ DXA_KEY_BYTES ] ;

		// 格納されているファイルの数だけ繰り返す
		FileHeadSize = sizeof( DARC_FILEHEAD ) ;
		File = ( DARC_FILEHEAD * )( FileP + Dir->FileHeadAddress ) ;
		for( i = 0 ; i < Dir->FileHeadNum ; i ++, File = (DARC_FILEHEAD *)( (u8 *)File + FileHeadSize ) )
		{
			// ディレクトリかどうかで処理を分岐
			if( File->Attributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				// ディレクトリの場合は再帰をかける
				DirectoryDecode( NameP, DirP, FileP, Head, ( DARC_DIRECTORY * )( DirP + File->DataAddress ), ArcP, Key, KeyString, KeyStringBytes, NoKey, KeyStringBuffer ) ;
			}
			else
			{
				FILE *DestP ;
				void *Buffer ;
			
				// ファイルの場合は展開する
				
				// バッファを確保する
				Buffer = malloc( DXA_BUFFERSIZE ) ;
				if( Buffer == NULL ) return -1 ;

				// ファイルを開く
				TCHAR *pName = GetOriginalFileName(NameP + File->NameAddress);

				DestP = _tfopen(pName, TEXT("wb"));
				// ファイル個別の鍵を作成
				if( NoKey == false )
				{
					KeyStringBufferBytes = CreateKeyFileString( ( int )Head->CharCodeFormat, KeyString, KeyStringBytes, Dir, File, FileP, DirP, NameP, ( BYTE * )KeyStringBuffer ) ;
					KeyCreate( KeyStringBuffer, KeyStringBufferBytes, lKey ) ;
				}

				delete[] pName;
			
				// データがある場合のみ転送
				if( File->DataSize != 0 )
				{
					void *temp ;

					// 初期位置をセットする
					if( _ftelli64( ArcP ) != ( s32 )( Head->DataStartAddress + File->DataAddress ) )
						_fseeki64( ArcP, Head->DataStartAddress + File->DataAddress, SEEK_SET ) ;
						
					// データが圧縮されているかどうかで処理を分岐
					if( File->PressDataSize != 0xffffffffffffffff )
					{
						// 圧縮されている場合

						// ハフマン圧縮もされているかどうかで処理を分岐
						if( File->HuffPressDataSize != 0xffffffffffffffff )
						{
							// 圧縮データが収まるメモリ領域の確保
							temp = malloc( ( size_t )( File->PressDataSize + File->HuffPressDataSize + File->DataSize ) ) ;

							// 圧縮データの読み込み
							KeyConvFileRead( temp, File->HuffPressDataSize, ArcP, NoKey ? NULL : lKey, File->DataSize ) ;

							// ハフマン圧縮を解凍
							Huffman_Decode( temp, (u8 *)temp + File->HuffPressDataSize ) ;

							// ファイルの前後をハフマン圧縮している場合は処理を分岐
							if( Head->HuffmanEncodeKB != 0xff && File->PressDataSize > Head->HuffmanEncodeKB * 1024 * 2 )
							{
								// 解凍したデータの内、後ろ半分を移動する
								memmove( 
									( u8 * )temp + File->HuffPressDataSize + File->PressDataSize - Head->HuffmanEncodeKB * 1024,
									( u8 * )temp + File->HuffPressDataSize + Head->HuffmanEncodeKB * 1024,
									Head->HuffmanEncodeKB * 1024 ) ;

								// 残りのLZ圧縮データを読み込む
								KeyConvFileRead(
									( u8 * )temp + File->HuffPressDataSize + Head->HuffmanEncodeKB * 1024,
									File->PressDataSize - Head->HuffmanEncodeKB * 1024 * 2,
									ArcP, NoKey ? NULL : lKey, File->DataSize + File->HuffPressDataSize ) ;
							}
						
							// 解凍
							Decode( (u8 *)temp + File->HuffPressDataSize, (u8 *)temp + File->HuffPressDataSize + File->PressDataSize ) ;
						
							// 書き出し
							fwrite64( (u8 *)temp + File->HuffPressDataSize + File->PressDataSize, File->DataSize, DestP ) ;
						
							// メモリの解放
							free( temp ) ;
						}
						else
						{
							// 圧縮データが収まるメモリ領域の確保
							temp = malloc( ( size_t )( File->PressDataSize + File->DataSize ) ) ;

							// 圧縮データの読み込み
							KeyConvFileRead( temp, File->PressDataSize, ArcP, NoKey ? NULL : lKey, File->DataSize ) ;
						
							// 解凍
							Decode( temp, (u8 *)temp + File->PressDataSize ) ;
						
							// 書き出し
							fwrite64( (u8 *)temp + File->PressDataSize, File->DataSize, DestP ) ;
						
							// メモリの解放
							free( temp ) ;
						}
					}
					else
					{
						// 圧縮されていない場合
					
						// ハフマン圧縮はされているかどうかで処理を分岐
						if( File->HuffPressDataSize != 0xffffffffffffffff )
						{
							// 圧縮データが収まるメモリ領域の確保
							temp = malloc( ( size_t )( File->HuffPressDataSize + File->DataSize ) ) ;

							// 圧縮データの読み込み
							KeyConvFileRead( temp, File->HuffPressDataSize, ArcP, NoKey ? NULL : lKey, File->DataSize ) ;

							// ハフマン圧縮を解凍
							Huffman_Decode( temp, (u8 *)temp + File->HuffPressDataSize ) ;

							// ファイルの前後のみハフマン圧縮している場合は処理を分岐
							if( Head->HuffmanEncodeKB != 0xff && File->DataSize > Head->HuffmanEncodeKB * 1024 * 2 )
							{
								// 解凍したデータの内、後ろ半分を移動する
								memmove( 
									( u8 * )temp + File->HuffPressDataSize + File->DataSize - Head->HuffmanEncodeKB * 1024,
									( u8 * )temp + File->HuffPressDataSize + Head->HuffmanEncodeKB * 1024,
									Head->HuffmanEncodeKB * 1024 ) ;

								// 残りのデータを読み込む
								KeyConvFileRead(
									( u8 * )temp + File->HuffPressDataSize + Head->HuffmanEncodeKB * 1024,
									File->DataSize - Head->HuffmanEncodeKB * 1024 * 2,
									ArcP, NoKey ? NULL : lKey, File->DataSize + File->HuffPressDataSize ) ;
							}
						
							// 書き出し
							fwrite64( (u8 *)temp + File->HuffPressDataSize, File->DataSize, DestP ) ;
						
							// メモリの解放
							free( temp ) ;
						}
						else
						{
							u64 MoveSize, WriteSize ;
							
							// 転送処理開始
							WriteSize = 0 ;
							while( WriteSize < File->DataSize )
							{
								MoveSize = File->DataSize - WriteSize > DXA_BUFFERSIZE ? DXA_BUFFERSIZE : File->DataSize - WriteSize ;

								// ファイルの反転読み込み
								KeyConvFileRead( Buffer, MoveSize, ArcP, NoKey ? NULL : lKey, File->DataSize + WriteSize ) ;

								// 書き出し
								fwrite64( Buffer, MoveSize, DestP ) ;
								
								WriteSize += MoveSize ;
							}
						}
					}
				}
				
				// ファイルを閉じる
				fclose( DestP ) ;

				// バッファを開放する
				free( Buffer ) ;

				// ファイルのタイムスタンプを設定する
				{
					HANDLE HFile ;
					FILETIME CreateTime, LastAccessTime, LastWriteTime ;
					TCHAR *pName = GetOriginalFileName(NameP + File->NameAddress);
					HFile = CreateFile(pName,
										GENERIC_WRITE, 0, NULL,
										OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
					delete[] pName;

					if( HFile == INVALID_HANDLE_VALUE )
					{
						HFile = HFile ;
					}

					CreateTime.dwHighDateTime     = ( u32 )( File->Time.Create     >> 32        ) ;
					CreateTime.dwLowDateTime      = ( u32 )( File->Time.Create     & 0xffffffffffffffff ) ;
					LastAccessTime.dwHighDateTime = ( u32 )( File->Time.LastAccess >> 32        ) ;
					LastAccessTime.dwLowDateTime  = ( u32 )( File->Time.LastAccess & 0xffffffffffffffff ) ;
					LastWriteTime.dwHighDateTime  = ( u32 )( File->Time.LastWrite  >> 32        ) ;
					LastWriteTime.dwLowDateTime   = ( u32 )( File->Time.LastWrite  & 0xffffffffffffffff ) ;
					SetFileTime( HFile, &CreateTime, &LastAccessTime, &LastWriteTime ) ;
					CloseHandle( HFile ) ;
				}

				// ファイル属性を付ける
				pName = GetOriginalFileName(NameP + File->NameAddress);
				SetFileAttributes( pName, ( u32 )File->Attributes ) ;
				delete[] pName;
			}
		}
	}
	
	// カレントディレクトリを元に戻す
	SetCurrentDirectory( DirPath ) ;

	// 終了
	return 0 ;
}

// ディレクトリ内のファイルパスを取得する
int DXArchive::GetDirectoryFilePath( const TCHAR *DirectoryPath, TCHAR *FileNameBuffer )
{
	WIN32_FIND_DATA FindData ;
	HANDLE FindHandle ;
	int FileNum ;
	TCHAR DirPath[256], String[256] ;

	// ディレクトリかどうかをチェックする
	if( DirectoryPath[0] != '\0' )
	{
		FindHandle = FindFirstFile( DirectoryPath, &FindData ) ;
		if( FindHandle == INVALID_HANDLE_VALUE || ( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 ) return -1 ;
		FindClose( FindHandle ) ;
	}

	// 指定のフォルダのファイルの名前を取得する
	FileNum = 0 ;
	if( DirectoryPath[0] != '\0' )
	{
		_tcscpy( DirPath, DirectoryPath ) ;
		if( DirPath[ _tcslen( DirPath ) - 1 ] != '\\' ) _tcscat( DirPath, TEXT("\\") ) ;
		_tcscpy( String, DirPath ) ;
		_tcscat( String, TEXT("*") ) ;
	}
	else
	{
		_tcscpy( DirPath, TEXT("") ) ;
		_tcscpy( String, TEXT("*") ) ;
	}
	FindHandle = FindFirstFile( String, &FindData ) ;
	if( FindHandle != INVALID_HANDLE_VALUE )
	{
		do
		{
			// 上のディレクトリに戻ったりするためのパスは無視する
			if( _tcscmp( FindData.cFileName, TEXT(".") ) == 0 || _tcscmp( FindData.cFileName, TEXT("..") ) == 0 ) continue ;

			// ファイルパスを保存する
			if( FileNameBuffer != NULL )
			{
				_tcscpy( FileNameBuffer, DirPath ) ;
				_tcscat( FileNameBuffer, FindData.cFileName ) ;
				FileNameBuffer += 256 ;
			}

			// ファイルの数を増やす
			FileNum ++ ;
		}
		while( FindNextFile( FindHandle, &FindData ) != 0 ) ;
		FindClose( FindHandle ) ;
	}

	// 数を返す
	return FileNum ;
}

// エンコードの進行状況を表示を消去する
void DXArchive::EncodeStatusErase( void )
{
	static char StringBuffer[ 8192 ] ;

	if( LogStringLength > 0 )
	{
		size_t i ;
		int p = 0 ;
		for( i = 0 ; i < LogStringLength ; i ++ )
		{
			StringBuffer[ p ] = '\b' ;
			p ++ ;
		}
		for( i = 0 ; i < LogStringLength ; i ++ )
		{
			StringBuffer[ p ] = ' ' ;
			p ++ ;
		}
		for( i = 0 ; i < LogStringLength ; i ++ )
		{
			StringBuffer[ p ] = '\b' ;
			p ++ ;
		}
		StringBuffer[ p ] = '\0' ;
		printf( StringBuffer ) ;
	}
}

// エンコードの進行状況を表示する
void DXArchive::EncodeStatusOutput( DARC_ENCODEINFO *EncodeInfo, bool Always )
{
	static TCHAR StringBuffer[ 8192 ] ;
	static TCHAR FileNameBuffer[ 2048 ] ;
	size_t FileNameLength ;
	unsigned int NowTime = GetTickCount() ;

	// 前回から16ms以上時間が経過していたら表示する
	if( Always == false && NowTime - EncodeInfo->PrevDispTime < 16 )
	{
		return ;
	}

	// 前回の表示内容を消去
	EncodeStatusErase() ;

	EncodeInfo->PrevDispTime = NowTime ;
	wcscpy( FileNameBuffer, EncodeInfo->ProcessFileName ) ;
	FileNameLength = _tcslen( EncodeInfo->ProcessFileName ) ;
	if( FileNameLength > 50 )
	{
		wcsncpy( FileNameBuffer, EncodeInfo->ProcessFileName, 50 ) ;
		wcscpy( &FileNameBuffer[ 50 ], TEXT("...") ) ;
	}
	wsprintf( StringBuffer, TEXT(" [%d/%d] %d%%%%  %s"), EncodeInfo->CompFileNum, EncodeInfo->TotalFileNum, EncodeInfo->CompFileNum * 100 / EncodeInfo->TotalFileNum, FileNameBuffer);
	LogStringLength = _tcslen( StringBuffer ) ;
	wprintf( StringBuffer ) ;
}

// ハフマン圧縮をする前後のサイズを取得する
void DXArchive::AnalyseHuffmanEncode( u64 DataSize, u8 HuffmanEncodeKB, u64 *HeadDataSize, u64 *FootDataSize )
{
	if( HuffmanEncodeKB == 0xff || DataSize < HuffmanEncodeKB * 1024 * 2 )
	{
		*HeadDataSize = DataSize ;
		*FootDataSize = 0 ;
	}
	else
	{
		*HeadDataSize = HuffmanEncodeKB * 1024 ;
		*FootDataSize = HuffmanEncodeKB * 1024 ;
	}
}

// エンコード( 戻り値:圧縮後のサイズ  -1 はエラー  Dest に NULL を入れることも可能 )
int DXArchive::Encode( void *Src, u32 SrcSize, void *Dest, bool OutStatus, bool MaxPress )
{
	s32 dstsize ;
	s32    bonus,    conbo,    conbosize,    address,    addresssize ;
	s32 maxbonus, maxconbo, maxconbosize, maxaddress, maxaddresssize ;
	u8 keycode, *srcp, *destp, *dp, *sp, *sp2, *sp1 ;
	u32 srcaddress, nextprintaddress, code ;
	s32 j ;
	u32 i, m ;
	u32 maxlistnum, maxlistnummask, listaddp ;
	u32 sublistnum, sublistmaxnum ;
	LZ_LIST *listbuf, *listtemp, *list, *newlist ;
	u8 *listfirsttable, *usesublistflagtable, *sublistbuf ;
	u32 searchlistnum ;

	// 最大一致長を捜すためのリストを辿る最大数のセット
	searchlistnum = MaxPress ? 0xffffffff : MAX_SEARCHLISTNUM ;

	// サブリストのサイズを決める
	{
			 if( SrcSize < 100 * 1024 )			sublistmaxnum = 1 ;
		else if( SrcSize < 3 * 1024 * 1024 )	sublistmaxnum = MAX_SUBLISTNUM / 3 ;
		else									sublistmaxnum = MAX_SUBLISTNUM ;
	}

	// リストのサイズを決める
	{
		maxlistnum = MAX_ADDRESSLISTNUM ;
		if( maxlistnum > SrcSize )
		{
			while( ( maxlistnum >> 1 ) > 0x100 && ( maxlistnum >> 1 ) > SrcSize )
				maxlistnum >>= 1 ;
		}
		maxlistnummask = maxlistnum - 1 ;
	}

	// メモリの確保
	usesublistflagtable   = (u8 *)malloc(
		sizeof( void * )  * 65536 +					// メインリストの先頭オブジェクト用領域
		sizeof( LZ_LIST ) * maxlistnum +			// メインリスト用領域
		sizeof( u8 )      * 65536 +					// サブリストを使用しているかフラグ用領域
		sizeof( void * )  * 256 * sublistmaxnum ) ;	// サブリスト用領域
		
	// アドレスのセット
	listfirsttable =     usesublistflagtable + sizeof(     u8 ) * 65536 ;
	sublistbuf     =          listfirsttable + sizeof( void * ) * 65536 ;
	listbuf        = (LZ_LIST *)( sublistbuf + sizeof( void * ) * 256 * sublistmaxnum ) ;
	
	// 初期化
	memset( usesublistflagtable, 0, sizeof(     u8 ) * 65536               ) ;
	memset(          sublistbuf, 0, sizeof( void * ) * 256 * sublistmaxnum ) ;
	memset(      listfirsttable, 0, sizeof( void * ) * 65536               ) ;
	list = listbuf ;
	for( i = maxlistnum / 8 ; i ; i --, list += 8 )
	{
		list[0].address =
		list[1].address =
		list[2].address =
		list[3].address =
		list[4].address =
		list[5].address =
		list[6].address =
		list[7].address = 0xffffffff ;
	}

	srcp  = (u8 *)Src ;
	destp = (u8 *)Dest ;

	// 圧縮元データの中で一番出現頻度が低いバイトコードを検索する
	{
		u32 qnum, table[256], mincode ;

		for( i = 0 ; i < 256 ; i ++ )
			table[i] = 0 ;
		
		sp   = srcp ;
		qnum = SrcSize / 8 ;
		i    = qnum * 8 ;
		for( ; qnum ; qnum --, sp += 8 )
		{
			table[sp[0]] ++ ;
			table[sp[1]] ++ ;
			table[sp[2]] ++ ;
			table[sp[3]] ++ ;
			table[sp[4]] ++ ;
			table[sp[5]] ++ ;
			table[sp[6]] ++ ;
			table[sp[7]] ++ ;
		}
		for( ; i < SrcSize ; i ++, sp ++ )
			table[*sp] ++ ;
			
		keycode = 0 ;
		mincode = table[0] ;
		for( i = 1 ; i < 256 ; i ++ )
		{
			if( mincode < table[i] ) continue ;
			mincode = table[i] ;
			keycode = (u8)i ;
		}
	}

	// 圧縮元のサイズをセット
	((u32 *)destp)[0] = SrcSize ;

	// キーコードをセット
	destp[8] = keycode ;

	// 圧縮処理
	dp               = destp + 9 ;
	sp               = srcp ;
	srcaddress       = 0 ;
	dstsize          = 0 ;
	listaddp         = 0 ;
	sublistnum       = 0 ;
	nextprintaddress = 1024 * 100 ;
	if( OutStatus )
	{
		printf( " 圧縮     " ) ;
		LogStringLength += 10 ;
	}
	while( srcaddress < SrcSize )
	{
		// 残りサイズが最低圧縮サイズ以下の場合は圧縮処理をしない
		if( srcaddress + MIN_COMPRESS >= SrcSize ) goto NOENCODE ;

		// リストを取得
		code = *((u16 *)sp) ;
		list = (LZ_LIST *)( listfirsttable + code * sizeof( void * ) ) ;
		if( usesublistflagtable[code] == 1 )
		{
			list = (LZ_LIST *)( (void **)list->next + sp[2] ) ;
		}
		else
		{
			if( sublistnum < sublistmaxnum )
			{
				list->next = (LZ_LIST *)( sublistbuf + sizeof( void * ) * 256 * sublistnum ) ;
				list       = (LZ_LIST *)( (void **)list->next + sp[2] ) ;
			
				usesublistflagtable[code] = 1 ;
				sublistnum ++ ;
			}
		}

		// 一番一致長の長いコードを探す
		maxconbo   = -1 ;
		maxaddress = -1 ;
		maxbonus   = -1 ;
		for( m = 0, listtemp = list->next ; m < searchlistnum && listtemp != NULL ; listtemp = listtemp->next, m ++ )
		{
			address = srcaddress - listtemp->address ;
			if( address >= MAX_POSITION )
			{
				if( listtemp->prev ) listtemp->prev->next = listtemp->next ;
				if( listtemp->next ) listtemp->next->prev = listtemp->prev ;
				listtemp->address = 0xffffffff ;
				continue ;
			}
			
			sp2 = &sp[-address] ;
			sp1 = sp ;
			if( srcaddress + MAX_COPYSIZE < SrcSize )
			{
				conbo = MAX_COPYSIZE / 4 ;
				while( conbo && *((u32 *)sp2) == *((u32 *)sp1) )
				{
					sp2 += 4 ;
					sp1 += 4 ;
					conbo -- ;
				}
				conbo = MAX_COPYSIZE - ( MAX_COPYSIZE / 4 - conbo ) * 4 ;

				while( conbo && *sp2 == *sp1 )
				{
					sp2 ++ ;
					sp1 ++ ;
					conbo -- ;
				}
				conbo = MAX_COPYSIZE - conbo ;
			}
			else
			{
				for( conbo = 0 ;
						conbo < MAX_COPYSIZE &&
						conbo + srcaddress < SrcSize &&
						sp[conbo - address] == sp[conbo] ;
							conbo ++ ){}
			}

			if( conbo >= 4 )
			{
				conbosize   = ( conbo - MIN_COMPRESS ) < 0x20 ? 0 : 1 ;
				addresssize = address < 0x100 ? 0 : ( address < 0x10000 ? 1 : 2 ) ;
				bonus       = conbo - ( 3 + conbosize + addresssize ) ;

				if( bonus > maxbonus )
				{
					maxconbo       = conbo ;
					maxaddress     = address ;
					maxaddresssize = addresssize ;
					maxconbosize   = conbosize ;
					maxbonus       = bonus ;
				}
			}
		}

		// リストに登録
		newlist = &listbuf[listaddp] ;
		if( newlist->address != 0xffffffff )
		{
			if( newlist->prev ) newlist->prev->next = newlist->next ;
			if( newlist->next ) newlist->next->prev = newlist->prev ;
			newlist->address = 0xffffffff ;
		}
		newlist->address = srcaddress ;
		newlist->prev    = list ;
		newlist->next    = list->next ;
		if( list->next != NULL ) list->next->prev = newlist ;
		list->next       = newlist ;
		listaddp         = ( listaddp + 1 ) & maxlistnummask ;

		// 一致コードが見つからなかったら非圧縮コードとして出力
		if( maxconbo == -1 )
		{
NOENCODE:
			// キーコードだった場合は２回連続で出力する
			if( *sp == keycode )
			{
				if( destp != NULL )
				{
					dp[0]  =
					dp[1]  = keycode ;
					dp += 2 ;
				}
				dstsize += 2 ;
			}
			else
			{
				if( destp != NULL )
				{
					*dp = *sp ;
					dp ++ ;
				}
				dstsize ++ ;
			}
			sp ++ ;
			srcaddress ++ ;
		}
		else
		{
			// 見つかった場合は見つけた位置と長さを出力する
			
			// キーコードと見つけた位置と長さを出力
			if( destp != NULL )
			{
				// キーコードの出力
				*dp++ = keycode ;

				// 出力する連続長は最低 MIN_COMPRESS あることが前提なので - MIN_COMPRESS したものを出力する
				maxconbo -= MIN_COMPRESS ;

				// 連続長０～４ビットと連続長、相対アドレスのビット長を出力
				*dp = (u8)( ( ( maxconbo & 0x1f ) << 3 ) | ( maxconbosize << 2 ) | maxaddresssize ) ;

				// キーコードの連続はキーコードと値の等しい非圧縮コードと
				// 判断するため、キーコードの値以上の場合は値を＋１する
				if( *dp >= keycode ) dp[0] += 1 ;
				dp ++ ;

				// 連続長５～１２ビットを出力
				if( maxconbosize == 1 )
					*dp++ = (u8)( ( maxconbo >> 5 ) & 0xff ) ;

				// maxconbo はまだ使うため - MIN_COMPRESS した分を戻す
				maxconbo += MIN_COMPRESS ;

				// 出力する相対アドレスは０が( 現在のアドレス－１ )を挿すので、－１したものを出力する
				maxaddress -- ;

				// 相対アドレスを出力
				*dp++ = (u8)( maxaddress ) ;
				if( maxaddresssize > 0 )
				{
					*dp++ = (u8)( maxaddress >> 8 ) ;
					if( maxaddresssize == 2 )
						*dp++ = (u8)( maxaddress >> 16 ) ;
				}
			}
			
			// 出力サイズを加算
			dstsize += 3 + maxaddresssize + maxconbosize ;
			
			// リストに情報を追加
			if( srcaddress + maxconbo < SrcSize )
			{
				sp2 = &sp[1] ;
				for( j = 1 ; j < maxconbo && (u64)&sp2[2] - (u64)srcp < SrcSize ; j ++, sp2 ++ )
				{
					code = *((u16 *)sp2) ;
					list = (LZ_LIST *)( listfirsttable + code * sizeof( void * ) ) ;
					if( usesublistflagtable[code] == 1 )
					{
						list = (LZ_LIST *)( (void **)list->next + sp2[2] ) ;
					}
					else
					{
						if( sublistnum < sublistmaxnum )
						{
							list->next = (LZ_LIST *)( sublistbuf + sizeof( void * ) * 256 * sublistnum ) ;
							list       = (LZ_LIST *)( (void **)list->next + sp2[2] ) ;
						
							usesublistflagtable[code] = 1 ;
							sublistnum ++ ;
						}
					}

					newlist = &listbuf[listaddp] ;
					if( newlist->address != 0xffffffff )
					{
						if( newlist->prev ) newlist->prev->next = newlist->next ;
						if( newlist->next ) newlist->next->prev = newlist->prev ;
						newlist->address = 0xffffffff ;
					}
					newlist->address = srcaddress + j ;
					newlist->prev = list ;
					newlist->next = list->next ;
					if( list->next != NULL ) list->next->prev = newlist ;
					list->next = newlist ;
					listaddp = ( listaddp + 1 ) & maxlistnummask ;
				}
			}
			
			sp         += maxconbo ;
			srcaddress += maxconbo ;
		}

		// 圧縮率の表示
		if( nextprintaddress < srcaddress )
		{
			nextprintaddress = srcaddress + 100 * 1024 ;
			if( OutStatus ) printf( "\b\b\b\b%3d%%", (s32)( (f32)srcaddress * 100 / SrcSize ) ) ;
		}
	}

	if( OutStatus )
	{
		printf( "\b\b\b\b100%%" ) ;
	}

	// 圧縮後のデータサイズを保存する
	*((u32 *)&destp[4]) = dstsize + 9 ;

	// 確保したメモリの解放
	free( usesublistflagtable ) ;

	// データのサイズを返す
	return dstsize + 9 ;
}

// デコード( 戻り値:解凍後のサイズ  -1 はエラー  Dest に NULL を入れることも可能 )
int DXArchive::Decode( void *Src, void *Dest )
{
	u32 srcsize, destsize, code, indexsize, keycode, conbo, index ;
	u8 *srcp, *destp, *dp, *sp ;

	destp = (u8 *)Dest ;
	srcp  = (u8 *)Src ;
	
	// 解凍後のデータサイズを得る
	destsize = *((u32 *)&srcp[0]) ;

	// 圧縮データのサイズを得る
	srcsize = *((u32 *)&srcp[4]) - 9 ;

	// キーコード
	keycode = srcp[8] ;
	
	// 出力先がない場合はサイズだけ返す
	if( Dest == NULL )
		return destsize ;
	
	// 展開開始
	sp  = srcp + 9 ;
	dp  = destp ;
	while( srcsize )
	{
		// キーコードか同かで処理を分岐
		if( sp[0] != keycode )
		{
			// 非圧縮コードの場合はそのまま出力
			*dp = *sp ;
			dp      ++ ;
			sp      ++ ;
			srcsize -- ;
			continue ;
		}
	
		// キーコードが連続していた場合はキーコード自体を出力
		if( sp[1] == keycode )
		{
			*dp = (u8)keycode ;
			dp      ++ ;
			sp      += 2 ;
			srcsize -= 2 ;
			
			continue ;
		}

		// 第一バイトを得る
		code = sp[1] ;

		// もしキーコードよりも大きな値だった場合はキーコード
		// とのバッティング防止の為に＋１しているので－１する
		if( code > keycode ) code -- ;

		sp      += 2 ;
		srcsize -= 2 ;

		// 連続長を取得する
		conbo = code >> 3 ;
		if( code & ( 0x1 << 2 ) )
		{
			conbo |= *sp << 5 ;
			sp      ++ ;
			srcsize -- ;
		}
		conbo += MIN_COMPRESS ;	// 保存時に減算した最小圧縮バイト数を足す

		// 参照相対アドレスを取得する
		indexsize = code & 0x3 ;
		switch( indexsize )
		{
		case 0 :
			index = *sp ;
			sp      ++ ;
			srcsize -- ;
			break ;
			
		case 1 :
			index = *((u16 *)sp) ;
			sp      += 2 ;
			srcsize -= 2 ;
			break ;
			
		case 2 :
			index = *((u16 *)sp) | ( sp[2] << 16 ) ;
			sp      += 3 ;
			srcsize -= 3 ;
			break ;
		}
		index ++ ;		// 保存時に－１しているので＋１する

		// 展開
		if( index < conbo )
		{
			u32 num ;

			num  = index ;
			while( conbo > num )
			{
				memcpy( dp, dp - num, num ) ;
				dp    += num ;
				conbo -= num ;
				num   += num ;
			}
			if( conbo != 0 )
			{
				memcpy( dp, dp - num, conbo ) ;
				dp += conbo ;
			}
		}
		else
		{
			memcpy( dp, dp - index, conbo ) ;
			dp += conbo ;
		}
	}

	// 解凍後のサイズを返す
	return (int)destsize ;
}

// バイナリデータを元に CRC32 のハッシュ値を計算する
u32 DXArchive::HashCRC32( const void *SrcData, size_t SrcDataSize )
{
	static DWORD CRC32Table[ 256 ] ;
	static int CRC32TableInit = 0 ;
	DWORD CRC = 0xffffffff ;
	BYTE *SrcByte = ( BYTE * )SrcData ;
	DWORD i ;

	// テーブルが初期化されていなかったら初期化する
	if( CRC32TableInit == 0 )
	{
		DWORD Magic = 0xedb88320 ;	// 0x4c11db7 をビットレベルで順番を逆にしたものが 0xedb88320
		DWORD j ;

		for( i = 0; i < 256; i++ )
		{
			DWORD Data = i ;
			for( j = 0; j < 8; j++ )
			{
				int b = ( Data & 1 ) ;
				Data >>= 1 ;
				if( b != 0 )
				{
					Data ^= Magic ;
				}
			}
			CRC32Table[ i ] = Data ;
		}

		// テーブルを初期化したフラグを立てる
		CRC32TableInit = 1 ;
	}

	for( i = 0 ; i < SrcDataSize ; i ++ )
	{
		CRC = CRC32Table[ ( BYTE )( CRC ^ SrcByte[ i ] ) ] ^ ( CRC >> 8 ) ;
	}

	return CRC ^ 0xffffffff ;
}


// アーカイブファイルを作成する(ディレクトリ一個だけ)
int DXArchive::EncodeArchiveOneDirectory( TCHAR *OutputFileName, TCHAR *DirectoryPath, bool Press, bool AlwaysHuffman, u8 HuffmanEncodeKB, const char *KeyString_, bool NoKey, bool OutputStatus, bool MaxPress )
{
	int i, FileNum, Result ;
	TCHAR **FilePathList, *NameBuffer ;

	// ファイルの数を取得する
	FileNum = GetDirectoryFilePath( DirectoryPath, NULL ) ;
	if( FileNum < 0 ) return -1 ;

	// ファイルの数の分だけファイル名とファイルポインタの格納用のメモリを確保する
	NameBuffer = (TCHAR *)malloc( FileNum * ( 256 + sizeof(TCHAR * ) ) ) ;

	// ファイルのパスを取得する
	GetDirectoryFilePath( DirectoryPath, NameBuffer ) ;

	// ファイルパスのリストを作成する
	FilePathList = (TCHAR **)( NameBuffer + FileNum * 256 ) ;
	for( i = 0 ; i < FileNum ; i ++ )
		FilePathList[i] = NameBuffer + i * 256 ;
	
	// エンコード
	Result = EncodeArchive( OutputFileName, FilePathList, FileNum, Press, AlwaysHuffman, HuffmanEncodeKB, KeyString_, NoKey, OutputStatus, MaxPress ) ;

	// 確保したメモリの解放
	free( NameBuffer ) ;

	// 結果を返す
	return Result ;
}

// アーカイブファイルを作成する
int DXArchive::EncodeArchive(TCHAR *OutputFileName, TCHAR **FileOrDirectoryPath, int FileNum, bool Press, bool AlwaysHuffman, u8 HuffmanEncodeKB, const char *KeyString_, bool NoKey, bool OutputStatus, bool MaxPress )
{
	DARC_HEAD Head ;
	DARC_DIRECTORY Directory, *DirectoryP ;
	u64 HeaderHuffDataSize ;
	SIZESAVE SizeSave ;
	FILE *DestFp ;
	u8 *NameP, *FileP, *DirP ;
	int i ;
	u32 Type ;
	void *TempBuffer ;
	u8 Key[ DXA_KEY_BYTES ] ;
	char KeyString[ DXA_KEY_STRING_LENGTH + 1 ] ;
	size_t KeyStringBytes ;
	char KeyStringBuffer[ DXA_KEY_STRING_MAXLENGTH ] ;
	DARC_ENCODEINFO EncodeInfo ;

	// 状況出力を行う場合はファイルの総数を数える
	EncodeInfo.CompFileNum = 0 ;
	EncodeInfo.TotalFileNum = 0 ;
	EncodeInfo.OutputStatus = OutputStatus ;
	if( EncodeInfo.OutputStatus )
	{
		for( i = 0 ; i < FileNum ; i++ )
		{
			// 指定されたファイルがあるかどうか検査
			Type = GetFileAttributes( FileOrDirectoryPath[i] ) ;
			if( ( signed int )Type == -1 ) continue ;

			// ファイルのタイプによって処理を分岐
			if( ( Type & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
			{
				FILE_INFOLIST FileList ;

				// フォルダ以下のファイルを取得する
				CreateFileList( FileOrDirectoryPath[i], &FileList, TRUE, TRUE, NULL, NULL, NULL ) ;

				// フォルダ以下のファイル数を加算する
				EncodeInfo.TotalFileNum += FileList.Num ;

				// ファイルリスト情報の後始末
				ReleaseFileList( &FileList ) ;
			}
			else
			{
				// ファイルだった場合はファイルの数を増やす
				EncodeInfo.TotalFileNum ++ ;
			}
		}
	}

	// 鍵文字列の保存と鍵の作成
	if( NoKey == false )
	{
		// 指定が無い場合はデフォルトの鍵文字列を使用する
		if( KeyString_ == NULL )
		{
			KeyString_ = DefaultKeyString ;
		}

		KeyStringBytes = CL_strlen( CHARCODEFORMAT_ASCII, KeyString_ ) ;
		if( KeyStringBytes > DXA_KEY_STRING_LENGTH )
		{
			KeyStringBytes = DXA_KEY_STRING_LENGTH ;
		}
		memcpy( KeyString, KeyString_, KeyStringBytes ) ;
		KeyString[ KeyStringBytes ] = '\0' ;

		// 鍵の作成
		KeyCreate( KeyString, KeyStringBytes, Key ) ;
	}

	// ファイル読み込みに使用するバッファの確保
	TempBuffer = malloc( DXA_BUFFERSIZE ) ;

	// 出力ファイルを開く
	DestFp = _tfopen( OutputFileName, TEXT("wb") ) ;

	// アーカイブのヘッダを出力する
	{
		memset( &Head, 0, sizeof( Head ) ) ;
		Head.Head						= DXA_HEAD ;
		Head.Version					= DXA_VER ;
		Head.HeadSize					= 0xffffffff ;
		Head.DataStartAddress			= sizeof( DARC_HEAD ) ;
		Head.FileNameTableStartAddress	= 0xffffffffffffffff ;
		Head.DirectoryTableStartAddress	= 0xffffffffffffffff ;
		Head.FileTableStartAddress		= 0xffffffffffffffff ;
		Head.CharCodeFormat				= GetACP() ;
		Head.Flags						= 0 ;
		Head.HuffmanEncodeKB			= HuffmanEncodeKB ;
		if( NoKey )						Head.Flags |= DXA_FLAG_NO_KEY ;
		if( Press == false )			Head.Flags |= DXA_FLAG_NO_HEAD_PRESS ;
		SetFileApisToANSI() ;

		KeyConvFileWrite( &Head, sizeof( DARC_HEAD ), DestFp, NoKey ? NULL : Key, 0 ) ;
	}
	
	// 各バッファを確保する
	NameP = ( u8 * )malloc( DXA_BUFFERSIZE ) ;
	if( NameP == NULL ) return -1 ;
	memset( NameP, 0, DXA_BUFFERSIZE ) ;

	FileP = ( u8 * )malloc( DXA_BUFFERSIZE ) ;
	if( FileP == NULL ) return -1 ;
	memset( FileP, 0, DXA_BUFFERSIZE ) ;

	DirP = ( u8 * )malloc( DXA_BUFFERSIZE ) ;
	if( DirP == NULL ) return -1 ;
	memset( DirP, 0, DXA_BUFFERSIZE ) ;

	// サイズ保存構造体にデータをセット
	SizeSave.DataSize		= 0 ;
	SizeSave.NameSize		= 0 ;
	SizeSave.DirectorySize	= 0 ;
	SizeSave.FileSize		= 0 ;
	
	// 最初のディレクトリのファイル情報を書き出す
	{
		DARC_FILEHEAD File ;
		
		memset( &File, 0, sizeof( DARC_FILEHEAD ) ) ;
		File.NameAddress	= SizeSave.NameSize ;
		File.Attributes		= FILE_ATTRIBUTE_DIRECTORY ;
		File.DataAddress	= SizeSave.DirectorySize ;
		File.DataSize		= 0 ;
		File.PressDataSize  = 0xffffffffffffffff ;

		// ディレクトリ名の書き出し
		SizeSave.NameSize += AddFileNameData(TEXT(""), NameP + SizeSave.NameSize ) ;

		// ファイル情報の書き出し
		memcpy( FileP + SizeSave.FileSize, &File, sizeof( DARC_FILEHEAD ) ) ;
		SizeSave.FileSize += sizeof( DARC_FILEHEAD ) ;
	}

	// 最初のディレクトリ情報を書き出す
	Directory.DirectoryAddress 			= 0 ;
	Directory.ParentDirectoryAddress 	= 0xffffffffffffffff ;
	Directory.FileHeadNum 				= FileNum ;
	Directory.FileHeadAddress 			= SizeSave.FileSize ;
	memcpy( DirP + SizeSave.DirectorySize, &Directory, sizeof( DARC_DIRECTORY ) ) ;
	DirectoryP = ( DARC_DIRECTORY * )( DirP + SizeSave.DirectorySize ) ;

	// サイズを加算する
	SizeSave.DirectorySize 	+= sizeof( DARC_DIRECTORY ) ;
	SizeSave.FileSize 		+= sizeof( DARC_FILEHEAD ) * FileNum ;

	// 渡されたファイルの数だけ処理を繰り返す
	for( i = 0 ; i < FileNum ; i ++ )
	{
		// 指定されたファイルがあるかどうか検査
		Type = GetFileAttributes( FileOrDirectoryPath[i] ) ;
		if( ( signed int )Type == -1 ) continue ;

		// ファイルのタイプによって処理を分岐
		if( ( Type & FILE_ATTRIBUTE_DIRECTORY ) != 0 )
		{
			// ディレクトリの場合はディレクトリのアーカイブに回す
			DirectoryEncode( ( int )Head.CharCodeFormat, FileOrDirectoryPath[i], NameP, DirP, FileP, &Directory, &SizeSave, i, DestFp, TempBuffer, Press, MaxPress, AlwaysHuffman, HuffmanEncodeKB, KeyString, KeyStringBytes, NoKey, KeyStringBuffer, &EncodeInfo ) ;
		}
		else
		{
			WIN32_FIND_DATA FindData ;
			HANDLE FindHandle ;
			DARC_FILEHEAD File ;
			u8 lKey[DXA_KEY_BYTES] ;
			size_t KeyStringBufferBytes ;
	
			// ファイルの情報を得る
			FindHandle = FindFirstFile( FileOrDirectoryPath[i], &FindData ) ;
			if( FindHandle == INVALID_HANDLE_VALUE ) continue ;

			// 進行状況出力
			if( EncodeInfo.OutputStatus )
			{
				// 処理ファイル名をセット
				wcscpy( EncodeInfo.ProcessFileName, FindData.cFileName ) ;

				// ファイル数を増やす
				EncodeInfo.CompFileNum ++ ;

				// 表示
				EncodeStatusOutput( &EncodeInfo ) ;
			}

			// ファイルヘッダをセットする
			{
				File.NameAddress       = SizeSave.NameSize ;
				File.Time.Create       = ( ( ( LONGLONG )FindData.ftCreationTime.dwHighDateTime   ) << 32 ) + FindData.ftCreationTime.dwLowDateTime   ;
				File.Time.LastAccess   = ( ( ( LONGLONG )FindData.ftLastAccessTime.dwHighDateTime ) << 32 ) + FindData.ftLastAccessTime.dwLowDateTime ;
				File.Time.LastWrite    = ( ( ( LONGLONG )FindData.ftLastWriteTime.dwHighDateTime  ) << 32 ) + FindData.ftLastWriteTime.dwLowDateTime  ;
				File.Attributes        = FindData.dwFileAttributes ;
				File.DataAddress       = SizeSave.DataSize ;
				File.DataSize          = ( ( ( LONGLONG )FindData.nFileSizeHigh ) << 32 ) + FindData.nFileSizeLow ;
				File.PressDataSize	   = 0xffffffffffffffff ;
				File.HuffPressDataSize = 0xffffffffffffffff ;
			}

			// ファイル名を書き出す
			SizeSave.NameSize += AddFileNameData( FindData.cFileName, NameP + SizeSave.NameSize ) ;

			// ファイル個別の鍵を作成
			if( NoKey == false )
			{
				KeyStringBufferBytes = CreateKeyFileString( ( int )Head.CharCodeFormat, KeyString, KeyStringBytes, DirectoryP, &File, FileP, DirP, NameP, ( BYTE * )KeyStringBuffer ) ;
				KeyCreate( KeyStringBuffer, KeyStringBufferBytes, lKey  ) ;
			}

			// ファイルデータを書き出す
			if( File.DataSize != 0 )
			{
				FILE *SrcP ;
				u64 FileSize, WriteSize, MoveSize ;
				bool Huffman = false ;
				bool AlwaysPress = false ;

				// ファイルを開く
				SrcP = _tfopen( FileOrDirectoryPath[i], TEXT("rb") ) ;
				
				// サイズを得る
				_fseeki64( SrcP, 0, SEEK_END ) ;
				FileSize = _ftelli64( SrcP ) ;
				_fseeki64( SrcP, 0, SEEK_SET ) ;

				// 圧縮の対象となるファイルフォーマットか調べる
				{
					u32 Len ;
					Len = ( u32 )_tcslen( FindData.cFileName ) ;
					if( Len > 4 )
					{
						TCHAR *sp ;
						
						sp = &FindData.cFileName[Len-3] ;
						if( StrICmp( sp, TEXT("wav") ) == 0 ||
							StrICmp( sp, TEXT("jpg") ) == 0 ||
							StrICmp( sp, TEXT("png") ) == 0 ||
							StrICmp( sp, TEXT("mpg") ) == 0 ||
							StrICmp( sp, TEXT("mp3") ) == 0 ||
							StrICmp( sp, TEXT("mp4") ) == 0 ||
							StrICmp( sp, TEXT("m4a") ) == 0 ||
							StrICmp( sp, TEXT("ogg") ) == 0 ||
							StrICmp( sp, TEXT("ogv") ) == 0 ||
							StrICmp( sp, TEXT("ops") ) == 0 ||
							StrICmp( sp, TEXT("wmv") ) == 0 ||
							StrICmp( sp, TEXT("tif") ) == 0 ||
							StrICmp( sp, TEXT("tga") ) == 0 ||
							StrICmp( sp, TEXT("bmp") ) == 0 ||
							StrICmp( sp - 1, TEXT("jpeg") ) == 0 )
						{
							Huffman = true ;
						}

						// wav や bmp の場合は必ず圧縮する
						if( StrICmp( sp, TEXT("wav") ) == 0 ||
							StrICmp( sp, TEXT("tga") ) == 0 ||
							StrICmp( sp, TEXT("bmp") ) == 0 )
						{
							AlwaysPress = true ;
						}
					}
				}

				// AlwaysHuffman が true の場合は必ずハフマン圧縮する
				if( AlwaysHuffman )
				{
					Huffman = true ;
				}

				// ハフマン圧縮するサイズが 0 の場合はハフマン圧縮を行わない
				if( HuffmanEncodeKB == 0 )
				{
					Huffman = false ;
				}

				// 圧縮の指定がある場合で、
				// 必ず圧縮するファイルフォーマットか、ファイルサイズが 10MB 以下の場合は圧縮を試みる
				if( Press == true && ( AlwaysPress || File.DataSize < 10 * 1024 * 1024 ) )
				{
					void *SrcBuf, *DestBuf ;
					u32 DestSize, Len ;
					
					// 一部のファイル形式の場合は予め弾く
					if( AlwaysPress == false && ( Len = ( int )_tcslen( FindData.cFileName ) ) > 4 )
					{
						TCHAR *sp ;
						
						sp = &FindData.cFileName[Len-3] ;
						if( StrICmp( sp, TEXT("wav") ) == 0 ||
							StrICmp( sp, TEXT("jpg") ) == 0 ||
							StrICmp( sp, TEXT("png") ) == 0 ||
							StrICmp( sp, TEXT("mpg") ) == 0 ||
							StrICmp( sp, TEXT("mp3") ) == 0 ||
							StrICmp( sp, TEXT("mp4") ) == 0 ||
							StrICmp( sp, TEXT("ogg") ) == 0 ||
							StrICmp( sp, TEXT("ogv") ) == 0 ||
							StrICmp( sp, TEXT("ops") ) == 0 ||
							StrICmp( sp, TEXT("wmv") ) == 0 ||
							StrICmp( sp - 1, TEXT("jpeg") ) == 0 ) goto NOPRESS ;
					}

					// データが丸ごと入るメモリ領域の確保
					SrcBuf  = calloc( 1, ( size_t )( FileSize + FileSize * 2 + 64 ) ) ;
					DestBuf = (u8 *)SrcBuf + FileSize ;
					
					// ファイルを丸ごと読み込む
					fread64( SrcBuf, FileSize, SrcP ) ;

					// 圧縮する場合は強制的に進行状況出力を更新
					if( EncodeInfo.OutputStatus )
					{
						EncodeStatusOutput( &EncodeInfo, true ) ;
					}
					
					// 圧縮
					DestSize = Encode( SrcBuf, ( u32 )FileSize, DestBuf, EncodeInfo.OutputStatus, MaxPress ) ;
					
					// 殆ど圧縮出来なかった場合は圧縮無しでアーカイブする
					if( AlwaysPress == false && ( (f64)DestSize / (f64)FileSize > 0.90 ) )
					{
						_fseeki64( SrcP, 0L, SEEK_SET ) ;
						free( SrcBuf ) ;
						goto NOPRESS ;
					}

					// 圧縮データのサイズを保存する
					File.PressDataSize = DestSize ;

					// ハフマン圧縮も行うかどうかで処理を分岐
					if( Huffman )
					{
						u8 *HuffData ;

						// ハフマン圧縮するサイズによって処理を分岐
						if( HuffmanEncodeKB == 0xff || DestSize <= ( u64 )( HuffmanEncodeKB * 1024 * 2 ) )
						{
							// ハフマン圧縮用のメモリ領域を確保
							HuffData = ( u8 * )calloc( 1, DestSize * 2 + 256 * 2 + 32 ) ;

							// ファイル全体をハフマン圧縮
							File.HuffPressDataSize = Huffman_Encode( DestBuf, DestSize, HuffData ) ;

							// 圧縮データに鍵を適用して書き出す
							WriteSize = ( File.HuffPressDataSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
							KeyConvFileWrite( HuffData, WriteSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;
						}
						else
						{
							// ハフマン圧縮用のメモリ領域を確保
							HuffData = ( u8 * )calloc( 1, HuffmanEncodeKB * 1024 * 2 * 4 + 256 * 2 + 32 ) ;

							// ファイルの前後をハフマン圧縮
							memcpy( HuffData,                                  DestBuf,                                     HuffmanEncodeKB * 1024 ) ;
							memcpy( HuffData + HuffmanEncodeKB * 1024, ( u8 * )DestBuf + DestSize - HuffmanEncodeKB * 1024, HuffmanEncodeKB * 1024 ) ;
							File.HuffPressDataSize = Huffman_Encode( HuffData, HuffmanEncodeKB * 1024 * 2, HuffData + HuffmanEncodeKB * 1024 * 2 ) ;

							// ハフマン圧縮した部分を書き出す
							KeyConvFileWrite( HuffData + HuffmanEncodeKB * 1024 * 2, File.HuffPressDataSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;

							// ハフマン圧縮していない箇所を書き出す
							WriteSize = File.HuffPressDataSize + DestSize - HuffmanEncodeKB * 1024 * 2 ;
							WriteSize = ( WriteSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
							KeyConvFileWrite( ( u8 * )DestBuf + HuffmanEncodeKB * 1024, WriteSize - File.HuffPressDataSize, DestFp, NoKey ? NULL : lKey, File.DataSize + File.HuffPressDataSize ) ;
						}

						// メモリの解放
						free( HuffData ) ;
					}
					else
					{
						// 圧縮データを反転して書き出す
						WriteSize = ( DestSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
						KeyConvFileWrite( DestBuf, WriteSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;
					}

					// メモリの解放
					free( SrcBuf ) ;
				}
				else
				{
NOPRESS:
					// ハフマン圧縮も行うかどうかで処理を分岐
					if( Press && Huffman )
					{
						u8 *SrcBuf, *HuffData ;

						// データが丸ごと入るメモリ領域の確保
						SrcBuf = ( u8 * )calloc( 1, ( size_t )( FileSize + 32 ) ) ;

						// ファイルを丸ごと読み込む
						fread64( SrcBuf, FileSize, SrcP ) ;
						
						// ハフマン圧縮するサイズによって処理を分岐
						if( HuffmanEncodeKB == 0xff || FileSize <= HuffmanEncodeKB * 1024 * 2 )
						{
							// ハフマン圧縮用のメモリ領域を確保
							HuffData = ( u8 * )calloc( 1, ( size_t )( FileSize * 2 + 256 * 2 + 32 ) ) ;

							// ファイル全体をハフマン圧縮
							File.HuffPressDataSize = Huffman_Encode( SrcBuf, FileSize, HuffData ) ;

							// 圧縮データに鍵を適用して書き出す
							WriteSize = ( File.HuffPressDataSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
							KeyConvFileWrite( HuffData, WriteSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;
						}
						else
						{
							// ハフマン圧縮用のメモリ領域を確保
							HuffData = ( u8 * )calloc( 1, HuffmanEncodeKB * 1024 * 2 * 4 + 256 * 2 + 32 ) ;

							// ファイルの前後をハフマン圧縮
							memcpy( HuffData,                          SrcBuf,                                     HuffmanEncodeKB * 1024 ) ;
							memcpy( HuffData + HuffmanEncodeKB * 1024, SrcBuf + FileSize - HuffmanEncodeKB * 1024, HuffmanEncodeKB * 1024 ) ;
							File.HuffPressDataSize = Huffman_Encode( HuffData, HuffmanEncodeKB * 1024 * 2, HuffData + HuffmanEncodeKB * 1024 * 2 ) ;

							// ハフマン圧縮した部分を書き出す
							KeyConvFileWrite( HuffData + HuffmanEncodeKB * 1024 * 2, File.HuffPressDataSize, DestFp, NoKey ? NULL : lKey, File.DataSize ) ;

							// ハフマン圧縮していない箇所を書き出す
							WriteSize = File.HuffPressDataSize + FileSize - HuffmanEncodeKB * 1024 * 2 ;
							WriteSize = ( WriteSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
							KeyConvFileWrite( SrcBuf + HuffmanEncodeKB * 1024, WriteSize - File.HuffPressDataSize, DestFp, NoKey ? NULL : lKey, File.DataSize + File.HuffPressDataSize ) ;
						}

						// メモリの解放
						free( SrcBuf ) ;
						free( HuffData ) ;
					}
					else
					{
						// 転送開始
						WriteSize = 0 ;
						while( WriteSize < FileSize )
						{
							// 転送サイズ決定
							MoveSize = DXA_BUFFERSIZE < FileSize - WriteSize ? DXA_BUFFERSIZE : FileSize - WriteSize ;
							MoveSize = ( MoveSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
						
							// ファイルの鍵適用読み込み
							memset( TempBuffer, 0, ( size_t )MoveSize ) ;
							KeyConvFileRead( TempBuffer, MoveSize, SrcP, NoKey ? NULL : lKey, File.DataSize + WriteSize ) ;

							// 書き出し
							fwrite64( TempBuffer, MoveSize, DestFp ) ;
						
							// 書き出しサイズの加算
							WriteSize += MoveSize ;
						}
					}
				}
				
				// 書き出したファイルを閉じる
				fclose( SrcP ) ;
			
				// データサイズの加算
				SizeSave.DataSize += WriteSize ;
			}
			
			// ファイルヘッダを書き出す
			memcpy( FileP + Directory.FileHeadAddress + sizeof( DARC_FILEHEAD ) * i, &File, sizeof( DARC_FILEHEAD ) ) ;

			// Find ハンドルを閉じる
			FindClose( FindHandle ) ;
		}
	}
	
	// バッファに溜め込んだ各種ヘッダデータを出力する
	{
		u8 *PressSource ;
		u64 TotalSize = SizeSave.NameSize + SizeSave.FileSize + SizeSave.DirectorySize ;

		// 全部のデータを纏める
		PressSource = ( u8 * )malloc( ( size_t )TotalSize ) ;
		if( PressSource == NULL ) return -1 ;
		memcpy( PressSource,                                         NameP, ( size_t )SizeSave.NameSize ) ;
		memcpy( PressSource + SizeSave.NameSize,                     FileP, ( size_t )SizeSave.FileSize ) ;
		memcpy( PressSource + SizeSave.NameSize + SizeSave.FileSize, DirP,  ( size_t )SizeSave.DirectorySize ) ;

		// 圧縮するかどうかで処理を分岐
		if( Press )
		{
			u8 *PressData ;
			int LZDataSize ;

			// 圧縮する場合

			// 圧縮データを格納するメモリ領域の確保
			PressData = ( u8 * )malloc( ( size_t )( TotalSize * 4 + 256 * 2 + 32 * 2 ) ) ;
			if( PressData == NULL ) return -1 ;

			// LZ圧縮
			LZDataSize = Encode( PressSource, ( u32 )TotalSize, PressData, false ) ;

			// ハフマン圧縮
			HeaderHuffDataSize = Huffman_Encode( PressData, ( u64 )LZDataSize, PressData + TotalSize * 2 + 32 ) ;

			// 纏めたものに鍵を適用して出力
			KeyConvFileWrite( PressData + TotalSize * 2 + 32, HeaderHuffDataSize, DestFp, NoKey ? NULL : Key, 0 ) ;

			// メモリの解放
			free( PressData ) ;
		}
		else
		{
			// 纏めたものに鍵を適用して出力
			KeyConvFileWrite( PressSource, TotalSize, DestFp, NoKey ? NULL : Key, 0 ) ;
		}

		// メモリの解放
		free( PressSource ) ;
	}
		
	// 再びアーカイブのヘッダを出力する
	{
		Head.Head                       = DXA_HEAD ;
		Head.Version                    = DXA_VER ;
		Head.HeadSize                   = ( u32 )( SizeSave.NameSize + SizeSave.FileSize + SizeSave.DirectorySize ) ;
		Head.DataStartAddress           = sizeof( DARC_HEAD ) ;
		Head.FileNameTableStartAddress  = SizeSave.DataSize + Head.DataStartAddress ;
		Head.FileTableStartAddress      = SizeSave.NameSize ;
		Head.DirectoryTableStartAddress = Head.FileTableStartAddress + SizeSave.FileSize ;

		_fseeki64( DestFp, 0, SEEK_SET ) ;
		fwrite64( &Head, sizeof( DARC_HEAD ), DestFp ) ;
	}
	
	// 書き出したファイルを閉じる
	fclose( DestFp ) ;
	
	// 確保したバッファを開放する
	free( NameP ) ;
	free( FileP ) ;
	free( DirP ) ;
	free( TempBuffer ) ;

	// 圧縮状況表示をクリア
	EncodeStatusErase() ;

	// 終了
	return 0 ;
}

// アーカイブファイルを展開する
int DXArchive::DecodeArchive(TCHAR *ArchiveName, const TCHAR *OutputPath, const char *KeyString_ )
{
	u8 *HeadBuffer = NULL ;
	DARC_HEAD Head ;
	u8 *FileP, *NameP, *DirP ;
	FILE *ArcP = NULL ;
	TCHAR OldDir[MAX_PATH] ;
	u8 Key[DXA_KEY_BYTES] ;
	char KeyString[ DXA_KEY_STRING_LENGTH + 1 ] ;
	size_t KeyStringBytes ;
	char KeyStringBuffer[ DXA_KEY_STRING_MAXLENGTH ] ;
	bool NoKey ;

	// 鍵文字列の保存と鍵の作成
	{
		// 指定が無い場合はデフォルトの鍵文字列を使用する
		if( KeyString_ == NULL )
		{
			KeyString_ = DefaultKeyString ;
		}

		KeyStringBytes = CL_strlen( CHARCODEFORMAT_ASCII, KeyString_ ) ;
		if( KeyStringBytes > DXA_KEY_STRING_LENGTH )
		{
			KeyStringBytes = DXA_KEY_STRING_LENGTH ;
		}
		memcpy( KeyString, KeyString_, KeyStringBytes ) ;
		KeyString[ KeyStringBytes ] = '\0' ;

		// 鍵の作成
		KeyCreate( KeyString, KeyStringBytes, Key ) ;
	}

	// アーカイブファイルを開く
	ArcP = _tfopen( ArchiveName, TEXT("rb") ) ;
	if( ArcP == NULL ) return -1 ;

	// 出力先のディレクトリにカレントディレクトリを変更する
	GetCurrentDirectory( MAX_PATH, OldDir ) ;
	SetCurrentDirectory( OutputPath ) ;

	// ヘッダを解析する
	{
		s64 FileSize ;

		// ヘッダの読み込み
		fread64( &Head, sizeof( DARC_HEAD ), ArcP ) ;
		
		// ＩＤの検査
		if( Head.Head != DXA_HEAD )
		{
			goto ERR ;
		}
		
		// バージョン検査
		if( Head.Version > DXA_VER || Head.Version < DXA_VER_MIN ) goto ERR ;

		// 鍵処理が行われていないかを取得する
		NoKey = ( Head.Flags & DXA_FLAG_NO_KEY ) != 0 ;
		
		// ヘッダのサイズ分のメモリを確保する
		HeadBuffer = ( u8 * )malloc( ( size_t )Head.HeadSize ) ;
		if( HeadBuffer == NULL ) goto ERR ;

		// ヘッダが圧縮されている場合は解凍する
		if( ( Head.Flags & DXA_FLAG_NO_HEAD_PRESS ) != 0 )
		{
			// 圧縮されていない場合は普通に読み込む
			KeyConvFileRead( HeadBuffer, Head.HeadSize, ArcP, NoKey ? NULL : Key, 0 ) ;
		}
		else
		{
			void *HuffHeadBuffer ;
			u64 HuffHeadSize ;
			void *LzHeadBuffer ;
			u64 LzHeadSize ;

			// ハフマン圧縮されたヘッダのサイズを取得する
			_fseeki64( ArcP, 0, SEEK_END ) ;
			FileSize = _ftelli64( ArcP ) ;
			_fseeki64( ArcP, Head.FileNameTableStartAddress, SEEK_SET ) ;
			HuffHeadSize = ( u32 )( FileSize - _ftelli64( ArcP ) ) ;

			// ハフマン圧縮されたヘッダを読み込むメモリを確保する
			HuffHeadBuffer = malloc( ( size_t )HuffHeadSize ) ;
			if( HuffHeadBuffer == NULL ) goto ERR ;

			// ハフマン圧縮されたヘッダをコピーと暗号化解除
			KeyConvFileRead( HuffHeadBuffer, HuffHeadSize, ArcP, NoKey ? NULL : Key, 0 ) ;

			// ハフマン圧縮されたヘッダの解凍後の容量を取得する
			LzHeadSize = Huffman_Decode( HuffHeadBuffer, NULL ) ;

			// ハフマン圧縮されたヘッダの解凍後のデータを格納するメモリ用域の確保
			LzHeadBuffer = malloc( ( size_t )LzHeadSize ) ;
			if( LzHeadBuffer == NULL )
			{
				free( HuffHeadBuffer ) ;
				goto ERR ;
			}

			// ハフマン圧縮されたヘッダを解凍する
			Huffman_Decode( HuffHeadBuffer, LzHeadBuffer ) ;

			// LZ圧縮されたヘッダを解凍する
			Decode( LzHeadBuffer, HeadBuffer ) ;

			// メモリの解放
			free( HuffHeadBuffer ) ;
			free( LzHeadBuffer ) ;
		}

		// 各アドレスをセットする
		NameP = HeadBuffer ;
		FileP = NameP + Head.FileTableStartAddress ;
		DirP  = NameP + Head.DirectoryTableStartAddress ;
	}

	// アーカイブの展開を開始する
	DirectoryDecode( NameP, DirP, FileP, &Head, ( DARC_DIRECTORY * )DirP, ArcP, Key, KeyString, KeyStringBytes, NoKey, KeyStringBuffer ) ;
	
	// ファイルを閉じる
	fclose( ArcP ) ;
	
	// ヘッダを読み込んでいたメモリを解放する
	free( HeadBuffer ) ;

	// カレントディレクトリを元に戻す
	SetCurrentDirectory( OldDir ) ;

	// 終了
	return 0 ;

ERR :
	if( HeadBuffer != NULL ) free( HeadBuffer ) ;
	if( ArcP != NULL ) fclose( ArcP ) ;

	// カレントディレクトリを元に戻す
	SetCurrentDirectory( OldDir ) ;

	// 終了
	return -1 ;
}



// コンストラクタ
DXArchive::DXArchive(TCHAR *ArchivePath )
{
	this->fp = NULL ;
	this->HeadBuffer = NULL ;
	this->NameP = this->DirP = this->FileP = NULL ;
	this->CurrentDirectory = NULL ;
	this->CacheBuffer = NULL ;

	if( ArchivePath != NULL )
	{
		this->OpenArchiveFile( ArchivePath ) ;
	}
}

// デストラクタ
DXArchive::~DXArchive()
{
	if( this->fp != NULL ) this->CloseArchiveFile() ;

	if( this->fp != NULL ) fclose( this->fp ) ;
	if( this->HeadBuffer != NULL ) free( this->HeadBuffer ) ;
	if( this->CacheBuffer != NULL ) free( this->CacheBuffer ) ;

	this->fp = NULL ;
	this->HeadBuffer = NULL ;
	this->NameP = this->DirP = this->FileP = NULL ;
	this->CurrentDirectory = NULL ;
}

// 指定のディレクトリデータの暗号化を解除する( 丸ごとメモリに読み込んだ場合用 )
int DXArchive::DirectoryKeyConv( DARC_DIRECTORY *Dir, char *KeyStringBuffer )
{
	// メモリイメージではない場合はエラー
	if( this->MemoryOpenFlag == false )
		return -1 ;

	// 鍵を使わない場合は何もせずに終了
	if( this->NoKey )
		return -1 ;
	
	// 暗号化解除処理開始
	{
		u32 i, FileHeadSize ;
		DARC_FILEHEAD *File ;
		unsigned char lKey[ DXA_KEY_BYTES ] ;
		size_t KeyStringBufferBytes ;

		// 格納されているファイルの数だけ繰り返す
		FileHeadSize = sizeof( DARC_FILEHEAD ) ;
		File = ( DARC_FILEHEAD * )( this->FileP + Dir->FileHeadAddress ) ;
		for( i = 0 ; i < Dir->FileHeadNum ; i ++, File = (DARC_FILEHEAD *)( (u8 *)File + FileHeadSize ) )
		{
			// ディレクトリかどうかで処理を分岐
			if( File->Attributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				// ディレクトリの場合は再帰をかける
				DirectoryKeyConv( ( DARC_DIRECTORY * )( this->DirP + File->DataAddress ), KeyStringBuffer ) ;
			}
			else
			{
				u8 *DataP ;

				// ファイルの場合は暗号化を解除する
				
				// データがある場合のみ処理
				if( File->DataSize != 0 )
				{
					// データ位置をセットする
					DataP = ( u8 * )this->fp + this->Head.DataStartAddress + File->DataAddress ;

					// ファイル個別の鍵を作成
					if( NoKey == false )
					{
						KeyStringBufferBytes = CreateKeyFileString( ( int )this->Head.CharCodeFormat, this->KeyString, this->KeyStringBytes, Dir, File, this->FileP, this->DirP, this->NameP, ( BYTE * )KeyStringBuffer ) ;
						KeyCreate( KeyStringBuffer, KeyStringBufferBytes, lKey ) ;
					}

					// ハフマン圧縮されているかどうかで処理を分岐
					if( File->HuffPressDataSize != 0xffffffffffffffff )
					{
						// ハフマン圧縮されている場合
						KeyConv( DataP, File->HuffPressDataSize, File->DataSize, lKey ) ;
					}
					else
					// データが圧縮されているかどうかで処理を分岐
					if( File->PressDataSize != 0xffffffffffffffff )
					{
						// 圧縮されている場合
						KeyConv( DataP, File->PressDataSize, File->DataSize, lKey ) ;
					}
					else
					{
						// 圧縮されていない場合
						KeyConv( DataP, File->DataSize, File->DataSize, lKey ) ;
					}
				}
			}
		}
	}

	// 終了
	return 0 ;
}

// アーカイブファイルを開く
int DXArchive::OpenArchiveFile( const TCHAR *ArchivePath, const char *KeyString_ )
{
	// 既になんらかのアーカイブを開いていた場合はエラー
	if( this->fp != NULL ) return -1 ;

	// アーカイブファイルを開こうと試みる
	this->fp = _tfopen(ArchivePath, TEXT("rb"));
	if( this->fp == NULL ) return -1 ;

	// 鍵文字列の保存と鍵の作成
	{
		// 指定が無い場合はデフォルトの鍵文字列を使用する
		if( KeyString_ == NULL )
		{
			KeyString_ = DefaultKeyString ;
		}

		KeyStringBytes = CL_strlen( CHARCODEFORMAT_ASCII, KeyString_ ) ;
		if( KeyStringBytes > DXA_KEY_STRING_LENGTH )
		{
			KeyStringBytes = DXA_KEY_STRING_LENGTH ;
		}
		memcpy( KeyString, KeyString_, KeyStringBytes ) ;
		KeyString[ KeyStringBytes ] = '\0' ;

		// 鍵の作成
		KeyCreate( KeyString, KeyStringBytes, Key ) ;
	}

	// ヘッダを解析する
	{
		// ヘッダの読み込み
		fread64( &this->Head, sizeof( DARC_HEAD ), this->fp ) ;
		
		// ＩＤの検査
		if( this->Head.Head != DXA_HEAD )
		{
			goto ERR ;
		}
		
		// バージョン検査
		if( this->Head.Version > DXA_VER || this->Head.Version < DXA_VER_MIN ) goto ERR ;

		// 鍵処理が行われていないかを取得する
		this->NoKey = ( Head.Flags & DXA_FLAG_NO_KEY ) != 0 ;
		
		// ヘッダのサイズ分のメモリを確保する
		this->HeadBuffer = ( u8 * )malloc( ( size_t )this->Head.HeadSize ) ;
		if( this->HeadBuffer == NULL ) goto ERR ;

		// ヘッダが圧縮されている場合は解凍する
		if( ( Head.Flags & DXA_FLAG_NO_HEAD_PRESS ) != 0 )
		{
			// 圧縮されていない場合は普通に読み込む
			_fseeki64( this->fp, this->Head.FileNameTableStartAddress, SEEK_SET ) ;
			KeyConvFileRead( HeadBuffer, this->Head.HeadSize, this->fp, this->NoKey ? NULL : this->Key, 0 ) ;
		}
		else
		{
			void *HuffHeadBuffer ;
			u64 HuffHeadSize ;
			void *LzHeadBuffer ;
			u64 LzHeadSize ;
			s64 FileSize ;

			// 圧縮されたヘッダの容量を取得する
			_fseeki64( this->fp, 0, SEEK_END ) ;
			FileSize = _ftelli64( this->fp ) ;
			_fseeki64( this->fp, this->Head.FileNameTableStartAddress, SEEK_SET ) ;
			HuffHeadSize = ( u32 )( FileSize - _ftelli64( this->fp ) ) ;

			// ハフマン圧縮されたヘッダを読み込むメモリを確保する
			HuffHeadBuffer = malloc( ( size_t )HuffHeadSize ) ;
			if( HuffHeadBuffer == NULL ) goto ERR ;

			// ハフマン圧縮されたヘッダをメモリに読み込む
			KeyConvFileRead( HuffHeadBuffer, HuffHeadSize, this->fp, NoKey ? NULL : Key, 0 ) ;

			// ハフマン圧縮されたヘッダの解凍後の容量を取得する
			LzHeadSize = Huffman_Decode( HuffHeadBuffer, NULL ) ;

			// ハフマン圧縮されたヘッダの解凍後のデータを格納するメモリ用域の確保
			LzHeadBuffer = malloc( ( size_t )LzHeadSize ) ;
			if( LzHeadBuffer == NULL )
			{
				free( HuffHeadBuffer ) ;
				goto ERR ;
			}

			// ハフマン圧縮されたヘッダを解凍する
			Huffman_Decode( HuffHeadBuffer, LzHeadBuffer ) ;

			// LZ圧縮されたヘッダを解凍する
			Decode( LzHeadBuffer, this->HeadBuffer ) ;

			// メモリの解放
			free( HuffHeadBuffer ) ;
			free( LzHeadBuffer ) ;
		}

		// 各アドレスをセットする
		this->NameP = this->HeadBuffer ;
		this->FileP = this->NameP + this->Head.FileTableStartAddress ;
		this->DirP = this->NameP + this->Head.DirectoryTableStartAddress ;
	}

	// カレントディレクトリのセット
	this->CurrentDirectory = ( DARC_DIRECTORY * )this->DirP ;

	// メモリイメージから開いている、フラグを倒す
	MemoryOpenFlag = false ;

	// 終了
	return 0 ;

ERR :
	if( this->fp != NULL ){ fclose( this->fp ) ; this->fp = NULL ; }
	if( this->HeadBuffer != NULL ){ free( this->HeadBuffer ) ; this->HeadBuffer = NULL ; }
	
	// 終了
	return -1 ;
}

// アーカイブファイルを開き最初にすべてメモリ上に読み込んでから処理する( 0:成功  -1:失敗 )
int DXArchive::OpenArchiveFileMem( const TCHAR *ArchivePath, const char *KeyString_ )
{
	FILE *fp ;
	u8 *datp ;
	void *ArchiveImage ;
	s64 ArchiveSize ;
	void *HuffHeadBuffer = NULL ;

	// 既になんらかのアーカイブを開いていた場合はエラー
	if( this->fp != NULL ) return -1 ;

	// 鍵文字列の保存と鍵の作成
	{
		// 指定が無い場合はデフォルトの鍵文字列を使用する
		if( KeyString_ == NULL )
		{
			KeyString_ = DefaultKeyString ;
		}

		KeyStringBytes = CL_strlen( CHARCODEFORMAT_ASCII, KeyString_ ) ;
		if( KeyStringBytes > DXA_KEY_STRING_LENGTH )
		{
			KeyStringBytes = DXA_KEY_STRING_LENGTH ;
		}
		memcpy( KeyString, KeyString_, KeyStringBytes ) ;
		KeyString[ KeyStringBytes ] = '\0' ;

		// 鍵の作成
		KeyCreate( KeyString, KeyStringBytes, Key ) ;
	}

	// メモリに読み込む
	{
		fp = _tfopen( ArchivePath, TEXT("rb") ) ;
		if( fp == NULL ) return -1 ;
		_fseeki64( fp, 0L, SEEK_END ) ;
		ArchiveSize = _ftelli64( fp ) ;
		_fseeki64( fp, 0L, SEEK_SET ) ;
		ArchiveImage = malloc( ( size_t )ArchiveSize ) ;
		if( ArchiveImage == NULL )
		{
			fclose( fp ) ;
			return -1 ;
		}
		fread64( ArchiveImage, ArchiveSize, fp ) ;
		fclose( fp ) ;
	}

	// ヘッダの部分をコピー
	memcpy( &this->Head, ArchiveImage, sizeof( DARC_HEAD ) ) ;

	// ＩＤが違う場合はエラー
	if( Head.Head != DXA_HEAD )
	{
		return -1 ;
	}

	// ポインタを保存
	this->fp = (FILE *)ArchiveImage ;
	datp = (u8 *)ArchiveImage ;

	// ヘッダを解析する
	{
		datp += sizeof( DARC_HEAD ) ;
		
		// バージョン検査
		if( this->Head.Version > DXA_VER || this->Head.Version < DXA_VER_MIN ) goto ERR ;

		// 鍵処理が行われていないかを取得する
		this->NoKey = ( Head.Flags & DXA_FLAG_NO_KEY ) != 0 ;

		// ヘッダのサイズ分のメモリを確保する
		this->HeadBuffer = ( u8 * )malloc( ( size_t )this->Head.HeadSize ) ;
		if( this->HeadBuffer == NULL ) goto ERR ;

		// ヘッダが圧縮されている場合は解凍する
		if( ( Head.Flags & DXA_FLAG_NO_HEAD_PRESS ) != 0 )
		{
			// 圧縮されていない場合は普通に読み込む
			memcpy( HeadBuffer, ( u8 * )this->fp + this->Head.FileNameTableStartAddress, this->Head.HeadSize ) ;
			if( this->NoKey == false ) KeyConv( HeadBuffer, this->Head.HeadSize, 0, this->Key ) ;
		}
		else
		{
			void *HuffHeadBuffer ;
			u64 HuffHeadSize ;
			void *LzHeadBuffer ;
			u64 LzHeadSize ;

			// ハフマン圧縮されたヘッダの容量を取得する
			HuffHeadSize = ( u32 )( ( u64 )ArchiveSize - this->Head.FileNameTableStartAddress ) ;

			// ハフマン圧縮されたヘッダを読み込むメモリを確保する
			HuffHeadBuffer = malloc( ( size_t )HuffHeadSize ) ;
			if( HuffHeadBuffer == NULL ) goto ERR ;

			// 圧縮されたヘッダをコピーと暗号化解除
			memcpy( HuffHeadBuffer, ( u8 * )this->fp + this->Head.FileNameTableStartAddress, ( size_t )HuffHeadSize ) ;
			if( this->NoKey == false ) KeyConv( HuffHeadBuffer, HuffHeadSize, 0, this->Key ) ;

			// ハフマン圧縮されたヘッダの解凍後の容量を取得する
			LzHeadSize = Huffman_Decode( HuffHeadBuffer, NULL ) ;

			// ハフマン圧縮されたヘッダの解凍後のデータを格納するメモリ用域の確保
			LzHeadBuffer = malloc( ( size_t )LzHeadSize ) ;
			if( LzHeadBuffer == NULL )
			{
				free( HuffHeadBuffer ) ;
				goto ERR ;
			}

			// ハフマン圧縮されたヘッダを解凍する
			Huffman_Decode( HuffHeadBuffer, LzHeadBuffer ) ;

			// LZ圧縮されたヘッダを解凍する
			Decode( LzHeadBuffer, this->HeadBuffer ) ;

			// メモリの解放
			free( HuffHeadBuffer ) ;
			free( LzHeadBuffer ) ;
		}

		// 各アドレスをセットする
		this->NameP = this->HeadBuffer ;
		this->FileP = this->NameP + this->Head.FileTableStartAddress ;
		this->DirP = this->NameP + this->Head.DirectoryTableStartAddress ;
	}

	// カレントディレクトリのセット
	this->CurrentDirectory = ( DARC_DIRECTORY * )this->DirP ;

	// メモリイメージから開いているフラグを立てる
	MemoryOpenFlag = true ;

	// 全てのファイルの暗号化を解除する
	if( this->NoKey == false )
	{
		char KeyStringBuffer[ DXA_KEY_STRING_MAXLENGTH ] ;
		DirectoryKeyConv( ( DARC_DIRECTORY * )this->DirP, KeyStringBuffer ) ;
	}
	
	// ユーザーのイメージから開いたわけではないのでフラグを倒す
	UserMemoryImageFlag = false ;

	// サイズも保存しておく
	MemoryImageSize = ArchiveSize ;

	// 終了
	return 0 ;

ERR :
	
	// 終了
	return -1 ;
}

// メモリ上にあるアーカイブイメージを開く( 0:成功  -1:失敗 )
int	DXArchive::OpenArchiveMem( void *ArchiveImage, s64 ArchiveSize, const char *KeyString_ )
{
	u8 *datp ;

	// 既になんらかのアーカイブを開いていた場合はエラー
	if( this->fp != NULL ) return -1 ;

	// 鍵文字列の保存と鍵の作成
	{
		// 指定が無い場合はデフォルトの鍵文字列を使用する
		if( KeyString_ == NULL )
		{
			KeyString_ = DefaultKeyString ;
		}

		KeyStringBytes = CL_strlen( CHARCODEFORMAT_ASCII, KeyString_ ) ;
		if( KeyStringBytes > DXA_KEY_STRING_LENGTH )
		{
			KeyStringBytes = DXA_KEY_STRING_LENGTH ;
		}
		memcpy( KeyString, KeyString_, KeyStringBytes ) ;
		KeyString[ KeyStringBytes ] = '\0' ;

		// 鍵の作成
		KeyCreate( KeyString, KeyStringBytes, Key ) ;
	}

	// ヘッダの部分をコピー
	memcpy( &this->Head, ArchiveImage, sizeof( DARC_HEAD ) ) ;

	// ＩＤが違う場合はエラー
	if( Head.Head != DXA_HEAD )
	{
		goto ERR ;
	}

	// ポインタを保存
	this->fp = (FILE *)ArchiveImage ;
	datp = (u8 *)ArchiveImage ;

	// ヘッダを解析する
	{
		datp += sizeof( DARC_HEAD ) ;

		// バージョン検査
		if( this->Head.Version > DXA_VER || this->Head.Version < DXA_VER_MIN ) goto ERR ;

		// 鍵処理が行われていないかを取得する
		this->NoKey = ( Head.Flags & DXA_FLAG_NO_KEY ) != 0 ;

		// ヘッダのサイズ分のメモリを確保する
		this->HeadBuffer = ( u8 * )malloc( ( size_t )this->Head.HeadSize ) ;
		if( this->HeadBuffer == NULL ) goto ERR ;

		// ヘッダが圧縮されている場合は解凍する
		if( ( Head.Flags & DXA_FLAG_NO_HEAD_PRESS ) != 0 )
		{
			// 圧縮されていない場合は普通に読み込む
			memcpy( HeadBuffer, ( u8 * )this->fp + this->Head.FileNameTableStartAddress, this->Head.HeadSize ) ;
			if( this->NoKey == false ) KeyConv( HeadBuffer, this->Head.HeadSize, 0, this->Key ) ;
		}
		else
		{
			void *HuffHeadBuffer ;
			u64 HuffHeadSize ;
			void *LzHeadBuffer ;
			u64 LzHeadSize ;

			// ハフマン圧縮されたヘッダの容量を取得する
			HuffHeadSize = ( u32 )( ( u64 )ArchiveSize - this->Head.FileNameTableStartAddress ) ;

			// ハフマン圧縮されたヘッダを読み込むメモリを確保する
			HuffHeadBuffer = malloc( ( size_t )HuffHeadSize ) ;
			if( HuffHeadBuffer == NULL ) goto ERR ;

			// ハフマン圧縮されたヘッダをコピーと暗号化解除
			memcpy( HuffHeadBuffer, ( u8 * )this->fp + this->Head.FileNameTableStartAddress, ( size_t )HuffHeadSize ) ;
			if( this->NoKey == false ) KeyConv( HuffHeadBuffer, HuffHeadSize, 0, this->Key ) ;

			// ハフマン圧縮されたヘッダの解凍後の容量を取得する
			LzHeadSize = Huffman_Decode( HuffHeadBuffer, NULL ) ;

			// ハフマン圧縮されたヘッダの解凍後のデータを格納するメモリ用域の確保
			LzHeadBuffer = malloc( ( size_t )LzHeadSize ) ;
			if( LzHeadBuffer == NULL )
			{
				free( HuffHeadBuffer ) ;
				goto ERR ;
			}

			// ハフマン圧縮されたヘッダを解凍する
			Huffman_Decode( HuffHeadBuffer, LzHeadBuffer ) ;

			// LZ圧縮されたヘッダを解凍する
			Decode( LzHeadBuffer, this->HeadBuffer ) ;

			// メモリの解放
			free( HuffHeadBuffer ) ;
			free( LzHeadBuffer ) ;
		}

		// 各アドレスをセットする
		this->NameP = this->HeadBuffer ;
		this->FileP = this->NameP + this->Head.FileTableStartAddress ;
		this->DirP = this->NameP + this->Head.DirectoryTableStartAddress ;
	}

	// カレントディレクトリのセット
	this->CurrentDirectory = ( DARC_DIRECTORY * )this->DirP ;

	// メモリイメージから開いているフラグを立てる
	MemoryOpenFlag = true ;

	// 全てのファイルの暗号化を解除する
	if( this->NoKey == false )
	{
		char KeyStringBuffer[ DXA_KEY_STRING_MAXLENGTH ] ;
		DirectoryKeyConv( ( DARC_DIRECTORY * )this->DirP, KeyStringBuffer ) ;
	}

	// ユーザーのイメージから開いたフラグを立てる
	UserMemoryImageFlag = true ;

	// サイズも保存しておく
	MemoryImageSize = ArchiveSize ;

	// 終了
	return 0 ;

ERR :
	// 終了
	return -1 ;
}

// アーカイブファイルを閉じる
int DXArchive::CloseArchiveFile( void )
{
	// 既に閉じていたら何もせず終了
	if( this->fp == NULL ) return 0 ;

	// メモリから開いているかどうかで処理を分岐
	if( MemoryOpenFlag == true )
	{
		// アーカイブクラスが読み込んだ場合とそうでない場合で処理を分岐
		if( UserMemoryImageFlag == true )
		{
			// 反転したデータを元に戻す
			if( this->NoKey == false )
			{
				char KeyStringBuffer[ DXA_KEY_STRING_MAXLENGTH ] ;
				DirectoryKeyConv( ( DARC_DIRECTORY * )this->DirP, KeyStringBuffer ) ;
				KeyConv( this->HeadBuffer, this->Head.HeadSize, 0, this->Key ) ;
			}
		}
		else
		{
			// 確保していたメモリを開放する
			free( this->fp ) ;
		}
	}
	else
	{
		// アーカイブファイルを閉じる
		fclose( this->fp ) ;
	}

	// ヘッダバッファを解放
	free( this->HeadBuffer ) ;

	// ポインタ初期化
	this->fp = NULL ;
	this->HeadBuffer = NULL ;
	this->NameP = this->DirP = this->FileP = NULL ;
	this->CurrentDirectory = NULL ;

	// 終了
	return 0 ;
}

// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
int	DXArchive::ChangeCurrentDirectoryFast( SEARCHDATA *SearchData )
{
	DARC_FILEHEAD *FileH ;
	int i, j, k, Num ;
	u8 *NameData, *PathData ;
	u16 PackNum, Parity ;

	PackNum  = SearchData->PackNum ;
	Parity   = SearchData->Parity ;
	PathData = SearchData->FileName ;

	// カレントディレクトリから同名のディレクトリを探す
	FileH = ( DARC_FILEHEAD * )( this->FileP + this->CurrentDirectory->FileHeadAddress ) ;
	Num = (s32)this->CurrentDirectory->FileHeadNum ;
	for( i = 0 ; i < Num ; i ++, FileH ++ )
	{
		// ディレクトリチェック
		if( ( FileH->Attributes & FILE_ATTRIBUTE_DIRECTORY ) == 0 ) continue ;

		// 文字列数とパリティチェック
		NameData = this->NameP + FileH->NameAddress ;
		if( PackNum != ((u16 *)NameData)[0] || Parity != ((u16 *)NameData)[1] ) continue ;

		// 文字列チェック
		NameData += 4 ;
		for( j = 0, k = 0 ; j < PackNum ; j ++, k += 4 )
			if( *((u32 *)&PathData[k]) != *((u32 *)&NameData[k]) ) break ;

		// 適合したディレクトリがあったらここで終了
		if( PackNum == j ) break ;
	}

	// 無かったらエラー
	if( i == Num ) return -1 ;

	// 在ったらカレントディレクトリを変更
	this->CurrentDirectory = ( DARC_DIRECTORY * )( this->DirP + FileH->DataAddress ) ;

	// 正常終了
	return 0 ;
}

// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
int DXArchive::ChangeCurrentDir( const TCHAR *DirPath )
{
	return ChangeCurrentDirectoryBase( DirPath, true ) ;
}

// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
int	DXArchive::ChangeCurrentDirectoryBase( const TCHAR *DirectoryPath, bool ErrorIsDirectoryReset, SEARCHDATA *LastSearchData )
{
	DARC_DIRECTORY *OldDir ;
	SEARCHDATA SearchData ;

	// ここに留まるパスだったら無視
	if( _tcscmp( DirectoryPath, TEXT(".") ) == 0 ) return 0 ;

	// 『\』だけの場合はルートディレクトリに戻る
	if( _tcscmp( DirectoryPath, TEXT("\\") ) == 0 )
	{
		this->CurrentDirectory = ( DARC_DIRECTORY * )this->DirP ;
		return 0 ;
	}

	// 下に一つ下がるパスだったら処理を分岐
	if( _tcscmp( DirectoryPath, TEXT("..") ) == 0 )
	{
		// ルートディレクトリに居たらエラー
		if( this->CurrentDirectory->ParentDirectoryAddress == 0xffffffffffffffff ) return -1 ;
		
		// 親ディレクトリがあったらそちらに移る
		this->CurrentDirectory = ( DARC_DIRECTORY * )( this->DirP + this->CurrentDirectory->ParentDirectoryAddress ) ;
		return 0 ;
	}

	// それ以外の場合は指定の名前のディレクトリを探す
	
	// 変更以前のディレクトリを保存しておく
	OldDir = this->CurrentDirectory ;

	// パス中に『\』があるかどうかで処理を分岐
	if( _tcschr( DirectoryPath, '\\' ) == NULL )
	{
		// ファイル名を検索専用の形式に変換する
		ConvSearchData( &SearchData, DirectoryPath, NULL ) ;

		// ディレクトリを変更
		if( ChangeCurrentDirectoryFast( &SearchData ) < 0 ) goto ERR ;

/*		// \ が無い場合は、同名のディレクトリを探す
		FileH = ( DARC_FILEHEAD * )( this->FileP + this->CurrentDirectory->FileHeadAddress ) ;
		for( i = 0 ;
			 i < (s32)this->CurrentDirectory->FileHeadNum &&
			 StrICmp( ( char * )( this->NameP + FileH->NameAddress ), DirectoryPath ) != 0 ;
			 i ++, FileH ++ ){}
*/
	}
	else
	{
		// \ がある場合は繋がったディレクトリを一つづつ変更してゆく
	
		int Point, StrLength ;

		Point = 0 ;
		// ループ
		for(;;)
		{
			// 文字列を取得する
			ConvSearchData( &SearchData, &DirectoryPath[Point], &StrLength ) ;
			Point += StrLength ;
/*			StrPoint = 0 ;
			while( DirectoryPath[Point] != '\0' && DirectoryPath[Point] != '\\' )
			{
				if( CheckMultiByteChar( &DirectoryPath[Point] ) == TRUE )
				{
					*((u16 *)&String[StrPoint]) = *((u16 *)&DirectoryPath[Point]) ;
					StrPoint += 2 ;
					Point    += 2 ;
				}
				else
				{
					String[StrPoint] = DirectoryPath[Point] ;
					StrPoint ++ ;
					Point    ++ ;
				}
			}
			String[StrPoint] = '\0' ;
*/
			// もし初っ端が \ だった場合はルートディレクトリに落とす
			if( StrLength == 0 && DirectoryPath[Point] == '\\' )
			{
				this->ChangeCurrentDirectoryBase(TEXT("\\"), false ) ;
			}
			else
			{
				// それ以外の場合は普通にディレクトリ変更
				if( this->ChangeCurrentDirectoryFast( &SearchData ) < 0 )
				{
					// エラーが起きて、更にエラーが起きた時に元のディレクトリに戻せの
					// フラグが立っている場合は元のディレクトリに戻す
					if( ErrorIsDirectoryReset == true ) this->CurrentDirectory = OldDir ;

					// エラー終了
					goto ERR ;
				}
			}

			// もし終端文字で終了した場合はループから抜ける
			// 又はあと \ しかない場合もループから抜ける
			if( DirectoryPath[Point] == '\0' ||
				( DirectoryPath[Point] == '\\' && DirectoryPath[Point+1] == '\0' ) ) break ;
			Point ++ ;
		}
	}

	if( LastSearchData != NULL )
	{
		memcpy( LastSearchData->FileName, SearchData.FileName, SearchData.PackNum * 4 ) ;
		LastSearchData->Parity  = SearchData.Parity ;
		LastSearchData->PackNum = SearchData.PackNum ;
	}

	// 正常終了
	return 0 ;

ERR:
	if( LastSearchData != NULL )
	{
		memcpy( LastSearchData->FileName, SearchData.FileName, SearchData.PackNum * 4 ) ;
		LastSearchData->Parity  = SearchData.Parity ;
		LastSearchData->PackNum = SearchData.PackNum ;
	}

	// エラー終了
	return -1 ;
}
		
// アーカイブ内のカレントディレクトリパスを取得する
int DXArchive::GetCurrentDir(TCHAR *DirPathBuffer, int BufferLength )
{
	TCHAR DirPath[MAX_PATH] ;
	DARC_DIRECTORY *Dir[200], *DirTempP ;
	int Depth, i ;

	// ルートディレクトリに着くまで検索する
	Depth = 0 ;
	DirTempP = this->CurrentDirectory ;
	while( DirTempP->DirectoryAddress != 0xffffffffffffffff && DirTempP->DirectoryAddress != 0 )
	{
		Dir[Depth] = DirTempP ;
		DirTempP = ( DARC_DIRECTORY * )( this->DirP + DirTempP->ParentDirectoryAddress ) ;
		Depth ++ ;
	}
	
	// パス名を連結する
	DirPath[0] = '\0' ;
	for( i = Depth - 1 ; i >= 0 ; i -- )
	{
		_tcscat( DirPath, TEXT("\\") ) ;
		_tcscat( DirPath, (TCHAR * )( this->NameP + ( ( DARC_FILEHEAD * )( this->FileP + Dir[i]->DirectoryAddress ) )->NameAddress ) ) ;
	}

	// バッファの長さが０か、長さが足りないときはディレクトリ名の長さを返す
	if( BufferLength == 0 || BufferLength < (s32)_tcslen( DirPath ) )
	{
		return ( int )( _tcslen( DirPath ) + 1 ) ;
	}
	else
	{
		// ディレクトリ名をバッファに転送する
		_tcscpy( DirPathBuffer, DirPath ) ;
	}

	// 終了
	return 0 ;
}



// アーカイブファイル中の指定のファイルをメモリに読み込む
s64 DXArchive::LoadFileToMem( const TCHAR *FilePath, void *Buffer, u64 BufferLength )
{
	DARC_FILEHEAD *FileH ;
	char KeyStringBuffer[ DXA_KEY_STRING_MAXLENGTH ] ;
	size_t KeyStringBufferBytes ;
	unsigned char lKey[ DXA_KEY_BYTES ] ;
	DARC_DIRECTORY *Directory ;
	void *HuffDataBuffer = NULL ;

	// 指定のファイルの情報を得る
	FileH = this->GetFileInfo( FilePath, &Directory ) ;
	if( FileH == NULL ) return -1 ;

	// ファイルサイズが足りているか調べる、足りていないか、バッファ、又はサイズが０だったらサイズを返す
	if( BufferLength < FileH->DataSize || BufferLength == 0 || Buffer == NULL )
	{
		return ( s64 )FileH->DataSize ;
	}

	// 足りている場合はバッファーに読み込む

	// ファイルが圧縮されているかどうかで処理を分岐
	if( FileH->PressDataSize != 0xffffffffffffffff )
	{
		// 圧縮されている場合

		// ハフマン圧縮されているかどうかで処理を分岐
		if( FileH->HuffPressDataSize != 0xffffffffffffffff )
		{
			// メモリ上に読み込んでいるかどうかで処理を分岐
			if( MemoryOpenFlag == true )
			{
				// ハフマン圧縮を解凍したデータを格納するメモリ領域の確保
				HuffDataBuffer = malloc( ( size_t )FileH->PressDataSize ) ;
				if( HuffDataBuffer == NULL ) return -1 ;

				// ハフマン圧縮データを解凍
				Huffman_Decode( (u8 *)this->fp + this->Head.DataStartAddress + FileH->DataAddress, HuffDataBuffer ) ;

				// メモリ上の圧縮データを解凍する
				Decode( HuffDataBuffer, Buffer ) ;

				// メモリを解放
				free( HuffDataBuffer ) ;
			}
			else
			{
				void *temp ;

				// 圧縮データをメモリに読み込んでから解凍する

				// 圧縮データが収まるメモリ領域の確保
				temp = malloc( ( size_t )( FileH->PressDataSize + FileH->HuffPressDataSize ) ) ;

				// 圧縮データの読み込み
				_fseeki64( this->fp, this->Head.DataStartAddress + FileH->DataAddress, SEEK_SET ) ;

				// ファイル個別の鍵を作成
				if( this->NoKey == false )
				{
					KeyStringBufferBytes = CreateKeyFileString( ( int )this->Head.CharCodeFormat, this->KeyString, this->KeyStringBytes, Directory, FileH, this->FileP, this->DirP, this->NameP, ( BYTE * )KeyStringBuffer ) ;
					KeyCreate( KeyStringBuffer, KeyStringBufferBytes, lKey ) ;
				}

				// 暗号化解除読み込み
				KeyConvFileRead( temp, FileH->HuffPressDataSize, this->fp, this->NoKey ? NULL : lKey, FileH->DataSize ) ;

				// ハフマン圧縮データを解凍
				Huffman_Decode( temp, ( u8 * )temp + FileH->HuffPressDataSize ) ;
			
				// 解凍
				Decode( ( u8 * )temp + FileH->HuffPressDataSize, Buffer ) ;
			
				// メモリの解放
				free( temp ) ;
			}
		}
		else
		{
			// メモリ上に読み込んでいるかどうかで処理を分岐
			if( MemoryOpenFlag == true )
			{
				// メモリ上の圧縮データを解凍する
				Decode( (u8 *)this->fp + this->Head.DataStartAddress + FileH->DataAddress, Buffer ) ;
			}
			else
			{
				void *temp ;

				// 圧縮データをメモリに読み込んでから解凍する

				// 圧縮データが収まるメモリ領域の確保
				temp = malloc( ( size_t )FileH->PressDataSize ) ;

				// 圧縮データの読み込み
				_fseeki64( this->fp, this->Head.DataStartAddress + FileH->DataAddress, SEEK_SET ) ;

				// ファイル個別の鍵を作成
				if( this->NoKey == false )
				{
					KeyStringBufferBytes = CreateKeyFileString( ( int )this->Head.CharCodeFormat, this->KeyString, this->KeyStringBytes, Directory, FileH, this->FileP, this->DirP, this->NameP, ( BYTE * )KeyStringBuffer ) ;
					KeyCreate( KeyStringBuffer, KeyStringBufferBytes, lKey ) ;
				}

				KeyConvFileRead( temp, FileH->PressDataSize, this->fp, this->NoKey ? NULL : lKey, FileH->DataSize ) ;
			
				// 解凍
				Decode( temp, Buffer ) ;
			
				// メモリの解放
				free( temp ) ;
			}
		}
	}
	else
	{
		// 圧縮されていない場合

		// ハフマン圧縮されているかどうかで処理を分岐
		if( FileH->HuffPressDataSize != 0xffffffffffffffff )
		{
			// メモリ上に読み込んでいるかどうかで処理を分岐
			if( MemoryOpenFlag == true )
			{
				// ハフマン圧縮を解凍したデータを格納するメモリ領域の確保
				HuffDataBuffer = malloc( ( size_t )FileH->PressDataSize ) ;
				if( HuffDataBuffer == NULL ) return -1 ;

				// ハフマン圧縮データを解凍
				Huffman_Decode( (u8 *)this->fp + this->Head.DataStartAddress + FileH->DataAddress, HuffDataBuffer ) ;

				// コピー
				memcpy( Buffer, HuffDataBuffer, ( size_t )FileH->DataSize ) ;

				// メモリを解放
				free( HuffDataBuffer ) ;
			}
			else
			{
				void *temp ;

				// 圧縮データをメモリに読み込んでから解凍する

				// ハフマン圧縮データが収まるメモリ領域の確保
				temp = malloc( ( size_t )FileH->HuffPressDataSize ) ;

				// 圧縮データの読み込み
				_fseeki64( this->fp, this->Head.DataStartAddress + FileH->DataAddress, SEEK_SET ) ;

				// ファイル個別の鍵を作成
				if( this->NoKey == false )
				{
					KeyStringBufferBytes = CreateKeyFileString( ( int )this->Head.CharCodeFormat, this->KeyString, this->KeyStringBytes, Directory, FileH, this->FileP, this->DirP, this->NameP, ( BYTE * )KeyStringBuffer ) ;
					KeyCreate( KeyStringBuffer, KeyStringBufferBytes, lKey ) ;
				}

				// 暗号化解除読み込み
				KeyConvFileRead( temp, FileH->HuffPressDataSize, this->fp, this->NoKey ? NULL : lKey, FileH->DataSize ) ;

				// ハフマン圧縮データを解凍
				Huffman_Decode( temp, Buffer ) ;
			
				// メモリの解放
				free( temp ) ;
			}
		}
		else
		{
			// メモリ上に読み込んでいるかどうかで処理を分岐
			if( MemoryOpenFlag == true )
			{
				// コピー
				memcpy( Buffer, (u8 *)this->fp + this->Head.DataStartAddress + FileH->DataAddress, ( size_t )FileH->DataSize ) ;
			}
			else
			{
				// ファイルポインタを移動
				_fseeki64( this->fp, this->Head.DataStartAddress + FileH->DataAddress, SEEK_SET ) ;

				// 読み込み

				// ファイル個別の鍵を作成
				if( this->NoKey == false )
				{
					KeyStringBufferBytes = CreateKeyFileString( ( int )this->Head.CharCodeFormat, this->KeyString, this->KeyStringBytes, Directory, FileH, this->FileP, this->DirP, this->NameP, ( BYTE * )KeyStringBuffer ) ;
					KeyCreate( KeyStringBuffer, KeyStringBufferBytes, lKey ) ;
				}

				KeyConvFileRead( Buffer, FileH->DataSize, this->fp, this->NoKey ? NULL : lKey, FileH->DataSize ) ;
			}
		}
	}
	
	// 終了
	return 0 ;
}

// アーカイブファイル中の指定のファイルをサイズを取得する
s64 DXArchive::GetFileSize( const TCHAR *FilePath )
{
	// ファイルサイズを返す
	return this->LoadFileToMem( FilePath, NULL, 0 ) ;
}

// アーカイブファイルをメモリに読み込んだ場合のファイルイメージが格納されている先頭アドレスを取得する( メモリから開いている場合のみ有効 )
void *DXArchive::GetFileImage( void )
{
	// メモリイメージから開いていなかったらエラー
	if( MemoryOpenFlag == false ) return NULL ;

	// 先頭アドレスを返す
	return this->fp ;
}

// アーカイブファイル中の指定のファイルのファイル内の位置とファイルの大きさを得る( -1:エラー )
int DXArchive::GetFileInfo( const TCHAR *FilePath, u64 *PositionP, u64 *SizeP )
{
	DARC_FILEHEAD *FileH ;

	// 指定のファイルの情報を得る
	FileH = this->GetFileInfo( FilePath ) ;
	if( FileH == NULL ) return -1 ;

	// ファイルのデータがある位置とファイルサイズを保存する
	if( PositionP != NULL ) *PositionP = this->Head.DataStartAddress + FileH->DataAddress ;
	if( SizeP     != NULL ) *SizeP     = FileH->DataSize ;

	// 成功終了
	return 0 ;
}

// アーカイブファイル中の指定のファイルを、クラス内のバッファに読み込む
void *DXArchive::LoadFileToCache( const TCHAR *FilePath )
{
	s64 FileSize ;

	// ファイルサイズを取得する
	FileSize = this->GetFileSize( FilePath ) ;

	// ファイルが無かったらエラー
	if( FileSize < 0 ) return NULL ;

	// 確保しているキャッシュバッファのサイズよりも大きい場合はバッファを再確保する
	if( FileSize > ( s64 )this->CacheBufferSize )
	{
		// キャッシュバッファのクリア
		this->ClearCache() ;

		// キャッシュバッファの再確保
		this->CacheBuffer = malloc( ( size_t )FileSize ) ;

		// 確保に失敗した場合は NULL を返す
		if( this->CacheBuffer == NULL ) return NULL ;

		// キャッシュバッファのサイズを保存
		this->CacheBufferSize = FileSize ;
	}

	// ファイルをメモリに読み込む
	this->LoadFileToMem( FilePath, this->CacheBuffer, FileSize ) ;

	// キャッシュバッファのアドレスを返す
	return this->CacheBuffer ;
}

// キャッシュバッファを開放する
int DXArchive::ClearCache( void )
{
	// メモリが確保されていたら解放する
	if( this->CacheBuffer != NULL )
	{
		free( this->CacheBuffer ) ;
		this->CacheBuffer = NULL ;
		this->CacheBufferSize = 0 ;
	}

	// 終了
	return 0 ;
}

	
// アーカイブファイル中の指定のファイルを開き、ファイルアクセス用オブジェクトを作成する
DXArchiveFile *DXArchive::OpenFile( const TCHAR *FilePath )
{
	DARC_FILEHEAD *FileH ;
	DARC_DIRECTORY *Directory ;
	DXArchiveFile *CDArc = NULL ;

	// メモリから開いている場合は無効
	if( MemoryOpenFlag == true ) return NULL ;

	// 指定のファイルの情報を得る
	FileH = this->GetFileInfo( FilePath, &Directory ) ;
	if( FileH == NULL ) return NULL ;

	// 新しく DXArchiveFile クラスを作成する
	CDArc = new DXArchiveFile( FileH, Directory, this ) ;
	
	// DXArchiveFile クラスのポインタを返す
	return CDArc ;
}













// コンストラクタ
DXArchiveFile::DXArchiveFile( DARC_FILEHEAD *FileHead, DARC_DIRECTORY *Directory, DXArchive *Archive )
{
	void *temp ;

	this->FileData  = FileHead ;
	this->Archive   = Archive ;
	this->EOFFlag   = FALSE ;
	this->FilePoint = 0 ;
	this->DataBuffer = NULL ;

	// 鍵を作成する
	if( this->Archive->GetNoKey() == false )
	{
		char KeyStringBuffer[ DXA_KEY_STRING_MAXLENGTH ] ;
		size_t KeyStringBufferBytes ;

		KeyStringBufferBytes = DXArchive::CreateKeyFileString(
			( int )this->Archive->GetHeader()->CharCodeFormat,
			this->Archive->GetKeyString(),
			this->Archive->GetKeyStringBytes(),
			Directory,
			FileHead,
			this->Archive->GetFileHeadTable(),
			this->Archive->GetDirectoryTable(),
			this->Archive->GetNameTable(),
			( BYTE * )KeyStringBuffer
		) ;
		DXArchive::KeyCreate( KeyStringBuffer, KeyStringBufferBytes, Key ) ;
	}

	// ファイルが圧縮されている場合は解凍データが収まるメモリ領域の確保
	if( FileHead->PressDataSize     != 0xffffffffffffffff ||
		FileHead->HuffPressDataSize != 0xffffffffffffffff )
	{
		// 解凍データが収まるメモリ領域の確保
		this->DataBuffer = malloc( ( size_t )FileHead->DataSize ) ;
	}
	
	// ファイルが圧縮されている場合はここで読み込んで解凍してしまう
	if( FileHead->PressDataSize != 0xffffffffffffffff )
	{
		// ハフマン圧縮もされているかどうかで処理を分岐
		if( FileHead->HuffPressDataSize != 0xffffffffffffffff )
		{
			// 圧縮データが収まるメモリ領域の確保
			temp = malloc( ( size_t )( FileHead->PressDataSize + FileHead->HuffPressDataSize ) ) ;

			// 圧縮データの読み込み
			_fseeki64( this->Archive->GetFilePointer(), this->Archive->GetHeader()->DataStartAddress + FileHead->DataAddress, SEEK_SET ) ;

			// 鍵解除読み込み
			DXArchive::KeyConvFileRead( temp, FileHead->HuffPressDataSize, this->Archive->GetFilePointer(), this->Archive->GetNoKey() ? NULL : Key, FileHead->DataSize ) ;

			// ハフマン圧縮データを解凍
			Huffman_Decode( temp, ( u8 * )temp + FileHead->HuffPressDataSize ) ;
		
			// 解凍
			DXArchive::Decode( ( u8 * )temp + FileHead->HuffPressDataSize, this->DataBuffer ) ;

			// メモリの解放
			free( temp ) ;
		}
		else
		{
			// 圧縮データが収まるメモリ領域の確保
			temp = malloc( ( size_t )FileHead->PressDataSize ) ;

			// 圧縮データの読み込み
			_fseeki64( this->Archive->GetFilePointer(), this->Archive->GetHeader()->DataStartAddress + FileHead->DataAddress, SEEK_SET ) ;
			DXArchive::KeyConvFileRead( temp, FileHead->PressDataSize, this->Archive->GetFilePointer(), this->Archive->GetNoKey() ? NULL : Key, FileHead->DataSize ) ;
		
			// 解凍
			DXArchive::Decode( temp, this->DataBuffer ) ;

			// メモリの解放
			free( temp ) ;
		}
	}
	else
	// ハフマン圧縮だけされているかどうかで処理を分岐
	if( FileHead->HuffPressDataSize != 0xffffffffffffffff )
	{
		// 圧縮データが収まるメモリ領域の確保
		temp = malloc( ( size_t )FileHead->HuffPressDataSize ) ;

		// 圧縮データの読み込み
		_fseeki64( this->Archive->GetFilePointer(), this->Archive->GetHeader()->DataStartAddress + FileHead->DataAddress, SEEK_SET ) ;

		// 暗号化解除読み込み
		DXArchive::KeyConvFileRead( temp, FileHead->HuffPressDataSize, this->Archive->GetFilePointer(), this->Archive->GetNoKey() ? NULL : Key, FileHead->DataSize ) ;

		// ハフマン圧縮データを解凍
		Huffman_Decode( temp, this->DataBuffer ) ;

		// メモリの解放
		free( temp ) ;
	}
}

// デストラクタ
DXArchiveFile::~DXArchiveFile()
{
	// メモリの解放
	if( this->DataBuffer != NULL )
	{
		free( this->DataBuffer ) ;
		this->DataBuffer = NULL ;
	}
}

// ファイルの内容を読み込む
s64 DXArchiveFile::Read( void *Buffer, s64 ReadLength )
{
	s64 ReadSize ;

	// EOF フラグが立っていたら０を返す
	if( this->EOFFlag == TRUE ) return 0 ;
	
	// アーカイブファイルポインタと、仮想ファイルポインタが一致しているか調べる
	// 一致していなかったらアーカイブファイルポインタを移動する
	if( this->DataBuffer == NULL && _ftelli64( this->Archive->GetFilePointer() ) != (s32)( this->FileData->DataAddress + this->Archive->GetHeader()->DataStartAddress + this->FilePoint ) )
	{
		_fseeki64( this->Archive->GetFilePointer(), this->FileData->DataAddress + this->Archive->GetHeader()->DataStartAddress + this->FilePoint, SEEK_SET ) ;
	}
	
	// EOF 検出
	if( this->FileData->DataSize == this->FilePoint )
	{
		this->EOFFlag = TRUE ;
		return 0 ;
	}
	
	// データを読み込む量を設定する
	ReadSize = ReadLength < (s64)( this->FileData->DataSize - this->FilePoint ) ? ReadLength : this->FileData->DataSize - this->FilePoint ;
	
	// データを読み込む
	if( this->DataBuffer == NULL )
	{
		DXArchive::KeyConvFileRead( Buffer, ReadSize, this->Archive->GetFilePointer(), this->Archive->GetNoKey() ? NULL : Key, this->FileData->DataSize + this->FilePoint ) ;
	}
	else
	{
		memcpy( Buffer, (u8 *)this->DataBuffer + this->FilePoint, ( size_t )ReadSize ) ;
	}
	
	// EOF フラグを倒す
	this->EOFFlag = FALSE ;

	// 読み込んだ分だけファイルポインタを移動する
	this->FilePoint += ReadSize ;
	
	// 読み込んだ容量を返す
	return ReadSize ;
}
	
// ファイルポインタを変更する
s64 DXArchiveFile::Seek( s64 SeekPoint, s64 SeekMode )
{
	// シークタイプによって処理を分岐
	switch( SeekMode )
	{
	case SEEK_SET : break ;		
	case SEEK_CUR : SeekPoint += this->FilePoint ; break ;
	case SEEK_END :	SeekPoint = this->FileData->DataSize + SeekPoint ; break ;
	}
	
	// 補正
	if( SeekPoint > (s64)this->FileData->DataSize ) SeekPoint = this->FileData->DataSize ;
	if( SeekPoint < 0 ) SeekPoint = 0 ;
	
	// セット
	this->FilePoint = SeekPoint ;
	
	// EOFフラグを倒す
	this->EOFFlag = FALSE ;
	
	// 終了
	return 0 ;
}

// 現在のファイルポインタを得る
s64 DXArchiveFile::Tell( void )
{
	return this->FilePoint ;
}

// ファイルの終端に来ているか、のフラグを得る
s64 DXArchiveFile::Eof( void )
{
	return this->EOFFlag ;
}

// ファイルのサイズを取得する
s64 DXArchiveFile::Size( void )
{
	return this->FileData->DataSize ;
}





WCHAR sjis2utf8(const char* sjis)
{
	LPCCH pSJIS = (LPCCH)sjis;

	int sjisSize = 2;
	WCHAR *pUTF8 = new WCHAR[sjisSize];
	MultiByteToWideChar(932, 0, (LPCCH)pSJIS, -1, pUTF8, 2);
	WCHAR utf8 = pUTF8[0];
	delete[] pUTF8;
	return utf8;
}
