#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iostream>

#include "GoBoard.h"
#include "UctRating.h"
#include "ZobristHash.h"

using namespace std;

/////////////////////
//     大域変数    //
/////////////////////

int pure_board_max = PURE_BOARD_MAX;    // 盤のの大きさ    
int pure_board_size = PURE_BOARD_SIZE;  // 盤の辺の大きさ  
int board_max = BOARD_MAX;              // 盤外を含む盤の大きさ  
int board_size = BOARD_SIZE;            // 盤外を含む盤の辺の大きさ 

int board_start = BOARD_START;  // 盤の左(上)端
int board_end = BOARD_END;      //  盤の右(下)端

double komi[S_WHITE + 1];          // コミの値
double dynamic_komi[S_WHITE + 1];  // ダイナミックコミの値
double default_komi = KOMI;        // デフォルトのコミの値

int board_pos_id[BOARD_MAX];  // 盤上の位置の識別番号 

int board_x[BOARD_MAX];  // x方向の座標  
int board_y[BOARD_MAX];  // y方向の座標  

unsigned char eye[PAT3_MAX];        // 目のパターン
unsigned char false_eye[PAT3_MAX];
unsigned char territory[PAT3_MAX];  // 領地のパターン
unsigned char nb4_empty[PAT3_MAX];  // 上下左右の空点の数
bool empty_pat[PAT3_MAX];           //  8近傍に石がないパターン

int border_dis_x[BOARD_MAX];                     // x方向の距離   
int border_dis_y[BOARD_MAX];                     // y方向の距離   
int move_dis[PURE_BOARD_SIZE][PURE_BOARD_SIZE];  // 着手距離  

int onboard_pos[PURE_BOARD_MAX];  //  実際の盤上の位置との対応    

int corner[4];
int corner_neighbor[4][2];

///////////////
// 関数宣言  //
///////////////

// 4近傍の空点数の初期化
static void InitializeNeighbor( void );

// 眼のパターンの設定
static void InitializeEye( void );

// 地のパターンの設定
static void InitializeTerritory( void );

// 8近傍に石がないパターンを設定 
static void InitializeNeighborEmptyPattern( void );

// ダメ(pos)を連(string)に加える
// 加えたダメ(pos)を返す
static int AddLiberty( string_t *string, int pos, int head );

// ダメ(pos)を連(string)から取り除く
static void RemoveLiberty( game_info_t *game, string_t *string, int pos );

// ダメ(pos)を連(string)から取り除く
static void PoRemoveLiberty( game_info_t *game, string_t *string, int pos, int color );

// 石1つの連を作る
static void MakeString( game_info_t *game, int pos, int color );

// 連と1つの石の接続
static void AddStone( game_info_t *game, int pos, int color, int id );

/// 2つ以上の連の接続
static void ConnectString( game_info_t *game, int pos, int color, int connection, int id[] );

// 2つ以上の連のマージ
static void MergeString( game_info_t *game, string_t *dst, string_t *src[3], int n );

// 連に1つ石を加える
static void AddStoneToString( game_info_t *game, string_t *string, int pos, int head );

// 連を盤上から除去
// 取り除いた石の数を返す
static int RemoveString( game_info_t *game, string_t *string );

// 連を盤上から除去
// 取り除いた石の数を返す
static int PoRemoveString( game_info_t *game, string_t *string, int color );

// 隣接する連IDの追加
static void AddNeighbor( string_t *string, int id, int head );

// 隣接する連IDの削除
static void RemoveNeighborString( string_t *string, int id );

///////////////////////
//  盤の大きさの設定  //
///////////////////////
void
SetBoardSize( int size )
{
  int i, x, y;

  pure_board_size = size;
  pure_board_max = size * size;
  board_size = size + 2 * OB_SIZE;
  board_max = board_size * board_size;

  board_start = OB_SIZE;
  board_end = (pure_board_size + OB_SIZE - 1);

  i = 0;
  for (y = board_start; y <= board_end; y++) {
    for (x = board_start; x <= board_end; x++) {
      onboard_pos[i++] = POS(x, y);
      board_x[POS(x, y)] = x;
      board_y[POS(x, y)] = y;
    }
  }

  for (y = board_start; y <= board_end; y++) {
    for (x = board_start; x <= (board_start + pure_board_size / 2); x++) {
      border_dis_x[POS(x, y)] = x - (OB_SIZE - 1);
      border_dis_x[POS(board_end + OB_SIZE - x, y)] = x - (OB_SIZE - 1);
      border_dis_y[POS(y, x)] = x - (OB_SIZE - 1);
      border_dis_y[POS(y, board_end + OB_SIZE - x)] = x - (OB_SIZE - 1);
    }
  }

  for (y = 0; y < pure_board_size; y++) {
    for (x = 0; x < pure_board_size; x++) {
      move_dis[x][y] = x + y + ((x > y) ? x : y);
      if (move_dis[x][y] >= MOVE_DISTANCE_MAX) move_dis[x][y] = MOVE_DISTANCE_MAX - 1;
    }
  }

  memset(board_pos_id, 0, sizeof(board_pos_id));
  i = 1;
  for (y = board_start; y <= (board_start + pure_board_size / 2); y++) {
    for (x = board_start; x <= y; x++) {
      board_pos_id[POS(x, y)] = i;
      board_pos_id[POS(board_end + OB_SIZE - x, y)] = i;
      board_pos_id[POS(y, x)] = i;
      board_pos_id[POS(y, board_end + OB_SIZE - x)] = i;
      board_pos_id[POS(x, board_end + OB_SIZE - y)] = i;
      board_pos_id[POS(board_end + OB_SIZE - x, board_end + OB_SIZE - y)] = i;
      board_pos_id[POS(board_end + OB_SIZE - y, x)] = i;
      board_pos_id[POS(board_end + OB_SIZE - y, board_end + OB_SIZE - x)] = i;
      i++;
    }
  }

  corner[0] = POS(board_start, board_start);
  corner[1] = POS(board_start, board_end);
  corner[2] = POS(board_end, board_start);
  corner[3] = POS(board_end, board_end);

  corner_neighbor[0][0] = EAST(POS(board_start, board_start));
  corner_neighbor[0][1] = SOUTH(POS(board_start, board_start));
  corner_neighbor[1][0] = NORTH(POS(board_start, board_end));
  corner_neighbor[1][1] = EAST(POS(board_start, board_end));
  corner_neighbor[2][0] = WEST(POS(board_end, board_start));
  corner_neighbor[2][1] = SOUTH(POS(board_end, board_start));
  corner_neighbor[3][0] = NORTH(POS(board_end, board_end));
  corner_neighbor[3][1] = WEST(POS(board_end, board_end));
}

