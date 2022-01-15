// -------------------------------------------------------------------------------
// 
// 		ＤＸライブラリアーカイバ
// 
//	Creator			: 山田 巧
//	Creation Date	: 2003/09/11
//	Version			: 1.08
//
// -------------------------------------------------------------------------------

// 多重インクルード防止用定義
#ifndef DX_ARCHIVE_H
#define DX_ARCHIVE_H

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

#define DXA_HEAD						*((u16 *)"DX")	// ヘッダ
#define DXA_VER							(0x0008)		// バージョン
#define DXA_VER_MIN						(0x0008)		// 対応している最低バージョン
#define DXA_BUFFERSIZE					(0x1000000)		// アーカイブ作成時に使用するバッファのサイズ
#define DXA_KEY_BYTES					(7)				// 鍵のバイト数
#define DXA_KEY_STRING_LENGTH			(63)			// 鍵用文字列の長さ
#define DXA_KEY_STRING_MAXLENGTH		(2048)			// 鍵用文字列バッファのサイズ

// フラグ
#define DXA_FLAG_NO_KEY					(0x00000001)	// 鍵処理無し
#define DXA_FLAG_NO_HEAD_PRESS			(0x00000002)	// ヘッダの圧縮無し

/*
	バージョンごとの違い

	0x0002 DARC_FILEHEAD に PressDataSize を追加
	0x0004 DARC_HEAD に CharCodeFormat を追加
	0x0005 暗号化処理を一部変更
	0x0006 64bit化
	0x0007 暗号化処理を変更( パスワードを指定した場合の解読難度を向上 )
	0x0008 暗号鍵の長さを56bitに変更
*/

