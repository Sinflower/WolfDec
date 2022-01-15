// ============================================================================
//
//		ファイル関係のライブラリ
//
//		Creator			: 山田　巧
//		Creation Data	: 09/17/2004
//
// ============================================================================

// include --------------------------------------
#include <string.h>
#include <stdio.h>
#include <mbstring.h>
#include <windows.h>
#include "FileLib.h"

// define ---------------------------------------

#define PATH_LENGTH		(2048)			// パスバッファの長さ
#define TEXTCHECKSIZE	(0x2000)		// テキストデータか調べるサイズ

// data type ------------------------------------

// data -----------------------------------------

unsigned char BinToChar128Table[ 128 ] =
{
	0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F, 0x40,
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x60,
	0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,
	0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0xA1, 0xA2,
	0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30,
	0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB0, 0xB1, 0xB2,
	0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC0, 0xC1, 0xC2,
} ;

unsigned char Char128ToBinTable[ 256 ] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e,
	0x5f, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
	0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e,
	0x1f, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e,
	0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e,
	0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x4e, 0x4f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c,
	0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c,
	0x7d, 0x7e, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
} ;

unsigned char BinToBase64Table[ 64 ] =
{
	0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
	0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A,	0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
	0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,	0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
	0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x2F
} ;

unsigned char Base64ToBinTable[ 256 ] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x3F,
	0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
	0x0F, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
	0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F, 0x30, 0x31, 0x32, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
} ;

// function proto type --------------------------

// ファイルオブジェクト列挙用関数( -1:エラー  0以上:ファイルの数 )
// flist は NULL でも良い
static int __EnumObject( TCHAR *Path, TCHAR *CurrentPath, FILE_INFOLIST *FileList,
							int OmitDirectory, int SubDirectory,
							TCHAR **OmitName, TCHAR **OmitExName, TCHAR **ValidExName, int *TotalFileNumCounter, int TotalFileNum,
						void ( *EnumFileCallback )( int Phase, int NowFileNum, int TotalFileNum, const TCHAR *FileName, const TCHAR *RelDirPath, const TCHAR *AbsDirPath ) ) ;

// function code --------------------------------