//////////////////////
//  コミの値の設定  //
//////////////////////
void
SetKomi( double new_komi )
{
  default_komi = new_komi;
  komi[0] = default_komi;
  komi[S_BLACK] = default_komi + 1;
  komi[S_WHITE] = default_komi - 1;
  dynamic_komi[0] = default_komi;
  dynamic_komi[S_BLACK] = default_komi + 1;
  dynamic_komi[S_WHITE] = default_komi - 1;
}

////////////////////////////
//  上下左右の座標の導出  //
////////////////////////////
void
GetNeighbor4( int neighbor4[4], int pos )
{
  neighbor4[0] = NORTH(pos);
  neighbor4[1] =  WEST(pos);
  neighbor4[2] =  EAST(pos);
  neighbor4[3] = SOUTH(pos);
}

////////////////////////
//  メモリ領域の確保  //
////////////////////////
game_info_t *
AllocateGame( void )
{
  game_info_t *game;
  game = (game_info_t*)malloc(sizeof(game_info_t));
  memset(game, 0, sizeof(game_info_t));

  return game;
}

////////////////////////
//  メモリ領域の解放  //
////////////////////////
void
FreeGame( game_info_t *game )
{
  if (game) free(game);
}

////////////////////////
//  対局情報の初期化  //
////////////////////////
void
InitializeBoard( game_info_t *game )
{
  int i, x, y, pos;

  memset(game->record,             0, sizeof(struct move) * MAX_RECORDS);
  memset(game->board,              0, sizeof(char) * board_max);            
  memset(game->pat,                0, sizeof(struct pattern) * board_max);  
  memset(game->tactical_features1, 0, sizeof(unsigned int) * board_max);    
  memset(game->tactical_features2, 0, sizeof(unsigned int) * board_max);    
  memset(game->update_num,         0, sizeof(int) * S_OB);
  memset(game->capture_num,        0, sizeof(int) * S_OB);
  memset(game->update_pos,         0, sizeof(int) * S_OB * pure_board_max); 
  memset(game->capture_pos,        0, sizeof(int) * S_OB * pure_board_max); 

  game->current_hash = 0;
  game->previous1_hash = 0;
  game->previous2_hash = 0;

  dynamic_komi[0] = default_komi;
  dynamic_komi[S_BLACK] = default_komi + 1.0;
  dynamic_komi[S_WHITE] = default_komi - 1.0;

  game->moves = 1;

  game->pass_count = 0;

  for (i = 0; i < BOARD_MAX; i++) { 
    game->candidates[i] = false;
  }

  for (y = 0; y < board_size; y++){
    for (x = 0; x < OB_SIZE; x++) {
      game->board[POS(x, y)] = S_OB;
      game->board[POS(y, x)] = S_OB;
      game->board[POS(y, board_size - 1 - x)] = S_OB;
      game->board[POS(board_size - 1 - x, y)] = S_OB;
    }
  }

  for (y = board_start; y <= board_end; y++) {
    for (x = board_start; x <= board_end; x++) {
      pos = POS(x, y);
      game->candidates[pos] = true;
    }
  }

  for (i = 0; i < MAX_STRING; i++) {
    game->string[i].flag = false;
  }

  ClearPattern(game->pat);

  InitializeNeighbor();
  InitializeNeighborEmptyPattern();
  InitializeEye();
}

//////////////
//  コピー  //
//////////////
void
CopyGame( game_info_t *dst, const game_info_t *src )
{
  int i;

  memcpy(dst->record,             src->record,             sizeof(struct move) * MAX_RECORDS);
  memcpy(dst->prisoner,           src->prisoner,           sizeof(int) * S_MAX);
  memcpy(dst->board,              src->board,              sizeof(char) * board_max);  
  memcpy(dst->pat,                src->pat,                sizeof(struct pattern) * board_max); 
  memcpy(dst->string_id,          src->string_id,          sizeof(int) * STRING_POS_MAX);
  memcpy(dst->string_next,        src->string_next,        sizeof(int) * STRING_POS_MAX);
  memcpy(dst->candidates,         src->candidates,         sizeof(bool) * board_max); 
  memcpy(dst->capture_num,        src->capture_num,        sizeof(int) * S_OB);
  memcpy(dst->update_num,         src->update_num,         sizeof(int) * S_OB);

  memset(dst->tactical_features1, 0, sizeof(unsigned int) * board_max);  
  memset(dst->tactical_features2, 0, sizeof(unsigned int) * board_max);  


  for (i = 0; i < MAX_STRING; i++) {
    if (src->string[i].flag) {
      memcpy(&dst->string[i], &src->string[i], sizeof(string_t));
    } else {
      dst->string[i].flag = false;
    }
  }

  dst->current_hash = src->current_hash;
  dst->previous1_hash = src->previous1_hash;
  dst->previous2_hash = src->previous2_hash;

  dst->pass_count = src->pass_count;

  dst->moves = src->moves;
  dst->ko_move = src->ko_move;
  dst->ko_pos = src->ko_pos;
}

