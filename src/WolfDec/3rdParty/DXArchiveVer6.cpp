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
#include "DXArchiveVer6.h"
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

WCHAR sjis2utf8_VER6(const char* sjis);

// struct -----------------------------

// 圧縮時間短縮用リスト
typedef struct LZ_LIST
{
	LZ_LIST *next, *prev ;
	u32 address ;
} LZ_LIST ;

// class code -------------------------

// ファイル名も一緒になっていると分かっているパス中からファイルパスとディレクトリパスを分割する
// フルパスである必要は無い
int DXArchive_VER6::GetFilePathAndDirPath(TCHAR *Src, TCHAR *FilePath, TCHAR *DirPath )
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
DARC_FILEHEAD_VER6 *DXArchive_VER6::GetFileInfo( const TCHAR *FilePath )
{
	DARC_DIRECTORY_VER6 *OldDir ;
	DARC_FILEHEAD_VER6 *FileH ;
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
	FileHeadSize = sizeof( DARC_FILEHEAD_VER6 ) ;
	FileH = ( DARC_FILEHEAD_VER6 * )( this->FileP + this->CurrentDirectory->FileHeadAddress ) ;
	Num = ( int )this->CurrentDirectory->FileHeadNum ;
	for( i = 0 ; i < Num ; i ++, FileH = (DARC_FILEHEAD_VER6 *)( (u8 *)FileH + FileHeadSize ) )
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
DARC_DIRECTORY_VER6 *DXArchive_VER6::GetCurrentDirectoryInfo( void )
{
	return CurrentDirectory ;
}

// どちらが新しいかを比較する
DXArchive_VER6::DATE_RESULT DXArchive_VER6::DateCmp( DARC_FILETIME_VER6 *date1, DARC_FILETIME_VER6 *date2 )
{
	if( date1->LastWrite == date2->LastWrite ) return DATE_RESULT_DRAW ;
	else if( date1->LastWrite > date2->LastWrite ) return DATE_RESULT_LEFTNEW ;
	else return DATE_RESULT_RIGHTNEW ;
}

// 比較対照の文字列中の大文字を小文字として扱い比較する( 0:等しい  1:違う )
int DXArchive_VER6::StrICmp( const TCHAR *Str1, const TCHAR *Str2 )
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
int DXArchive_VER6::ConvSearchData( SEARCHDATA *SearchData, const TCHAR *Src, int *Length )
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
int DXArchive_VER6::AddFileNameData( const TCHAR *FileName, u8 *FileNameTable )
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
TCHAR *DXArchive_VER6::GetOriginalFileName( u8 *FileNameTable )
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
			pFileStr[sf] = sjis2utf8_VER6(ss);
			si++;
		}
		else
#endif
			pFileStr[sf] = pName[si];
	}

	return pFileStr;
}

// 標準ストリームにデータを書き込む( 64bit版 )
void DXArchive_VER6::fwrite64( void *Data, s64 Size, FILE *fp )
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
void DXArchive_VER6::fread64( void *Buffer, s64 Size, FILE *fp )
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
void DXArchive_VER6::NotConv( void *Data , s64 Size )
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
void DXArchive_VER6::NotConvFileWrite( void *Data, s64 Size, FILE *fp )
{
	// データを反転する
	NotConv( Data, Size ) ;

	// 書き出す
	fwrite64( Data, Size, fp ) ;

	// 再び反転
	NotConv( Data, Size ) ;
}

// データを反転させてファイルから読み込む関数
void DXArchive_VER6::NotConvFileRead( void *Data, s64 Size, FILE *fp )
{
	// 読み込む
	fread64( Data, Size, fp ) ;

	// データを反転
	NotConv( Data, Size ) ;
}

// 鍵文字列を作成
void DXArchive_VER6::KeyCreate( const char *Source, unsigned char *Key )
{
	int Len ;

	if( Source == NULL )
	{
		memset( Key, 0xaaaaaaaa, DXA_KEYSTR_LENGTH_VER6 ) ;
	}
	else
	{
		Len = ( int )strlen( Source ) ;
		if( Len > DXA_KEYSTR_LENGTH_VER6 )
		{
			memcpy( Key, Source, DXA_KEYSTR_LENGTH_VER6 ) ;
		}
		else
		{
			// 鍵文字列が DXA_KEYSTR_LENGTH_VER6 より短かったらループする
			int i ;

			for( i = 0 ; i + Len <= DXA_KEYSTR_LENGTH_VER6 ; i += Len )
				memcpy( Key + i, Source, Len ) ;
			if( i < DXA_KEYSTR_LENGTH_VER6 )
				memcpy( Key + i, Source, DXA_KEYSTR_LENGTH_VER6 - i ) ;
		}
	}

	Key[0] = ~Key[0] ;
	Key[1] = ( Key[1] >> 4 ) | ( Key[1] << 4 ) ;
	Key[2] = Key[2] ^ 0x8a ;
	Key[3] = ~( ( Key[3] >> 4 ) | ( Key[3] << 4 ) ) ;
	Key[4] = ~Key[4] ;
	Key[5] = Key[5] ^ 0xac ;
	Key[6] = ~Key[6] ;
	Key[7] = ~( ( Key[7] >> 3 ) | ( Key[7] << 5 ) ) ;
	Key[8] = ( Key[8] >> 5 ) | ( Key[8] << 3 ) ;
	Key[9] = Key[9] ^ 0x7f ;
	Key[10] = ( ( Key[10] >> 4 ) | ( Key[10] << 4 ) ) ^ 0xd6 ;
	Key[11] = Key[11] ^ 0xcc ;
}

// 鍵文字列を使用して Xor 演算( Key は必ず DXA_KEYSTR_LENGTH_VER6 の長さがなければならない )
void DXArchive_VER6::KeyConv( void *Data, s64 Size, s64 Position, unsigned char *Key )
{
	Position %= DXA_KEYSTR_LENGTH_VER6 ;

	if( Size < 0x100000000 )
	{
		u32 i, j ;

		j = ( u32 )Position ;
		for( i = 0 ; i < Size ; i ++ )
		{
			((u8 *)Data)[i] ^= Key[j] ;

			j ++ ;
			if( j == DXA_KEYSTR_LENGTH_VER6 ) j = 0 ;
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
			if( j == DXA_KEYSTR_LENGTH_VER6 ) j = 0 ;
		}
	}
}

// データを鍵文字列を使用して Xor 演算した後ファイルに書き出す関数( Key は必ず DXA_KEYSTR_LENGTH_VER6 の長さがなければならない )
void DXArchive_VER6::KeyConvFileWrite( void *Data, s64 Size, FILE *fp, unsigned char *Key, s64 Position )
{
	s64 pos ;

	// ファイルの位置を取得しておく
	if( Position == -1 ) pos = _ftelli64( fp ) ;
	else                 pos = Position ;

	// データを鍵文字列を使って Xor 演算する
	KeyConv( Data, Size, pos, Key ) ;

	// 書き出す
	fwrite64( Data, Size, fp ) ;

	// 再び Xor 演算
	KeyConv( Data, Size, pos, Key ) ;
}

