#ifndef _UCTRATING_H_
#define _UCTRATING_H_

#include <string>

#include "GoBoard.h"
#include "PatternHash.h"

enum UCT_FEATURE1{
  UCT_SAVE_CAPTURE_1_1,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_1_2,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_1_3,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_2_1,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_2_2,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_2_3,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_3_1,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_3_2,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_3_3,  // アタリにされた石を守るためのトリ
  UCT_SAVE_CAPTURE_SELF_ATARI,  // アタリにされた石を守るためのトリ

  UCT_CAPTURE,          // トリ
  UCT_CAPTURE_AFTER_KO,  // 新たな劫を解消するトリ
  UCT_2POINT_CAPTURE_S_S,    // 呼吸点が2つになった連に隣接するトリ
  UCT_2POINT_CAPTURE_S_L,    // 呼吸点が2つになった連に隣接するトリ
  UCT_2POINT_CAPTURE_L_S,    // 呼吸点が2つになった連に隣接するトリ
  UCT_2POINT_CAPTURE_L_L,    // 呼吸点が2つになった連に隣接するトリ
  UCT_3POINT_CAPTURE_S_S,    // 呼吸点が3つになった連に隣接するトリ
  UCT_3POINT_CAPTURE_S_L,    // 呼吸点が3つになった連に隣接するトリ
  UCT_3POINT_CAPTURE_L_S,    // 呼吸点が3つになった連に隣接するトリ
  UCT_3POINT_CAPTURE_L_L,    // 呼吸点が3つになった連に隣接するトリ

  UCT_SEMEAI_CAPTURE,    // 呼吸点が1つの連に隣接する連のトリ
  UCT_SELF_ATARI_SMALL,  // 2子以下の自己アタリ
  UCT_SELF_ATARI_NAKADE, // 3子以上の自己アタリ(ナカデになる)
  UCT_SELF_ATARI_LARGE,  // 3子以上の自己アタリ(ナカデにならない)
  UCT_SAVE_EXTENSION_1,    // 1つの石を守るためのノビ
  UCT_SAVE_EXTENSION_2,    // 1つの石を守るためのノビ
  UCT_SAVE_EXTENSION_3,    // 1つの石を守るためのノビ
  UCT_LADDER_EXTENSION,  // シチョウで取られるノビ
  UCT_ATARI,             // アタリ
  UCT_CAPTURABLE_ATARI,  // 1手で捕獲できるアタリ 

  UCT_OIOTOSHI,          // 追い落とし
  UCT_SNAPBACK,          // ウッテガエシ
  UCT_2POINT_ATARI_S_S,      // ダメ2つの自分の連に隣接する敵連に対するアタリ(逃げられる)
  UCT_2POINT_ATARI_S_L,      // ダメ2つの自分の連に隣接する敵連に対するアタリ(逃げられる)
  UCT_2POINT_ATARI_L_S,      // ダメ2つの自分の連に隣接する敵連に対するアタリ(逃げられる)
  UCT_2POINT_ATARI_L_L,      // ダメ2つの自分の連に隣接する敵連に対するアタリ(逃げられる)
  UCT_2POINT_C_ATARI_S_S,    // ダメ2つの自分の連に隣接する敵連に対するアタリ(捕獲できる)
  UCT_2POINT_C_ATARI_S_L,    // ダメ2つの自分の連に隣接する敵連に対するアタリ(捕獲できる)
  UCT_2POINT_C_ATARI_L_S,    // ダメ2つの自分の連に隣接する敵連に対するアタリ(捕獲できる)
  UCT_2POINT_C_ATARI_L_L,    // ダメ2つの自分の連に隣接する敵連に対するアタリ(捕獲できる)
  UCT_3POINT_ATARI_S_S,   // 呼吸点が3つの連に隣接する呼吸点が2つの敵連に対するアタリ(逃げられる)
  UCT_3POINT_ATARI_S_L,   // 呼吸点が3つの連に隣接する呼吸点が2つの敵連に対するアタリ(逃げられる)
  UCT_3POINT_ATARI_L_S,   // 呼吸点が3つの連に隣接する呼吸点が2つの敵連に対するアタリ(逃げられる)
  UCT_3POINT_ATARI_L_L,   // 呼吸点が3つの連に隣接する呼吸点が2つの敵連に対するアタリ(逃げられる)
  UCT_3POINT_C_ATARI_S_S, // 呼吸点が3つの連に隣接する呼吸点が2つの敵連に対するアタリ(捕獲できる)
  UCT_3POINT_C_ATARI_S_L, // 呼吸点が3つの連に隣接する呼吸点が2つの敵連に対するアタリ(捕獲できる)
  UCT_3POINT_C_ATARI_L_S, // 呼吸点が3つの連に隣接する呼吸点が2つの敵連に対するアタリ(捕獲できる)
  UCT_3POINT_C_ATARI_L_L, // 呼吸点が3つの連に隣接する呼吸点が2つの敵連に対するアタリ(捕獲できる)
  UCT_3POINT_DAME_S_S,    // 呼吸点が3つの連に隣接する呼吸点が3つの敵連を詰める手
  UCT_3POINT_DAME_S_L,    // 呼吸点が3つの連に隣接する呼吸点が3つの敵連を詰める手
  UCT_3POINT_DAME_L_S,    // 呼吸点が3つの連に隣接する呼吸点が3つの敵連を詰める手
  UCT_3POINT_DAME_L_L,    // 呼吸点が3つの連に隣接する呼吸点が3つの敵連を詰める手
  UCT_2POINT_EXTENSION_DECREASE, // 呼吸点が2つの連に隣接する呼吸点が2つの敵連があるときのノビ
  UCT_2POINT_EXTENSION_EVEN,     // 呼吸点が2つの連に隣接する呼吸点が2つの敵連があるときのノビ
  UCT_2POINT_EXTENSION_INCREASE, // 呼吸点が2つの連に隣接する呼吸点が2つの敵連があるときのノビ
  UCT_3POINT_EXTENSION_DECREASE, // 呼吸点が3つの連に隣接する呼吸点が3つの敵連があるときのノビ  
  UCT_3POINT_EXTENSION_EVEN,     // 呼吸点が3つの連に隣接する呼吸点が3つの敵連があるときのノビ  
  UCT_3POINT_EXTENSION_INCREASE, // 呼吸点が3つの連に隣接する呼吸点が3つの敵連があるときのノビ  
  UCT_THROW_IN_2,                // 2目の抜き跡に対するホウリコミ
  UCT_NAKADE_3,                  // 3目の抜き跡に対するナカデ
  UCT_KEIMA_TSUKEKOSHI,          // ケイマのツケコシ
  UCT_DOUBLE_KEIMA,              // 両ケイマ
  UCT_KO_CONNECTION,             // 劫の解消

