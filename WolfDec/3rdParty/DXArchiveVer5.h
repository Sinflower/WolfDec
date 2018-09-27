// -------------------------------------------------------------------------------
// 
// 		ＤＸライブラリアーカイバ Ver5
// 
//	Creator			: 山田 巧
//	Creation Date	: 2003/09/11
//	Version			: 1.02
//
// -------------------------------------------------------------------------------

// 多重インクルード防止用定義
#ifndef __DXARCHIVE_VER5
#define __DXARCHIVE_VER5

// include --------------------------------------
#include <stdio.h>
#include <tchar.h>

// define ---------------------------------------

// データ型定義
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

#define DXA_HEAD_VER5			*((u16 *)"DX")	// ヘッダ
#define DXA_VER_VER5			(0x0005)		// バージョン
#define DXA_BUFFERSIZE_VER5		(0x1000000)		// アーカイブ作成時に使用するバッファのサイズ
#define DXA_KEYSTR_LENGTH_VER5	(12)			// 鍵文字列の長さ

/*
	バージョンごとの違い

	0x0002 DARC_FILEHEAD_VER5 に PressDataSize を追加
	0x0004 DARC_HEAD_VER5 に CodePage を追加
	0x0005 暗号化処理を一部変更
*/

/*
	データマップ
		
	DARC_HEAD_VER5
	ファイル実データ
	ファイル名テーブル
	DARC_FILEHEAD_VER5 テーブル
	DARC_DIRECTORY_VER5 テーブル
*/

/*
	ファイル名のデータ形式
	2byte:文字列の長さ(バイトサイズ÷４)
	2byte:文字列のパリティデータ(全ての文字の値を足したもの)
	英字は大文字に変換されたファイル名のデータ(４の倍数のサイズ)
	英字が大文字に変換されていないファイル名のデータ
*/

// struct ---------------------------------------

#pragma pack(push)
#pragma pack(1)

// アーカイブデータの最初のヘッダ
typedef struct tagDARC_HEAD_VER5
{
	u16 Head ;								// ヘッダ
	u16 Version ;							// バージョン
	u32 HeadSize ;							// ヘッダ情報の DARC_HEAD_VER5 を抜いた全サイズ
	u32 DataStartAddress ;					// 最初のファイルのデータが格納されているデータアドレス(ファイルの先頭アドレスをアドレス０とする)
	u32 FileNameTableStartAddress ;			// ファイル名テーブルの先頭アドレス(ファイルの先頭アドレスをアドレス０とする)
	u32 FileTableStartAddress ;				// ファイルテーブルの先頭アドレス(メンバ変数 FileNameTableStartAddress のアドレスを０とする)
	u32 DirectoryTableStartAddress ;		// ディレクトリテーブルの先頭アドレス(メンバ変数 FileNameTableStartAddress のアドレスを０とする)
											// アドレス０から配置されている DARC_DIRECTORY_VER5 構造体がルートディレクトリ
	u32 CodePage ;							// ファイル名に使用しているコードページ番号
} DARC_HEAD_VER5 ;

// アーカイブデータの最初のヘッダ(Ver 0x0003まで)
typedef struct tagDARC_HEAD_VER3
{
	u16 Head ;								// ヘッダ
	u16 Version ;							// バージョン
	u32 HeadSize ;							// ヘッダ情報の DARC_HEAD_VER5 を抜いた全サイズ
	u32 DataStartAddress ;					// 最初のファイルのデータが格納されているデータアドレス(ファイルの先頭アドレスをアドレス０とする)
	u32 FileNameTableStartAddress ;			// ファイル名テーブルの先頭アドレス(ファイルの先頭アドレスをアドレス０とする)
	u32 FileTableStartAddress ;				// ファイルテーブルの先頭アドレス(メンバ変数 FileNameTableStartAddress のアドレスを０とする)
	u32 DirectoryTableStartAddress ;		// ディレクトリテーブルの先頭アドレス(メンバ変数 FileNameTableStartAddress のアドレスを０とする)
											// アドレス０から配置されている DARC_DIRECTORY_VER5 構造体がルートディレクトリ
} DARC_HEAD_VER3 ;

// ファイルの時間情報
typedef struct tagDARC_FILETIME_VER5
{
	u64 Create ;			// 作成時間
	u64 LastAccess ;		// 最終アクセス時間
	u64 LastWrite ;			// 最終更新時間
} DARC_FILETIME_VER5 ;