////////////////////
//  定数の初期化  //
////////////////////
void
InitializeConst( void )
{
  int x, y;
  int i;

  komi[0] = default_komi;
  komi[S_BLACK] = default_komi + 1.0;
  komi[S_WHITE] = default_komi - 1.0;

  i = 0;
  for (y = board_start; y <= board_end; y++) {
    for (x = board_start; x <= board_end; x++) {
      onboard_pos[i++] = POS(x, y);
      board_x[POS(x, y)] = x;
      board_y[POS(x, y)] = y;
    }
  }

  for (y = board_start; y <= board_end; y++) {
    for (x = board_start; x <= (board_start + pure_board_size / 2); x++) {
      border_dis_x[POS(x, y)] = x - (OB_SIZE - 1);
      border_dis_x[POS(board_end + OB_SIZE - x, y)] = x - (OB_SIZE - 1);
      border_dis_y[POS(y, x)] = x - (OB_SIZE - 1);
      border_dis_y[POS(y, board_end + OB_SIZE - x)] = x - (OB_SIZE - 1);
    }
  }

  for (y = 0; y < pure_board_size; y++) {
    for (x = 0; x < pure_board_size; x++) {
      move_dis[x][y] = x + y + ((x>y) ? x : y);
      if (move_dis[x][y] >= MOVE_DISTANCE_MAX) move_dis[x][y] = MOVE_DISTANCE_MAX - 1;
    }
  }

  memset(board_pos_id, 0, sizeof(board_pos_id));
  i = 1;
  for (y = board_start; y <= (board_start + pure_board_size / 2); y++) {
    for (x = board_start; x <= y; x++) {
      board_pos_id[POS(x, y)] = i;
      board_pos_id[POS(board_end + OB_SIZE - x, y)] = i;
      board_pos_id[POS(y, x)] = i;
      board_pos_id[POS(y, board_end + OB_SIZE - x)] = i;
      board_pos_id[POS(x, board_end + OB_SIZE - y)] = i;
      board_pos_id[POS(board_end + OB_SIZE - x, board_end + OB_SIZE - y)] = i;
      board_pos_id[POS(board_end + OB_SIZE - y, x)] = i;
      board_pos_id[POS(board_end + OB_SIZE - y, board_end + OB_SIZE - x)] = i;
      i++;
    }
  }

  corner[0] = POS(board_start, board_start);
  corner[1] = POS(board_start, board_end);
  corner[2] = POS(board_end, board_start);
  corner[3] = POS(board_end, board_end);

  corner_neighbor[0][0] = EAST(POS(board_start, board_start));
  corner_neighbor[0][1] = SOUTH(POS(board_start, board_start));
  corner_neighbor[1][0] = NORTH(POS(board_start, board_end));
  corner_neighbor[1][1] = EAST(POS(board_start, board_end));
  corner_neighbor[2][0] = WEST(POS(board_end, board_start));
  corner_neighbor[2][1] = SOUTH(POS(board_end, board_start));
  corner_neighbor[3][0] = NORTH(POS(board_end, board_end));
  corner_neighbor[3][1] = WEST(POS(board_end, board_end));

  InitializeNeighbor();
  InitializeEye();
  InitializeTerritory();
}

//////////////////////////////
//  隣接する空点数の初期化  //
//////////////////////////////
static void
InitializeNeighbor( void )
{
  int i;
  char empty;

  for (i = 0; i < PAT3_MAX; i++) {
    empty = 0;

    if (((i >> 2) & 0x3) == S_EMPTY) empty++;
    if (((i >> 6) & 0x3) == S_EMPTY) empty++;
    if (((i >> 8) & 0x3) == S_EMPTY) empty++;
    if (((i >> 12) & 0x3) == S_EMPTY) empty++;

    nb4_empty[i] = empty;
  }
}

////////////////////////////
//  眼のパターンの初期化  //
////////////////////////////
static void
InitializeEye( void )
{
  int i, j;
  unsigned int transp[8];
  //  眼のパターンはそれぞれ1か所あたり2ビットで表現
  //  123
  //  4*5
  //  678
  //  それぞれの番号×2ビットだけシフトさせる
  //  O:自分の石
  //  X:相手の石
  //  +:空点
  //  #:盤外
  const int eye_pat3[] = {
    // +OO     XOO     +O+     XO+
    // O*O     O*O     O*O     O*O
    // OOO     OOO     OOO     OOO
    0x5554, 0x5556, 0x5544, 0x5546,

    // +OO     XOO     +O+     XO+
    // O*O     O*O     O*O     O*O
    // OO+     OO+     OO+     OO+
    0x1554, 0x1556, 0x1544, 0x1546,

    // +OX     XO+     +OO     OOO
    // O*O     O*O     O*O     O*O
    // OO+     +O+     ###     ###
    0x1564, 0x1146, 0xFD54, 0xFD55,

    // +O#     OO#     XOX     XOX
    // O*#     O*#     O+O     O+O
    // ###     ###     OOO     ###
    0xFF74, 0xFF75, 0x5566, 0xFD66,
  };
  const int false_eye_pat3[2] = {
    // XOO     XO# 
    // O*O     O*# 
    // ###     ### 
    0xFD56, 0xFF76,
  };

  // BBB
  // B*B
  // BBB
  eye[0x5555] = S_BLACK;

  // WWW
  // W*W
  // WWW
  eye[Pat3Reverse(0x5555)] = S_WHITE;

  // +B+
  // B*B
  // +B+
  eye[0x1144] = S_BLACK;

  // +W+
  // W*W
  // +W+
  eye[Pat3Reverse(0x1144)] = S_WHITE;

  for (i = 0; i < 14; i++) {
    Pat3Transpose8(eye_pat3[i], transp);
    for (j = 0; j < 8; j++) {
      eye[transp[j]] = S_BLACK;
      eye[Pat3Reverse(transp[j])] = S_WHITE;
    }
  }

  for (i = 0; i < 2; i++) {
    Pat3Transpose8(false_eye_pat3[i], transp);
    for (j = 0; j < 8; j++) {
      false_eye[transp[j]] = S_BLACK;
      false_eye[Pat3Reverse(transp[j])] = S_WHITE;
    }
  }
}

/////////////////////////////////////////
//  地のパターン（4近傍が同色）を設定  //
/////////////////////////////////////////
static void
InitializeTerritory( void )
{
  int i;

  for (i = 0; i < PAT3_MAX; i++) {
    if ((i & 0x1144) == 0x1144) {
      territory[i] = S_BLACK;
    } else if ((i & 0x2288) == 0x2288) {
      territory[i] = S_WHITE;
    }
  }
}

/////////////////////////////////////
//  8近傍に石がないパターンを設定  //
/////////////////////////////////////
static void
InitializeNeighborEmptyPattern( void )
{
  int i, j;
  unsigned int transp[8];
  unsigned int empty_pattern[3] = {
    0x0000, 0x003f, 0xc33f
  };

  for (i = 0; i < PAT3_MAX; i++) {
    empty_pat[i] = false;
  }

  for (i = 0; i < 3; i++) {
    Pat3Transpose8(empty_pattern[i], transp);
    for (j = 0; j < 8; j++) {
      empty_pat[transp[j]] = true;
    }
  }
}