  UCT_MAX,
};

enum PASS_FEATURES { 
  UCT_PASS_AFTER_MOVE,
  UCT_PASS_AFTER_PASS,
  UCT_PASS_MAX,
};

const int LFR_DIMENSION = 5;

const int UCT_MASK_MAX = 64;
const int UCT_TACTICAL_FEATURE_MAX = UCT_MAX;
const int POS_ID_MAX = 64;        // 7bitで表現
const int MOVE_DISTANCE_MAX = 16; // 4bitで表現
const int CFG_DISTANCE_MAX = 8;

const int LARGE_PAT_MAX = 150000;

// Ownerは
// 0-5% 6-15% 16-25% 26-35% 36-45% 46-55%
// 56-65% 66-75% 76-85% 86-95% 96-100%
// の11区分
const int OWNER_MAX = 11;
const int CRITICALITY_MAX = 7;//13;  //7

const int UCT_PHYSICALS_MAX = (1 << 14);

const double CRITICALITY_INIT = 0.765745;
const double CRITICALITY_BIAS = 0.036;

const double OWNER_K = 0.05;
const double OWNER_BIAS = 34.0;

const std::string uct_features_name[UCT_TACTICAL_FEATURE_MAX] = {
  "SAVE_CAPTURE_1_1           ",
  "SAVE_CAPTURE_1_2           ",
  "SAVE_CAPTURE_1_3           ",
  "SAVE_CAPTURE_2_1           ",
  "SAVE_CAPTURE_2_2           ",

  "SAVE_CAPTURE_2_3           ",
  "SAVE_CAPTURE_3_1           ",
  "SAVE_CAPTURE_3_2           ",
  "SAVE_CAPTURE_3_3           ",
  "SAVE_CAPTURE_SELF_ATARI    ",

  "CAPTURE                    ",
  "CAPTURE_AFTER_KO           ",
  "2POINT_CAPTURE_S_S         ",
  "2POINT_CAPTURE_S_L         ",
  "2POINT_CAPTURE_L_S         ",

  "2POINT_CAPTURE_L_L         ",
  "3POINT_CAPTURE_S_S         ",
  "3POINT_CAPTURE_S_L         ",
  "3POINT_CAPTURE_L_S         ",
  "3POINT_CAPTURE_L_L         ",

  "SEMEAI_CAPTURE             ",
  "SELF_ATARI_SMALL           ",
  "SELF_ATARI_NAKADE          ",
  "SELF_ATARI_LARGE           ",
  "SAVE_EXTENSION_1           ",

  "SAVE_EXTENSION_2           ",
  "SAVE_EXTENSION_3           ",
  "LADDER_EXTENSION           ",
  "ATARI                      ",
  "CAPTURABLE_ATARI           ",

  "OIOTOSHI                   ",
  "SNAPBACK                   ",
  "2POINT_ATARI_S_S           ",
  "2POINT_ATARI_S_L           ",
  "2POINT_ATARI_L_S           ",
  "2POINT_ATARI_L_L           ",
  "2POINT_C_ATARI_S_S         ",
  "2POINT_C_ATARI_S_L         ",
  "2POINT_C_ATARI_L_S         ",
  "2POINT_C_ATARI_L_L         ",

  "3POINT_ATARI_S_S           ",
  "3POINT_ATARI_S_L           ",
  "3POINT_ATARI_L_S           ",
  "3POINT_ATARI_L_L           ",
  "3POINT_C_ATARI_S_S         ",
  "3POINT_C_ATARI_S_L         ",
  "3POINT_C_ATARI_L_S         ",
  "3POINT_C_ATARI_L_L         ",


  "3POINT_DAME_S_S            ",
  "3POINT_DAME_S_L            ",
  "3POINT_DAME_L_S            ",
  "3POINT_DAME_L_L            ",
  "2POINT_EXTENSION_DECREASE  ",
  "2POINT_EXTENSION_EVEN      ",
  "2POINT_EXTENSION_INCREASE  ",
  "3POINT_EXTENSION_DECREASE  ",

  "3POINT_EXTENSION_EVEN      ",
  "3POINT_EXTENSION_INCREASE  ",
  "THROW_IN_2                 ",
  "NAKADE_3                   ",
  "KEIMA_TSUKEKOSHI           ",

  "DOUBLE_KEIMA               ",
  "KO_CONNECTION              ",
};

