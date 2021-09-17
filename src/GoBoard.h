#ifndef _GO_BOARD_H_
#define _GO_BOARD_H_

#include "Pattern.h"

////////////////
//    定数    //
////////////////

const int PURE_BOARD_SIZE = 19;  // 盤の大きさ

const int OB_SIZE = 5; // 盤外の幅
const int BOARD_SIZE = (PURE_BOARD_SIZE + OB_SIZE + OB_SIZE); // 盤外を含めた盤の幅

const int PURE_BOARD_MAX = (PURE_BOARD_SIZE * PURE_BOARD_SIZE); // 盤の大きさ 
const int BOARD_MAX = (BOARD_SIZE * BOARD_SIZE);                // 盤外を含めた盤の大きさ

const int MAX_STRING = (PURE_BOARD_MAX * 4 / 5); // 連の最大数 
const int MAX_NEIGHBOR = MAX_STRING;             // 隣接する敵連の最大数

const int BOARD_START = OB_SIZE;                        // 盤の始点  
const int BOARD_END = (PURE_BOARD_SIZE + OB_SIZE - 1);  // 盤の終点  

const int STRING_LIB_MAX = (BOARD_SIZE * (PURE_BOARD_SIZE + OB_SIZE));  // 1つの連の持つ呼吸点の最大数
const int STRING_POS_MAX = (BOARD_SIZE * (PURE_BOARD_SIZE + OB_SIZE));  // 連が持ちうる座標の最大値

const int STRING_END = (STRING_POS_MAX - 1); // 連の終端を表す値
const int NEIGHBOR_END = (MAX_NEIGHBOR - 1);  // 隣接する敵連の終端を表す値
const int LIBERTY_END = (STRING_LIB_MAX - 1); // 呼吸点の終端を表す値

const int MAX_RECORDS = (PURE_BOARD_MAX * 3); // 記録する着手の最大数 
const int MAX_MOVES = (MAX_RECORDS - 1);      // 着手数の最大値

const int PASS = 0;     // パスに相当する値
const int RESIGN = -1;  // 投了に相当する値

const double KOMI = 6.5; // デフォルトのコミの値

//////////////////
//  マクロ関数  //
//////////////////
#define POS(x, y) ((x) + (y) * board_size)  // (x, y)から座標を導出
#define X(pos)        ((pos) % board_size)  // posのx座標の導出
#define Y(pos)        ((pos) / board_size)  // posのy座標の導出

#define CORRECT_X(pos) ((pos) % board_size - OB_SIZE + 1)  // 実際の盤上のx座標
#define CORRECT_Y(pos) ((pos) / board_size - OB_SIZE + 1)  // 実際の盤上のy座標

#define NORTH(pos) ((pos) - board_size)  // posの上の座標
#define  WEST(pos) ((pos) - 1)           // posの左の座標
#define  EAST(pos) ((pos) + 1)           // posの右の座標
#define SOUTH(pos) ((pos) + board_size)  // posの下の座標

#define FLIP_COLOR(col) ((col) ^ 0x3) // 色の反転

#define DX(pos1, pos2)  (abs(board_x[(pos1)] - board_x[(pos2)]))     // x方向の距離
#define DY(pos1, pos2)  (abs(board_y[(pos1)] - board_y[(pos2)]))     // y方向の距離
#define DIS(pos1, pos2) (move_dis[DX(pos1, pos2)][DY(pos1, pos2)])   // 着手距離


enum stone {
  S_EMPTY,  // 空点
  S_BLACK,  // 黒石
  S_WHITE,  // 白石
  S_OB,     // 盤外
  S_MAX     // 番兵
};

enum eye_condition_t : unsigned char {
  E_NOT_EYE,           // 眼でない
  E_COMPLETE_HALF_EYE, // 完全に欠け眼(8近傍に打って1眼にできない)
  E_HALF_3_EYE,        // 欠け眼であるが, 3手で1眼にできる
  E_HALF_2_EYE,        // 欠け眼であるが, 2手で1眼にできる
  E_HALF_1_EYE,        // 欠け眼であるが, 1手で1眼にできる
  E_COMPLETE_ONE_EYE,  // 完全な1眼
  E_MAX,
};

// 着手を記録する構造体
struct record_t {
  int color;                // 着手した石の色
  int pos;                  // 着手箇所の座標
  unsigned long long hash;  // 局面のハッシュ値
};

// 連を表す構造体 (19x19 : 1987bytes)
struct string_t {
  char color;                    // 連の色
  int libs;                      // 連の持つ呼吸点数
  short lib[STRING_LIB_MAX];     // 連の持つ呼吸点の座標
  int neighbors;                 // 隣接する敵の連の数
  short neighbor[MAX_NEIGHBOR];  // 隣接する敵の連の連番号
  int origin;                    // 連の始点の座標
  int size;                      // 連を構成する石の数
  bool flag;                     // 連の存在フラグ
};


// 局面を表す構造体
struct game_info_t {
  record_t record[MAX_RECORDS];  // 着手箇所と色の記録
  int moves;                        // 着手数の記録
  int prisoner[S_MAX];              // アゲハマ
  int ko_pos;                       // 劫となっている箇所
  int ko_move;                      // 劫となった時の着手数