//////////////////
//  合法手判定  //
//////////////////
bool
IsLegal( const game_info_t *game, int pos, int color )
{
  // 既に石がある
  if (game->board[pos] != S_EMPTY) {
    return false;
  }

  // 自殺手である
  if (nb4_empty[Pat3(game->pat, pos)] == 0 &&
      IsSuicide(game, game->string, color, pos)) {
    return false;
  }

  // 劫である
  if (game->ko_pos == pos &&
      game->ko_move == (game->moves - 1)) {
    return false;
  }

  return true;
}

////////////////////
//  盤端での処理  //
////////////////////
bool
IsEdgeConnection( const game_info_t *game, int pos, int color )
{
  // +++++XOO#
  // +++++XO+#
  // +++XXXOO#
  // ++XOOXXO#
  // +++O*OO*#
  // #########
  // シミュレーション中に上の局面の*を眼と認識せずに打つように,
  // ++++XXXX#
  // +++XXOOO#
  // +++XO+XO#
  // +++XOOO*#
  // #########
  // シミュレーション中に上の局面の*を眼と認識して打たないようにする.
  //
  // 隣接する2つの自分の連の呼吸点に共通するものがなければ打ち
  // 共通するものがあれば打たないようにしている.
  // 以下の局面は誤認識して打ってしまうので要対応.
  // ++++XXXX#
  // +++XXOOO#
  // +++XOX+O#
  // +++XO+XO#
  // +++XOOO*#
  // #########
  const string_t *string = game->string;
  const int *string_id = game->string_id;
  const char *board = game->board;
  int checked_string[4] = { 0 };
  int string_liberties[4] = { 0 };
  int strings = 0;
  int id, lib, libs = 0, lib_sum;
  int liberty[STRING_LIB_MAX];
  int i, j, count;
  bool checked;
  int neighbor4[4];
  bool already_checked;

  GetNeighbor4(neighbor4, pos);

  // 隣接する座標が自分の連なら
  // その連の呼吸点を取り出す
  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == color) {
      id = string_id[neighbor4[i]];
      already_checked = false;
      for (j = 0; j < strings; j++) {
	if (checked_string[j] == id) {
	  already_checked = true;
	  break;
	}
      }
      if (already_checked) continue;
      lib = string[id].lib[0];
      count = 0;
      while (lib != LIBERTY_END) {
	if (lib != pos) {
	  checked = false;
	  for (i = 0; i < libs; i++) {
	    if (liberty[i] == lib) {
	      checked = true;
	      break;
	    }
	  }
	  if (!checked) {
	    liberty[libs + count] = lib;
	    count++;
	  }
	}
	lib = string[id].lib[lib];
      }
      libs += count;
      string_liberties[strings] = string[id].libs;
      checked_string[strings++] = id;
    }
  }

  // その連が持っている呼吸点を求める
  for (i = 0, lib_sum = 0; i < strings; i++) {
    lib_sum += string_liberties[i] - 1;
  }

  // 隣接する連が一続きなら眼なのでfalseを返す
  if (strings == 1) {
    return false;
  }

  // 2つの連が呼吸点を共有していなければtrue
  // そうでなければfalseを返す
  if (libs == lib_sum) {
    return true;
  } else {
    return false;
  }
}

////////////////////////////////////
//  合法手でかつ目でないかを判定  //
////////////////////////////////////
bool
IsLegalNotEye( game_info_t *game, int pos, int color )
{
  int *string_id = game->string_id;
  string_t *string = game->string;

  // 既に石がある
  if (game->board[pos] != S_EMPTY) {
    // 候補手から除外
    game->candidates[pos] = false;

    return false;
  }

  // 眼
  if (eye[Pat3(game->pat, pos)] != color ||
      string[string_id[NORTH(pos)]].libs == 1 ||
      string[string_id[EAST(pos)]].libs == 1 ||
      string[string_id[SOUTH(pos)]].libs == 1 ||
      string[string_id[WEST(pos)]].libs == 1){

    // 自殺手かどうか
    if (nb4_empty[Pat3(game->pat, pos)] == 0 &&
	IsSuicide(game, string, color, pos)) {
      return false;
    }

    // 劫
    if (game->ko_pos == pos &&
	game->ko_move == (game->moves - 1)) {
      return false;
    }

    // 盤端の特殊処理
    if (false_eye[Pat3(game->pat, pos)] == color) {
      if (IsEdgeConnection(game, pos, color)) {
	return true;
      } else {
	game->candidates[pos] = false;
	return false;
      }
    }

    return true;
  }

  // 候補手から除外
  game->candidates[pos] = false;

  return false;
}

////////////////////
//  自殺手の判定  //
////////////////////
bool
IsSuicide( const game_info_t *game, const string_t *string, int color, int pos )
{
  const char *board = game->board;
  const int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  int neighbor4[4], i;

  GetNeighbor4(neighbor4, pos);

  // 隣接するの石についての判定
  // 隣接する石が相手でも、その石を含む連の呼吸点が1の時は合法手
  // 隣接する石が自分で、その石を含む連の呼吸点が2以上の時は合法手
  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == other &&
	string[string_id[neighbor4[i]]].libs == 1) {
      return false;
    } else if (board[neighbor4[i]] == color &&
	       string[string_id[neighbor4[i]]].libs > 1) {
      return false;
    }
  }

  return true;
}