typedef struct {
  unsigned long long tactical_features1[BOARD_MAX]; 
  unsigned long long tactical_features2[BOARD_MAX]; 
  unsigned long long tactical_features3[BOARD_MAX]; 
} uct_features_t;

typedef struct {
  double w;
  double v[LFR_DIMENSION];
} latent_factor_t;

extern double uct_owner[OWNER_MAX];
extern double uct_criticality[CRITICALITY_MAX];

// extern index_hash_t md3_index[HASH_MAX];
// extern index_hash_t md4_index[HASH_MAX];
// extern index_hash_t md5_index[HASH_MAX];
extern index_hash_t *md3_index;
extern index_hash_t *md4_index;
extern index_hash_t *md5_index;

extern char uct_params_path[1024];

extern unsigned long long atari_mask;
extern unsigned long long capture_mask;

extern const unsigned long long uct_mask[UCT_MASK_MAX];

//  初期化
void InitializeUctRating( void );
void InitializePhysicalFeaturesSet( void );

//  戦術的特徴のレートの計算
double CalculateLFRScore( game_info_t *game, int pos, int pat_index[], uct_features_t *uct_features );

//  特徴の判定
void UctCheckFeatures( game_info_t *game, int color, uct_features_t *uct_features );

//  2目の抜き後の判定
void UctCheckRemove2Stones( game_info_t *game, int color, uct_features_t *uct_features );

//  3目の抜き後の判定
void UctCheckRemove3Stones( game_info_t *game, int color, uct_features_t *uct_features );

//  劫を解消するトリの判定
void UctCheckCaptureAfterKo( game_info_t *game, int color, uct_features_t *uct_features );

//  自己アタリの判定
bool UctCheckSelfAtari( game_info_t *game, int color, int pos, uct_features_t *uct_features );

//  トリの判定
void UctCheckCapture( game_info_t *game, int color, int pos, uct_features_t *uct_features );

//  アタリの判定
void UctCheckAtari( game_info_t *game, int color, int pos, uct_features_t *uct_features );

//  ウッテガエシの判定
void UctCheckSnapBack( game_info_t *game, int color, int pos, uct_features_t *uct_features );

//  ケイマのツケコシの判定
void UctCheckKeimaTsukekoshi( game_info_t *game, int color, int pos, uct_features_t *uct_features );

//  両ケイマの判定
void UctCheckDoubleKeima( game_info_t *game, int color, int pos, uct_features_t *uct_features );

//  ウッテガエシの判定
int UctCheckUtteGaeshi( game_info_t *game, int color, int pos, uct_features_t *uct_features );

//  劫の解消
void UctCheckKoConnection( game_info_t *game, uct_features_t *uct_features );

//  現局面の評価
void AnalyzeUctRating( game_info_t *game, int color, double rate[] );

#endif
