#ifndef _PATTERN_H_
#define _PATTERN_H_

const int MD2_MAX = 16777216;	// 2^24
const int PAT3_MAX = 65536;	// 2^16

const int MD2_LIMIT = 1060624;
const int PAT3_LIMIT = 4468;

enum MD {
  MD_2,
  MD_3,
  MD_4,
  MD_MAX
};

enum LARGE_MD {
  MD_5,
  MD_LARGE_MAX
};

//////////////
//  構造体  //
//////////////

//  パターン
struct pattern {
  unsigned int list[MD_MAX];
  unsigned long long large_list[MD_LARGE_MAX];
};

////////////
//  関数  //
////////////

//  初期設定
void ClearPattern( struct pattern *pat );

//  更新
void UpdatePat3Empty( struct pattern *pat, int pos );
void UpdatePat3Stone( struct pattern *pat, int color, int pos );
void UpdateMD2Empty( struct pattern *pat, int pos );
void UpdateMD2Stone( struct pattern *pat, int color, int pos );
void UpdatePatternEmpty( struct pattern *pat, int pos );
void UpdatePatternStone( struct pattern *pat, int color, int pos );

//  同一パターン
void Pat3Transpose8( unsigned int pat3, unsigned int *transp );
void Pat3Transpose16( unsigned int pat3, unsigned int *transp );
void MD2Transpose8( unsigned int md2, unsigned int *transp );
void MD2Transpose16( unsigned int md2, unsigned int *transp );
void MD3Transpose8( unsigned int md3, unsigned int *transp );
void MD3Transpose16( unsigned int md3, unsigned int *transp );
void MD4Transpose8( unsigned int md4, unsigned int *transp );
void MD4Transpose16( unsigned int md4, unsigned int *transp );
void MD5Transpose8( unsigned long long md5, unsigned long long *transp );
void MD5Transpose16( unsigned long long md5, unsigned long long *transp );

//  色反転
unsigned int Pat3Reverse( unsigned int pat3 );
unsigned int MD2Reverse( unsigned int md2 );
unsigned int MD3Reverse( unsigned int md3 );
unsigned int MD4Reverse( unsigned int md4 );
unsigned long long MD5Reverse( unsigned long long md5 );

//  上下対称
unsigned int Pat3VerticalMirror( unsigned int pat3 );
unsigned int MD2VerticalMirror( unsigned int md2 );
unsigned int MD3VerticalMirror( unsigned int md3 );
unsigned int MD4VerticalMirror( unsigned int md4 );
unsigned long long MD5VerticalMirror( unsigned long long md5 );

//  左右対称
unsigned int Pat3HorizontalMirror( unsigned int pat3 );
unsigned int MD2HorizontalMirror( unsigned int md2 );
unsigned int MD3HorizontalMirror( unsigned int md3 );
unsigned int MD4HorizontalMirror( unsigned int md4 );
unsigned long long MD5HorizontalMirror( unsigned long long md5 );

//  90度回転
unsigned int Pat3Rotate90( unsigned int pat3 );
unsigned int MD2Rotate90( unsigned int md2 );
unsigned int MD3Rotate90( unsigned int md3 );
unsigned int MD4Rotate90( unsigned int md4 );
unsigned long long MD5Rotate90( unsigned long long md5 );

//  パターンを返す
unsigned int Pat3( const struct pattern *pat, int pos );
unsigned int MD2( const struct pattern *pat, int pos );
unsigned int MD3( const struct pattern *pat, int pos );
unsigned int MD4( const struct pattern *pat, int pos );
unsigned long long MD5( const struct pattern *pat, int pos );

//  表示
void DisplayInputPat3( unsigned int pat3 );
void DisplayInputMD2( unsigned int md2 );
void DisplayInputMD3( unsigned int md3 );
void DisplayInputMD4( unsigned int md4 );
void DisplayInputMD5( unsigned long long md5 );
void DisplayInputPattern( struct pattern *pattern, int size );
#endif	//  _PATTERN_H_