////////////////
//  石を置く  //
////////////////
void
PutStone( game_info_t *game, int pos, int color )
{
  int *string_id = game->string_id;
  char *board = game->board;
  string_t *string = game->string;
  int other = FLIP_COLOR(color);
  int connection = 0;
  int connect[4] = { 0 };
  int prisoner = 0;
  int neighbor[4];
  int i;

  // この手番の着手で打ち上げた石の数を0にする
  game->capture_num[color] = 0;

  // 着手箇所の戦術的特徴を全て消す
  game->tactical_features1[pos] = 0;
  game->tactical_features2[pos] = 0;

  game->previous2_hash = game->previous1_hash;
  game->previous1_hash = game->current_hash;

  if (game->ko_move != 0 && game->ko_move == game->moves - 1) {
    game->current_hash ^= hash_bit[game->ko_pos][HASH_KO];
  }

  // 着手箇所と色を記録
  if (game->moves < MAX_RECORDS) {
    game->record[game->moves].color = color;
    game->record[game->moves].pos = pos;
  }

  // 着手がパスなら手数を進めて終了
  if (pos == PASS) {
    game->current_hash ^= hash_bit[game->pass_count++][HASH_PASS];
    if (game->pass_count >= BOARD_MAX) { 
      game->pass_count = 0;
    }
    game->moves++;
    return;
  }

  // 石を置く
  board[pos] = (char)color;

  // 候補手から除外
  game->candidates[pos] = false;

  game->current_hash ^= hash_bit[pos][color];

  // パターンの更新(MD5)
  UpdatePatternStone(game->pat, color, pos);

  // 着手点の上下左右の座標を導出
  GetNeighbor4(neighbor, pos);

  // 着手箇所の上下左右の確認
  // 自分の連があれば, その連の呼吸点を1つ減らし, 接続候補に入れる
  // 敵の連であれば, その連の呼吸点を1つ減らし, 呼吸点が0になったら取り除く
  for (i = 0; i < 4; i++) {
    if (board[neighbor[i]] == color) {
      RemoveLiberty(game, &string[string_id[neighbor[i]]], pos);
      connect[connection++] = string_id[neighbor[i]];
    } else if (board[neighbor[i]] == other) {
      RemoveLiberty(game, &string[string_id[neighbor[i]]], pos);
      if (string[string_id[neighbor[i]]].libs == 0) {
	prisoner += RemoveString(game, &string[string_id[neighbor[i]]]);
      }
    }
  }

  // 打ち上げた石をアゲハマに追加
  game->prisoner[color] += prisoner;

  // 接続候補がなければ, 新しい連を作成して, 劫かどうかの確認をする
  // 接続候補が1つならば, その連に石を追加する
  // 接続候補が2つ以上ならば, 連同士を繋ぎ合わせて, 石を追加する
  if (connection == 0) {
    MakeString(game, pos, color);
    if (prisoner == 1 &&
	string[string_id[pos]].libs == 1) {
      game->ko_move = game->moves;
      game->ko_pos = string[string_id[pos]].lib[0];
      game->current_hash ^= hash_bit[game->ko_pos][HASH_KO];
    }
  } else if (connection == 1) {
    AddStone(game, pos, color, connect[0]);
  } else {
    ConnectString(game, pos, color, connection, connect);
  }

  // 手数を1つだけ進める
  game->moves++;
}

////////////////
//  石を置く  //
////////////////
void
PoPutStone( game_info_t *game, int pos, int color )
{
  int *string_id = game->string_id;
  char *board = game->board;
  string_t *string = game->string;
  int other = FLIP_COLOR(color);
  int connection = 0;
  int connect[4] = { 0 };
  int prisoner = 0;
  int neighbor[4];
  int i;

  // この手番で取った石の個数を0に
  game->capture_num[color] = 0;

  // 着手制限の限界を超えていなければ記録
  if (game->moves < MAX_RECORDS) {
    game->record[game->moves].color = color;
    game->record[game->moves].pos = pos;
  }

  // 着手がパスなら手数を進めて終了
  if (pos == PASS) {
    game->moves++;
    return;
  }

  // 碁盤に石を置く
  board[pos] = (char)color;

  // 候補酒から除外
  game->candidates[pos] = false;

  // 着手箇所の戦術的特徴を全て消す
  game->tactical_features1[pos] = 0;
  game->tactical_features2[pos] = 0;

  // 着手箇所のレートを0に戻す
  game->sum_rate[0] -= game->rate[0][pos];
  game->sum_rate_row[0][board_y[pos]] -= game->rate[0][pos];
  game->rate[0][pos] = 0;
  game->sum_rate[1] -= game->rate[1][pos];
  game->sum_rate_row[1][board_y[pos]] -= game->rate[1][pos];
  game->rate[1][pos] = 0;

  // パターンの更新(MD2)  
  UpdateMD2Stone(game->pat, color, pos);

  // 着手箇所の上下左右の座標の導出
  GetNeighbor4(neighbor, pos);

  // 着手箇所の上下左右の確認
  // 自分の連があれば, その連の呼吸点を1つ減らし, 接続候補に入れる
  // 敵の連であれば, その連の呼吸点を1つ減らし, 呼吸点が0になったら取り除く  
  for (i = 0; i < 4; i++) {
    if (board[neighbor[i]] == color) {
      PoRemoveLiberty(game, &string[string_id[neighbor[i]]], pos, color);
      connect[connection++] = string_id[neighbor[i]];
    } else if (board[neighbor[i]] == other) {
      PoRemoveLiberty(game, &string[string_id[neighbor[i]]], pos, color);
      if (string[string_id[neighbor[i]]].libs == 0) {
  prisoner += PoRemoveString(game, &string[string_id[neighbor[i]]], color);
      }
    }
  }

  // 打ち上げた石をアゲハマに追加
  game->prisoner[color] += prisoner;

  // 接続候補がなければ, 新しい連を作成して, 劫かどうかの確認をする
  // 接続候補が1つならば, その連に石を追加する
  // 接続候補が2つ以上ならば, 連同士を繋ぎ合わせて, 石を追加する  
  if (connection == 0) {
    MakeString(game, pos, color);
    if (prisoner == 1 &&
	string[string_id[pos]].libs == 1) {
      game->ko_move = game->moves;
      game->ko_pos = string[string_id[pos]].lib[0];
    }
  } else if (connection == 1) {
    AddStone(game, pos, color, connect[0]);
  } else {
    ConnectString(game, pos, color, connection, connect);
  }

  // 手数を進める
  game->moves++;
}