// ファイル格納情報(Ver 0x0001)
typedef struct tagDARC_FILEHEAD_VER1
{
	u32 NameAddress ;			// ファイル名が格納されているアドレス( ARCHIVE_HEAD構造体 のメンバ変数 FileNameTableStartAddress のアドレスをアドレス０とする) 

	u32 Attributes ;			// ファイル属性
	DARC_FILETIME_VER5 Time ;	// 時間情報
	u32 DataAddress ;			// ファイルが格納されているアドレス
								//			ファイルの場合：DARC_HEAD_VER5構造体 のメンバ変数 DataStartAddress が示すアドレスをアドレス０とする
								//			ディレクトリの場合：DARC_HEAD_VER5構造体 のメンバ変数 DirectoryTableStartAddress のが示すアドレスをアドレス０とする
	u32 DataSize ;				// ファイルのデータサイズ
} DARC_FILEHEAD_VER1 ;

// ファイル格納情報
typedef struct tagDARC_FILEHEAD_VER5
{
	u32 NameAddress ;			// ファイル名が格納されているアドレス( ARCHIVE_HEAD構造体 のメンバ変数 FileNameTableStartAddress のアドレスをアドレス０とする) 

	u32 Attributes ;			// ファイル属性
	DARC_FILETIME_VER5 Time ;	// 時間情報
	u32 DataAddress ;			// ファイルが格納されているアドレス
								//			ファイルの場合：DARC_HEAD_VER5構造体 のメンバ変数 DataStartAddress が示すアドレスをアドレス０とする
								//			ディレクトリの場合：DARC_HEAD_VER5構造体 のメンバ変数 DirectoryTableStartAddress のが示すアドレスをアドレス０とする
	u32 DataSize ;				// ファイルのデータサイズ
	u32 PressDataSize ;			// 圧縮後のデータのサイズ( 0xffffffff:圧縮されていない ) ( Ver0x0002 で追加された )
} DARC_FILEHEAD_VER5 ;

// ディレクトリ格納情報
typedef struct tagDARC_DIRECTORY_VER5
{
	u32 DirectoryAddress ;			// 自分の DARC_FILEHEAD_VER5 が格納されているアドレス( DARC_HEAD_VER5 構造体 のメンバ変数 FileTableStartAddress が示すアドレスをアドレス０とする)
	u32 ParentDirectoryAddress ;	// 親ディレクトリの DARC_DIRECTORY_VER5 が格納されているアドレス( DARC_HEAD_VER5構造体 のメンバ変数 DirectoryTableStartAddress が示すアドレスをアドレス０とする)
	u32 FileHeadNum ;				// ディレクトリ内のファイルの数
	u32 FileHeadAddress ;			// ディレクトリ内のファイルのヘッダ列が格納されているアドレス( DARC_HEAD_VER5構造体 のメンバ変数 FileTableStartAddress が示すアドレスをアドレス０とする) 
} DARC_DIRECTORY_VER5 ;

#pragma pack(pop)

// class ----------------------------------------

// アーカイブクラス
class DXArchive_VER5
{
public :
	// 日付の比較結果
	enum DATE_RESULT
	{
		DATE_RESULT_LEFTNEW = 0,		// 第一引数が新しい
		DATE_RESULT_RIGHTNEW,			// 第二引数が新しい
		DATE_RESULT_DRAW,				// 日付は同じ
	} ;

	DXArchive_VER5(TCHAR *ArchivePath = NULL ) ;
	~DXArchive_VER5() ;

	static int			EncodeArchive(TCHAR *OutputFileName, TCHAR **FileOrDirectoryPath, int FileNum, bool Press = false, const char *KeyString = NULL ) ;	// アーカイブファイルを作成する
	static int			EncodeArchiveOneDirectory(TCHAR *OutputFileName, TCHAR *FolderPath, bool Press = false, const char *KeyString = NULL ) ;		// アーカイブファイルを作成する(ディレクトリ一個だけ)
	static int			DecodeArchive(TCHAR *ArchiveName, const TCHAR *OutputPath, const char *KeyString = NULL ) ;								// アーカイブファイルを展開する

	int					OpenArchiveFile( const TCHAR *ArchivePath, const char *KeyString = NULL ) ;				// アーカイブファイルを開く( 0:成功  -1:失敗 )
	int					OpenArchiveFileMem( const TCHAR *ArchivePath, const char *KeyString = NULL ) ;			// アーカイブファイルを開き最初にすべてメモリ上に読み込んでから処理する( 0:成功  -1:失敗 )
	int					OpenArchiveMem( void *ArchiveImage, int ArchiveSize, const char *KeyString = NULL ) ;	// メモリ上にあるアーカイブファイルイメージを開く( 0:成功  -1:失敗 )
	int					CloseArchiveFile( void ) ;																// アーカイブファイルを閉じる