// ファイルオブジェクト列挙用関数( -1:エラー  0以上:ファイルの数 )
// flist は NULL でも良い
static int __EnumObject( TCHAR *Path, TCHAR *CurrentPath, FILE_INFOLIST *FileList,
							int OmitDirectory, int SubDirectory,
							TCHAR **OmitName, TCHAR **OmitExName, TCHAR **ValidExName, int *TotalFileNumCounter, int TotalFileNum,
						void ( *EnumFileCallback )( int Phase, int NowFileNum, int TotalFileNum, const TCHAR *FileName, const TCHAR *RelDirPath, const TCHAR *AbsDirPath ) )
{
	WIN32_FIND_DATA FindData ;
	HANDLE FindHandle = INVALID_HANDLE_VALUE ;
	int FileNum, IsDirectory ;
	TCHAR RelDir[PATH_LENGTH] ;
	TCHAR *AbsDir ;
	int RelDirLen, AbsDirLen, StartNum ;

	// ディレクトリパスの作成
	AbsDir = Path ;
	AbsDirLen = ( int )_tcslen( AbsDir ) ;
	_tcscpy( RelDir, AbsDir + _tcslen( CurrentPath ) ) ;
	RelDirLen = ( int )_tcslen( RelDir ) ;

	// ファイルの列挙開始
	{
		TCHAR temp[PATH_LENGTH] ;

		_tcscpy( temp, AbsDir ) ;
		_tcscat( temp, TEXT( "*" ) ) ;
		FindHandle = FindFirstFile( temp, &FindData ) ;
		if( FindHandle == INVALID_HANDLE_VALUE )
			return -1 ;
	}

	// ファイルの情報を列挙する
	FileNum = 0 ;
	if( FileList != NULL ) StartNum = FileList->Num ;
	do
	{
		// 上のフォルダに戻ったりするためのパスは無視する
		if( _tcscmp( FindData.cFileName, TEXT( "." ) ) == 0 || _tcscmp( FindData.cFileName, TEXT( ".." ) ) == 0 ) continue ;

		// ディレクトリかどうかを得る
		IsDirectory = ( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ? 1 : 0 ;

		// 有効拡張子指定があり、有効拡張子ではない場合は無視する
		if( ValidExName != NULL && IsDirectory == 0 )
		{
			int i ;
			const TCHAR *name ;

			name = _tcschr( FindData.cFileName, TEXT( '.' ) ) ;
			if( name == NULL ) name = TEXT( "" ) ;
			else name ++ ;

			for( i = 0 ; ValidExName[i] != NULL && _tcsicmp( name, ValidExName[i] ) != 0 ; i ++ ){}
			if( ValidExName[i] == NULL ) continue ;
		}

		// 除外指定がある文字列は無視する
		if( OmitName != NULL )
		{
			int i ;

			for( i = 0 ; OmitName[i] != NULL && _tcscmp( FindData.cFileName, OmitName[i] ) != 0 ; i ++ ){}
			if( OmitName[i] != NULL ) continue ;
		}

		// 除外指定がある拡張子を持つファイルは無視する
		if( OmitExName != NULL && IsDirectory == 0 )
		{
			int i ;
			const TCHAR *name ;

			name = _tcschr( FindData.cFileName, TEXT( '.' ) ) ;
			if( name == NULL ) name = TEXT( "" ) ;
			else name ++ ;

			for( i = 0 ; OmitExName[i] != NULL && _tcscmp( name, OmitExName[i] ) != 0 ; i ++ ){}
			if( OmitExName[i] != NULL ) continue ;
		}

		// ディレクトリの場合でサブディレクトリも調べる指定がある場合はサブディレクトリ内も列挙する
		if( IsDirectory == 1 && SubDirectory == 1 )
		{
			int res ;
			TCHAR tempAbs[PATH_LENGTH], tempRel[PATH_LENGTH] ;

			// 絶対パスの作成
			_tcscpy( tempAbs, AbsDir ) ;
			_tcscat( tempAbs, FindData.cFileName ) ;
			_tcscat( tempAbs, TEXT( "\\" ) ) ;

			// 相対パスの作成
			_tcscpy( tempRel, RelDir ) ;
			_tcscat( tempRel, FindData.cFileName ) ;
			_tcscat( tempRel, TEXT( "\\" ) ) ;
			
			// 列挙
			res = __EnumObject( tempAbs, CurrentPath, FileList, OmitDirectory, SubDirectory, OmitName, OmitExName, ValidExName, TotalFileNumCounter, TotalFileNum, EnumFileCallback ) ;
			if( res < 0 )
				goto ERR ;
			FileNum += res ;
		}

		if( EnumFileCallback != 0 )
		{
			EnumFileCallback( FileList == NULL ? 0 : 1, *TotalFileNumCounter, TotalFileNum, FindData.cFileName, RelDir, AbsDir ) ;
		}

		// データを格納することが出来る場合はデータを格納する
		if( FileList != NULL &&
			( IsDirectory == 0 || ( IsDirectory == 1 && OmitDirectory == 0 ) ) )
		{
			FILE_INFO *info ;
			int FileNameLen ;

			info = &FileList->List[FileList->Num] ;
			memset( info, 0, sizeof( FILE_INFO ) ) ;

			// 時刻を保存
			info->Date.Create		= ( ( ( LONGLONG )FindData.ftCreationTime	.dwHighDateTime ) << 32 ) + FindData.ftCreationTime		.dwLowDateTime ;
			info->Date.LastAccess	= ( ( ( LONGLONG )FindData.ftLastAccessTime	.dwHighDateTime ) << 32 ) + FindData.ftLastAccessTime	.dwLowDateTime ;
			info->Date.LastWrite	= ( ( ( LONGLONG )FindData.ftLastWriteTime	.dwHighDateTime ) << 32 ) + FindData.ftLastWriteTime	.dwLowDateTime ;

			info->Size			= ( u64 )FindData.nFileSizeLow | ( ( ( u64 )FindData.nFileSizeHigh ) << 32 ) ;		// サイズを保存
			info->Attributes	= FindData.dwFileAttributes ;	// 属性を保存
			info->IsDirectory	= (u8)IsDirectory ;

			// パス系の保存
			{
				// パス系を保存するメモリ領域の確保
				FileNameLen = ( int )_tcslen( FindData.cFileName ) ;
				info->FileName = ( TCHAR * )malloc( sizeof( TCHAR ) * ( ( FileNameLen + 1 ) + ( AbsDirLen + 1 ) + ( RelDirLen + 1 ) ) ) ;
				if( info->FileName == NULL )
					goto ERR ;
				info->RelDirectoryPath = info->FileName + FileNameLen + 1 ;
				info->AbsDirectoryPath = info->RelDirectoryPath + RelDirLen + 1 ;

				// コピー
				_tcscpy( info->FileName, FindData.cFileName ) ;
				_tcscpy( info->RelDirectoryPath, RelDir ) ;
				_tcscpy( info->AbsDirectoryPath, AbsDir ) ;

				// ハッシュ値を計算
				info->FileNameHash = FileLib_HashCRC32( info->FileName, FileNameLen * sizeof( TCHAR ) ) ;
			}

			// ファイルの数を増やす
			FileList->Num ++ ;
		}

		// ファイルの数を増やす
		FileNum ++ ;
		*TotalFileNumCounter += 1 ;
	}
	while( FindNextFile( FindHandle, &FindData ) != 0 ) ;

	// 列挙終了
	FindClose( FindHandle ) ;
	FindHandle = INVALID_HANDLE_VALUE ;

	// 終了
	return FileNum ;

	// エラー処理
ERR :
	if( FindHandle != INVALID_HANDLE_VALUE ) FindClose( FindHandle ) ;

	// 既に確保してしまったメモリの解放処理
	if( FileList != NULL )
	{
		int i ;

		for( i = StartNum ; i < FileList->Num ; i ++ )
			if( FileList->List[i].FileName != NULL ) free( FileList->List[i].FileName ) ;
	}

	// エラー終了
	return -1 ;
}

// データを簡易暗号化する関数
extern void EasyEncode( void *Data, unsigned int Size )
{
	if( Size == 0 ) return ;

#ifdef _WIN64
	unsigned int i ;
	BYTE *p = ( BYTE * )Data ;
	for( i = 0 ; i < Size ; i ++ )
	{
		p[ i ] = ( ~p[ i ] << 4 ) | ( ~p[ i ] >> 4 ) ;
	}
#else
	unsigned int tempSize, tempData ;

	tempData = ( unsigned int )Data ;
	tempSize = ( unsigned int )Size ;
	__asm
	{
		// 準備
		mov edi, tempData
		mov ecx, tempSize

		// 値を反転、回転4ビットして書き戻す
LOOP1:
		mov al, [edi]
		not al
		rol al, 4
		mov [edi], al
		inc edi

		// ループ処理
		dec ecx
		jnz LOOP1
	} ;
#endif
}

// データを簡易暗号化してファイルに書き出す関数
extern void EasyEncodeFileWrite( void *Data, int Size, FILE *fp )
{
	EasyEncode( Data, Size ) ;
	fwrite( Data, Size, 1, fp ) ;
	EasyEncode( Data, Size ) ;
}

// データを簡易暗号化してファイルから読み込む関数
extern void EasyEncodeFileRead( void *Data, int Size, FILE *fp )
{
	fread( Data, Size, 1, fp ) ;
	EasyEncode( Data, Size ) ;
}

// バイナリデータを半角文字列に変換する( 戻り値:変換後のデータサイズ )
extern unsigned int BinToChar128( void *Src, unsigned int SrcSize, void *Dest )
{
	unsigned int DestSize ;

	DestSize  = SrcSize + ( SrcSize + 6 ) / 7 + 5 ;

	if( Dest != NULL )
	{
		unsigned int PackNum ;
		unsigned int ModNum ;
		unsigned char *DestP ;
		unsigned char *SrcP ;
		unsigned int i ;

		DestP = ( unsigned char * )Dest ;
		SrcP  = ( unsigned char * )&SrcSize ;

		DestP[ 0 ] = BinToChar128Table[                                 ( SrcP[ 0 ] >> 1 ) ] ;
		DestP[ 1 ] = BinToChar128Table[ ( ( SrcP[ 0 ] & 0x01 ) << 6 ) | ( SrcP[ 1 ] >> 2 ) ] ;
		DestP[ 2 ] = BinToChar128Table[ ( ( SrcP[ 1 ] & 0x03 ) << 5 ) | ( SrcP[ 2 ] >> 3 ) ] ;
		DestP[ 3 ] = BinToChar128Table[ ( ( SrcP[ 2 ] & 0x07 ) << 4 ) | ( SrcP[ 3 ] >> 4 ) ] ;
		DestP[ 4 ] = BinToChar128Table[ ( ( SrcP[ 3 ] & 0x0f ) << 3 )                      ] ;

		DestP += 5 ;

		PackNum = SrcSize / 7 ;
		ModNum  = SrcSize - PackNum * 7 ;
		SrcP  = ( unsigned char * )Src ;
		for( i = 0 ; i < PackNum ; i ++ )
		{
			DestP[ 0 ] = BinToChar128Table[                                 ( SrcP[ 0 ] >> 1 ) ] ;
			DestP[ 1 ] = BinToChar128Table[ ( ( SrcP[ 0 ] & 0x01 ) << 6 ) | ( SrcP[ 1 ] >> 2 ) ] ;
			DestP[ 2 ] = BinToChar128Table[ ( ( SrcP[ 1 ] & 0x03 ) << 5 ) | ( SrcP[ 2 ] >> 3 ) ] ;
			DestP[ 3 ] = BinToChar128Table[ ( ( SrcP[ 2 ] & 0x07 ) << 4 ) | ( SrcP[ 3 ] >> 4 ) ] ;
			DestP[ 4 ] = BinToChar128Table[ ( ( SrcP[ 3 ] & 0x0f ) << 3 ) | ( SrcP[ 4 ] >> 5 ) ] ;
			DestP[ 5 ] = BinToChar128Table[ ( ( SrcP[ 4 ] & 0x1f ) << 2 ) | ( SrcP[ 5 ] >> 6 ) ] ;
			DestP[ 6 ] = BinToChar128Table[ ( ( SrcP[ 5 ] & 0x3f ) << 1 ) | ( SrcP[ 6 ] >> 7 ) ] ;
			DestP[ 7 ] = BinToChar128Table[ ( ( SrcP[ 6 ] & 0x7f )      )                      ] ;

			DestP += 8 ;
			SrcP  += 7 ;
		}

		if( ModNum != 0 )
		{
			DestP[ 0 ] = BinToChar128Table[                                 ( SrcP[ 0 ] >> 1 ) ] ;
			if( ModNum == 1 )
			{
				DestP[ 1 ] = BinToChar128Table[ ( ( SrcP[ 0 ] & 0x01 ) << 6 ) ] ;
			}
			else
			{
				DestP[ 1 ] = BinToChar128Table[ ( ( SrcP[ 0 ] & 0x01 ) << 6 ) | ( SrcP[ 1 ] >> 2 ) ] ;
				if( ModNum == 2 )
				{
					DestP[ 2 ] = BinToChar128Table[ ( ( SrcP[ 1 ] & 0x03 ) << 5 ) ] ;
				}
				else
				{
					DestP[ 2 ] = BinToChar128Table[ ( ( SrcP[ 1 ] & 0x03 ) << 5 ) | ( SrcP[ 2 ] >> 3 ) ] ;
					if( ModNum == 3 )
					{
						DestP[ 3 ] = BinToChar128Table[ ( ( SrcP[ 2 ] & 0x07 ) << 4 ) ] ;
					}
					else
					{
						DestP[ 3 ] = BinToChar128Table[ ( ( SrcP[ 2 ] & 0x07 ) << 4 ) | ( SrcP[ 3 ] >> 4 ) ] ;
						if( ModNum == 4 )
						{
							DestP[ 4 ] = BinToChar128Table[ ( ( SrcP[ 3 ] & 0x0f ) << 3 ) ] ;
						}
						else
						{
							DestP[ 4 ] = BinToChar128Table[ ( ( SrcP[ 3 ] & 0x0f ) << 3 ) | ( SrcP[ 4 ] >> 5 ) ] ;
							if( ModNum == 5 )
							{
								DestP[ 5 ] = BinToChar128Table[ ( ( SrcP[ 4 ] & 0x1f ) << 2 ) ] ;
							}
							else
							{
								DestP[ 5 ] = BinToChar128Table[ ( ( SrcP[ 4 ] & 0x1f ) << 2 ) | ( SrcP[ 5 ] >> 6 ) ] ;
								if( ModNum == 6 )
								{
									DestP[ 6 ] = BinToChar128Table[ ( ( SrcP[ 5 ] & 0x3f ) << 1 ) ] ;
								}
								else
								{
									DestP[ 6 ] = BinToChar128Table[ ( ( SrcP[ 5 ] & 0x3f ) << 1 ) | ( SrcP[ 6 ] >> 7 ) ] ;
									DestP[ 7 ] = BinToChar128Table[ ( ( SrcP[ 6 ] & 0x7f )      )                      ] ;
								}
							}
						}
					}
				}
			}
		}
	}

	return DestSize ;
}

// 半角文字列をバイナリデータに変換する( 戻り値:変換後のデータサイズ )
extern unsigned int Char128ToBin( void *Src, void *Dest )
{
	unsigned int DestSize ;
	unsigned char *SrcP ;
	unsigned char *DestP ;

	SrcP    = ( unsigned char * )Src ;
	DestP   = ( unsigned char * )&DestSize ;

	DestP[ 0 ] = ( Char128ToBinTable[ SrcP[ 0 ] ] << 1 ) | ( Char128ToBinTable[ SrcP[ 1 ] ] >> 6 ) ;
	DestP[ 1 ] = ( Char128ToBinTable[ SrcP[ 1 ] ] << 2 ) | ( Char128ToBinTable[ SrcP[ 2 ] ] >> 5 ) ;
	DestP[ 2 ] = ( Char128ToBinTable[ SrcP[ 2 ] ] << 3 ) | ( Char128ToBinTable[ SrcP[ 3 ] ] >> 4 ) ;
	DestP[ 3 ] = ( Char128ToBinTable[ SrcP[ 3 ] ] << 4 ) | ( Char128ToBinTable[ SrcP[ 4 ] ] >> 3 ) ;

	SrcP += 5 ;

	if( Dest != NULL )
	{
		unsigned int PackNum ;
		unsigned int ModNum ;
		unsigned int i ;

		PackNum = DestSize / 7 ;
		ModNum  = DestSize - PackNum * 7 ;
		DestP = ( unsigned char * )Dest ;
		for( i = 0 ; i < PackNum ; i ++ )
		{
			DestP[ 0 ] = ( Char128ToBinTable[ SrcP[ 0 ] ] << 1 ) | ( Char128ToBinTable[ SrcP[ 1 ] ] >> 6 ) ;
			DestP[ 1 ] = ( Char128ToBinTable[ SrcP[ 1 ] ] << 2 ) | ( Char128ToBinTable[ SrcP[ 2 ] ] >> 5 ) ;
			DestP[ 2 ] = ( Char128ToBinTable[ SrcP[ 2 ] ] << 3 ) | ( Char128ToBinTable[ SrcP[ 3 ] ] >> 4 ) ;
			DestP[ 3 ] = ( Char128ToBinTable[ SrcP[ 3 ] ] << 4 ) | ( Char128ToBinTable[ SrcP[ 4 ] ] >> 3 ) ;
			DestP[ 4 ] = ( Char128ToBinTable[ SrcP[ 4 ] ] << 5 ) | ( Char128ToBinTable[ SrcP[ 5 ] ] >> 2 ) ;
			DestP[ 5 ] = ( Char128ToBinTable[ SrcP[ 5 ] ] << 6 ) | ( Char128ToBinTable[ SrcP[ 6 ] ] >> 1 ) ;
			DestP[ 6 ] = ( Char128ToBinTable[ SrcP[ 6 ] ] << 7 ) | ( Char128ToBinTable[ SrcP[ 7 ] ]      ) ;

			DestP += 7 ;
			SrcP  += 8 ;
		}

		if( ModNum != 0 )
		{
			DestP[ 0 ] = ( Char128ToBinTable[ SrcP[ 0 ] ] << 1 ) | ( Char128ToBinTable[ SrcP[ 1 ] ] >> 6 ) ;
			if( ModNum > 1 )
			{
				DestP[ 1 ] = ( Char128ToBinTable[ SrcP[ 1 ] ] << 2 ) | ( Char128ToBinTable[ SrcP[ 2 ] ] >> 5 ) ;
				if( ModNum > 2 )
				{
					DestP[ 2 ] = ( Char128ToBinTable[ SrcP[ 2 ] ] << 3 ) | ( Char128ToBinTable[ SrcP[ 3 ] ] >> 4 ) ;
					if( ModNum > 3 )
					{
							DestP[ 3 ] = ( Char128ToBinTable[ SrcP[ 3 ] ] << 4 ) | ( Char128ToBinTable[ SrcP[ 4 ] ] >> 3 ) ;
						if( ModNum > 4 )
						{
							DestP[ 4 ] = ( Char128ToBinTable[ SrcP[ 4 ] ] << 5 ) | ( Char128ToBinTable[ SrcP[ 5 ] ] >> 2 ) ;
							if( ModNum > 5 )
							{
								DestP[ 5 ] = ( Char128ToBinTable[ SrcP[ 5 ] ] << 6 ) | ( Char128ToBinTable[ SrcP[ 6 ] ] >> 1 ) ;
								if( ModNum > 6 )
								{
									DestP[ 6 ] = ( Char128ToBinTable[ SrcP[ 6 ] ] << 7 ) | ( Char128ToBinTable[ SrcP[ 7 ] ]      ) ;
								}
							}
						}
					}
				}
			}
		}
	}

	return DestSize ;
}

// バイナリデータをBase64文字列に変換する( 戻り値:変換後のデータサイズ )
extern unsigned int BinToBase64( void *Src, unsigned int SrcSize, void *Dest )
{
	unsigned int DestSize ;
	unsigned int ModNum ;
	unsigned int PackNum ;

	PackNum = SrcSize / 3 ;
	ModNum  = SrcSize - PackNum * 3 ;
	DestSize  = PackNum * 4 + ( ModNum > 0 ? ModNum + 1 : 0 ) + 6 ;

	if( Dest != NULL )
	{
		unsigned char *DestP ;
		unsigned char *SrcP ;
		unsigned int i ;

		DestP = ( unsigned char * )Dest ;
		SrcP  = ( unsigned char * )&SrcSize ;

		DestP[ 0 ] = BinToBase64Table[                                 ( SrcP[ 0 ] >> 2 ) ] ;
		DestP[ 1 ] = BinToBase64Table[ ( ( SrcP[ 0 ] & 0x03 ) << 4 ) | ( SrcP[ 1 ] >> 4 ) ] ;
		DestP[ 2 ] = BinToBase64Table[ ( ( SrcP[ 1 ] & 0x0f ) << 2 ) | ( SrcP[ 2 ] >> 6 ) ] ;
		DestP[ 3 ] = BinToBase64Table[ ( ( SrcP[ 2 ] & 0x3f )      )                      ] ;
		DestP[ 4 ] = BinToBase64Table[                                 ( SrcP[ 3 ] >> 2 ) ] ;
		DestP[ 5 ] = BinToBase64Table[ ( ( SrcP[ 3 ] & 0x03 ) << 4 )                      ] ;

		DestP += 6 ;

		SrcP  = ( unsigned char * )Src ;
		for( i = 0 ; i < PackNum ; i ++ )
		{
			DestP[ 0 ] = BinToBase64Table[                                 ( SrcP[ 0 ] >> 2 ) ] ;
			DestP[ 1 ] = BinToBase64Table[ ( ( SrcP[ 0 ] & 0x03 ) << 4 ) | ( SrcP[ 1 ] >> 4 ) ] ;
			DestP[ 2 ] = BinToBase64Table[ ( ( SrcP[ 1 ] & 0x0f ) << 2 ) | ( SrcP[ 2 ] >> 6 ) ] ;
			DestP[ 3 ] = BinToBase64Table[ ( ( SrcP[ 2 ] & 0x3f )      )                      ] ;

			DestP += 4 ;
			SrcP  += 3 ;
		}

		if( ModNum != 0 )
		{
			DestP[ 0 ] = BinToBase64Table[ ( SrcP[ 0 ] >> 2 ) ] ;
			if( ModNum == 1 )
			{
				DestP[ 1 ] = BinToBase64Table[ ( ( SrcP[ 0 ] & 0x03 ) << 4 )                      ] ;
				DestP += 2 ;
			}
			else
			{
				DestP[ 1 ] = BinToBase64Table[ ( ( SrcP[ 0 ] & 0x03 ) << 4 ) | ( SrcP[ 1 ] >> 4 ) ] ;
				DestP[ 2 ] = BinToBase64Table[ ( ( SrcP[ 1 ] & 0x0f ) << 2 )                      ] ;
				DestP += 3 ;
			}
		}
	}

	return DestSize ;
}

// Base64文字列をバイナリデータに変換する( 戻り値:変換後のデータサイズ )
extern unsigned int Base64ToBin( void *Src, void *Dest )
{
	unsigned int DestSize ;
	unsigned char *SrcP ;
	unsigned char *DestP ;

	SrcP    = ( unsigned char * )Src ;
	DestP   = ( unsigned char * )&DestSize ;

	DestP[ 0 ] = ( Base64ToBinTable[ SrcP[ 0 ] ] << 2 ) | ( Base64ToBinTable[ SrcP[ 1 ] ] >> 4 ) ;
	DestP[ 1 ] = ( Base64ToBinTable[ SrcP[ 1 ] ] << 4 ) | ( Base64ToBinTable[ SrcP[ 2 ] ] >> 2 ) ;
	DestP[ 2 ] = ( Base64ToBinTable[ SrcP[ 2 ] ] << 6 ) | ( Base64ToBinTable[ SrcP[ 3 ] ]      ) ;
	DestP[ 3 ] = ( Base64ToBinTable[ SrcP[ 4 ] ] << 2 ) | ( Base64ToBinTable[ SrcP[ 5 ] ] >> 4 ) ;

	SrcP += 6 ;

	if( Dest != NULL )
	{
		unsigned int PackNum ;
		unsigned int ModNum ;
		unsigned int i ;

		PackNum = DestSize / 3 ;
		ModNum  = DestSize - PackNum * 3 ;
		DestP = ( unsigned char * )Dest ;
		for( i = 0 ; i < PackNum ; i ++ )
		{
			DestP[ 0 ] = ( Base64ToBinTable[ SrcP[ 0 ] ] << 2 ) | ( Base64ToBinTable[ SrcP[ 1 ] ] >> 4 ) ;
			DestP[ 1 ] = ( Base64ToBinTable[ SrcP[ 1 ] ] << 4 ) | ( Base64ToBinTable[ SrcP[ 2 ] ] >> 2 ) ;
			DestP[ 2 ] = ( Base64ToBinTable[ SrcP[ 2 ] ] << 6 ) | ( Base64ToBinTable[ SrcP[ 3 ] ]      ) ;

			DestP += 3 ;
			SrcP  += 4 ;
		}

		if( ModNum != 0 )
		{
			DestP[ 0 ] = ( Base64ToBinTable[ SrcP[ 0 ] ] << 2 ) | ( Base64ToBinTable[ SrcP[ 1 ] ] >> 4 ) ;
			if( ModNum > 1 )
			{
				DestP[ 1 ] = ( Base64ToBinTable[ SrcP[ 1 ] ] << 4 ) | ( Base64ToBinTable[ SrcP[ 2 ] ] >> 2 ) ;
				if( ModNum > 2 )
				{
					DestP[ 2 ] = ( Base64ToBinTable[ SrcP[ 2 ] ] << 6 ) | ( Base64ToBinTable[ SrcP[ 3 ] ]      ) ;
				}
			}
		}
	}

	return DestSize ;
}

// ファイルの内容をメモリに読み込む( 0:成功  -1:失敗 )
extern int LoadFileMem( const TCHAR *Path, void **DataBuf, size_t *Size )
{
	FILE *fp = NULL ;
	void *buf = NULL ;
	__int64 size ;

	fp = _tfopen( Path, TEXT( "rb" ) ) ;
	if( fp == NULL ) goto ERR ;

	// サイズを得る
	fseek( fp, 0L, SEEK_END ) ;
	size = _ftelli64( fp ) ;
	if( size == 0 ) goto ERR ;
	fseek( fp, 0L, SEEK_SET ) ;

	// メモリの確保
	buf = malloc( ( size_t )size ) ;
	if( buf == NULL ) goto ERR ;

	// 読み込み
	fread( buf, ( size_t )size, 1, fp ) ;
	fclose( fp ) ;

	// セット
	if( DataBuf != NULL ) *DataBuf = buf ;
	if( Size != NULL ) *Size = ( size_t )size ;

	// 終了
	return 0 ;

ERR :
	if( buf != NULL ) free( buf ) ;
	if( fp != NULL ) fclose( fp ) ;

	return -1 ;
}

// ファイルの内容をメモリに読み込む( 0:成功  -1:失敗 )
extern int LoadFileMem( const TCHAR *Path, void *DataBuf, size_t *Size )
{
	FILE *fp = NULL ;
	__int64 size ;

	fp = _tfopen( Path, TEXT( "rb" ) ) ;
	if( fp == NULL ) return -1;

	// サイズを得る
	fseek( fp, 0L, SEEK_END ) ;
	size = _ftelli64( fp ) ;
	fseek( fp, 0L, SEEK_SET ) ;

	// 読み込み
	if( DataBuf )
	{
		fread( DataBuf, ( size_t )size, 1, fp ) ;
	}
	fclose( fp ) ;

	// セット
	if( Size != NULL ) *Size = ( size_t )size ;

	// 終了
	return 0 ;
}

// メモリの内容をファイルに書き出す 
extern int SaveFileMem( const TCHAR *Path, void *Data, size_t Size )
{
	FILE *fp ;

	fp = _tfopen( Path, TEXT( "wb" ) ) ;
	if( fp == NULL ) return -1 ;
	fwrite( Data, Size, 1, fp ) ;
	fclose( fp ) ;

	return 0 ;
}


// 指定のディレクトリを作成する、中間のディレクトリも存在しない場合は作成する
// 最後尾に '\' があっても無視する
// ドライブ名の後に '\' がない場合は正常に動作しない
extern int __CreateDirectory( const TCHAR *Path )
{
	TCHAR dir[MAX_PATH] ;

	// フルパスを得る
	ConvertFullPath__( Path, dir, NULL ) ;
	SetEnMark( dir ) ;

	// 指定のディレクトリが存在したら何もせず終了
	{
		WIN32_FIND_DATA FindData ;
		HANDLE FindHandle ;

		FindHandle = FindFirstFile( dir, &FindData ) ;
		if( FindHandle != INVALID_HANDLE_VALUE )
		{
			FindClose( FindHandle ) ;
			return 0 ;
		}
	}

	// ディレクトリを作成するループ
	{
		TCHAR *p ;
		p = _tcschr( dir, TEXT( '\\' ) ) ;
		while( p != NULL )
		{
			*p = '\0' ;
			CreateDirectory( dir, NULL ) ;
			*p = '\\' ;

			p = _tcschr( p + 1, TEXT( '\\' ) ) ;
		}
	}

	// 終了
	return 0 ;
}


// 指定のパスが示しているものがディレクトリかどうかを得る
extern int IsDirectory( const TCHAR *Path )
{
	WIN32_FIND_DATA FindData ;
	HANDLE FindHandle ;
	
	// ファイルの情報を得る
	FindHandle = FindFirstFile( Path, &FindData ) ;
	if( FindHandle == INVALID_HANDLE_VALUE ) return -1 ;
	FindClose( FindHandle ) ;
	
	// ディレクトリかどうかを返す
	return ( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ? 1 : 0 ;
}

// 指定のパスの情報を得る
extern int CreateFileInfo( const TCHAR *Path, FILE_INFO *FileInfoBuffer )
{
	FILE_INFO *info ;
	WIN32_FIND_DATA FindData ;
	HANDLE FindHandle = INVALID_HANDLE_VALUE ;
	int AbsDirLen, RelDirLen ;
	TCHAR RelDir[1] ;
	TCHAR AbsDir[PATH_LENGTH] ;
	
	info = FileInfoBuffer ;
	
	// ディレクトリパスの作成
	{
		ConvertFullPath__( Path, AbsDir ) ;
		DelChr( AbsDir, TEXT( '\\' ) ) ;
		_tcsrchr( AbsDir, TEXT( '\\' ) )[1] = TEXT( '\0' ) ;
		AbsDirLen = ( int )_tcslen( AbsDir ) ;
		RelDir[0] = '\0' ;
		RelDirLen = 0 ;
	}

	// ファイルの情報を得る
	FindHandle = FindFirstFile( Path, &FindData ) ;
	if( FindHandle == INVALID_HANDLE_VALUE ) return -1 ;
	FindClose( FindHandle ) ;
	
	// とりあえず零初期化
	memset( info, 0, sizeof( FILE_INFO ) ) ;

	// 時刻を保存
	info->Date.Create		= ( ( ( LONGLONG )FindData.ftCreationTime	.dwHighDateTime ) << 32 ) + FindData.ftCreationTime		.dwLowDateTime ;
	info->Date.LastAccess	= ( ( ( LONGLONG )FindData.ftLastAccessTime	.dwHighDateTime ) << 32 ) + FindData.ftLastAccessTime	.dwLowDateTime ;
	info->Date.LastWrite	= ( ( ( LONGLONG )FindData.ftLastWriteTime	.dwHighDateTime ) << 32 ) + FindData.ftLastWriteTime	.dwLowDateTime ;

	info->Size			= FindData.nFileSizeLow ;		// サイズを保存
	info->Attributes	= FindData.dwFileAttributes ;	// 属性を保存
	info->IsDirectory	= (u8)(( FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ) != 0 ? 1 : 0) ;	// ディレクトリかどうかを保存

	// パス系の保存
	{
		int FileNameLen ;
	
		// パス系を保存するメモリ領域の確保
		FileNameLen = ( int )_tcslen( FindData.cFileName ) ;
		info->FileName = ( TCHAR * )malloc( sizeof( TCHAR ) * ( ( FileNameLen + 1 ) + ( AbsDirLen + 1 ) + ( RelDirLen + 1 ) ) ) ;
		if( info->FileName == NULL ) return -1 ;
		info->RelDirectoryPath = info->FileName + FileNameLen + 1 ;
		info->AbsDirectoryPath = info->RelDirectoryPath + RelDirLen + 1 ;

		// コピー
		_tcscpy( info->FileName, FindData.cFileName ) ;
		_tcscpy( info->RelDirectoryPath, RelDir ) ;
		_tcscpy( info->AbsDirectoryPath, AbsDir ) ;
	}
	
	// 終了
	return 0 ;
}

// パス情報の後始末を行う
extern int ReleaseFileInfo( FILE_INFO *FileInfo )
{
	if( FileInfo->FileName != NULL ) free( FileInfo->FileName ) ;
	FileInfo->FileName = NULL ;
	
	// 終了
	return 0 ;
}

// 指定のパスのファイルのタイムスタンプを FileInfo に格納されているタイムスタンプにする
extern int SetFileTimeStamp( const TCHAR *Path, FILE_INFO *FileInfo )
{
	HANDLE HFile ;
	FILETIME CreateTime, LastAccessTime, LastWriteTime ;

	HFile = CreateFile( Path,
						GENERIC_WRITE, 0, NULL,
						OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ;
	if( HFile == INVALID_HANDLE_VALUE )
	{
		HFile = HFile ;
	}

	CreateTime.dwHighDateTime     = ( u32 )( FileInfo->Date.Create     >> 32        ) ;
	CreateTime.dwLowDateTime      = ( u32 )( FileInfo->Date.Create     & 0xffffffff ) ;
	LastAccessTime.dwHighDateTime = ( u32 )( FileInfo->Date.LastAccess >> 32        ) ;
	LastAccessTime.dwLowDateTime  = ( u32 )( FileInfo->Date.LastAccess & 0xffffffff ) ;
	LastWriteTime.dwHighDateTime  = ( u32 )( FileInfo->Date.LastWrite  >> 32        ) ;
	LastWriteTime.dwLowDateTime   = ( u32 )( FileInfo->Date.LastWrite  & 0xffffffff ) ;
	SetFileTime( HFile, &CreateTime, &LastAccessTime, &LastWriteTime ) ;
	CloseHandle( HFile ) ;

	// 終了
	return 0;
}

// 二つのファイルのタイムスタンプが一致しているかどうかを得る(0:一致している -1:一致していない)
extern int CmpFileTimeStamp( FILE_INFO *FileInfo1, FILE_INFO *FileInfo2, bool Create, bool LastAccess, bool LastWrite )
{
	if( Create     && FileInfo1->Date.Create     != FileInfo2->Date.Create     ) return -1;
	if( LastAccess && FileInfo1->Date.LastAccess != FileInfo2->Date.LastAccess ) return -1;
	if( LastWrite  && FileInfo1->Date.LastWrite  != FileInfo2->Date.LastWrite  ) return -1;
	return 0;
}

// 指定のディレクトリのファイルリストを取得する
// パス文字列の最後に『\』が無くても常に最後の文字列をディレクトリと判断する
// フルパスではない場合は現在のカレントディレクトリからの相対パスとなる
// FileListInfo が 0 ではない場合使用後 ReleaseFileObjectList を呼ぶ必要がある
// SubDirectory を 1 にするとサブディレクトリ内のファイルも全て列挙する
// NULL 、もしくは文字が一つもない文字列を渡した場合カレントディレクトリのファイルが列挙される
// OmitDirectory を 1 にするとディレクトリは列挙しない
// OmitName に除外したいファイル名を渡すとその文字列を除外してくれる( ';'で複数の文字列を連結可能 )
// OmitExName に除外したい拡張子を渡すとその拡張子を持つファイルを除外してくれる( ';'で複数の文字列を連結可能 )
// ValidExName に有効にしたい拡張子を渡すとその拡張子を持つファイルのみ列挙してくれる( Omit系よりも優先度が高い, ';'で複数の文字列を連結可能 )
// 戻り値 : -1 = エラー  0以上 = 列挙したファイルの数
extern int CreateFileList( const TCHAR *DirectoryPath, FILE_INFOLIST *FileListInfo,
							int OmitDirectory, int SubDirectory,
							const TCHAR *OmitName, const TCHAR *OmitExName, const TCHAR *ValidExName,
						void ( *EnumFileCallback )( int Phase, int NowFileNum, int TotalFileNum, const TCHAR *FileName, const TCHAR *RelDirPath, const TCHAR *AbsDirPath ) )
{
	TCHAR DirPath[PATH_LENGTH] ;
	FILE_INFOLIST FileList ;
	TCHAR *OmitStrBuf = NULL, *OmitExStrBuf = NULL, *ValidExStrBuf = NULL ;
	TCHAR *OmitStr[50], *OmitExStr[50], *ValidExStr[100] ;
	int TotalFileNum ;
	int TotalFileNumCounter ;
	
	// フルパスを得る
	ConvertFullPath__( DirectoryPath, DirPath, NULL ) ;
	
	// ディレクトリではなくファイルだったら此処で処理してしまう
	{
		int res ;
		
		DelChr( DirPath, '\\' ) ;
		
		res = IsDirectory( DirPath ) ;
		if( res == -1 ) return -1 ;
		
		if( IsDirectory( DirPath ) == 0 )
		{
			FileListInfo->Num = 1 ;
			
			FileListInfo->List = ( FILE_INFO * )malloc( sizeof( FILE_INFO ) ) ;
			CreateFileInfo( DirectoryPath, FileListInfo->List ) ;
			
			return 1 ;
		}

		SetEnMark( DirPath ) ;
	}

	// 初期化
	FileList.Num = 0 ;
	FileList.List = NULL ;
	OmitStrBuf = ( TCHAR * )malloc( sizeof( TCHAR ) * 2048 ) ;
	if( OmitStrBuf == NULL ) goto ERR ;
	OmitExStrBuf = ( TCHAR * )malloc( sizeof( TCHAR ) * 2048 ) ;
	if( OmitExStrBuf == NULL ) goto ERR ;
	ValidExStrBuf = ( TCHAR * )malloc( sizeof( TCHAR ) * 2048 ) ;
	if( ValidExStrBuf == NULL ) goto ERR ;

	// 除外文字列リストの作成
	if( OmitName != NULL )
	{
		int i ;
		TCHAR *p ;

		_tcscpy( OmitStrBuf, OmitName ) ;
		i = 0 ;
		p = OmitStrBuf ;
		OmitStr[i] = p ;
		i ++ ;
		while( ( p = _tcschr( p, TEXT( ';' ) ) ) != NULL )
		{
			*p = TEXT( '\0' ) ;
			p ++ ;
			OmitStr[i] = p ;
			i ++ ;
		}
		OmitStr[i] = NULL ;
	}
	else OmitStr[0] = NULL ;

	// 除外拡張子リストの作成
	if( OmitExName != NULL )
	{
		int i ;
		TCHAR *p ;

		_tcscpy( OmitExStrBuf, OmitExName ) ;
		i = 0 ;
		p = OmitExStrBuf ;
		OmitExStr[i] = p ;
		i ++ ;
		while( ( p = _tcschr( p, TEXT( ';' ) ) ) != NULL )
		{
			*p = TEXT( '\0' ) ;
			p ++ ;
			OmitExStr[i] = p ;
			i ++ ;
		}
		OmitExStr[i] = NULL ;
	}
	else OmitExStr[0] = NULL ;

	// 有効拡張子リストの作成
	if( ValidExName != NULL )
	{
		int i ;
		TCHAR *p ;

		_tcscpy( ValidExStrBuf, ValidExName ) ;
		i = 0 ;
		p = ValidExStrBuf ;
		ValidExStr[i] = p ;
		i ++ ;
		while( ( p = _tcschr( p, TEXT( ';' ) ) ) != NULL )
		{
			*p = TEXT( '\0' ) ;
			p ++ ;
			ValidExStr[i] = p ;
			i ++ ;
		}
		ValidExStr[i] = NULL ;
	}
	else ValidExStr[0] = NULL ;

	// 列挙用関数に投げてファイルの数を得る
	TotalFileNumCounter = 0 ;
	FileList.Num = __EnumObject( DirPath, DirPath, NULL, OmitDirectory, SubDirectory, OmitStr, OmitExStr, ValidExStr[0] != NULL ? ValidExStr : NULL, &TotalFileNumCounter, 0, EnumFileCallback ) ;
	if( FileList.Num < 0 )
		goto ERR ;
	TotalFileNum = TotalFileNumCounter ;

	// ファイルリストのデータが格納できるメモリ領域を確保
	FileList.List = ( FILE_INFO * )malloc( FileList.Num * sizeof( FILE_INFO ) ) ;
	if( FileList.List == NULL )
		goto ERR ;
	memset( FileList.List, 0, FileList.Num * sizeof( FILE_INFO ) ) ;

	// ファイルリストにデータを格納する
	TotalFileNumCounter = 0 ;
	FileList.Num = 0 ;
	if( __EnumObject( DirPath, DirPath, &FileList, OmitDirectory, SubDirectory, OmitStr, OmitExStr, ValidExStr[0] != NULL ? ValidExStr : NULL, &TotalFileNumCounter, TotalFileNum, EnumFileCallback ) < 0 )
		goto ERR ;

	// データを格納
	if( FileListInfo != NULL ) *FileListInfo = FileList ;

	// 除外文字列バッファ解放
	free( OmitStrBuf ) ;

	// 除外拡張子バッファ解放
	free( OmitExStrBuf ) ;

	// 有効拡張子バッファ解放
	free( ValidExStrBuf ) ;

	// 終了
	return FileList.Num ;

	// エラー処理
ERR :
	ReleaseFileList( &FileList ) ;
	if( OmitStrBuf != NULL ) free( OmitStrBuf ) ;
	if( OmitExStrBuf != NULL ) free( OmitExStrBuf ) ;
	if( ValidExStrBuf != NULL ) free( ValidExStrBuf ) ;
	
	return -1 ;
}

extern int ReleaseFileList( FILE_INFOLIST *DirectoryInfo )
{
	// メモリの解放
	if( DirectoryInfo->List != NULL )
	{
		int i ;
		FILE_INFO *finfo ;

		finfo = DirectoryInfo->List ;
		for( i = 0 ; i < DirectoryInfo->Num ; i ++, finfo ++ )
			if( finfo->FileName != NULL ) free( finfo->FileName ) ;

		free( DirectoryInfo->List ) ;
		DirectoryInfo->List = NULL ;
	}

	// ゼロ初期化
	memset( DirectoryInfo, 0, sizeof( FILE_INFOLIST ) ) ;

	// 終了
	return 0 ;
}

// 特定のパス文字列から色々な情報を取得する
// ( CurrentDir はフルパスである必要がある(語尾に『\』があっても無くても良い) )
// ( CurrentDir が 0 の場合は実際のカレントディレクトリを使用する )
extern int AnalyseFilePath(
	const TCHAR *Src,
	TCHAR *FullPath, TCHAR *DirPath, TCHAR *FileName, TCHAR *Name, TCHAR *ExeName, const TCHAR *CurrentDir )
{
	TCHAR full[1124], dir[1124], fname[1124], name[1124], exename[1124];

	ConvertFullPath__( Src, full, CurrentDir );
	AnalysisFileNameAndDirPath( full, fname, dir );
	AnalysisFileNameAndExeName( fname, name, exename );
	if( FullPath ) _tcscpy( FullPath, full    );
	if( DirPath  ) _tcscpy( DirPath,  dir     );
	if( FileName ) _tcscpy( FileName, fname   );
	if( Name     ) _tcscpy( Name,     name    );
	if( ExeName  ) _tcscpy( ExeName,  exename );

	return 0;
}

// フルパスではないパス文字列をフルパスに変換する
// ( CurrentDir はフルパスである必要がある(語尾に『\』があっても無くても良い) )
// ( CurrentDir が NULL の場合は実際のカレントディレクトリを使用する )
extern int ConvertFullPath__( const TCHAR *Src, TCHAR *Dest, const TCHAR *CurrentDir )
{
	int i, j, k ;
	TCHAR iden[512], cur[1024] ;

	if( CurrentDir == NULL )
	{
		GetCurrentDirectory( 1024, cur ) ;
		CurrentDir = cur ;
	}

	if( Src == NULL )
	{
		_tcscpy( Dest, CurrentDir ) ;
		goto END ;
	}

	i = 0 ;
	j = 0 ;
	k = 0 ;
	
	// 最初に『\』又は『/』が２回連続で続いている場合はネットワークを介していると判断
	if( ( Src[0] == TEXT( '\\' ) && Src[1] == TEXT( '\\' ) ) ||
		( Src[0] == TEXT( '/'  ) && Src[1] == TEXT( '/'  ) ) )
	{
		Dest[0] = TEXT( '\\' );
		Dest[1] = TEXT( '\0' );

		i += 2;
		j ++ ;
	}
	else
	// 最初が『\』又は『/』の場合はカレントドライブのルートディレクトリまで落ちる
	if( Src[0] == TEXT( '\\' ) )
	{
		Dest[0] = CurrentDir[0] ;
		Dest[1] = CurrentDir[1] ;
		Dest[2] = TEXT( '\0' ) ;

		i ++ ;
		j = 2 ;
	}
	else
	// ドライブ名が書かれていたらそのドライブへ
	if( Src[1] == TEXT( ':' ) )
	{
		Dest[0] = Src[0] ;
		Dest[1] = Src[1] ;
		Dest[2] = TEXT( '\0' ) ;

		i = 2 ;
		j = 2 ;

		if( Src[i] == TEXT( '\\' ) ) i ++ ;
	}
	else
	// それ以外の場合はカレントディレクトリ
	{
		_tcscpy( Dest, CurrentDir ) ;
		j = ( int )_tcslen( Dest ) ;
		if( Dest[j-1] == TEXT( '\\' ) || Dest[j-1] == TEXT( '/' ) )
		{
			Dest[j-1] = TEXT( '\0' ) ;
			j -- ;
		}
	}

	for(;;)
	{
		switch( Src[i] )
		{
		case TEXT( '\0' ) :
			if( k != 0 )
			{
				Dest[j] = TEXT( '\\' ) ; j ++ ;
				_tcscpy( &Dest[j], iden ) ;
				j += k ;
				k = 0 ;
			}
			goto END ;

		case TEXT( '\\' ) :
		case TEXT( '/'  ) :
			// 文字列が無かったらスキップ
			if( k == 0 )
			{
				i ++ ;
				break;
			}
			if( _tcscmp( iden, TEXT( "." ) ) == 0 )
			{
				// なにもしない
			}
			else
			if( _tcscmp( iden, TEXT( ".." ) ) == 0 )
			{
				// 一つ下のディレクトリへ
				j -- ;
				while( Dest[j] != TEXT( '\\' ) && Dest[j] != TEXT( '/' ) && Dest[j] != TEXT( ':' ) ) j -- ;
				if( Dest[j] != TEXT( ':' ) ) Dest[j] = TEXT( '\0' ) ;
				else j ++ ;
			}
			else
			{
				Dest[j] = TEXT( '\\' ) ; j ++ ;
				_tcscpy( &Dest[j], iden ) ;
				j += k ;
			}

			k = 0 ;
			i ++ ;
			break ;
		
		default :
#ifdef _UNICODE
			iden[k] = Src[i] ;
			iden[k+1] = 0 ; 
			k ++ ;
			i ++ ;
#else // _UNICODE
			if( _mbsbtype( ( const unsigned char * )&Src[i], 0 ) == 0/*(_MBC_SINGLE)*/ )
			{
				iden[k] = Src[i] ;
				iden[k+1] = 0 ; 
				k ++ ;
				i ++ ;
			}
			else
			{
				*(( unsigned short * )&iden[k] ) = *(( unsigned short * )&Src[i] ) ;
				iden[k+2] = '\0' ;
				k += 2 ;
				i += 2 ;
			}
#endif // _UNICODE
			break ;
		}
	}
	
END :
	// 正常終了
	return 0 ;
}

// ファイル名も一緒になっていると分かっているパス中からファイル名とディレクトリパスを分割する
// フルパスである必要は無い、ファイル名だけでも良い
extern int AnalysisFileNameAndDirPath( const TCHAR *Src, TCHAR *FileName, TCHAR *DirPath )
{
	int i, Last ;
	
	// ファイル名を抜き出す
	i = 0 ;
	Last = -1 ;
	while( Src[i] != TEXT( '\0' ) )
	{
#ifdef _UNICODE
		if( Src[i] == TEXT( '\\' ) || Src[i] == TEXT( '/' ) || Src[i] == TEXT( '\0' ) || Src[i] == TEXT( ':' ) ) Last = i ;
		i ++ ;
#else // _UNICODE
		if( _mbsbtype( ( const unsigned char * )&Src[i], 0 ) == 0/*(_MBC_SINGLE)*/ )
		{
			if( Src[i] == '\\' || Src[i] == '/' || Src[i] == '\0' || Src[i] == ':' ) Last = i ;
			i ++ ;
		}
		else
		{
			i += 2 ;
		}
#endif // _UNICODE
	}
	if( FileName != NULL )
	{
		if( Last != -1 ) _tcscpy( FileName, &Src[Last+1] ) ;
		else _tcscpy( FileName, Src ) ;
	}
	
	// ディレクトリパスを抜き出す
	if( DirPath != NULL )
	{
		if( Last != -1 )
		{
			_tcsncpy( DirPath, Src, Last ) ;
			DirPath[Last] = TEXT( '\0' ) ;
		}
		else
		{
			DirPath[0] = TEXT( '\0' ) ;
		}
	}
	
	// 終了
	return 0 ;
}

// ファイルパスからファイル名と拡張子を取得する
extern int AnalysisFileNameAndExeName( const TCHAR *Src, TCHAR *Name, TCHAR *ExeName )
{
	TCHAR FileName[256], *p, ename[256], name[256] ;

	// ファイル名のみを取得
	AnalysisFileNameAndDirPath( Src, FileName, 0 ) ;

	// 『.』があるかどうかで処理を分岐
	if( ( p = _tcsrchr( FileName, TEXT( '.' ) ) ) == NULL )
	{
		_tcscpy( name, FileName ) ;
		ename[0] = TEXT( '\0' ) ;
	}
	else
	{
		_tcsncpy( name, FileName, p - FileName ) ;
		name[p - FileName] = TEXT( '\0' ) ;
		_tcscpy( ename, p + 1 ) ;
	}

	if( Name != NULL ) _tcscpy( Name, name ) ;
	if( ExeName != NULL ) _tcscpy( ExeName, ename ) ;

	// 終了
	return 0 ;
}

// ファイルパスの拡張子を変えた文字列を得る
extern int GetChangeExeNamePath( const TCHAR *Src, TCHAR *Dest, const TCHAR *ExeName )
{
	TCHAR DirPath[1024], FileName[1024] ;

	AnalysisFileNameAndDirPath( Src, NULL, DirPath ) ;
	AnalysisFileNameAndExeName( Src, FileName, 0 ) ;
	SetEnMark( DirPath ) ;
	_stprintf( Dest, TEXT( "%s%s.%s" ), DirPath, FileName, ExeName ) ;

	// 終了
	return 0 ;
}


// 語尾に『\』がついていない場合は付ける
extern void SetEnMark( TCHAR *PathBuf )
{
	int Len = ( int )_tcslen( PathBuf ) ;
	
	if( PathBuf[Len-1] != TEXT( '\\' ) )
	{
		PathBuf[Len] = TEXT( '\\' ) ;
		PathBuf[Len+1] = TEXT( '\0' ) ;
	}
}

// 語尾に指定の文字がない場合はつける
extern void SetChr( TCHAR *PathBuf, TCHAR chr )
{
	int Len = ( int )_tcslen( PathBuf ) ;
	
	if( PathBuf[Len-1] != chr )
	{
		PathBuf[Len] = chr ;
		PathBuf[Len+1] = TEXT( '\0' ) ;
	}
}

// 語尾に指定の文字がある場合は削除する
extern void DelChr( TCHAR *PathBuf, TCHAR chr )
{
	int Len = ( int )_tcslen( PathBuf ) ;

	if( PathBuf[Len-1] == chr )
	{
		PathBuf[Len-1] = TEXT( '\0' ) ;
	}
}


// 拡張子を得る
extern int GetExName( const TCHAR *Path, TCHAR *ExNameBuf )
{
	TCHAR *p ;
	
	// 一番最後に '.' が出る部分を取得する
	p = _tcsrchr( ( TCHAR * )Path, TEXT( '.' ) ) ;
	if( p == NULL ) ExNameBuf[0] = TEXT( '\0' ) ;
	else _tcscpy( ExNameBuf, p + 1 ) ;

	// 終了
	return 0 ;
}

// 拡張子を変更する
extern int SetExName( const TCHAR *Path, const TCHAR *ExName, TCHAR *DestBuf )
{
	TCHAR *p ;
	TCHAR tempstr[1024] ;
	
	_tcscpy( tempstr, Path ) ;

	// 一番最後に '.' が出る部分を取得する
	p = _tcsrchr( tempstr, TEXT( '.' ) ) ;
	if( p == NULL )
	{
		_stprintf( DestBuf, TEXT( "%s.%s" ), tempstr, ExName ) ;
	}
	else
	{
		_tcsncpy( DestBuf, tempstr, p - tempstr + 1 ) ;
		_tcscpy( DestBuf + ( p - tempstr + 1 ), ExName ) ;
	}

	// 終了
	return 0 ;
}


// テキストデータかどうかを判定する( 1:テキストデータ  0:バイナリデータ )
extern int CheckTextData( void *buffer, int size )
{
	unsigned char *p ;
	int len, saerch ;
	
	// 先頭８ＫＢのみ見る
	saerch = size > TEXTCHECKSIZE ? TEXTCHECKSIZE : size ;

	// 終端文字が２回以上出てきたらバイナリデータ
	{
		int con ;
	
		p = ( unsigned char * )buffer ;
		con = 0 ;
		for( len = saerch ; len > 0 ; len --, p ++ )
		{
			if( *p == '\0' )
			{
				con ++ ;
				if( con >= 2 ) return 0 ;
			}
		}
	}
	
	// 改行、タブ以外のコントロール文字が出てきたらバイナリデータ
	{
		int con ;
		
		p = ( unsigned char * )buffer ;
		con = 0 ;
		for( len = saerch ; len > 0 ; len --, p ++ )
		{
			if( *p < 0x20 && *p != '\r' && *p != '\n' && *p != '\t' )
			{
				con ++ ;
				if( con >= 1 ) return 0 ;
			}
		}
	}
	
	// 文字ではないコードが出てきたらバイナリデータ
	{
		p = ( unsigned char * )buffer ;
		for( len = saerch ; len > 0 ; len --, p ++ )
		{
			if( ( *p >= 0x81 && *p <= 0x9f ) || ( *p >= 0xe0 && *p <= 0xfc ) )
			{
				len -- ;
				p ++ ;
				if( len == 0 && size <= saerch ) return 0 ;
				
				if( !( ( *p >= 0x40 && *p <= 0x7e ) || ( *p >= 0x80 && *p <= 0xfc ) ) )
					return 0 ;
				
				if( len == 0 ) return 1 ;
			}
		}
	}
	
	// 此処に来たらテキストデータ
	return 1 ;
}

// テキストファイルかどうかを判定する( 1:テキストデータ  0:バイナリデータ )
extern int CheckTextFile( const TCHAR *Path )
{
	void *buf ;
	int size, res ;
	FILE *fp ;

	fp = _tfopen( Path, TEXT( "rb" ) ) ;
	if( fp == NULL ) return -1 ;
	
	fseek( fp, 0L, SEEK_END ) ;
	size = ftell( fp ) ;
	fseek( fp, 0L, SEEK_SET ) ;
	
	size = size > TEXTCHECKSIZE ? TEXTCHECKSIZE : size ;
	
	buf = malloc( size ) ;
	if( buf == NULL )
	{
		fclose( fp ) ;
		return -1 ;
	}
	
	fread( buf, size, 1, fp ) ;
	fclose( fp ) ;
	
	res = CheckTextData( buf, size ) ;
	
	free( buf ) ;

	return res ;
}

// ２バイト文字か調べる( TRUE:２バイト文字 FALSE:１バイト文字 )
extern int CheckMultiByteChar( char *Buf )
{
	return  ( (unsigned char)*Buf >= 0x81 && (unsigned char)*Buf <= 0x9F ) || ( (unsigned char)*Buf >= 0xE0 && (unsigned char)*Buf <= 0xFC ) ;
}

// バイナリデータを元に CRC32 のハッシュ値を計算する
extern u32 FileLib_HashCRC32( const void *SrcData, size_t SrcDataSize )
{
	static u32 CRC32Table[ 256 ] ;
	static int CRC32TableInit = 0 ;
	u32 CRC = 0xffffffff ;
	u8 *SrcByte = ( u8 * )SrcData ;
	u32 i ;

	// テーブルが初期化されていなかったら初期化する
	if( CRC32TableInit == 0 )
	{
		u32 Magic = 0xedb88320 ;	// 0x4c11db7 をビットレベルで順番を逆にしたものが 0xedb88320
		u32 j ;

		for( i = 0; i < 256; i++ )
		{
			u32 Data = i ;
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
		CRC = CRC32Table[ ( u8 )( CRC ^ SrcByte[ i ] ) ] ^ ( CRC >> 8 ) ;
	}

	return CRC ^ 0xffffffff ;
}