//////////////////////
//  新しい連の作成  //
//////////////////////
static void
MakeString( game_info_t *game, int pos, int color )
{
  string_t *string = game->string;
  string_t *new_string;
  char *board = game->board;
  int *string_id = game->string_id;
  int id = 1;
  int lib_add = 0;
  int other = FLIP_COLOR(color);
  int neighbor, neighbor4[4], i;

  // 未使用の連のインデックスを見つける
  while (string[id].flag) { id++; }

  // 新しく連のデータを格納する箇所を保持
  new_string = &game->string[id];

  // 連のデータの初期化
  memset(new_string->lib, 0, sizeof(short) * STRING_LIB_MAX);
  memset(new_string->neighbor, 0, sizeof(short) * MAX_NEIGHBOR);
  new_string->lib[0] = LIBERTY_END;
  new_string->neighbor[0] = NEIGHBOR_END;
  new_string->libs = 0;
  new_string->color = (char)color;
  new_string->origin = pos;
  new_string->size = 1;
  new_string->neighbors = 0;
  game->string_id[pos] = id;
  game->string_next[pos] = STRING_END;

  // 上下左右の座標の導出
  GetNeighbor4(neighbor4, pos);

  // 新しく作成した連の上下左右の座標を確認
  // 空点ならば, 作成した連に呼吸点を追加する
  // 敵の連ならば, 隣接する連をお互いに追加する
  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == S_EMPTY) {
      lib_add = AddLiberty(new_string, neighbor4[i], lib_add);
    } else if (board[neighbor4[i]] == other) {
      neighbor = string_id[neighbor4[i]];
      AddNeighbor(&string[neighbor], id, 0);
      AddNeighbor(&string[id], neighbor, 0);
    }
  }

  // 連の存在フラグをオンにする
  new_string->flag = true;
}

///////////////////////////
//  連に石を1つ追加する  //
///////////////////////////
static void
AddStoneToString( game_info_t *game, string_t *string, int pos, int head )
// game_info_t *game : 盤面の情報を示すポインタ 
// string_t *string  : 石の追加先の連
// int pos         : 追加する石の座標
// int head        : 高速処理のための変数
{
  int *string_next = game->string_next;
  int str_pos;

  if (pos == STRING_END) return;

  // 追加先の連の先頭の前ならば先頭に追加
  // そうでなければ挿入位置を探し出し追加
  if (string->origin > pos) {
    string_next[pos] = string->origin;
    string->origin = pos;
  } else {
    if (head != 0) {
      str_pos = head;
    } else {
      str_pos = string->origin;
    }
    while (string_next[str_pos] < pos){
      if (str_pos == string_next[str_pos]) {
        cerr << "Illegal string" << endl;
        abort();
      }
      str_pos = string_next[str_pos];
    }
    string_next[pos] = string_next[str_pos];
    string_next[str_pos] = pos;
  }
  string->size++;
}

////////////////////////
//  連に石を追加する  //
////////////////////////
static void
AddStone( game_info_t *game, int pos, int color, int id )
// game_info_t *game : 盤面の情報を示すポインタ
// int pos           : 置いた石の座標
// int color         : 置いた石の色
// int id            : 石を追加する先の連のID
{
  string_t *string = game->string;
  string_t *add_str;
  char *board = game->board;
  int *string_id = game->string_id;
  int lib_add = 0;
  int other = FLIP_COLOR(color);
  int neighbor, neighbor4[4], i;

  // IDを更新
  string_id[pos] = id;

  // 追加先の連を取り出す
  add_str = &string[id];

  // 石を追加する
  AddStoneToString(game, add_str, pos, 0);

  // 上下左右の座標の導出
  GetNeighbor4(neighbor4, pos);

  // 空点なら呼吸点を追加し
  // 敵の石があれば隣接する敵連の情報を更新
  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == S_EMPTY) {
      lib_add = AddLiberty(add_str, neighbor4[i], lib_add);
    } else if (board[neighbor4[i]] == other) {
      neighbor = string_id[neighbor4[i]];
      AddNeighbor(&string[neighbor], id, 0);
      AddNeighbor(&string[id], neighbor, 0);
    }
  }
}

//////////////////////////
//  連同士の結合の判定  //
//////////////////////////
static void
ConnectString( game_info_t *game, int pos, int color, int connection, int id[] )
// game_info_t *game : 盤面の情報を示すポインタ
// int pos           : 置いた石の座標
// int color         : 置いた石の色
// int connection    : 接続する連の候補の個数
// int id[]          : 接続する連の候補のID
{
  int i, j, min = id[0];
  string_t *string = game->string;
  string_t *str[3];
  int connections = 0;
  bool flag = true;

  // 接続する連の個数を厳密に確認
  for (i = 1; i < connection; i++) {
    flag = true;
    for (j = 0; j < i; j++) {
      if (id[j] == id[i]) {
	flag = false;
	break;
      }
    }
    if (flag) {
      if (min > id[i]) {
	str[connections] = &string[min];
	min = id[i];
      } else {
	str[connections] = &string[id[i]];
      }
      connections++;
    }
  }

  // 石を追加
  AddStone(game, pos, color, min);

  // 複数の連が接続するときの処理
  if (connections > 0) {
    MergeString(game, &game->string[min], str, connections);
  }
}

////////////////
//  連の結合  //
////////////////
static void
MergeString( game_info_t *game, string_t *dst, string_t *src[3], int n )
// game_info_t *game : 盤面の情報を示すポインタ
// string_t *dst     : マージ先の連
// string_t *src[3]  : マージ元の連(最大3つ)
// int n             : マージする連の個数
{
  int i, tmp, pos, prev, neighbor;
  int *string_next = game->string_next;
  int *string_id = game->string_id;
  int id = string_id[dst->origin], rm_id;
  string_t *string = game->string;

  for (i = 0; i < n; i++) {
    // 接続で消える連のID
    rm_id = string_id[src[i]->origin];

    // 呼吸点をマージ
    prev = 0;
    pos = src[i]->lib[0];
    while (pos != LIBERTY_END) {
      prev = AddLiberty(dst, pos, prev);
      pos = src[i]->lib[pos];
    }

    // 連のIDを更新
    prev = 0;
    pos = src[i]->origin;
    while (pos != STRING_END) {
      string_id[pos] = id;
      tmp = string_next[pos];
      AddStoneToString(game, dst, pos, prev);
      prev = pos;
      pos = tmp;
    }

    // 隣接する敵連の情報をマージ
    prev = 0;
    neighbor = src[i]->neighbor[0];
    while (neighbor != NEIGHBOR_END) {
      RemoveNeighborString(&string[neighbor], rm_id);
      AddNeighbor(dst, neighbor, prev);
      AddNeighbor(&string[neighbor], id, prev);
      prev = neighbor;
      neighbor = src[i]->neighbor[neighbor];
    }

    // 使用済みフラグをオフ
    src[i]->flag = false;
  }
}