	int					LoadFileToMem( const TCHAR *FilePath, void *Buffer, unsigned int BufferLength ) ;	// アーカイブファイル中の指定のファイルをメモリに読み込む( -1:エラー 0以上:ファイルサイズ )
	int					GetFileSize( const TCHAR *FilePath ) ;										// アーカイブファイル中の指定のファイルをサイズを取得する( -1:エラー )
	int					GetFileInfo( const TCHAR *FilePath, int *PositionP, int *SizeP ) ;			// アーカイブファイル中の指定のファイルのファイル内の位置とファイルの大きさを得る( -1:エラー )
	void				*GetFileImage( void ) ;														// アーカイブファイルをメモリに読み込んだ場合のファイルイメージが格納されている先頭アドレスを取得する( メモリから開いている場合のみ有効、圧縮している場合は、圧縮された状態のデータが格納されているので注意 )
	class DXArchiveFile_VER5 *OpenFile( const TCHAR *FilePath ) ;											// アーカイブファイル中の指定のファイルを開き、ファイルアクセス用オブジェクトを作成する( ファイルから開いている場合のみ有効 )

	void *				LoadFileToCash( const TCHAR *FilePath ) ;									// アーカイブファイル中の指定のファイルを、クラス内のキャッシュバッファに読み込む
	void *				GetCash( void ) ;															// キャッシュバッファのアドレスを取得する
	int					ClearCash( void ) ;															// キャッシュバッファを開放する

	int					ChangeCurrentDir( const TCHAR *DirPath ) ;									// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
	int					GetCurrentDir(TCHAR *DirPathBuffer, int BufferLength ) ;					// アーカイブ内のカレントディレクトリパスを取得する



	// 以下は割と内部で使用
	static void NotConv( void *Data , int Size ) ;									// データを反転させる関数
	static void NotConvFileWrite( void *Data, int Size, FILE *fp ) ;				// データを反転させてファイルに書き出す関数
	static void NotConvFileRead( void *Data, int Size, FILE *fp ) ;					// データを反転させてファイルから読み込む関数
	static void KeyCreate( const char *Source, unsigned char *Key ) ;				// 鍵文字列を作成
	static void KeyConv( void *Data, int Size, int Position, unsigned char *Key ) ;	// 鍵文字列を使用して Xor 演算( Key は必ず DXA_KEYSTR_LENGTH_VER5 の長さがなければならない )
	static void KeyConvFileWrite( void *Data, int Size, FILE *fp, unsigned char *Key, int Position = -1 ) ;	// データを鍵文字列を使用して Xor 演算した後ファイルに書き出す関数( Key は必ず DXA_KEYSTR_LENGTH_VER5 の長さがなければならない )
	static void KeyConvFileRead( void *Data, int Size, FILE *fp, unsigned char *Key, int Position = -1 ) ;	// ファイルから読み込んだデータを鍵文字列を使用して Xor 演算する関数( Key は必ず DXA_KEYSTR_LENGTH_VER5 の長さがなければならない )
	static DATE_RESULT DateCmp( DARC_FILETIME_VER5 *date1, DARC_FILETIME_VER5 *date2 ) ;		// どちらが新しいかを比較する
	static int Encode( void *Src, unsigned int SrcSize, void *Dest ) ;				// データを圧縮する( 戻り値:圧縮後のデータサイズ )
	static int Decode( void *Src, void *Dest ) ;									// データを解凍する( 戻り値:解凍後のデータサイズ )

	DARC_DIRECTORY_VER5 *GetCurrentDirectoryInfo( void ) ;									// アーカイブ内のカレントディレクトリの情報を取得する
	DARC_FILEHEAD_VER5 *GetFileInfo( const TCHAR *FilePath ) ;							// ファイルの情報を得る
	inline DARC_HEAD_VER5 *GetHeader( void ){ return &Head ; }
	inline u8 *GetKey( void ){ return Key ; }
	inline FILE *GetFilePointer( void ){ return fp ; }
	inline TCHAR *GetNameP( void ){ return NameP ; }

protected :
	FILE *fp ;							// アーカイブファイルのポインタ	
	TCHAR *HeadBuffer ;					// ヘッダーバッファー
	TCHAR *FileP, *DirP, *NameP ;			// 各種テーブル(ファイルヘッダ情報テーブル、ディレクトリ情報テーブル、名前情報テーブル)へのポインタ
	DARC_DIRECTORY_VER5 *CurrentDirectory ;	// カレントディレクトリデータへのポインタ

	void *CashBuffer ;					// 読み込んだファイルデータを一時的に保存しておくバッファ
	u32 CashBufferSize ;				// キャッシュバッファに確保しているメモリ容量
	bool MemoryOpenFlag ;				// メモリ上のファイルを開いているか、フラグ
	bool UserMemoryImageFlag ;			// ユーザーが展開したメモリイメージを使用しているか、フラグ
	int MemoryImageSize ;				// メモリ上のファイルから開いていた場合のイメージのサイズ
	u8 Key[DXA_KEYSTR_LENGTH_VER5] ;			// 鍵文字列