/*
	データマップ
		
	DARC_HEAD
	ファイル実データ
	ファイル名テーブル
	DARC_FILEHEAD テーブル
	DARC_DIRECTORY テーブル
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
typedef struct tagDARC_HEAD
{
	u16 Head ;								// ヘッダ
	u16 Version ;							// バージョン
	u32 HeadSize ;							// ヘッダ情報の DARC_HEAD を抜いた全サイズ
	u64 DataStartAddress ;					// 最初のファイルのデータが格納されているデータアドレス(ファイルの先頭アドレスをアドレス０とする)
	u64 FileNameTableStartAddress ;			// ファイル名テーブルの先頭アドレス(ファイルの先頭アドレスをアドレス０とする)
	u64 FileTableStartAddress ;				// ファイルテーブルの先頭アドレス(メンバ変数 FileNameTableStartAddress のアドレスを０とする)
	u64 DirectoryTableStartAddress ;		// ディレクトリテーブルの先頭アドレス(メンバ変数 FileNameTableStartAddress のアドレスを０とする)
											// アドレス０から配置されている DARC_DIRECTORY 構造体がルートディレクトリ
	u32 CharCodeFormat ;					// ファイル名に使用しているコードページ番号
	u32 Flags ;								// フラグ( DXA_FLAG_NO_KEY 等 )
	u8  HuffmanEncodeKB ;					// ファイルの前後のハフマン圧縮するサイズ( 単位：キロバイト 0xff の場合はすべて圧縮する )
	u8  Reserve[ 15 ] ;						// 予約領域
} DARC_HEAD ;

// ファイルの時間情報
typedef struct tagDARC_FILETIME
{
	u64 Create ;			// 作成時間
	u64 LastAccess ;		// 最終アクセス時間
	u64 LastWrite ;			// 最終更新時間
} DARC_FILETIME ;

// ファイル格納情報
typedef struct tagDARC_FILEHEAD
{
	u64 NameAddress ;			// ファイル名が格納されているアドレス( ARCHIVE_HEAD構造体 のメンバ変数 FileNameTableStartAddress のアドレスをアドレス０とする) 

	u64 Attributes ;			// ファイル属性
	DARC_FILETIME Time ;		// 時間情報
	u64 DataAddress ;			// ファイルが格納されているアドレス
								//			ファイルの場合：DARC_HEAD構造体 のメンバ変数 DataStartAddress が示すアドレスをアドレス０とする
								//			ディレクトリの場合：DARC_HEAD構造体 のメンバ変数 DirectoryTableStartAddress のが示すアドレスをアドレス０とする
	u64 DataSize ;				// ファイルのデータサイズ
	u64 PressDataSize ;			// 圧縮後のデータのサイズ( 0xffffffffffffffff:圧縮されていない ) ( Ver0x0002 で追加された )
	u64 HuffPressDataSize ;		// ハフマン圧縮後のデータのサイズ( 0xffffffffffffffff:圧縮されていない ) ( Ver0x0008 で追加された )
} DARC_FILEHEAD ;

// ディレクトリ格納情報
typedef struct tagDARC_DIRECTORY
{
	u64 DirectoryAddress ;			// 自分の DARC_FILEHEAD が格納されているアドレス( DARC_HEAD 構造体 のメンバ変数 FileTableStartAddress が示すアドレスをアドレス０とする)
	u64 ParentDirectoryAddress ;	// 親ディレクトリの DARC_DIRECTORY が格納されているアドレス( DARC_HEAD構造体 のメンバ変数 DirectoryTableStartAddress が示すアドレスをアドレス０とする)
	u64 FileHeadNum ;				// ディレクトリ内のファイルの数
	u64 FileHeadAddress ;			// ディレクトリ内のファイルのヘッダ列が格納されているアドレス( DARC_HEAD構造体 のメンバ変数 FileTableStartAddress が示すアドレスをアドレス０とする) 
} DARC_DIRECTORY ;

#pragma pack(pop)

// エンコード処理進行状況保存用情報
typedef struct tagDARC_ENCODEINFO
{
	int TotalFileNum ;				// ファイル総数
	int CompFileNum ;				// 処理したファイルの数
	unsigned int PrevDispTime ;		// 前回状況出力した時間
	TCHAR ProcessFileName[ 2048 ] ;	// 現在処理しているファイルの名前
	bool OutputStatus ;				// 状況出力を行うかどうか
} DARC_ENCODEINFO ;

// class ----------------------------------------

// アーカイブクラス
class DXArchive
{
public :
	// 日付の比較結果
	enum DATE_RESULT
	{
		DATE_RESULT_LEFTNEW = 0,		// 第一引数が新しい
		DATE_RESULT_RIGHTNEW,			// 第二引数が新しい
		DATE_RESULT_DRAW,				// 日付は同じ
	} ;

	DXArchive(TCHAR *ArchivePath = NULL ) ;
	~DXArchive() ;

	static int			EncodeArchive(TCHAR *OutputFileName, TCHAR **FileOrDirectoryPath, int FileNum, bool Press = false, bool AlwaysHuffman = false, u8 HuffmanEncodeKB = 0, const char *KeyString_ = NULL, bool NoKey = false, bool OutputStatus = true, bool MaxPress = false ) ;	// アーカイブファイルを作成する
	static int			EncodeArchiveOneDirectory(TCHAR *OutputFileName, TCHAR *FolderPath, bool Press = false, bool AlwaysHuffman = false, u8 HuffmanEncodeKB = 0, const char *KeyString_ = NULL, bool NoKey = false, bool OutputStatus = true, bool MaxPress = false ) ;		// アーカイブファイルを作成する(ディレクトリ一個だけ)
	static int			DecodeArchive(TCHAR *ArchiveName, const TCHAR *OutputPath, const char *KeyString_ = NULL ) ;								// アーカイブファイルを展開する

	int					OpenArchiveFile( const TCHAR *ArchivePath, const char *KeyString_ = NULL ) ;				// アーカイブファイルを開く( 0:成功  -1:失敗 )
	int					OpenArchiveFileMem( const TCHAR *ArchivePath, const char *KeyString_ = NULL ) ;			// アーカイブファイルを開き最初にすべてメモリ上に読み込んでから処理する( 0:成功  -1:失敗 )
	int					OpenArchiveMem( void *ArchiveImage, s64 ArchiveSize, const char *KeyString_ = NULL ) ;	// メモリ上にあるアーカイブファイルイメージを開く( 0:成功  -1:失敗 )
	int					CloseArchiveFile( void ) ;																// アーカイブファイルを閉じる

	s64					LoadFileToMem( const TCHAR *FilePath, void *Buffer, u64 BufferLength ) ;		// アーカイブファイル中の指定のファイルをメモリに読み込む( -1:エラー 0以上:ファイルサイズ )
	s64					GetFileSize( const TCHAR *FilePath ) ;										// アーカイブファイル中の指定のファイルをサイズを取得する( -1:エラー )
	int					GetFileInfo( const TCHAR *FilePath, u64 *PositionP, u64 *SizeP ) ;			// アーカイブファイル中の指定のファイルのファイル内の位置とファイルの大きさを得る( -1:エラー )
	void				*GetFileImage( void ) ;														// アーカイブファイルをメモリに読み込んだ場合のファイルイメージが格納されている先頭アドレスを取得する( メモリから開いている場合のみ有効、圧縮している場合は、圧縮された状態のデータが格納されているので注意 )
	class DXArchiveFile *OpenFile( const TCHAR *FilePath ) ;											// アーカイブファイル中の指定のファイルを開き、ファイルアクセス用オブジェクトを作成する( ファイルから開いている場合のみ有効 )

	void *				LoadFileToCache( const TCHAR *FilePath ) ;									// アーカイブファイル中の指定のファイルを、クラス内のキャッシュバッファに読み込む
	int					ClearCache( void ) ;															// キャッシュバッファを開放する

	int					ChangeCurrentDir( const TCHAR *DirPath ) ;									// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
	int					GetCurrentDir(TCHAR *DirPathBuffer, int BufferLength ) ;					// アーカイブ内のカレントディレクトリパスを取得する



	// 以下は割と内部で使用
	static void fwrite64( void *Data, s64 Size, FILE *fp ) ;													// 標準ストリームにデータを書き込む( 64bit版 )
	static void fread64( void *Buffer, s64 Size, FILE *fp ) ;													// 標準ストリームからデータを読み込む( 64bit版 )
	static void NotConv( void *Data , s64 Size ) ;																// データを反転させる関数
	static void NotConvFileWrite( void *Data, s64 Size, FILE *fp ) ;											// データを反転させてファイルに書き出す関数
	static void NotConvFileRead( void *Data, s64 Size, FILE *fp ) ;												// データを反転させてファイルから読み込む関数
	static size_t CreateKeyFileString( int CharCodeFormat, const char *KeyString, size_t KeyStringBytes, DARC_DIRECTORY *Directory, DARC_FILEHEAD *FileHead, u8 *FileTable, u8 *DirectoryTable, u8 *NameTable, u8 *FileString ) ;	// カレントディレクトリにある指定のファイルの鍵用の文字列を作成する、戻り値は文字列の長さ( 単位：Byte )( FileString は DXA_KEY_STRING_MAXLENGTH の長さが必要 )
	static void KeyCreate( const char *Source, size_t SourceBytes, u8 *Key ) ;									// 鍵文字列を作成
	static void KeyConv( void *Data, s64 Size, s64 Position, unsigned char *Key ) ;								// 鍵文字列を使用して Xor 演算( Key は必ず DXA_KEY_BYTES の長さがなければならない )
	static void KeyConvFileWrite( void *Data, s64 Size, FILE *fp, unsigned char *Key, s64 Position = -1 ) ;		// データを鍵文字列を使用して Xor 演算した後ファイルに書き出す関数( Key は必ず DXA_KEY_BYTES の長さがなければならない )
	static void KeyConvFileRead( void *Data, s64 Size, FILE *fp, unsigned char *Key, s64 Position = -1 ) ;		// ファイルから読み込んだデータを鍵文字列を使用して Xor 演算する関数( Key は必ず DXA_KEY_BYTES の長さがなければならない )
	static DATE_RESULT DateCmp( DARC_FILETIME *date1, DARC_FILETIME *date2 ) ;									// どちらが新しいかを比較する
	static int Encode( void *Src, u32 SrcSize, void *Dest, bool OutStatus = true, bool MaxPress = false ) ;		// データを圧縮する( 戻り値:圧縮後のデータサイズ )
	static int Decode( void *Src, void *Dest ) ;																// データを解凍する( 戻り値:解凍後のデータサイズ )
	static u32 HashCRC32( const void *SrcData, size_t SrcDataSize ) ;											// バイナリデータを元に CRC32 のハッシュ値を計算する

	DARC_DIRECTORY *GetCurrentDirectoryInfo( void ) ;															// アーカイブ内のカレントディレクトリの情報を取得する
	DARC_FILEHEAD *GetFileInfo( const TCHAR *FilePath, DARC_DIRECTORY **DirectoryP = NULL ) ;					// ファイルの情報を得る
	inline DARC_HEAD *GetHeader( void ){ return &Head ; }
	inline u8 *GetKey( void ){ return Key ; }
	inline bool GetNoKey( void ){ return NoKey ; }
	inline char *GetKeyString( void ){ return KeyString ; }
	inline size_t GetKeyStringBytes( void ){ return KeyStringBytes ; }
	inline FILE *GetFilePointer( void ){ return fp ; }
	inline u8 *GetNameP( void ){ return NameP ; }
	inline u8 *GetFileHeadTable( void ){ return FileP ; }
	inline u8 *GetDirectoryTable( void ){ return DirP ; }
	inline u8 *GetNameTable( void ){ return NameP ; }

protected :
	FILE *fp ;							// アーカイブファイルのポインタ	
	u8 *HeadBuffer ;					// ヘッダーバッファー
	u8 *FileP, *DirP, *NameP ;			// 各種テーブル(ファイルヘッダ情報テーブル、ディレクトリ情報テーブル、名前情報テーブル)へのポインタ
	DARC_DIRECTORY *CurrentDirectory ;	// カレントディレクトリデータへのポインタ

	void *CacheBuffer ;					// 読み込んだファイルデータを一時的に保存しておくバッファ
	u64 CacheBufferSize ;				// キャッシュバッファに確保しているメモリ容量
	bool MemoryOpenFlag ;				// メモリ上のファイルを開いているか、フラグ
	bool UserMemoryImageFlag ;			// ユーザーが展開したメモリイメージを使用しているか、フラグ
	s64 MemoryImageSize ;				// メモリ上のファイルから開いていた場合のイメージのサイズ
	bool NoKey ;						// 鍵処理を行わないかどうか
	u8 Key[ DXA_KEY_BYTES ] ;			// 鍵
	char KeyString[ DXA_KEY_STRING_LENGTH + 1 ] ;	// 鍵文字列
	size_t KeyStringBytes ;				// 鍵文字列のバイト数

	DARC_HEAD Head ;					// アーカイブのヘッダ

	// サイズ保存用構造体
	typedef struct tagSIZESAVE
	{
		u64 DataSize ;			// 実データの総量
		u64 NameSize ;			// ファイル名データの総量
		u64 DirectorySize ;		// ディレクトリデータの総量
		u64 FileSize ;			// ファイルプロパティデータの総量
	} SIZESAVE ;

	// ファイル名検索用データ構造体
	typedef struct tagSEARCHDATA
	{
		u8 FileName[1024] ;
		u16 Parity ;
		u16 PackNum ;
	} SEARCHDATA ;

	static int DirectoryEncode( int CharCodeFormat, TCHAR *DirectoryName, u8 *NameP, u8 *DirP, u8 *FileP, DARC_DIRECTORY *ParentDir, SIZESAVE *Size, int DataNumber, FILE *DestFp, void *TempBuffer, bool Press, bool MaxPress, bool AlwaysHuffman, u8 HuffmanEncodeKB, const char *KeyString, size_t KeyStringBytes, bool NoKey, char *KeyStringBuffer, DARC_ENCODEINFO *EncodeInfo ) ;	// 指定のディレクトリにあるファイルをアーカイブデータに吐き出す
	static int DirectoryDecode( u8 *NameP, u8 *DirP, u8 *FileP, DARC_HEAD *Head, DARC_DIRECTORY *Dir, FILE *ArcP, unsigned char *Key, const char *KeyString, size_t KeyStringBytes, bool NoKey, char *KeyStringBuffer ) ;											// 指定のディレクトリデータにあるファイルを展開する
	static int StrICmp( const TCHAR *Str1, const TCHAR *Str2 ) ;							// 比較対照の文字列中の大文字を小文字として扱い比較する( 0:等しい  1:違う )
	static int ConvSearchData( SEARCHDATA *Dest, const TCHAR *Src, int *Length ) ;		// 文字列を検索用のデータに変換( ヌル文字か \ があったら終了 )
	static int AddFileNameData( const TCHAR *FileName, u8 *FileNameTable ) ;				// ファイル名データを追加する( 戻り値は使用したデータバイト数 )
	static TCHAR *GetOriginalFileName( u8 *FileNameTable ) ;						// ファイル名データから元のファイル名の文字列を取得する
	static int GetDirectoryFilePath( const TCHAR *DirectoryPath, TCHAR *FilePathBuffer = NULL ) ;	// ディレクトリ内のファイルのパスを取得する( FilePathBuffer は一ファイルに付き256バイトの容量が必要 )
	static void EncodeStatusErase( void ) ;														// エンコードの進行状況を表示を消去する
	static void EncodeStatusOutput( DARC_ENCODEINFO *EncodeInfo, bool Always = false ) ;		// エンコードの進行状況を表示する
	static void AnalyseHuffmanEncode( u64 DataSize, u8 HuffmanEncodeKB, u64 *HeadDataSize, u64 *FootDataSize ) ;	// ハフマン圧縮をする前後のサイズを取得する
	int	ChangeCurrentDirectoryFast( SEARCHDATA *SearchData ) ;							// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
	int	ChangeCurrentDirectoryBase( const TCHAR *DirectoryPath, bool ErrorIsDirectoryReset, SEARCHDATA *LastSearchData = NULL ) ;		// アーカイブ内のディレクトリパスを変更する( 0:成功  -1:失敗 )
	int DirectoryKeyConv( DARC_DIRECTORY *Dir, char *KeyStringBuffer ) ;										// 指定のディレクトリデータの暗号化を解除する( 丸ごとメモリに読み込んだ場合用 )

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
class DXArchiveFile
{
protected :
	DARC_FILEHEAD *FileData ;		// ファイルデータへのポインタ
	DXArchive *Archive ;			// アーカイブクラスへのポインタ
	void *DataBuffer ;				// メモリにデータを展開した際のバッファのポインタ

	u8 Key[ DXA_KEY_BYTES ] ;		// 鍵

	int EOFFlag ;					// EOFフラグ
	u64 FilePoint ;					// ファイルポインタ

public :
	DXArchiveFile( DARC_FILEHEAD *FileHead, DARC_DIRECTORY *Directory, DXArchive *Archive ) ;
	~DXArchiveFile() ;

	s64 Read( void *Buffer, s64 ReadLength ) ;					// ファイルの内容を読み込む
	s64 Seek( s64 SeekPoint, s64 SeekMode ) ;					// ファイルポインタを変更する
	s64 Tell( void ) ;											// 現在のファイルポインタを得る
	s64 Eof( void ) ;											// ファイルの終端に来ているか、のフラグを得る
	s64 Size( void ) ;											// ファイルのサイズを取得する

	inline DARC_FILEHEAD *GetFileData( void ){ return FileData ; }
} ;

#endif