////////////////////
//  呼吸点の追加  //
////////////////////
static int
AddLiberty( string_t *string, int pos, int head )
// string_t *string : 呼吸点を追加する対象の連
// int pos        : 追加する呼吸点の座標
// int head       : 探索対象の先頭のインデックス
{
  int lib;

  // 既に追加されている場合は何もしない
  if (string->lib[pos] != 0) return pos;

  // 探索対象の先頭のインデックスを代入
  lib = head;

  // 追加する場所を見つけるまで進める
  while (string->lib[lib] < pos) {
    lib = string->lib[lib];
  }

  // 呼吸点の座標を追加する
  string->lib[pos] = string->lib[lib];
  string->lib[lib] = (short)pos;

  // 呼吸点の数を1つ増やす
  string->libs++;

  // 追加した呼吸点の座標を返す
  return pos;
}

////////////////////
//  呼吸点の除去  //
////////////////////
static void
RemoveLiberty( game_info_t *game, string_t *string, int pos )
// game_info_t *game : 盤面の情報を示すポインタ
// string_t *string  : 呼吸点を取り除く対象の連
// int pos         : 取り除かれる呼吸点
{
  int lib = 0;

  // 既に取り除かれている場合は何もしない
  if (string->lib[pos] == 0) return;

  // 取り除く呼吸点の座標を見つけるまで進める
  while (string->lib[lib] != pos) {
    lib = string->lib[lib];
  }

  // 呼吸点の座標の情報を取り除く
  string->lib[lib] = string->lib[string->lib[lib]];
  string->lib[pos] = (short)0;

  // 連の呼吸点の数を1つ減らす
  string->libs--;

  // 呼吸点が1つならば, その連の呼吸点を候補手に追加
  if (string->libs == 1) {
    game->candidates[string->lib[0]] = true;
  }
}

//////////////////////
//  呼吸点の除去    //
// (プレイアウト用) //
//////////////////////
static void
PoRemoveLiberty( game_info_t *game, string_t *string, int pos, int color )
// game_info_t *game : 盤面の情報を示すポインタ
// string_t *string  : 呼吸点を取り除く対象の連
// int pos         : 取り除かれる呼吸点
// int color       : その手番の色
{
  int lib = 0;

  // 既に取り除かれている場合は何もしない
  if (string->lib[pos] == 0) return;

  // 取り除く呼吸点の座標を見つけるまで進める
  while (string->lib[lib] != pos) {
    lib = string->lib[lib];
  }

  // 呼吸点の座標の情報を取り除く
  string->lib[lib] = string->lib[string->lib[lib]];
  string->lib[pos] = 0;

  // 呼吸点の数を1つ減らす
  string->libs--;

  // 連の呼吸点の数を確認
  // 呼吸点が1つならば, その呼吸点を候補手に戻して, レートの更新対象に加える
  // 呼吸点が2つならば, レートの更新対象に加える
  if (string->libs == 1) {
    game->candidates[string->lib[0]] = true;
    game->update_pos[color][game->update_num[color]++] = string->lib[0];
  }
}

////////////////
//  連の除去  //
////////////////
static int
RemoveString( game_info_t *game, string_t *string )
// game_info_t *game : 盤面の情報を示すポインタ
// string_t *string  : 取り除く対象の連
{
  string_t *str = game->string;
  int *string_next = game->string_next;
  int *string_id = game->string_id;
  int pos = string->origin, next;
  char *board = game->board;
  bool *candidates = game->candidates;
  int neighbor, rm_id = string_id[string->origin];
  int removed_color = board[pos];

  do {
    // 空点に戻す
    board[pos] = S_EMPTY;

    // 候補手に追加する
    candidates[pos] = true;

    // パターンの更新
    UpdatePatternEmpty(game->pat, pos);

    game->current_hash ^= hash_bit[pos][removed_color];

    // 上下左右を確認する
    // 隣接する連があれば呼吸点を追加する
    if (str[string_id[NORTH(pos)]].flag) AddLiberty(&str[string_id[NORTH(pos)]], pos, 0);
    if (str[string_id[ WEST(pos)]].flag) AddLiberty(&str[string_id[ WEST(pos)]], pos, 0);
    if (str[string_id[ EAST(pos)]].flag) AddLiberty(&str[string_id[ EAST(pos)]], pos, 0);
    if (str[string_id[SOUTH(pos)]].flag) AddLiberty(&str[string_id[SOUTH(pos)]], pos, 0);

    // 連を構成する次の石の座標を記録
    next = string_next[pos];

    // 連の構成要素から取り除き, 
    // 石を取り除いた箇所の連IDを元に戻す
    string_next[pos] = 0;
    string_id[pos] = 0;

    // 連を構成する次の石の座標に移動
    pos = next;
  } while (pos != STRING_END);

  // 取り除いた連に隣接する連から隣接情報を取り除く
  neighbor = string->neighbor[0];
  while (neighbor != NEIGHBOR_END) {
    RemoveNeighborString(&str[neighbor], rm_id);
    neighbor = string->neighbor[neighbor];
  }

  // 連の存在フラグをオフ
  string->flag = false;

  // 打ち上げた石の数を返す
  return string->size;
}