  unsigned long long current_hash;     // 現在の局面のハッシュ値
  unsigned long long previous1_hash;   // 1手前の局面のハッシュ値
  unsigned long long previous2_hash;   // 2手前の局面のハッシュ値
  unsigned long long positional_hash;  // 局面のハッシュ値(石の位置のみ)
  unsigned long long move_hash;        // 着手列のハッシュ値(合流なし)
  
  char board[BOARD_MAX];            // 盤面 

  int pass_count;                   // パスした回数

  pattern_t pat[BOARD_MAX];    // 周囲の石の配置 

  string_t string[MAX_STRING];        // 連のデータ(19x19 : 573,845bytes)
  int string_id[STRING_POS_MAX];    // 各座標の連のID
  int string_next[STRING_POS_MAX];  // 連を構成する石のデータ構造

  bool candidates[BOARD_MAX];  // 候補手かどうかのフラグ 
  bool seki[BOARD_MAX];
  
  unsigned int tactical_features1[BOARD_MAX];  // 戦術的特徴 
  unsigned int tactical_features2[BOARD_MAX];  // 戦術的特徴 

  int capture_num[S_OB];                   // 前の着手で打ち上げた石の数
  int capture_pos[S_OB][PURE_BOARD_MAX];   // 前の着手で石を打ち上げた座標 

  int update_num[S_OB];                    // 戦術的特徴が更新された数
  int update_pos[S_OB][PURE_BOARD_MAX];    // 戦術的特徴が更新された座標 

  long long rate[2][BOARD_MAX];           // シミュレーション時の各座標のレート 
  long long sum_rate_row[2][BOARD_SIZE];  // シミュレーション時の各列のレートの合計値  
  long long sum_rate[2];                  // シミュレーション時の全体のレートの合計値
};

enum class scoring_type {
  AREA,
  TERRITORY,
};


////////////////
//    変数    //
////////////////

// 碁盤の大きさ
extern int pure_board_size;

// 碁盤の交点の個数
extern int pure_board_max;

// 碁盤の大きさ(盤外込み)
extern int board_size;

// 碁盤の交点の個数(盤外込み)
extern int board_max;

// 碁盤の右端(上端)
extern int board_start;

// 碁盤の左端(下端)
extern int board_end;

// 初手の候補手の個数
extern int first_move_candidates;

// コミ
extern double komi[S_OB];

// Dynamic Komi
extern double dynamic_komi[S_OB];

// 領域/地
extern scoring_type scoring;

// 盤上の位置のID
extern int board_pos_id[BOARD_MAX];  

// 盤上のx座標
extern int board_x[BOARD_MAX];  

//  盤上のy座標
extern int board_y[BOARD_MAX];  

// 眼のパターン
extern unsigned char eye[PAT3_MAX];

// 領地のパターン
extern unsigned char territory[PAT3_MAX];

// 上下左右4近傍の空点の数
extern unsigned char nb4_empty[PAT3_MAX];

// 眼の状態
extern eye_condition_t eye_condition[PAT3_MAX];

// x方向の距離
extern int border_dis_x[BOARD_MAX]; 

// y方向の距離
extern int border_dis_y[BOARD_MAX]; 

// 着手距離
extern int move_dis[PURE_BOARD_SIZE][PURE_BOARD_SIZE];

// 盤上の位置からデータ上の位置の対応
extern int onboard_pos[PURE_BOARD_MAX];

// 初手の候補手
extern int first_move_candidate[PURE_BOARD_MAX];

//////////////
//   関数   //
//////////////

// 超劫の設定
void SetSuperKo( const bool flag );

// 盤の大きさの設定
void SetBoardSize( const int size );

// メモリ領域の確保
game_info_t *AllocateGame( void );

// メモリ領域の解放
void FreeGame( game_info_t *game );

// 盤面情報のコピー
void CopyGame( game_info_t *dst, const game_info_t *src );

// 定数の初期化
void InitializeConst( void );

// 盤面の初期化
void InitializeBoard( game_info_t *game );

// 合法手判定
// 合法手ならばtrueを返す
bool IsLegal( const game_info_t *game, const int pos, const int color );

// 合法手かつ眼でないか判定
// 合法手かつ眼でなければtrueを返す
bool IsLegalNotEye( game_info_t *game, const int pos, const int color );

// 自殺手判定
// 自殺手ならばtrueを返す
bool IsSuicide( const game_info_t *game, const string_t *string, const int color, const int pos );

// 石を置く
void PutStone( game_info_t *game, const int pos, const int color );

// 石を置く(プレイアウト用)
void PoPutStone( game_info_t *game, const int pos, const int color );

// スコアの判定
int CalculateScore( const game_info_t *game );
int CalculateScoreNoFill( const game_info_t *game, bool japanese_score = false );

// コミの値の設定
void SetKomi( const double new_komi );

// 上下左右の座標の計算
void GetNeighbor4( int neighbor4[4], const int pos );

// 隅のマガリ四目の確認
void CheckBentFourInTheCorner( game_info_t *game );

#endif