// ファイルから読み込んだデータを鍵文字列を使用して Xor 演算する関数( Key は必ず DXA_KEYSTR_LENGTH_VER6 の長さがなければならない )
void DXArchive_VER6::KeyConvFileRead( void *Data, s64 Size, FILE *fp, unsigned char *Key, s64 Position )
{
	s64 pos ;

	// ファイルの位置を取得しておく
	if( Position == -1 ) pos = _ftelli64( fp ) ;
	else                 pos = Position ;

	// 読み込む
	fread64( Data, Size, fp ) ;

	// データを鍵文字列を使って Xor 演算
	KeyConv( Data, Size, pos, Key ) ;
}

/*
// ２バイト文字か調べる( TRUE:２バイト文字 FALSE:１バイト文字 )
int DXArchive_VER6::CheckMultiByteChar( const char *Buf )
{
	return  ( (unsigned char)*Buf >= 0x81 && (unsigned char)*Buf <= 0x9F ) || ( (unsigned char)*Buf >= 0xE0 && (unsigned char)*Buf <= 0xFC ) ;
}
*/

// 指定のディレクトリにあるファイルをアーカイブデータに吐き出す
int DXArchive_VER6::DirectoryEncode(TCHAR *DirectoryName, u8 *NameP, u8 *DirP, u8 *FileP, DARC_DIRECTORY_VER6 *ParentDir, SIZESAVE *Size, int DataNumber, FILE *DestP, void *TempBuffer, bool Press, unsigned char *Key )
{
	TCHAR DirPath[MAX_PATH] ;
	WIN32_FIND_DATA FindData ;
	HANDLE FindHandle ;
	DARC_DIRECTORY_VER6 Dir ;
	DARC_FILEHEAD_VER6 File ;

	// ディレクトリの情報を得る
	FindHandle = FindFirstFile( DirectoryName, &FindData ) ;
	if( FindHandle == INVALID_HANDLE_VALUE ) return 0 ;
	
	// ディレクトリ情報を格納するファイルヘッダをセットする
	{
		File.NameAddress     = Size->NameSize ;
		File.Time.Create     = ( ( ( LONGLONG )FindData.ftCreationTime.dwHighDateTime ) << 32 ) + FindData.ftCreationTime.dwLowDateTime ;
		File.Time.LastAccess = ( ( ( LONGLONG )FindData.ftLastAccessTime.dwHighDateTime ) << 32 ) + FindData.ftLastAccessTime.dwLowDateTime ;
		File.Time.LastWrite  = ( ( ( LONGLONG )FindData.ftLastWriteTime.dwHighDateTime ) << 32 ) + FindData.ftLastWriteTime.dwLowDateTime ;
		File.Attributes      = FindData.dwFileAttributes ;
		File.DataAddress     = Size->DirectorySize ;
		File.DataSize        = 0 ;
		File.PressDataSize	 = 0xffffffffffffffff ;
	}

	// ディレクトリ名を書き出す
	Size->NameSize += AddFileNameData( FindData.cFileName, NameP + Size->NameSize ) ;

	// ディレクトリ情報が入ったファイルヘッダを書き出す
	memcpy( FileP + ParentDir->FileHeadAddress + DataNumber * sizeof( DARC_FILEHEAD_VER6 ),
			&File, sizeof( DARC_FILEHEAD_VER6 ) ) ;

	// Find ハンドルを閉じる
	FindClose( FindHandle ) ;

	// 指定のディレクトリにカレントディレクトリを移す
	GetCurrentDirectory( MAX_PATH, DirPath ) ;
	SetCurrentDirectory( DirectoryName ) ;

	// ディレクトリ情報のセット
	{
		Dir.DirectoryAddress = ParentDir->FileHeadAddress + DataNumber * sizeof( DARC_FILEHEAD_VER6 ) ;
		Dir.FileHeadAddress  = Size->FileSize ;

		// 親ディレクトリの情報位置をセット
		if( ParentDir->DirectoryAddress != 0xffffffffffffffff && ParentDir->DirectoryAddress != 0 )
		{
			Dir.ParentDirectoryAddress = ((DARC_FILEHEAD_VER6 *)( FileP + ParentDir->DirectoryAddress ))->DataAddress ;
		}
		else
		{
			Dir.ParentDirectoryAddress = 0 ;
		}

		// ディレクトリ中のファイルの数を取得する
		Dir.FileHeadNum = GetDirectoryFilePath(TEXT(""), NULL ) ;
	}

	// ディレクトリの情報を出力する
	memcpy( DirP + Size->DirectorySize, &Dir, sizeof( DARC_DIRECTORY_VER6 ) ) ;	

	// アドレスを推移させる
	Size->DirectorySize += sizeof( DARC_DIRECTORY_VER6 ) ;
	Size->FileSize      += sizeof( DARC_FILEHEAD_VER6 ) * Dir.FileHeadNum ;
	
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
				if( DirectoryEncode( FindData.cFileName, NameP, DirP, FileP, &Dir, Size, i, DestP, TempBuffer, Press, Key ) < 0 ) return -1 ;
			}
			else
			{
				// ファイルだった場合の処理

				// ファイルのデータをセット
				File.NameAddress     = Size->NameSize ;
				File.Time.Create     = ( ( ( LONGLONG )FindData.ftCreationTime.dwHighDateTime   ) << 32 ) + FindData.ftCreationTime.dwLowDateTime   ;
				File.Time.LastAccess = ( ( ( LONGLONG )FindData.ftLastAccessTime.dwHighDateTime ) << 32 ) + FindData.ftLastAccessTime.dwLowDateTime ;
				File.Time.LastWrite  = ( ( ( LONGLONG )FindData.ftLastWriteTime.dwHighDateTime  ) << 32 ) + FindData.ftLastWriteTime.dwLowDateTime  ;
				File.Attributes      = FindData.dwFileAttributes ;
				File.DataAddress     = Size->DataSize ;
				File.DataSize        = ( ( ( LONGLONG )FindData.nFileSizeHigh ) << 32 ) + FindData.nFileSizeLow ;
				File.PressDataSize   = 0xffffffffffffffff ;

				// ファイル名を書き出す
				Size->NameSize += AddFileNameData( FindData.cFileName, NameP + Size->NameSize ) ;
				
				// ファイルデータを書き出す
				if( File.DataSize != 0 )
				{
					FILE *SrcP ;
					u64 FileSize, WriteSize, MoveSize ;

					// ファイルを開く
					SrcP = _tfopen( FindData.cFileName, TEXT("rb") ) ;
					
					// サイズを得る
					_fseeki64( SrcP, 0, SEEK_END ) ;
					FileSize = _ftelli64( SrcP ) ;
					_fseeki64( SrcP, 0, SEEK_SET ) ;
					
					// ファイルサイズが 10MB 以下の場合で、圧縮の指定がある場合は圧縮を試みる
					if( Press == true && File.DataSize < 10 * 1024 * 1024 )
					{
						void *SrcBuf, *DestBuf ;
						u32 DestSize, Len ;
						
						// 一部のファイル形式の場合は予め弾く
						if( ( Len = ( int )_tcslen( FindData.cFileName ) ) > 4 )
						{
							TCHAR *sp ;
							
							sp = &FindData.cFileName[Len-3] ;
							if( StrICmp( sp, TEXT("wav") ) == 0 ||
								StrICmp( sp, TEXT("jpg") ) == 0 ||
								StrICmp( sp, TEXT("png") ) == 0 ||
								StrICmp( sp, TEXT("mpg") ) == 0 ||
								StrICmp( sp, TEXT("mp3") ) == 0 ||
								StrICmp( sp, TEXT("ogg") ) == 0 ||
								StrICmp( sp, TEXT("wmv") ) == 0 ||
								StrICmp( sp - 1, TEXT("jpeg") ) == 0 ) goto NOPRESS ;
						}
						
						// データが丸ごと入るメモリ領域の確保
						SrcBuf  = malloc( ( size_t )( FileSize + FileSize * 2 + 64 ) ) ;
						DestBuf = (u8 *)SrcBuf + FileSize ;
						
						// ファイルを丸ごと読み込む
						fread64( SrcBuf, FileSize, SrcP ) ;
						
						// 圧縮
						DestSize = Encode( SrcBuf, ( u32 )FileSize, DestBuf ) ;
						
						// 殆ど圧縮出来なかった場合は圧縮無しでアーカイブする
						if( (f64)DestSize / (f64)FileSize > 0.90 )
						{
							_fseeki64( SrcP, 0L, SEEK_SET ) ;
							free( SrcBuf ) ;
							goto NOPRESS ;
						}
						
						// 圧縮データを反転して書き出す
						WriteSize = ( DestSize + 3 ) / 4 * 4 ;
						KeyConvFileWrite( DestBuf, WriteSize, DestP, Key, File.DataSize ) ;
						
						// メモリの解放
						free( SrcBuf ) ;
						
						// 圧縮データのサイズを保存する
						File.PressDataSize = DestSize ;
					}
					else
					{
NOPRESS:					
						// 転送開始
						WriteSize = 0 ;
						while( WriteSize < FileSize )
						{
							// 転送サイズ決定
							MoveSize = DXA_BUFFERSIZE_VER6 < FileSize - WriteSize ? DXA_BUFFERSIZE_VER6 : FileSize - WriteSize ;
							MoveSize = ( MoveSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
							
							// ファイルの反転読み込み
							KeyConvFileRead( TempBuffer, MoveSize, SrcP, Key, File.DataSize + WriteSize ) ;

							// 書き出し
							fwrite64( TempBuffer, MoveSize, DestP ) ;
							
							// 書き出しサイズの加算
							WriteSize += MoveSize ;
						}
					}
					
					// 書き出したファイルを閉じる
					fclose( SrcP ) ;
				
					// データサイズの加算
					Size->DataSize += WriteSize ;
				}
				
				// ファイルヘッダを書き出す
				memcpy( FileP + Dir.FileHeadAddress + sizeof( DARC_FILEHEAD_VER6 ) * i, &File, sizeof( DARC_FILEHEAD_VER6 ) ) ;
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

#include <vector>

// 指定のディレクトリデータにあるファイルを展開する
int DXArchive_VER6::DirectoryDecode( u8 *NameP, u8 *DirP, u8 *FileP, DARC_HEAD_VER6 *Head, DARC_DIRECTORY_VER6 *Dir, FILE *ArcP, unsigned char *Key )
{
	TCHAR DirPath[MAX_PATH] ;
	
	// 現在のカレントディレクトリを保存
	GetCurrentDirectory( MAX_PATH, DirPath ) ;

	// ディレクトリ情報がある場合は、まず展開用のディレクトリを作成する
	if( Dir->DirectoryAddress != 0xffffffffffffffff && Dir->ParentDirectoryAddress != 0xffffffffffffffff )
	{
		DARC_FILEHEAD_VER6 *DirFile ;
		
		// DARC_FILEHEAD_VER6 のアドレスを取得
		DirFile = ( DARC_FILEHEAD_VER6 * )( FileP + Dir->DirectoryAddress ) ;
		
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
		DARC_FILEHEAD_VER6 *File ;

		// 格納されているファイルの数だけ繰り返す
		FileHeadSize = sizeof( DARC_FILEHEAD_VER6 ) ;
		File = ( DARC_FILEHEAD_VER6 * )( FileP + Dir->FileHeadAddress ) ;
		for( i = 0 ; i < Dir->FileHeadNum ; i ++, File = (DARC_FILEHEAD_VER6 *)( (u8 *)File + FileHeadSize ) )
		{
			// ディレクトリかどうかで処理を分岐
			if( File->Attributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				// ディレクトリの場合は再帰をかける
				DirectoryDecode( NameP, DirP, FileP, Head, ( DARC_DIRECTORY_VER6 * )( DirP + File->DataAddress ), ArcP, Key ) ;
			}
			else
			{
				FILE *DestP ;
				void *Buffer ;
			
				// ファイルの場合は展開する
				
				// バッファを確保する
				Buffer = malloc( DXA_BUFFERSIZE_VER6 ) ;
				if( Buffer == NULL ) return -1 ;

				// ファイルを開く
				TCHAR *pName = GetOriginalFileName(NameP + File->NameAddress);

				DestP = _tfopen(pName, TEXT("wb"));

				delete[] pName;
			
				// データがある場合のみ転送
				if( File->DataSize != 0 )
				{
					// 初期位置をセットする
					if( _ftelli64( ArcP ) != ( s32 )( Head->DataStartAddress + File->DataAddress ) )
						_fseeki64( ArcP, Head->DataStartAddress + File->DataAddress, SEEK_SET ) ;
						
					// データが圧縮されているかどうかで処理を分岐
					if( File->PressDataSize != 0xffffffffffffffff )
					{
						void *temp ;
						
						// 圧縮されている場合

						// 圧縮データが収まるメモリ領域の確保
						temp = malloc( ( size_t )( File->PressDataSize + File->DataSize ) ) ;

						// 圧縮データの読み込み
						KeyConvFileRead( temp, File->PressDataSize, ArcP, Key, File->DataSize ) ;
						
						// 解凍
						Decode( temp, (u8 *)temp + File->PressDataSize ) ;
						
						// 書き出し
						fwrite64( (u8 *)temp + File->PressDataSize, File->DataSize, DestP ) ;
						
						// メモリの解放
						free( temp ) ;
					}
					else
					{
						// 圧縮されていない場合
					
						// 転送処理開始
						{
							u64 MoveSize, WriteSize ;
							
							WriteSize = 0 ;
							while( WriteSize < File->DataSize )
							{
								MoveSize = File->DataSize - WriteSize > DXA_BUFFERSIZE_VER6 ? DXA_BUFFERSIZE_VER6 : File->DataSize - WriteSize ;

								// ファイルの反転読み込み
								KeyConvFileRead( Buffer, MoveSize, ArcP, Key, File->DataSize + WriteSize ) ;

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
int DXArchive_VER6::GetDirectoryFilePath( const TCHAR *DirectoryPath, TCHAR *FileNameBuffer )
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

// エンコード( 戻り値:圧縮後のサイズ  -1 はエラー  Dest に NULL を入れることも可能 )
int DXArchive_VER6::Encode( void *Src, u32 SrcSize, void *Dest )
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
		for( m = 0, listtemp = list->next ; m < MAX_SEARCHLISTNUM && listtemp != NULL ; listtemp = listtemp->next, m ++ )
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
		}
	}

	// 圧縮後のデータサイズを保存する
	*((u32 *)&destp[4]) = dstsize + 9 ;

	// 確保したメモリの解放
	free( usesublistflagtable ) ;

	// データのサイズを返す
	return dstsize + 9 ;
}

// デコード( 戻り値:解凍後のサイズ  -1 はエラー  Dest に NULL を入れることも可能 )
int DXArchive_VER6::Decode( void *Src, void *Dest )
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


// アーカイブファイルを作成する(ディレクトリ一個だけ)
int DXArchive_VER6::EncodeArchiveOneDirectory(TCHAR *OutputFileName, TCHAR *DirectoryPath, bool Press, const char *KeyString )
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
	Result = EncodeArchive( OutputFileName, FilePathList, FileNum, Press, KeyString ) ;

	// 確保したメモリの解放
	free( NameBuffer ) ;

	// 結果を返す
	return Result ;
}

// アーカイブファイルを作成する
int DXArchive_VER6::EncodeArchive(TCHAR *OutputFileName, TCHAR **FileOrDirectoryPath, int FileNum, bool Press, const char *KeyString )
{
	DARC_HEAD_VER6 Head ;
	DARC_DIRECTORY_VER6 Directory ;
	SIZESAVE SizeSave ;
	FILE *DestFp ;
	u8 *NameP, *FileP, *DirP ;
	int i ;
	u32 Type ;
	void *TempBuffer ;
	u8 Key[DXA_KEYSTR_LENGTH_VER6] ;

	// 鍵文字列の作成
	KeyCreate( KeyString, Key ) ;

	// ファイル読み込みに使用するバッファの確保
	TempBuffer = malloc( DXA_BUFFERSIZE_VER6 ) ;
	
	// 出力ファイルを開く
	DestFp = _tfopen( OutputFileName, TEXT("wb") ) ;

	// アーカイブのヘッダを出力する
	{
		Head.Head						= DXA_HEAD_VER6 ;
		Head.Version					= DXA_VER_VER6 ;
		Head.HeadSize					= 0xffffffff ;
		Head.DataStartAddress			= sizeof( DARC_HEAD_VER6 ) ;
		Head.FileNameTableStartAddress	= 0xffffffffffffffff ;
		Head.DirectoryTableStartAddress	= 0xffffffffffffffff ;
		Head.FileTableStartAddress		= 0xffffffffffffffff ;
		Head.CodePage					= GetACP() ;
		SetFileApisToANSI() ;

		KeyConvFileWrite( &Head, sizeof( DARC_HEAD_VER6 ), DestFp, Key, 0 ) ;
	}
	
	// 各バッファを確保する
	NameP = ( u8 * )malloc( DXA_BUFFERSIZE_VER6 ) ;
	if( NameP == NULL ) return -1 ;
	memset( NameP, 0, DXA_BUFFERSIZE_VER6 ) ;

	FileP = ( u8 * )malloc( DXA_BUFFERSIZE_VER6 ) ;
	if( FileP == NULL ) return -1 ;
	memset( FileP, 0, DXA_BUFFERSIZE_VER6 ) ;

	DirP = ( u8 * )malloc( DXA_BUFFERSIZE_VER6 ) ;
	if( DirP == NULL ) return -1 ;
	memset( DirP, 0, DXA_BUFFERSIZE_VER6 ) ;

	// サイズ保存構造体にデータをセット
	SizeSave.DataSize		= 0 ;
	SizeSave.NameSize		= 0 ;
	SizeSave.DirectorySize	= 0 ;
	SizeSave.FileSize		= 0 ;
	
	// 最初のディレクトリのファイル情報を書き出す
	{
		DARC_FILEHEAD_VER6 File ;
		
		memset( &File, 0, sizeof( DARC_FILEHEAD_VER6 ) ) ;
		File.NameAddress	= SizeSave.NameSize ;
		File.Attributes		= FILE_ATTRIBUTE_DIRECTORY ;
		File.DataAddress	= SizeSave.DirectorySize ;
		File.DataSize		= 0 ;
		File.PressDataSize  = 0xffffffffffffffff ;

		// ディレクトリ名の書き出し
		SizeSave.NameSize += AddFileNameData(TEXT(""), NameP + SizeSave.NameSize ) ;

		// ファイル情報の書き出し
		memcpy( FileP + SizeSave.FileSize, &File, sizeof( DARC_FILEHEAD_VER6 ) ) ;
		SizeSave.FileSize += sizeof( DARC_FILEHEAD_VER6 ) ;
	}

	// 最初のディレクトリ情報を書き出す
	Directory.DirectoryAddress 			= 0 ;
	Directory.ParentDirectoryAddress 	= 0xffffffffffffffff ;
	Directory.FileHeadNum 				= FileNum ;
	Directory.FileHeadAddress 			= SizeSave.FileSize ;
	memcpy( DirP + SizeSave.DirectorySize, &Directory, sizeof( DARC_DIRECTORY_VER6 ) ) ;

	// サイズを加算する
	SizeSave.DirectorySize 	+= sizeof( DARC_DIRECTORY_VER6 ) ;
	SizeSave.FileSize 		+= sizeof( DARC_FILEHEAD_VER6 ) * FileNum ;

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
			DirectoryEncode( FileOrDirectoryPath[i], NameP, DirP, FileP, &Directory, &SizeSave, i, DestFp, TempBuffer, Press, Key ) ;
		}
		else
		{
			WIN32_FIND_DATA FindData ;
			HANDLE FindHandle ;
			DARC_FILEHEAD_VER6 File ;
	
			// ファイルの情報を得る
			FindHandle = FindFirstFile( FileOrDirectoryPath[i], &FindData ) ;
			if( FindHandle == INVALID_HANDLE_VALUE ) continue ;
			
			// ファイルヘッダをセットする
			{
				File.NameAddress     = SizeSave.NameSize ;
				File.Time.Create     = ( ( ( LONGLONG )FindData.ftCreationTime.dwHighDateTime   ) << 32 ) + FindData.ftCreationTime.dwLowDateTime   ;
				File.Time.LastAccess = ( ( ( LONGLONG )FindData.ftLastAccessTime.dwHighDateTime ) << 32 ) + FindData.ftLastAccessTime.dwLowDateTime ;
				File.Time.LastWrite  = ( ( ( LONGLONG )FindData.ftLastWriteTime.dwHighDateTime  ) << 32 ) + FindData.ftLastWriteTime.dwLowDateTime  ;
				File.Attributes      = FindData.dwFileAttributes ;
				File.DataAddress     = SizeSave.DataSize ;
				File.DataSize        = ( ( ( LONGLONG )FindData.nFileSizeHigh ) << 32 ) + FindData.nFileSizeLow ;
				File.PressDataSize	 = 0xffffffffffffffff ;
			}

			// ファイル名を書き出す
			SizeSave.NameSize += AddFileNameData( FindData.cFileName, NameP + SizeSave.NameSize ) ;

			// ファイルデータを書き出す
			if( File.DataSize != 0 )
			{
				FILE *SrcP ;
				u64 FileSize, WriteSize, MoveSize ;

				// ファイルを開く
				SrcP = _tfopen( FileOrDirectoryPath[i], TEXT("rb") ) ;
				
				// サイズを得る
				_fseeki64( SrcP, 0, SEEK_END ) ;
				FileSize = _ftelli64( SrcP ) ;
				_fseeki64( SrcP, 0, SEEK_SET ) ;
				
				// ファイルサイズが 10MB 以下の場合で、圧縮の指定がある場合は圧縮を試みる
				if( Press == true && File.DataSize < 10 * 1024 * 1024 )
				{
					void *SrcBuf, *DestBuf ;
					u32 DestSize, Len ;
					
					// 一部のファイル形式の場合は予め弾く
					if( ( Len = ( int )_tcslen( FindData.cFileName ) ) > 4 )
					{
						TCHAR *sp ;
						
						sp = &FindData.cFileName[Len-3] ;
						if( StrICmp( sp, TEXT("wav") ) == 0 ||
							StrICmp( sp, TEXT("jpg") ) == 0 ||
							StrICmp( sp, TEXT("png") ) == 0 ||
							StrICmp( sp, TEXT("mpg") ) == 0 ||
							StrICmp( sp, TEXT("mp3") ) == 0 ||
							StrICmp( sp, TEXT("ogg") ) == 0 ||
							StrICmp( sp, TEXT("wmv") ) == 0 ||
							StrICmp( sp - 1, TEXT("jpeg") ) == 0 ) goto NOPRESS ;
					}
					
					// データが丸ごと入るメモリ領域の確保
					SrcBuf  = malloc( ( size_t )( FileSize + FileSize * 2 + 64 ) ) ;
					DestBuf = (u8 *)SrcBuf + FileSize ;
					
					// ファイルを丸ごと読み込む
					fread64( SrcBuf, FileSize, SrcP ) ;
					
					// 圧縮
					DestSize = Encode( SrcBuf, ( u32 )FileSize, DestBuf ) ;
					
					// 殆ど圧縮出来なかった場合は圧縮無しでアーカイブする
					if( (f64)DestSize / (f64)FileSize > 0.90 )
					{
						_fseeki64( SrcP, 0L, SEEK_SET ) ;
						free( SrcBuf ) ;
						goto NOPRESS ;
					}
					
					// 圧縮データを反転して書き出す
					WriteSize = ( DestSize + 3 ) / 4 * 4 ;
					KeyConvFileWrite( DestBuf, WriteSize, DestFp, Key, File.DataSize ) ;
					
					// メモリの解放
					free( SrcBuf ) ;
					
					// 圧縮データのサイズを保存する
					File.PressDataSize = DestSize ;
				}
				else
				{
NOPRESS:					
					// 転送開始
					WriteSize = 0 ;
					while( WriteSize < FileSize )
					{
						// 転送サイズ決定
						MoveSize = DXA_BUFFERSIZE_VER6 < FileSize - WriteSize ? DXA_BUFFERSIZE_VER6 : FileSize - WriteSize ;
						MoveSize = ( MoveSize + 3 ) / 4 * 4 ;	// サイズは４の倍数に合わせる
						
						// ファイルの反転読み込み
						KeyConvFileRead( TempBuffer, MoveSize, SrcP, Key, File.DataSize + WriteSize ) ;

						// 書き出し
						fwrite64( TempBuffer, MoveSize, DestFp ) ;
						
						// 書き出しサイズの加算
						WriteSize += MoveSize ;
					}
				}
				
				// 書き出したファイルを閉じる
				fclose( SrcP ) ;
			
				// データサイズの加算
				SizeSave.DataSize += WriteSize ;
			}
			
			// ファイルヘッダを書き出す
			memcpy( FileP + Directory.FileHeadAddress + sizeof( DARC_FILEHEAD_VER6 ) * i, &File, sizeof( DARC_FILEHEAD_VER6 ) ) ;

			// Find ハンドルを閉じる
			FindClose( FindHandle ) ;
		}
	}
	
	// バッファに溜め込んだ各種ヘッダデータを出力する
	{
		// 出力する順番は ファイルネームテーブル、 DARC_FILEHEAD_VER6 テーブル、 DARC_DIRECTORY_VER6 テーブル の順
		KeyConvFileWrite( NameP, SizeSave.NameSize,      DestFp, Key, 0 ) ;
		KeyConvFileWrite( FileP, SizeSave.FileSize,      DestFp, Key, SizeSave.NameSize ) ;
		KeyConvFileWrite( DirP,  SizeSave.DirectorySize, DestFp, Key, SizeSave.NameSize + SizeSave.FileSize ) ;
	}
		
	// 再びアーカイブのヘッダを出力する
	{
		Head.Head                       = DXA_HEAD_VER6 ;
		Head.Version                    = DXA_VER_VER6 ;
		Head.HeadSize                   = ( u32 )( SizeSave.NameSize + SizeSave.DirectorySize + SizeSave.FileSize ) ;
		Head.DataStartAddress           = sizeof( DARC_HEAD_VER6 ) ;
		Head.FileNameTableStartAddress  = SizeSave.DataSize + Head.DataStartAddress ;
		Head.FileTableStartAddress      = SizeSave.NameSize ;
		Head.DirectoryTableStartAddress = Head.FileTableStartAddress + SizeSave.FileSize ;

		_fseeki64( DestFp, 0, SEEK_SET ) ;
		KeyConvFileWrite( &Head, sizeof( DARC_HEAD_VER6 ), DestFp, Key, 0 ) ;
	}
	
	// 書き出したファイルを閉じる
	fclose( DestFp ) ;
	
	// 確保したバッファを開放する
	free( NameP ) ;
	free( FileP ) ;
	free( DirP ) ;
	free( TempBuffer ) ;

	// 終了
	return 0 ;
}

// アーカイブファイルを展開する
int DXArchive_VER6::DecodeArchive(TCHAR *ArchiveName, const TCHAR *OutputPath, const char *KeyString )
{
	u8 *HeadBuffer = NULL ;
	DARC_HEAD_VER6 Head ;
	u8 *FileP, *NameP, *DirP ;
	FILE *ArcP = NULL ;
	TCHAR OldDir[MAX_PATH] ;
	u8 Key[DXA_KEYSTR_LENGTH_VER6] ;

	// 鍵文字列の作成
	KeyCreate( KeyString, Key ) ;

	// アーカイブファイルを開く
	ArcP = _tfopen( ArchiveName, TEXT("rb") ) ;
	if( ArcP == NULL ) return -1 ;

	// 出力先のディレクトリにカレントディレクトリを変更する
	GetCurrentDirectory( MAX_PATH, OldDir ) ;
	SetCurrentDirectory( OutputPath ) ;

	// ヘッダを解析する
	{
		KeyConvFileRead( &Head, sizeof( DARC_HEAD_VER6 ), ArcP, Key, 0 ) ;

		// ＩＤの検査
		if( Head.Head != DXA_HEAD_VER6 )
			goto ERR ;
		
		// バージョン検査
		if( Head.Version > DXA_VER_VER6 || Head.Version < 0x0006 ) goto ERR ;
		
		// ヘッダのサイズ分のメモリを確保する
		HeadBuffer = ( u8 * )malloc( ( size_t )Head.HeadSize ) ;
		if( HeadBuffer == NULL ) goto ERR ;
		
		// ヘッダパックをメモリに読み込む
		_fseeki64( ArcP, Head.FileNameTableStartAddress, SEEK_SET ) ;
		KeyConvFileRead( HeadBuffer, Head.HeadSize, ArcP, Key, 0 ) ;
		
		// 各アドレスをセットする
		NameP = HeadBuffer ;
		FileP = NameP + Head.FileTableStartAddress ;
		DirP  = NameP + Head.DirectoryTableStartAddress ;
	}

	// アーカイブの展開を開始する
	DirectoryDecode( NameP, DirP, FileP, &Head, ( DARC_DIRECTORY_VER6 * )DirP, ArcP, Key ) ;
	
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
DXArchive_VER6::DXArchive_VER6(TCHAR *ArchivePath )
{
	this->fp = NULL ;
	this->HeadBuffer = NULL ;
	this->NameP = this->DirP = this->FileP = NULL ;
	this->CurrentDirectory = NULL ;
	this->CashBuffer = NULL ;

	if( ArchivePath != NULL )
	{
		this->OpenArchiveFile( ArchivePath ) ;
	}
}

// デストラクタ
DXArchive_VER6::~DXArchive_VER6()
{
	if( this->fp != NULL ) this->CloseArchiveFile() ;

	if( this->fp != NULL ) fclose( this->fp ) ;
	if( this->HeadBuffer != NULL ) free( this->HeadBuffer ) ;
	if( this->CashBuffer != NULL ) free( this->CashBuffer ) ;

	this->fp = NULL ;
	this->HeadBuffer = NULL ;
	this->NameP = this->DirP = this->FileP = NULL ;
	this->CurrentDirectory = NULL ;
}

// 指定のディレクトリデータの暗号化を解除する( 丸ごとメモリに読み込んだ場合用 )
int DXArchive_VER6::DirectoryKeyConv( DARC_DIRECTORY_VER6 *Dir )
{
	// メモリイメージではない場合はエラー
	if( this->MemoryOpenFlag == false )
		return -1 ;

	// バージョン 0x0005 より前では何もしない
	if( this->Head.Version < 0x0005 )
		return 0 ;
	
	// 暗号化解除処理開始
	{
		u32 i, FileHeadSize ;
		DARC_FILEHEAD_VER6 *File ;

		// 格納されているファイルの数だけ繰り返す
		FileHeadSize = sizeof( DARC_FILEHEAD_VER6 ) ;
		File = ( DARC_FILEHEAD_VER6 * )( this->FileP + Dir->FileHeadAddress ) ;
		for( i = 0 ; i < Dir->FileHeadNum ; i ++, File = (DARC_FILEHEAD_VER6 *)( (u8 *)File + FileHeadSize ) )
		{
			// ディレクトリかどうかで処理を分岐
			if( File->Attributes & FILE_ATTRIBUTE_DIRECTORY )
			{
				// ディレクトリの場合は再帰をかける
				DirectoryKeyConv( ( DARC_DIRECTORY_VER6 * )( this->DirP + File->DataAddress ) ) ;
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

					// データが圧縮されているかどうかで処理を分岐
					if( File->PressDataSize != 0xffffffffffffffff )
					{
						// 圧縮されている場合
						KeyConv( DataP, File->PressDataSize, File->DataSize, this->Key ) ;
					}
					else
					{
						// 圧縮されていない場合
						KeyConv( DataP, File->DataSize, File->DataSize, this->Key ) ;
					}
				}
			}
		}
	}

	// 終了
	return 0 ;
}

// メモリ上にあるアーカイブイメージを開く( 0:成功  -1:失敗 )
int	DXArchive_VER6::OpenArchiveMem( void *ArchiveImage, s64 ArchiveSize, const char *KeyString )
{
	u8 *datp ;

	// 既になんらかのアーカイブを開いていた場合はエラー
	if( this->fp != NULL ) return -1 ;

	// 鍵の作成
	KeyCreate( KeyString, this->Key ) ;

	// 最初にヘッダの部分を反転する
	memcpy( &this->Head, ArchiveImage, sizeof( DARC_HEAD_VER6 ) ) ;
	KeyConv( &this->Head, sizeof( DARC_HEAD_VER6 ), 0, this->Key ) ;

	// ＩＤが違う場合はエラー
	if( Head.Head != DXA_HEAD_VER6 )
		goto ERR ;

	// ポインタを保存
	this->fp = (FILE *)ArchiveImage ;
	datp = (u8 *)ArchiveImage ;

	// ヘッダを解析する
	{
		memcpy( &this->Head, datp, sizeof( DARC_HEAD_VER6 ) ) ;
		KeyConv( &this->Head, sizeof( DARC_HEAD_VER6 ), 0, this->Key ) ;

		datp += sizeof( DARC_HEAD_VER6 ) ;

		// ＩＤの検査
		if( this->Head.Head != DXA_HEAD_VER6 ) goto ERR ;
		
		// バージョン検査
		if( this->Head.Version > DXA_VER_VER6 || this->Head.Version < 0x0006 ) goto ERR ;

		// ヘッダパックのアドレスをセットする
		this->HeadBuffer = (u8 *)this->fp + this->Head.FileNameTableStartAddress ;

		// 各アドレスをセットする
		KeyConv( this->HeadBuffer, this->Head.HeadSize, 0, this->Key ) ;
		this->NameP = this->HeadBuffer ;
		this->FileP = this->NameP + this->Head.FileTableStartAddress ;
		this->DirP = this->NameP + this->Head.DirectoryTableStartAddress ;
	}

	// カレントディレクトリのセット
	this->CurrentDirectory = ( DARC_DIRECTORY_VER6 * )this->DirP ;

	// メモリイメージから開いているフラグを立てる
	MemoryOpenFlag = true ;

	// 全てのファイルの暗号化を解除する
	DirectoryKeyConv( ( DARC_DIRECTORY_VER6 * )this->DirP ) ;

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

// アーカイブファイルを開き最初にすべてメモリ上に読み込んでから処理する( 0:成功  -1:失敗 )
int DXArchive_VER6::OpenArchiveFileMem( const TCHAR *ArchivePath, const char *KeyString )
{
	FILE *fp ;
	u8 *datp ;
	void *ArchiveImage ;
	s64 ArchiveSize ;

	// 既になんらかのアーカイブを開いていた場合はエラー
	if( this->fp != NULL ) return -1 ;

	// 鍵の作成
	KeyCreate( KeyString, this->Key ) ;

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

	// 最初にヘッダの部分を反転する
	memcpy( &this->Head, ArchiveImage, sizeof( DARC_HEAD_VER6 ) ) ;
	KeyConv( &this->Head, sizeof( DARC_HEAD_VER6 ), 0, this->Key ) ;

	// ＩＤが違う場合はエラー
	if( Head.Head != DXA_HEAD_VER6 )
		return -1 ;

	// ポインタを保存
	this->fp = (FILE *)ArchiveImage ;
	datp = (u8 *)ArchiveImage ;

	// ヘッダを解析する
	{
		memcpy( &this->Head, datp, sizeof( DARC_HEAD_VER6 ) ) ;
		KeyConv( &this->Head, sizeof( DARC_HEAD_VER6 ), 0, this->Key ) ;

		datp += sizeof( DARC_HEAD_VER6 ) ;
		
		// ＩＤの検査
		if( this->Head.Head != DXA_HEAD_VER6 ) goto ERR ;
		
		// バージョン検査
		if( this->Head.Version > DXA_VER_VER6 || this->Head.Version < 0x0006 ) goto ERR ;

		// ヘッダパックのアドレスをセットする
		this->HeadBuffer = (u8 *)this->fp + this->Head.FileNameTableStartAddress ;

		// 各アドレスをセットする
		KeyConv( this->HeadBuffer, this->Head.HeadSize, 0, this->Key ) ;
		this->NameP = this->HeadBuffer ;
		this->FileP = this->NameP + this->Head.FileTableStartAddress ;
		this->DirP = this->NameP + this->Head.DirectoryTableStartAddress ;
	}

	// カレントディレクトリのセット
	this->CurrentDirectory = ( DARC_DIRECTORY_VER6 * )this->DirP ;

	// メモリイメージから開いているフラグを立てる
	MemoryOpenFlag = true ;

	// 全てのファイルの暗号化を解除する
	DirectoryKeyConv( ( DARC_DIRECTORY_VER6 * )this->DirP ) ;
	
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

// アーカイブファイルを開く
int DXArchive_VER6::OpenArchiveFile( const TCHAR *ArchivePath, const char *KeyString )
{
	// 既になんらかのアーカイブを開いていた場合はエラー
	if( this->fp != NULL ) return -1 ;

	// アーカイブファイルを開こうと試みる
	this->fp = _tfopen( ArchivePath, TEXT("rb") ) ;
	if( this->fp == NULL ) return -1 ;

	// 鍵文字列の作成
	KeyCreate( KeyString, this->Key ) ;

	// ヘッダを解析する
	{
		KeyConvFileRead( &this->Head, sizeof( DARC_HEAD_VER6 ), this->fp, this->Key, 0 ) ;
		
		// ＩＤの検査
		if( this->Head.Head != DXA_HEAD_VER6 )
			goto ERR ;
		
		// バージョン検査
		if( this->Head.Version > DXA_VER_VER6 || this->Head.Version < 0x0006 ) goto ERR ;
		
		// ヘッダのサイズ分のメモリを確保する
		this->HeadBuffer = (u8 * )malloc( ( size_t )this->Head.HeadSize ) ;
		if( this->HeadBuffer == NULL ) goto ERR ;
		
		// ヘッダパックをメモリに読み込む
		_fseeki64( this->fp, this->Head.FileNameTableStartAddress, SEEK_SET ) ;
		KeyConvFileRead( this->HeadBuffer, this->Head.HeadSize, this->fp, this->Key, 0 ) ;
		
		// 各アドレスをセットする
		this->NameP = this->HeadBuffer ;
		this->FileP = this->NameP + this->Head.FileTableStartAddress ;
		this->DirP = this->NameP + this->Head.DirectoryTableStartAddress ;
	}

	// カレントディレクトリのセット
	this->CurrentDirectory = ( DARC_DIRECTORY_VER6 * )this->DirP ;

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

// アーカイブファイルを閉じる
int DXArchive_VER6::CloseArchiveFile( void )
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
			DirectoryKeyConv( ( DARC_DIRECTORY_VER6 * )this->DirP ) ;
			KeyConv( this->HeadBuffer, this->Head.HeadSize, 0, this->Key ) ;
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
		
		// ヘッダバッファも解放
		free( this->HeadBuffer ) ;
	}

	// ポインタ初期化
	this->fp = NULL ;
	this->HeadBuffer = NULL ;
	this->NameP = this->DirP = this->FileP = NULL ;
	this->CurrentDirectory = NULL ;

	// 終了
	return 0 ;
}

// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
int	DXArchive_VER6::ChangeCurrentDirectoryFast( SEARCHDATA *SearchData )
{
	DARC_FILEHEAD_VER6 *FileH ;
	int i, j, k, Num ;
	u8 *NameData, *PathData ;
	u16 PackNum, Parity ;

	PackNum  = SearchData->PackNum ;
	Parity   = SearchData->Parity ;
	PathData = SearchData->FileName ;

	// カレントディレクトリから同名のディレクトリを探す
	FileH = ( DARC_FILEHEAD_VER6 * )( this->FileP + this->CurrentDirectory->FileHeadAddress ) ;
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
	this->CurrentDirectory = ( DARC_DIRECTORY_VER6 * )( this->DirP + FileH->DataAddress ) ;

	// 正常終了
	return 0 ;
}

// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
int DXArchive_VER6::ChangeCurrentDir( const TCHAR *DirPath )
{
	return ChangeCurrentDirectoryBase( DirPath, true ) ;
}

// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
int	DXArchive_VER6::ChangeCurrentDirectoryBase( const TCHAR *DirectoryPath, bool ErrorIsDirectoryReset, SEARCHDATA *LastSearchData )
{
	DARC_DIRECTORY_VER6 *OldDir ;
	SEARCHDATA SearchData ;

	// ここに留まるパスだったら無視
	if( _tcscmp( DirectoryPath, TEXT(".") ) == 0 ) return 0 ;

	// 『\』だけの場合はルートディレクトリに戻る
	if( _tcscmp( DirectoryPath, TEXT("\\") ) == 0 )
	{
		this->CurrentDirectory = ( DARC_DIRECTORY_VER6 * )this->DirP ;
		return 0 ;
	}

	// 下に一つ下がるパスだったら処理を分岐
	if( _tcscmp( DirectoryPath, TEXT("..") ) == 0 )
	{
		// ルートディレクトリに居たらエラー
		if( this->CurrentDirectory->ParentDirectoryAddress == 0xffffffffffffffff ) return -1 ;
		
		// 親ディレクトリがあったらそちらに移る
		this->CurrentDirectory = ( DARC_DIRECTORY_VER6 * )( this->DirP + this->CurrentDirectory->ParentDirectoryAddress ) ;
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
		FileH = ( DARC_FILEHEAD_VER6 * )( this->FileP + this->CurrentDirectory->FileHeadAddress ) ;
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
int DXArchive_VER6::GetCurrentDir(TCHAR *DirPathBuffer, int BufferLength )
{
	TCHAR DirPath[MAX_PATH] ;
	DARC_DIRECTORY_VER6 *Dir[200], *DirTempP ;
	int Depth, i ;

	// ルートディレクトリに着くまで検索する
	Depth = 0 ;
	DirTempP = this->CurrentDirectory ;
	while( DirTempP->DirectoryAddress != 0xffffffffffffffff && DirTempP->DirectoryAddress != 0 )
	{
		Dir[Depth] = DirTempP ;
		DirTempP = ( DARC_DIRECTORY_VER6 * )( this->DirP + DirTempP->ParentDirectoryAddress ) ;
		Depth ++ ;
	}
	
	// パス名を連結する
	DirPath[0] = '\0' ;
	for( i = Depth - 1 ; i >= 0 ; i -- )
	{
		_tcscat( DirPath, TEXT("\\") ) ;
		_tcscat( DirPath, (TCHAR * )( this->NameP + ( ( DARC_FILEHEAD_VER6 * )( this->FileP + Dir[i]->DirectoryAddress ) )->NameAddress ) ) ;
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
s64 DXArchive_VER6::LoadFileToMem( const TCHAR *FilePath, void *Buffer, u64 BufferLength )
{
	DARC_FILEHEAD_VER6 *FileH ;

	// 指定のファイルの情報を得る
	FileH = this->GetFileInfo( FilePath ) ;
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
			KeyConvFileRead( temp, FileH->PressDataSize, this->fp, this->Key, FileH->DataSize ) ;
			
			// 解凍
			Decode( temp, Buffer ) ;
			
			// メモリの解放
			free( temp ) ;
		}
	}
	else
	{
		// 圧縮されていない場合

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
			KeyConvFileRead( Buffer, FileH->DataSize, this->fp, this->Key, FileH->DataSize ) ;
		}
	}
	
	// 終了
	return 0 ;
}

// アーカイブファイル中の指定のファイルをサイズを取得する
s64 DXArchive_VER6::GetFileSize( const TCHAR *FilePath )
{
	// ファイルサイズを返す
	return this->LoadFileToMem( FilePath, NULL, 0 ) ;
}

// アーカイブファイルをメモリに読み込んだ場合のファイルイメージが格納されている先頭アドレスを取得する( メモリから開いている場合のみ有効 )
void *DXArchive_VER6::GetFileImage( void )
{
	// メモリイメージから開いていなかったらエラー
	if( MemoryOpenFlag == false ) return NULL ;

	// 先頭アドレスを返す
	return this->fp ;
}

// アーカイブファイル中の指定のファイルのファイル内の位置とファイルの大きさを得る( -1:エラー )
int DXArchive_VER6::GetFileInfo( const TCHAR *FilePath, u64 *PositionP, u64 *SizeP )
{
	DARC_FILEHEAD_VER6 *FileH ;

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
void *DXArchive_VER6::LoadFileToCash( const TCHAR *FilePath )
{
	s64 FileSize ;

	// ファイルサイズを取得する
	FileSize = this->GetFileSize( FilePath ) ;

	// ファイルが無かったらエラー
	if( FileSize < 0 ) return NULL ;

	// 確保しているキャッシュバッファのサイズよりも大きい場合はバッファを再確保する
	if( FileSize > ( s64 )this->CashBufferSize )
	{
		// キャッシュバッファのクリア
		this->ClearCash() ;

		// キャッシュバッファの再確保
		this->CashBuffer = malloc( ( size_t )FileSize ) ;

		// 確保に失敗した場合は NULL を返す
		if( this->CashBuffer == NULL ) return NULL ;

		// キャッシュバッファのサイズを保存
		this->CashBufferSize = FileSize ;
	}

	// ファイルをメモリに読み込む
	this->LoadFileToMem( FilePath, this->CashBuffer, FileSize ) ;

	// キャッシュバッファのアドレスを返す
	return this->CashBuffer ;
}

// キャッシュバッファを開放する
int DXArchive_VER6::ClearCash( void )
{
	// メモリが確保されていたら解放する
	if( this->CashBuffer != NULL )
	{
		free( this->CashBuffer ) ;
		this->CashBuffer = NULL ;
		this->CashBufferSize = 0 ;
	}

	// 終了
	return 0 ;
}

	
// アーカイブファイル中の指定のファイルを開き、ファイルアクセス用オブジェクトを作成する
DXArchiveFile_VER6*DXArchive_VER6::OpenFile( const TCHAR *FilePath )
{
	DARC_FILEHEAD_VER6 *FileH ;
	DXArchiveFile_VER6*CDArc = NULL ;

	// メモリから開いている場合は無効
	if( MemoryOpenFlag == true ) return NULL ;

	// 指定のファイルの情報を得る
	FileH = this->GetFileInfo( FilePath ) ;
	if( FileH == NULL ) return NULL ;

	// 新しく DXArchiveFile クラスを作成する
	CDArc = new DXArchiveFile_VER6( FileH, this ) ;
	
	// DXArchiveFile_VER6 クラスのポインタを返す
	return CDArc ;
}













// コンストラクタ
DXArchiveFile_VER6::DXArchiveFile_VER6( DARC_FILEHEAD_VER6 *FileHead, DXArchive_VER6*Archive )
{
	this->FileData  = FileHead ;
	this->Archive   = Archive ;
	this->EOFFlag   = FALSE ;
	this->FilePoint = 0 ;
	this->DataBuffer = NULL ;
	
	// ファイルが圧縮されている場合はここで読み込んで解凍してしまう
	if( FileHead->PressDataSize != 0xffffffffffffffff )
	{
		void *temp ;

		// 圧縮データが収まるメモリ領域の確保
		temp = malloc( ( size_t )FileHead->PressDataSize ) ;

		// 解凍データが収まるメモリ領域の確保
		this->DataBuffer = malloc( ( size_t )FileHead->DataSize ) ;

		// 圧縮データの読み込み
		_fseeki64( this->Archive->GetFilePointer(), this->Archive->GetHeader()->DataStartAddress + FileHead->DataAddress, SEEK_SET ) ;
		DXArchive_VER6::KeyConvFileRead( temp, FileHead->PressDataSize, this->Archive->GetFilePointer(), this->Archive->GetKey(), FileHead->DataSize ) ;
		
		// 解凍
		DXArchive_VER6::Decode( temp, this->DataBuffer ) ;
		
		// メモリの解放
		free( temp ) ;
	}
}

// デストラクタ
DXArchiveFile_VER6::~DXArchiveFile_VER6()
{
	// メモリの解放
	if( this->DataBuffer != NULL )
	{
		free( this->DataBuffer ) ;
		this->DataBuffer = NULL ;
	}
}

// ファイルの内容を読み込む
s64 DXArchiveFile_VER6::Read( void *Buffer, s64 ReadLength )
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
		DXArchive_VER6::KeyConvFileRead( Buffer, ReadSize, this->Archive->GetFilePointer(), this->Archive->GetKey(), this->FileData->DataSize + this->FilePoint ) ;
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
s64 DXArchiveFile_VER6::Seek( s64 SeekPoint, s64 SeekMode )
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
s64 DXArchiveFile_VER6::Tell( void )
{
	return this->FilePoint ;
}

// ファイルの終端に来ているか、のフラグを得る
s64 DXArchiveFile_VER6::Eof( void )
{
	return this->EOFFlag ;
}

// ファイルのサイズを取得する
s64 DXArchiveFile_VER6::Size( void )
{
	return this->FileData->DataSize ;
}





WCHAR sjis2utf8_VER6(const char* sjis)
{
	LPCCH pSJIS = (LPCCH)sjis;

	int sjisSize = 2;
	WCHAR *pUTF8 = new WCHAR[sjisSize];
	MultiByteToWideChar(932, 0, (LPCCH)pSJIS, -1, pUTF8, 2);
	WCHAR utf8 = pUTF8[0];
	delete[] pUTF8;
	return utf8;
}