////////////////
//  連の除去  //
////////////////
static int
PoRemoveString( game_info_t *game, string_t *string, int color )
// game_info_t *game : 盤面の情報を示すポインタ
// string_t *string  : 取り除く対象の連
// int color       : 手番の色(連を構成する色とは違う色)
{
  string_t *str = game->string;
  int *string_next = game->string_next;
  int *string_id = game->string_id;
  int pos = string->origin, next;
  char *board = game->board;
  bool *candidates = game->candidates;
  int neighbor, rm_id = string_id[string->origin];
  int *capture_pos = game->capture_pos[color];
  int *capture_num = &game->capture_num[color];
  int *update_pos = game->update_pos[color];
  int *update_num = &game->update_num[color];
  int lib;
  
  // 隣接する連の呼吸点を更新の対象に加える
  neighbor = string->neighbor[0];
  while (neighbor != NEIGHBOR_END) {
    if (str[neighbor].libs < 3) {
      lib = str[neighbor].lib[0];
      while (lib != LIBERTY_END) {
  update_pos[(*update_num)++] = lib;
  lib = str[neighbor].lib[lib];
      }
    }
    neighbor = string->neighbor[neighbor];
  }
  
  do {
    // 空点に戻す
    board[pos] = S_EMPTY;
    // 候補手に追加する
    candidates[pos] = true;

    // レーティング更新対象に追加
    capture_pos[(*capture_num)++] = pos;

    // 3x3のパターンの更新
    UpdateMD2Empty(game->pat, pos);
    
    // 上下左右を確認する
    // 隣接する連があれば呼吸点を追加する
    if (str[string_id[NORTH(pos)]].flag) AddLiberty(&str[string_id[NORTH(pos)]], pos, 0);
    if (str[string_id[ WEST(pos)]].flag) AddLiberty(&str[string_id[ WEST(pos)]], pos, 0);
    if (str[string_id[ EAST(pos)]].flag) AddLiberty(&str[string_id[ EAST(pos)]], pos, 0);
    if (str[string_id[SOUTH(pos)]].flag) AddLiberty(&str[string_id[SOUTH(pos)]], pos, 0);

    // 連を構成する次の石の座標を記録
    next = string_next[pos];

    // 連の構成要素から取り除き, 
    // 石を取り除いた箇所の連IDを元に戻す
    string_next[pos] = 0;
    string_id[pos] = 0;

    // 連を構成する次の石へ移動
    pos = next;
  } while (pos != STRING_END);

  // 取り除いた連に隣接する連から隣接情報を取り除く
  neighbor = string->neighbor[0];
  while (neighbor != NEIGHBOR_END) {
    RemoveNeighborString(&str[neighbor], rm_id);
    neighbor = string->neighbor[neighbor];
  }

  // 連の存在フラグをオフ
  string->flag = false;

  // 打ち上げた石の個数を返す
  return string->size;
}

////////////////////////////////////
//  隣接する連IDの追加(重複確認)  //
////////////////////////////////////
static void
AddNeighbor( string_t *string, int id, int head )
// string_t *string : 隣接情報を追加する連
// int id         : 追加される連ID
// int head       : 探索対象の先頭のインデックス
{
  int neighbor = 0;

  // 既に追加されている場合は何もしない
  if (string->neighbor[id] != 0) return;

  // 探索対象の先頭のインデックスを代入
  neighbor = head;

  // 追加場所を見つけるまで進める
  while (string->neighbor[neighbor] < id) {
    neighbor = string->neighbor[neighbor];
  }

  // 隣接する連IDを追加する
  string->neighbor[id] = string->neighbor[neighbor];
  string->neighbor[neighbor] = (short)id;

  // 隣接する連の数を1つ増やす
  string->neighbors++;
}

//////////////////////////
//  隣接する連IDの除去  //
//////////////////////////
static void
RemoveNeighborString( string_t *string, int id )
// string_t *string : 隣接する連のIDを取り除く対象の連
// int id         : 取り除く連のID
{
  int neighbor = 0;

  // 既に除外されていれば何もしない
  if (string->neighbor[id] == 0) return;

  // 取り除く隣接する連IDを見つけるまで進める
  while (string->neighbor[neighbor] != id) {
    neighbor = string->neighbor[neighbor];
  }

  // 隣接する連IDを取り除く
  string->neighbor[neighbor] = string->neighbor[string->neighbor[neighbor]];
  string->neighbor[id] = 0;

  // 隣接する連の数を1つ減らす
  string->neighbors--;
}

///////////////////////////
//  隅のマガリ四目の確認  //
///////////////////////////
void
CheckBentFourInTheCorner( game_info_t *game )
{
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int *string_next = game->string_next;
  int pos;
  int i;
  int id;
  int neighbor;
  int color;
  int lib1, lib2;
  int neighbor_lib1, neighbor_lib2;

  // 四隅について隅のマガリ四目が存在するか確認し
  // 存在すれば地を訂正する
  for (i = 0; i < 4; i++) {
    id = string_id[corner[i]];
    if (string[id].size == 3 &&
  string[id].libs == 2 &&
  string[id].neighbors == 1) {
      color = string[id].color;
      lib1 = string[id].lib[0];
      lib2 = string[id].lib[lib1];
      if ((board[corner_neighbor[i][0]] == S_EMPTY ||
     board[corner_neighbor[i][0]] == color) &&
    (board[corner_neighbor[i][1]] == S_EMPTY ||
     board[corner_neighbor[i][1]] == color)) {
  neighbor = string[id].neighbor[0];
  if (string[neighbor].libs == 2 &&
      string[neighbor].size > 6) {
    // 呼吸点を共有しているかの確認
    neighbor_lib1 = string[neighbor].lib[0];
    neighbor_lib2 = string[neighbor].lib[neighbor_lib1];
    if ((neighbor_lib1 == lib1 && neighbor_lib2 == lib2) ||
        (neighbor_lib1 == lib2 && neighbor_lib2 == lib1)) {
      pos = string[neighbor].origin;
      while (pos != STRING_END) {
        board[pos] = (char)color;
        pos = string_next[pos];
      }
      pos = string[neighbor].lib[0];
      board[pos] = (char)color;
      pos = string[neighbor].lib[pos];
      board[pos] = (char)color;
    }
  }
      }
    }
  }
}

////////////////
//  地の計算  //
////////////////
int
CalculateScore( game_info_t *game )
// game_info_t *game : 盤面の情報を示すポインタ
{
  const char *board = game->board;
  int i;
  int pos;
  int color;
  int scores[S_MAX] = { 0 };

  // 隅のマガリ四目の確認
  CheckBentFourInTheCorner(game);

  // 地の数え上げ
  for (i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    color = board[pos];
    if (color == S_EMPTY) color = territory[Pat3(game->pat, pos)];
    scores[color]++;
  }

  //  黒－白を返す(コミなし)
  return(scores[S_BLACK] - scores[S_WHITE]);
}