	DARC_HEAD_VER5 Head ;				// アーカイブのヘッダ

	// サイズ保存用構造体
	typedef struct tagSIZESAVE
	{
		u32 DataSize ;			// 実データの総量
		u32 NameSize ;			// ファイル名データの総量
		u32 DirectorySize ;		// ディレクトリデータの総量
		u32 FileSize ;			// ファイルプロパティデータの総量
	} SIZESAVE ;

	// ファイル名検索用データ構造体
	typedef struct tagSEARCHDATA
	{
		TCHAR FileName[1024] ;
		u16 Parity ;
		u16 PackNum ;
	} SEARCHDATA ;

	static int DirectoryEncode(TCHAR *DirectoryName, u8 *NameP, u8 *DirP, u8 *FileP, DARC_DIRECTORY_VER5 *ParentDir, SIZESAVE *Size, int DataNumber, FILE *DestP, void *TempBuffer, bool Press, unsigned char *Key ) ;	// 指定のディレクトリにあるファイルをアーカイブデータに吐き出す
	static int DirectoryDecode( u8 *NameP, u8 *DirP, u8 *FileP, DARC_HEAD_VER5 *Head, DARC_DIRECTORY_VER5 *Dir, FILE *ArcP, unsigned char *Key ) ;											// 指定のディレクトリデータにあるファイルを展開する
	static int StrICmp( const TCHAR *Str1, const TCHAR *Str2 ) ;							// 比較対照の文字列中の大文字を小文字として扱い比較する( 0:等しい  1:違う )
	static int ConvSearchData( SEARCHDATA *Dest, const TCHAR *Src, int *Length ) ;		// 文字列を検索用のデータに変換( ヌル文字か \ があったら終了 )
	static int AddFileNameData( const TCHAR *FileName, u8 *FileNameTable ) ;				// ファイル名データを追加する( 戻り値は使用したデータバイト数 )
	static TCHAR *GetOriginalFileName( u8 *FileNameTable ) ;						// ファイル名データから元のファイル名の文字列を取得する
	static int GetDirectoryFilePath( const TCHAR *DirectoryPath, TCHAR *FilePathBuffer = NULL ) ;	// ディレクトリ内のファイルのパスを取得する( FilePathBuffer は一ファイルに付き256バイトの容量が必要 )
	int	ChangeCurrentDirectoryFast( SEARCHDATA *SearchData ) ;							// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
	int	ChangeCurrentDirectoryBase( const TCHAR *DirectoryPath, bool ErrorIsDirectoryReset, SEARCHDATA *LastSearchData = NULL ) ;		// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
	int DirectoryKeyConv( DARC_DIRECTORY_VER5 *Dir ) ;										// 指定のディレクトリデータの暗号化を解除する( 丸ごとメモリに読み込んだ場合用 )

	// ２バイト文字か調べる( TRUE:２バイト文字 FALSE:１バイト文字 )
	inline static int CheckMultiByteChar( const TCHAR *Buf )
	{
		return  ( (unsigned char)*Buf >= 0x81 && (unsigned char)*Buf <= 0x9F ) || ( (unsigned char)*Buf >= 0xE0 && (unsigned char)*Buf <= 0xFC ) ;
	}

	// ファイル名も一緒になっていると分かっているパス中からファイルパスとディレクトリパスを分割する
	// フルパスである必要は無い
	static int GetFilePathAndDirPath(TCHAR *Src, TCHAR *FilePath, TCHAR *DirPath ) ;
} ;


// アーカイブされたファイルのアクセス用のクラス
class DXArchiveFile_VER5
{
protected :
	DARC_FILEHEAD_VER5 *FileData ;		// ファイルデータへのポインタ
	DXArchive_VER5 *Archive ;			// アーカイブクラスへのポインタ
	void *DataBuffer ;				// メモリにデータを展開した際のバッファのポインタ

	int EOFFlag ;					// EOFフラグ
	u32 FilePoint ;					// ファイルポインタ

public :
	DXArchiveFile_VER5( DARC_FILEHEAD_VER5 *FileHead, DXArchive_VER5 *Archive ) ;
	~DXArchiveFile_VER5() ;

	int Read( void *Buffer, int ReadLength ) ;					// ファイルの内容を読み込む
	int Seek( int SeekPoint, int SeekMode ) ;					// ファイルポインタを変更する
	int Tell( void ) ;											// 現在のファイルポインタを得る
	int Eof( void ) ;											// ファイルの終端に来ているか、のフラグを得る
	int Size( void ) ;											// ファイルのサイズを取得する

	inline DARC_FILEHEAD_VER5 *GetFileData( void ){ return FileData ; }
} ;

#endif

