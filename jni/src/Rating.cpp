#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <iomanip>
#include <climits>
#include <algorithm>
#include <functional>
#include <string>

#include "Message.h"
#include "Nakade.h"
#include "Point.h"
#include "Rating.h"
#include "Semeai.h"
#include "Utility.h"

using namespace std;

////////////////
//    変数    //
////////////////

// 戦術的特徴のγ値
float po_tactical_features[TACTICAL_FEATURE_MAX];
// 3x3パターンのγ値
float po_pat3[PAT3_MAX];
// // MD2のパターンのγ値
// float po_md2[MD2_MAX];
// // 3x3とMD2のパターンのγ値の積
// float po_pattern[MD2_MAX];
// MD2のパターンのγ値
float *po_md2;
// 3x3とMD2のパターンのγ値の積
float *po_pattern;
// 学習した着手距離の特徴 
float po_neighbor_orig[PREVIOUS_DISTANCE_MAX];
// 補正した着手距離の特徴
float po_previous_distance[PREVIOUS_DISTANCE_MAX];
// 戦術的特徴1
float po_tactical_set1[PO_TACTICALS_MAX1];
// 戦術的特徴2
float po_tactical_set2[PO_TACTICALS_MAX2];
// パラメータのファイルを格納しているディレクトリのパス
char po_params_path[1024];

// ビットマスク
unsigned int po_tactical_features_mask[F_MASK_MAX] = {
  0x00000001,
  0x00000002,
  0x00000004,
  0x00000008,
  0x00000010,

  0x00000020,
  0x00000040,
  0x00000080,
  0x00000100,
  0x00000200,

  0x00000400,
  0x00000800,
  0x00001000,
  0x00002000,
  0x00004000,

  0x00008000,
  0x00010000,
  0x00020000,
  0x00040000,
  0x00080000,

  0x00100000,
  0x00200000,
  0x00400000,
  0x00800000,
  0x01000000,

  0x02000000,
  0x04000000,
  0x08000000,
  0x10000000,
  0x20000000,
};

// MD2パターンが届く範囲
static int neighbor[UPDATE_NUM];

// コスミの位置
static int cross[4];

// 着手距離2, 3のγ値の補正
double neighbor_bias = NEIGHBOR_BIAS;
// 着手距離4のγ値の補正
double jump_bias = JUMP_BIAS;
double po_bias = PO_BIAS;

//////////////////
//  関数の宣言  //
//////////////////

//  γ読み込み
static void InputPOGamma( void );
static void InputMD2( const char *filename, float *ap );

/////////////////
// 近傍の設定  //
/////////////////
void
SetNeighbor( void )
{
  neighbor[ 0] = -2 * board_size;
  neighbor[ 1] = - board_size - 1;
  neighbor[ 2] = - board_size;
  neighbor[ 3] = - board_size + 1;
  neighbor[ 4] = -2;
  neighbor[ 5] = -1;
  neighbor[ 6] = 0;
  neighbor[ 7] = 1;
  neighbor[ 8] = 2;
  neighbor[ 9] = board_size - 1;
  neighbor[10] = board_size;
  neighbor[11] = board_size + 1;
  neighbor[12] = 2 * board_size;

  cross[0] = -board_size - 1;
  cross[1] = -board_size + 1;
  cross[2] = board_size - 1;
  cross[3] = board_size + 1;
}

//////////////
//  初期化  //
//////////////
void
InitializeRating( void )
{
  // γ読み込み
  InputPOGamma();
  // 戦術的特徴をまとめる
  InitializePoTacticalFeaturesSet();
}

////////////////////////////
//  戦術的特徴をまとめる  //
////////////////////////////
void
InitializePoTacticalFeaturesSet( void )
{
  int i;
  double rate;

  for (i = 0; i < PO_TACTICALS_MAX1; i++){
    rate = 1.0;

    if ((i & po_tactical_features_mask[F_SAVE_CAPTURE3_3]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE3_3];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE3_2]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE3_2];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE3_1]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE3_1];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE2_3]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE2_3];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE2_2]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE2_2];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE2_1]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE2_1];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE1_3]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE1_3];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE1_2]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE1_2];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE1_1]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE1_1];
    } else if ((i & po_tactical_features_mask[F_SAVE_CAPTURE_SELF_ATARI]) > 0) {
      rate *= po_tactical_features[F_SAVE_CAPTURE_SELF_ATARI];
    } else if ((i & po_tactical_features_mask[F_CAPTURE_AFTER_KO]) > 0) {
      rate *= po_tactical_features[F_CAPTURE_AFTER_KO];
    } else if ((i & po_tactical_features_mask[F_2POINT_CAPTURE_LARGE]) > 0) {
      rate *= po_tactical_features[F_2POINT_CAPTURE_LARGE];
    } else if ((i & po_tactical_features_mask[F_3POINT_CAPTURE_LARGE]) > 0) {
      rate *= po_tactical_features[F_3POINT_CAPTURE_LARGE];
    } else if ((i & po_tactical_features_mask[F_2POINT_CAPTURE_SMALL]) > 0) {
      rate *= po_tactical_features[F_2POINT_CAPTURE_SMALL];
    } else if ((i & po_tactical_features_mask[F_3POINT_CAPTURE_SMALL]) > 0) {
      rate *= po_tactical_features[F_3POINT_CAPTURE_SMALL];
    } else if ((i & po_tactical_features_mask[F_CAPTURE]) > 0) {
      rate *= po_tactical_features[F_CAPTURE];
    } 

    if ((i & po_tactical_features_mask[F_SAVE_EXTENSION_SAFELY3]) > 0) {
      rate *= po_tactical_features[F_SAVE_EXTENSION_SAFELY3];
    } else if ((i & po_tactical_features_mask[F_SAVE_EXTENSION_SAFELY2]) > 0) {
      rate *= po_tactical_features[F_SAVE_EXTENSION_SAFELY2];
    } else if ((i & po_tactical_features_mask[F_SAVE_EXTENSION_SAFELY1]) > 0) {
      rate *= po_tactical_features[F_SAVE_EXTENSION_SAFELY1];
    } else if ((i & po_tactical_features_mask[F_SAVE_EXTENSION3]) > 0) {
      rate *= po_tactical_features[F_SAVE_EXTENSION3];
    } else if ((i & po_tactical_features_mask[F_SAVE_EXTENSION2]) > 0) {
      rate *= po_tactical_features[F_SAVE_EXTENSION2];
    } else if ((i & po_tactical_features_mask[F_SAVE_EXTENSION1]) > 0) {
      rate *= po_tactical_features[F_SAVE_EXTENSION1];
    }

    po_tactical_set1[i] = (float)rate;
  }

  for (i = 0; i < PO_TACTICALS_MAX2; i++) {
    rate = 1.0;

    if ((i & po_tactical_features_mask[F_SELF_ATARI_SMALL]) > 0) {
      rate *= po_tactical_features[F_SELF_ATARI_SMALL + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_SELF_ATARI_NAKADE]) > 0) {
      rate *= po_tactical_features[F_SELF_ATARI_NAKADE + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_SELF_ATARI_LARGE]) > 0) {
      rate *= po_tactical_features[F_SELF_ATARI_LARGE + F_MAX1];
    }

    if ((i & po_tactical_features_mask[F_2POINT_C_ATARI_LARGE]) > 0) {
      rate *= po_tactical_features[F_2POINT_C_ATARI_LARGE + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_3POINT_C_ATARI_LARGE]) > 0) {
      rate *= po_tactical_features[F_3POINT_C_ATARI_LARGE + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_2POINT_C_ATARI_SMALL]) > 0) {
      rate *= po_tactical_features[F_2POINT_C_ATARI_SMALL + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_3POINT_C_ATARI_SMALL]) > 0) {
      rate *= po_tactical_features[F_3POINT_C_ATARI_SMALL + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_2POINT_ATARI_LARGE]) > 0) {
      rate *= po_tactical_features[F_2POINT_ATARI_LARGE + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_3POINT_ATARI_LARGE]) > 0) {
      rate *= po_tactical_features[F_3POINT_ATARI_LARGE + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_2POINT_ATARI_SMALL]) > 0) {
      rate *= po_tactical_features[F_2POINT_ATARI_SMALL + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_3POINT_ATARI_SMALL]) > 0) {
      rate *= po_tactical_features[F_3POINT_ATARI_SMALL + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_ATARI]) > 0) {
      rate *= po_tactical_features[F_ATARI + F_MAX1];
    }
		
    if ((i & po_tactical_features_mask[F_2POINT_EXTENSION_SAFELY]) > 0) {
      rate *= po_tactical_features[F_2POINT_EXTENSION_SAFELY + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_3POINT_EXTENSION_SAFELY]) > 0) {
      rate *= po_tactical_features[F_3POINT_EXTENSION_SAFELY + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_2POINT_EXTENSION]) > 0) {
      rate *= po_tactical_features[F_2POINT_EXTENSION + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_3POINT_EXTENSION]) > 0) {
      rate *= po_tactical_features[F_3POINT_EXTENSION + F_MAX1];
    }

    if ((i & po_tactical_features_mask[F_3POINT_DAME_LARGE]) > 0) {
      rate *= po_tactical_features[F_3POINT_DAME_LARGE + F_MAX1];
    } else if ((i & po_tactical_features_mask[F_3POINT_DAME_SMALL]) > 0) {
      rate *= po_tactical_features[F_3POINT_DAME_SMALL + F_MAX1];
    }

    if ((i & po_tactical_features_mask[F_THROW_IN_2]) > 0) {
      rate *= po_tactical_features[F_THROW_IN_2 + F_MAX1];
    }

    po_tactical_set2[i] = (float)rate;
  }
}

//////////////////////
//  着手( rating )  // 
//////////////////////
int
RatingMove( game_info_t *game, int color, std::mt19937_64 *mt )
{
  long long *rate = game->rate[color - 1];
  long long *sum_rate_row = game->sum_rate_row[color - 1];
  long long *sum_rate = &game->sum_rate[color - 1];
  int y, pos;
  long long rand_num;

  // レートの部分更新
  PartialRating(game, color, sum_rate, sum_rate_row, rate);

  // 合法手を選択するまでループ
  while (true){
    if (*sum_rate == 0) return PASS;

    rand_num = ((*mt)() % (*sum_rate)) + 1;

    // 縦方向の位置を求める
    y = board_start;
    while (rand_num > sum_rate_row[y]){
      rand_num -= sum_rate_row[y++];
    }

    // 横方向の位置を求める
    pos = POS(board_start, y);
    do{
      rand_num -= rate[pos];
      if (rand_num <= 0) break;
      pos++;
    } while (true);

    // 選ばれた手が合法手ならループを抜け出し
    // そうでなければその箇所のレートを0にし, 手を選びなおす
    if (IsLegalNotEye(game, pos, color)) {
      break;
    } else {
      *sum_rate -= rate[pos];
      sum_rate_row[y] -= rate[pos];
      rate[pos] = 0;
    }
  }

  return pos;
}

////////////////////////////
//  12近傍の座標を求める  //
////////////////////////////
void
Neighbor12( int previous_move, int distance_2[], int distance_3[], int distance_4[] )
{
  // 着手距離2の座標
  distance_2[0] = previous_move + neighbor[ 2];
  distance_2[1] = previous_move + neighbor[ 5];
  distance_2[2] = previous_move + neighbor[ 7];
  distance_2[3] = previous_move + neighbor[10];

  // 着手距離3の座標
  distance_3[0] = previous_move + neighbor[ 1];
  distance_3[1] = previous_move + neighbor[ 3];
  distance_3[2] = previous_move + neighbor[ 9];
  distance_3[3] = previous_move + neighbor[11];

  // 着手距離4の座標
  distance_4[0] = previous_move + neighbor[ 0];
  distance_4[1] = previous_move + neighbor[ 4];
  distance_4[2] = previous_move + neighbor[ 8];
  distance_4[3] = previous_move + neighbor[12];
}

//////////////////////////////
//  直前の着手の周辺の更新  //
//////////////////////////////
void
NeighborUpdate( game_info_t *game, int color, long long *sum_rate, long long *sum_rate_row, long long *rate, int *update, bool *flag, int index )
{
  int i, pos;
  double gamma;
  double bias[4];
  bool self_atari_flag;

  bias[0] = bias[1] = bias[2] = bias[3] = 1.0;

  // 盤端での特殊処理
  if (index == 1) {
    pos = game->record[game->moves - 1].pos;
    if ((border_dis_x[pos] == 1 && border_dis_y[pos] == 2) ||
	    (border_dis_x[pos] == 2 && border_dis_y[pos] == 1)) {
      for (i = 0; i < 4; i++) {
      	if ((border_dis_x[update[i]] == 1 && border_dis_y[update[i]] == 2) ||
      	    (border_dis_x[update[i]] == 2 && border_dis_y[update[i]] == 1)) {
      	  bias[i] = 1000.0;
      	  break;
      	}
      }
    }
  }

  for (i = 0; i < 4; i++) {
    pos = update[i];
    if (game->candidates[pos]){
      if (flag[pos] && bias[i] == 1.0) continue;
      self_atari_flag = PoCheckSelfAtari(game, color, pos);

      // 元あったレートを消去
      *sum_rate -= rate[pos];
      sum_rate_row[board_y[pos]] -= rate[pos];

      if (!self_atari_flag){
	      rate[pos] = 0;
      } else {
	      PoCheckCaptureAndAtari(game, color, pos);

      	gamma = po_pattern[MD2(game->pat, pos)] * po_previous_distance[index];
      	gamma *= po_tactical_set1[game->tactical_features1[pos]];
      	gamma *= po_tactical_set2[game->tactical_features2[pos]];
      	gamma *= bias[i];
      	rate[pos] = (long long)(gamma)+1;

      	// 新たに計算したレートを代入
      	*sum_rate += rate[pos];
      	sum_rate_row[board_y[pos]] += rate[pos];
      }

      game->tactical_features1[pos] = 0;
      game->tactical_features2[pos] = 0;
    }
    flag[pos] = true;
  }
}

//////////////////////////
//  ナカデの急所の更新  //
//////////////////////////
void
NakadeUpdate( game_info_t *game, int color, long long *sum_rate, long long *sum_rate_row, long long *rate, int *nakade_pos, int nakade_num, bool *flag, int pm1 )
{
  int i, pos, dis;
  double gamma;
  bool self_atari_flag;

  for (i = 0; i < nakade_num; i++) {
    pos = nakade_pos[i];
    if (pos != NOT_NAKADE && game->candidates[pos]){
      self_atari_flag = PoCheckSelfAtari(game, color, pos);

      // 元あったレートを消去
      *sum_rate -= rate[pos];
      sum_rate_row[board_y[pos]] -= rate[pos];

      if (!self_atari_flag) {
	      rate[pos] = 0;
      } else {
      	PoCheckCaptureAndAtari(game, color, pos);
      	dis = DIS(pm1, pos);
      	if (dis < 5) {
      	  gamma = 10000.0 * po_previous_distance[dis - 2];
      	} else {
      	  gamma = 10000.0;
      	}
      	gamma *= po_pattern[MD2(game->pat, pos)];
      	gamma *= po_tactical_set1[game->tactical_features1[pos]];
      	gamma *= po_tactical_set2[game->tactical_features2[pos]];
      	rate[pos] = (long long)(gamma) + 1;
      	// 新たに計算したレートを代入      
      	*sum_rate += rate[pos];
      	sum_rate_row[board_y[pos]] += rate[pos];     
      }

      game->tactical_features1[pos] = 0;
      game->tactical_features2[pos] = 0;
      flag[pos] = true;
    }
  }
}

////////////////////
//  レートの更新  //
////////////////////
void
OtherUpdate( game_info_t *game, int color, long long *sum_rate, long long *sum_rate_row, long long *rate, int update_num, int *update, bool *flag )
{
  int i, pos;
  double gamma;
  bool self_atari_flag;

  for (i = 0; i < update_num; i++) {
    pos = update[i];
    if (flag[pos]) continue;

    if (game->candidates[pos]) {
      self_atari_flag = PoCheckSelfAtari(game, color, pos);

      // 元あったレートを消去
      *sum_rate -= rate[pos];
      sum_rate_row[board_y[pos]] -= rate[pos];

      // パターン、戦術的特徴、距離のγ値
      if (!self_atari_flag) {
	      rate[pos] = 0;
      } else {
      	PoCheckCaptureAndAtari(game, color, pos);
      	gamma = po_pattern[MD2(game->pat, pos)];
      	gamma *= po_tactical_set1[game->tactical_features1[pos]];
      	gamma *= po_tactical_set2[game->tactical_features2[pos]];
      	rate[pos] = (long long)(gamma) + 1;

      	// 新たに計算したレートを代入
      	*sum_rate += rate[pos];
      	sum_rate_row[board_y[pos]] += rate[pos];
      }

      game->tactical_features1[pos] = 0;
      game->tactical_features2[pos] = 0;
    }
    // 更新済みフラグを立てる
    flag[pos] = true;
  }
}

/////////////////////////////////
//  MD2パターンの範囲内の更新  //
/////////////////////////////////
void
Neighbor12Update( game_info_t *game, int color, long long *sum_rate, long long *sum_rate_row, long long *rate, int update_num, int *update, bool *flag )
{
  int i, j, pos;
  double gamma;
  bool self_atari_flag;

  for (i = 0; i < update_num; i++) {
    for (j = 0; j < UPDATE_NUM; j++) {
      pos = update[i] + neighbor[j];
      if (flag[pos]) continue;

      if (game->candidates[pos]) {
      	self_atari_flag = PoCheckSelfAtari(game, color, pos);

      	// 元あったレートを消去
        *sum_rate -= rate[pos];
        sum_rate_row[board_y[pos]] -= rate[pos];

        // パターン、戦術的特徴、距離のγ値
        if (!self_atari_flag){
          rate[pos] = 0;
        } else {
          PoCheckCaptureAndAtari(game, color, pos);
          gamma = po_pattern[MD2(game->pat, pos)];
          gamma *= po_tactical_set1[game->tactical_features1[pos]];
          gamma *= po_tactical_set2[game->tactical_features2[pos]];
          rate[pos] = (long long)(gamma) + 1;

          // 新たに計算したレートを代入
          *sum_rate += rate[pos];
          sum_rate_row[board_y[pos]] += rate[pos];
        }

        game->tactical_features1[pos] = 0;
        game->tactical_features2[pos] = 0;
      }
      // 更新済みフラグを立てる
      flag[pos] = true;
    }
  }
}

////////////////
//  部分更新  //
////////////////
void
PartialRating( game_info_t *game, int color, long long *sum_rate, long long *sum_rate_row, long long *rate )
{
  int pm1 = PASS, pm2 = PASS, pm3 = PASS;
  int distance_2[4], distance_3[4], distance_4[4];
  bool flag[BOARD_MAX] = { false };  
  int *update_pos = game->update_pos[color];
  int *update_num = &game->update_num[color];
  int other = FLIP_COLOR(color);
  int nakade_pos[4] = { 0 };
  int nakade_num = 0;
  int prev_feature = game->update_num[color];
  int prev_feature_pos[PURE_BOARD_MAX];

  for (int i = 0; i < prev_feature; i++){
    prev_feature_pos[i] = update_pos[i];
  }

  *update_num = 0;

  pm1 = game->record[game->moves - 1].pos;
  if (game->moves > 2) pm2 = game->record[game->moves - 2].pos;
  if (game->moves > 3) pm3 = game->record[game->moves - 3].pos;

  if (game->ko_move == game->moves - 2){
    PoCheckCaptureAfterKo(game, color, update_pos, update_num);
  }

  if (pm1 != PASS) {
    Neighbor12(pm1, distance_2, distance_3, distance_4);
    PoCheckFeatures(game, color, update_pos, update_num);
    PoCheckRemove2Stones(game, color, update_pos, update_num);

    SearchNakade(game, &nakade_num, nakade_pos);
    NakadeUpdate(game, color, sum_rate, sum_rate_row, rate, nakade_pos, nakade_num, flag, pm1);
    // 着手距離2の更新
    NeighborUpdate(game, color, sum_rate, sum_rate_row, rate, distance_2, flag, 0);
    // 着手距離3の更新
    NeighborUpdate(game, color, sum_rate, sum_rate_row, rate, distance_3, flag, 1);
    // 着手距離4の更新
    NeighborUpdate(game, color, sum_rate, sum_rate_row, rate, distance_4, flag, 2);
  }

  // 2手前の着手の12近傍の更新
  if (pm2 != PASS) Neighbor12Update(game, color, sum_rate, sum_rate_row, rate, 1, &pm2, flag);
  // 3手前の着手の12近傍の更新
  if (pm3 != PASS) Neighbor12Update(game, color, sum_rate, sum_rate_row, rate, 1, &pm3, flag);

  // 以前の着手で戦術的特徴が現れた箇所の更新
  OtherUpdate(game, color, sum_rate, sum_rate_row, rate, prev_feature, prev_feature_pos, flag);
  // 最近の自分の着手の時に戦術的特徴が現れた箇所の更新
  OtherUpdate(game, color, sum_rate, sum_rate_row, rate, game->update_num[color], game->update_pos[color], flag);
  // 最近の相手の着手の時に戦術的特徴が現れた箇所の更新
  OtherUpdate(game, color, sum_rate, sum_rate_row, rate, game->update_num[other], game->update_pos[other], flag);
  // 自分の着手で石を打ち上げた箇所のとその周囲の更新
  Neighbor12Update(game, color, sum_rate, sum_rate_row, rate, game->capture_num[color], game->capture_pos[color], flag);
  // 相手の着手で石を打ち上げられた箇所とその周囲の更新
  Neighbor12Update(game, color, sum_rate, sum_rate_row, rate, game->capture_num[other], game->capture_pos[other], flag);
}

////////////////////
//  レーティング  //
////////////////////
void
Rating( game_info_t *game, int color, long long *sum_rate, long long *sum_rate_row, long long *rate )
{
  int i, pos;
  int pm1 = PASS;
  double gamma;
  int update_num = 0;
  int update_pos[PURE_BOARD_MAX];  
  bool self_atari_flag;
  int dis;

  pm1 = game->record[game->moves - 1].pos;

  PoCheckFeatures(game, color, update_pos, &update_num);
  if (game->ko_move == game->moves - 2) {
    PoCheckCaptureAfterKo(game, color, update_pos, &update_num);
  }

  for (i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    if (game->candidates[pos] && IsLegalNotEye(game, pos, color)) {
      self_atari_flag = PoCheckSelfAtari(game, color, pos);
      PoCheckCaptureAndAtari(game, color, pos);

      if (!self_atari_flag) {
	      rate[pos] = 0;
      } else {
      	gamma = po_pattern[MD2(game->pat, pos)];
      	gamma *= po_tactical_set1[game->tactical_features1[pos]];
      	gamma *= po_tactical_set2[game->tactical_features2[pos]];
      	if (pm1 != PASS) {
      	  dis = DIS(pos, pm1);
      	  if (dis < 5) {
      	    gamma *= po_previous_distance[dis - 2];
      	  }
      	}
      	rate[pos] = (long long)(gamma)+1;
      }

      *sum_rate += rate[pos];
      sum_rate_row[board_y[pos]] += rate[pos];

      game->tactical_features1[pos] = 0;
      game->tactical_features2[pos] = 0;
    }
  }
}

/////////////////////////////////////////
//  呼吸点が1つの連に対する特徴の判定  //
/////////////////////////////////////////
void
PoCheckFeaturesLib1( game_info_t *game, int color, int id, int *update, int *update_num )
{
  char *board = game->board;
  string_t *string = game->string;
  int neighbor = string[id].neighbor[0];
  int lib, liberty;
  int other = FLIP_COLOR(color);
  bool contact = false;

  // 呼吸点が1つになった連の呼吸点を取り出す
  lib = string[id].lib[0];
  liberty = lib;

  // 呼吸点の上下左右が敵石に接触しているか確認
  if (board[NORTH(lib)] == other) contact = true;
  if (board[EAST(lib)] == other) contact = true;
  if (board[WEST(lib)] == other) contact = true;
  if (board[SOUTH(lib)] == other) contact = true;

  // 呼吸点の上下左右が敵連に接しているか
  // また逃げ出す連の大きさで特徴を判定
  if (contact) {
    if (string[id].size == 1) {
      game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_EXTENSION1];
    } else if (string[id].size == 2) {
      game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_EXTENSION2];
    } else {
      game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_EXTENSION3];
    }
  } else {
    if (string[id].size == 1) {
      game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_EXTENSION_SAFELY1];
    } else if (string[id].size == 2) {
      game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_EXTENSION_SAFELY2];
    } else {
      game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_EXTENSION_SAFELY3];
    }
  }

  // レートの更新対象に入れる
  update[(*update_num)++] = lib;

  // 敵連を取ることによって連を助ける手の特徴の判定
  // 自分の連の大きさと敵の連の大きさで特徴を判定
  if (string[id].size == 1) {
    while (neighbor != NEIGHBOR_END) {
      if (string[neighbor].libs == 1) {
      	lib = string[neighbor].lib[0];
      	if (string[neighbor].size == 1) {
      	  game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE1_1];
      	} else if (string[neighbor].size == 2) {
      	  game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE1_2];
      	} else {
      	  game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE1_3];
      	}
      	update[(*update_num)++] = lib;
      }
      neighbor = string[id].neighbor[neighbor];
    }
  } else if (string[id].size == 2) {
    while (neighbor != NEIGHBOR_END) {
      if (string[neighbor].libs == 1) {
      	lib = string[neighbor].lib[0];
      	if (string[neighbor].size == 1) {
      	  if (IsSelfAtariCaptureForSimulation(game, lib, color, liberty)) {
      	    game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE_SELF_ATARI];
      	  } else {
      	    game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE2_1];
      	  }
      	} else if (string[neighbor].size == 2) {
      	  game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE2_2];
      	} else {
      	  game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE2_3];
      	}
      	update[(*update_num)++] = lib;
      }
      neighbor = string[id].neighbor[neighbor];
    }
  } else if (string[id].size >= 3) {
    while (neighbor != NEIGHBOR_END) {
      if (string[neighbor].libs == 1) {
      	lib = string[neighbor].lib[0];
      	if (string[neighbor].size == 1) {
      	  if (IsSelfAtariCaptureForSimulation(game, lib, color, liberty)) {
      	    game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE_SELF_ATARI];
      	  } else {
      	    game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE3_1];
      	  }
      	} else if (string[neighbor].size == 2) {
      	  game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE3_2];
      	} else {
      	  game->tactical_features1[lib] |= po_tactical_features_mask[F_SAVE_CAPTURE3_3];
      	}
      	update[(*update_num)++] = lib;
      }
      neighbor = string[id].neighbor[neighbor];
    }
  }
}

/////////////////////////////////////////
//  呼吸点が2つの連に対する特徴の判定  //
/////////////////////////////////////////
void
PoCheckFeaturesLib2( game_info_t *game, int color, int id, int *update, int *update_num )
{
  int *string_id = game->string_id;
  string_t *string = game->string;
  char *board = game->board;
  int neighbor = string[id].neighbor[0];
  int lib1, lib2;
  bool capturable1, capturable2;

  // 呼吸点が2つになった連の呼吸点を取り出す
  lib1 = string[id].lib[0];
  lib2 = string[id].lib[lib1];

  // 呼吸点の周囲が空点3つ, または呼吸点が3つ以上の自分の連に接続できるかで特徴を判定
  if (nb4_empty[Pat3(game->pat, lib1)] == 3 ||
      (board[NORTH(lib1)] == color && string_id[NORTH(lib1)] != id &&
       string[string_id[NORTH(lib1)]].libs >= 3) ||
      (board[WEST(lib1)] == color && string_id[WEST(lib1)] != id &&
       string[string_id[WEST(lib1)]].libs >= 3) ||
      (board[EAST(lib1)] == color && string_id[EAST(lib1)] != id &&
       string[string_id[EAST(lib1)]].libs >= 3) ||
      (board[SOUTH(lib1)] == color && string_id[SOUTH(lib1)] != id &&
       string[string_id[SOUTH(lib1)]].libs >= 3)) {
    game->tactical_features2[lib1] |= po_tactical_features_mask[F_2POINT_EXTENSION_SAFELY];
  } else {
    game->tactical_features2[lib1] |= po_tactical_features_mask[F_2POINT_EXTENSION];
  }

  // 呼吸点の周囲が空点3つ, または呼吸点が3つ以上の自分の連に接続できるかで特徴を判定
  if (nb4_empty[Pat3(game->pat, lib2)] == 3 ||
      (board[NORTH(lib2)] == color && string_id[NORTH(lib2)] != id &&
       string[string_id[NORTH(lib2)]].libs >= 3) ||
      (board[WEST(lib2)] == color && string_id[WEST(lib2)] != id &&
       string[string_id[WEST(lib2)]].libs >= 3) ||
      (board[EAST(lib2)] == color && string_id[EAST(lib2)] != id &&
       string[string_id[EAST(lib2)]].libs >= 3) ||
      (board[SOUTH(lib2)] == color && string_id[SOUTH(lib2)] != id &&
       string[string_id[SOUTH(lib2)]].libs >= 3)) {
    game->tactical_features2[lib2] |= po_tactical_features_mask[F_2POINT_EXTENSION_SAFELY];
  } else {
    game->tactical_features2[lib2] |= po_tactical_features_mask[F_2POINT_EXTENSION];
  }

  // レートの更新対象に入れる
  update[(*update_num)++] = lib1;
  update[(*update_num)++] = lib2;

  // 呼吸点が2つになった連の周囲の敵連を調べる
  // 1. 呼吸点が1つの敵連
  // 2. 呼吸点が2つの敵連
  // それぞれに対して, 特徴を判定する
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      lib1 = string[neighbor].lib[0];
      update[(*update_num)++] = lib1;
      if (string[neighbor].size <= 2) {
	      game->tactical_features1[lib1] |= po_tactical_features_mask[F_2POINT_CAPTURE_SMALL];
      } else {
	      game->tactical_features1[lib1] |= po_tactical_features_mask[F_2POINT_CAPTURE_LARGE];
      }
    } else if (string[neighbor].libs == 2) {
      lib1 = string[neighbor].lib[0];
      lib2 = string[neighbor].lib[lib1];
      update[(*update_num)++] = lib1;
      update[(*update_num)++] = lib2; 
      capturable1 = IsCapturableAtariForSimulation(game, lib1, color, neighbor);
      capturable2 = IsCapturableAtariForSimulation(game, lib2, color, neighbor);
      if (string[neighbor].size <= 2) {
      	if (capturable1) {
      	  game->tactical_features2[lib1] |= po_tactical_features_mask[F_2POINT_C_ATARI_SMALL];
      	} else {
      	  game->tactical_features2[lib1] |= po_tactical_features_mask[F_2POINT_ATARI_SMALL];
      	}
      	if (capturable2) {
      	  game->tactical_features2[lib2] |= po_tactical_features_mask[F_2POINT_C_ATARI_SMALL];
      	} else {
      	  game->tactical_features2[lib2] |= po_tactical_features_mask[F_2POINT_ATARI_SMALL];
      	}
      } else {
      	if (capturable1) {
      	  game->tactical_features2[lib1] |= po_tactical_features_mask[F_2POINT_C_ATARI_LARGE];
      	} else {
      	  game->tactical_features2[lib1] |= po_tactical_features_mask[F_2POINT_ATARI_LARGE];
      	}
      	if (capturable2) {
      	  game->tactical_features2[lib2] |= po_tactical_features_mask[F_2POINT_C_ATARI_LARGE];
      	} else {
      	  game->tactical_features2[lib2] |= po_tactical_features_mask[F_2POINT_ATARI_LARGE];
      	}
      }
    }
    neighbor = string[id].neighbor[neighbor];
  }
}

/////////////////////////////////////////
//  呼吸点が3つの連に対する特徴の判定  //
/////////////////////////////////////////
void
PoCheckFeaturesLib3( game_info_t *game, int color, int id, int *update, int *update_num )
{
  int *string_id = game->string_id;
  string_t *string = game->string;
  int neighbor = string[id].neighbor[0];
  char *board = game->board;
  int lib1, lib2, lib3;
  bool capturable1, capturable2;

  // 呼吸点が3つになった連の呼吸点を取り出す
  lib1 = string[id].lib[0];
  lib2 = string[id].lib[lib1];
  lib3 = string[id].lib[lib2];

  // 呼吸点の周囲が空点3つ, または呼吸点が3つ以上の自分の連に接続できるかで特徴を判定
  if (nb4_empty[Pat3(game->pat, lib1)] == 3 ||
      (board[NORTH(lib1)] == color && string_id[NORTH(lib1)] != id &&
       string[string_id[NORTH(lib1)]].libs >= 3) ||
      (board[WEST(lib1)] == color && string_id[WEST(lib1)] != id &&
       string[string_id[WEST(lib1)]].libs >= 3) ||
      (board[EAST(lib1)] == color && string_id[EAST(lib1)] != id &&
       string[string_id[EAST(lib1)]].libs >= 3) ||
      (board[SOUTH(lib1)] == color && string_id[SOUTH(lib1)] != id &&
       string[string_id[SOUTH(lib1)]].libs >= 3)) {
    game->tactical_features2[lib1] |= po_tactical_features_mask[F_3POINT_EXTENSION_SAFELY];
  } else {
    game->tactical_features2[lib1] |= po_tactical_features_mask[F_3POINT_EXTENSION];
  }

  // 呼吸点の周囲が空点3つ, または呼吸点が3つ以上の自分の連に接続できるかで特徴を判定
  if (nb4_empty[Pat3(game->pat, lib2)] == 3 ||
      (board[NORTH(lib2)] == color && string_id[NORTH(lib2)] != id &&
       string[string_id[NORTH(lib2)]].libs >= 3) ||
      (board[WEST(lib2)] == color && string_id[WEST(lib2)] != id &&
       string[string_id[WEST(lib2)]].libs >= 3) ||
      (board[EAST(lib2)] == color && string_id[EAST(lib2)] != id &&
       string[string_id[EAST(lib2)]].libs >= 3) ||
      (board[SOUTH(lib2)] == color && string_id[SOUTH(lib2)] != id &&
       string[string_id[SOUTH(lib2)]].libs >= 3)) {
    game->tactical_features2[lib2] |= po_tactical_features_mask[F_3POINT_EXTENSION_SAFELY];
  } else {
    game->tactical_features2[lib2] |= po_tactical_features_mask[F_3POINT_EXTENSION];
  }

  // 呼吸点の周囲が空点3つ, または呼吸点が3つ以上の自分の連に接続できるかで特徴を判定
  if (nb4_empty[Pat3(game->pat, lib3)] == 3 ||
      (board[NORTH(lib3)] == color && string_id[NORTH(lib3)] != id &&
       string[string_id[NORTH(lib3)]].libs >= 3) ||
      (board[WEST(lib3)] == color && string_id[WEST(lib3)] != id &&
       string[string_id[WEST(lib3)]].libs >= 3) ||
      (board[EAST(lib3)] == color && string_id[EAST(lib3)] != id &&
       string[string_id[EAST(lib3)]].libs >= 3) ||
      (board[SOUTH(lib3)] == color && string_id[SOUTH(lib3)] != id &&
       string[string_id[SOUTH(lib3)]].libs >= 3)) {
    game->tactical_features2[lib3] |= po_tactical_features_mask[F_3POINT_EXTENSION_SAFELY];
  } else {
    game->tactical_features2[lib3] |= po_tactical_features_mask[F_3POINT_EXTENSION];
  }

  // レートの更新対象に入れる
  update[(*update_num)++] = lib1;
  update[(*update_num)++] = lib2;
  update[(*update_num)++] = lib3;

  // 呼吸点が3つになった連の周囲の敵連を調べる
  // 1. 呼吸点が1つの敵連
  // 2. 呼吸点が2つの敵連
  // 3. 呼吸点が3つの敵連
  // それぞれに対して, 特徴を判定する
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      lib1 = string[neighbor].lib[0];
      update[(*update_num)++] = lib1;
      if (string[neighbor].size <= 2) {
	      game->tactical_features1[lib1] |= po_tactical_features_mask[F_3POINT_CAPTURE_SMALL];
      } else {
	      game->tactical_features1[lib1] |= po_tactical_features_mask[F_3POINT_CAPTURE_LARGE];
      }
    } else if (string[neighbor].libs == 2) {
      lib1 = string[neighbor].lib[0];
      update[(*update_num)++] = lib1;
      lib2 = string[neighbor].lib[lib1];
      update[(*update_num)++] = lib2;
      capturable1 = IsCapturableAtariForSimulation(game, lib1, color, neighbor);
      capturable2 = IsCapturableAtariForSimulation(game, lib2, color, neighbor);
      if (string[neighbor].size <= 2) {
      	if (capturable1) {
      	  game->tactical_features2[lib1] |= po_tactical_features_mask[F_3POINT_C_ATARI_SMALL];
      	} else {
      	  game->tactical_features2[lib1] |= po_tactical_features_mask[F_3POINT_ATARI_SMALL];
      	}
      	if (capturable2) {
      	  game->tactical_features2[lib2] |= po_tactical_features_mask[F_3POINT_C_ATARI_SMALL];
      	} else {
      	  game->tactical_features2[lib2] |= po_tactical_features_mask[F_3POINT_ATARI_SMALL];
      	}
      } else {
      	if (capturable1) {
      	  game->tactical_features2[lib1] |= po_tactical_features_mask[F_3POINT_C_ATARI_LARGE];
      	} else {
      	  game->tactical_features2[lib1] |= po_tactical_features_mask[F_3POINT_ATARI_LARGE];
      	}
      	if (capturable2) {
      	  game->tactical_features2[lib2] |= po_tactical_features_mask[F_3POINT_C_ATARI_LARGE];
      	} else {
      	  game->tactical_features2[lib2] |= po_tactical_features_mask[F_3POINT_ATARI_LARGE];
      	}
      }
    } else if (string[neighbor].libs == 3) {
      lib1 = string[neighbor].lib[0];
      lib2 = string[neighbor].lib[lib1];
      lib3 = string[neighbor].lib[lib2];
      update[(*update_num)++] = lib1;
      update[(*update_num)++] = lib2;
      update[(*update_num)++] = lib3;
      if (string[neighbor].size <= 2) {
      	game->tactical_features2[lib1] |= po_tactical_features_mask[F_3POINT_DAME_SMALL];
      	game->tactical_features2[lib2] |= po_tactical_features_mask[F_3POINT_DAME_SMALL];
      	game->tactical_features2[lib3] |= po_tactical_features_mask[F_3POINT_DAME_SMALL];
      } else {
      	game->tactical_features2[lib1] |= po_tactical_features_mask[F_3POINT_DAME_LARGE];
      	game->tactical_features2[lib2] |= po_tactical_features_mask[F_3POINT_DAME_LARGE];
      	game->tactical_features2[lib3] |= po_tactical_features_mask[F_3POINT_DAME_LARGE];
      }
    }
    neighbor = string[id].neighbor[neighbor];
  }
}

//////////////////
//  特徴の判定  //
//////////////////
void
PoCheckFeatures( game_info_t *game, int color, int *update, int *update_num )
{
  string_t *string = game->string;
  char *board = game->board;
  int *string_id = game->string_id;
  int previous_move = game->record[game->moves - 1].pos;
  int id;
  int check[3] = { 0 };
  int checked = 0;

  if (game->moves > 1) previous_move = game->record[game->moves - 1].pos;
  else return;

  if (previous_move == PASS) return;

  // 直前の着手の上を確認
  if (board[NORTH(previous_move)] == color) {
    id = string_id[NORTH(previous_move)];
    if (string[id].libs == 1) {
      PoCheckFeaturesLib1(game, color, id, update, update_num);
    } else if (string[id].libs == 2) {
      PoCheckFeaturesLib2(game, color, id, update, update_num);
    } else if (string[id].libs == 3) {
      PoCheckFeaturesLib3(game, color, id, update, update_num);
    }
    check[checked++] = id;
  }

  // 直前の着手の左を確認
  if (board[WEST(previous_move)] == color) {
    id = string_id[WEST(previous_move)];
    if (id != check[0]) {
      if (string[id].libs == 1) {
	      PoCheckFeaturesLib1(game, color, id, update, update_num);
      } else if (string[id].libs == 2) {
	      PoCheckFeaturesLib2(game, color, id, update, update_num);
      } else if (string[id].libs == 3) {
	      PoCheckFeaturesLib3(game, color, id, update, update_num);
      }
    }
    check[checked++] = id;
  }

  // 直前の着手の右を確認
  if (board[EAST(previous_move)] == color) {
    id = string_id[EAST(previous_move)];
    if (id != check[0] && id != check[1]) {
      if (string[id].libs == 1) {
        PoCheckFeaturesLib1(game, color, id, update, update_num);
      } else if (string[id].libs == 2) {
        PoCheckFeaturesLib2(game, color, id, update, update_num);
      } else if (string[id].libs == 3) {
        PoCheckFeaturesLib3(game, color, id, update, update_num);
      }
    }
    check[checked++] = id;
  }

  // 直前の着手の下の確認
  if (board[SOUTH(previous_move)] == color) {
    id = string_id[SOUTH(previous_move)];
    if (id != check[0] && id != check[1] && id != check[2]) {
      if (string[id].libs == 1) {
	      PoCheckFeaturesLib1(game, color, id, update, update_num);
      } else if (string[id].libs == 2) {
	      PoCheckFeaturesLib2(game, color, id, update, update_num);
      } else if (string[id].libs == 3) {
	      PoCheckFeaturesLib3(game, color, id, update, update_num);
      }
    }
  }
}

////////////////////////
//  劫を解消するトリ  //
////////////////////////
void
PoCheckCaptureAfterKo( game_info_t *game, int color, int *update, int *update_num )
{
  string_t *string = game->string;
  char *board = game->board;
  int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  int previous_move_2 = game->record[game->moves - 2].pos;
  int id ,lib;
  int check[4] = { 0 };
  int checked = 0;

  //  上
  if (board[NORTH(previous_move_2)] == other) {
    id = string_id[NORTH(previous_move_2)];
    if (string[id].libs == 1) {
      lib = string[id].lib[0];
      update[(*update_num)++] = lib;
      game->tactical_features1[lib] |= po_tactical_features_mask[F_CAPTURE_AFTER_KO];
    }
    check[checked++] = id;
  }

  //  右
  if (board[EAST(previous_move_2)] == other) {
    id = string_id[EAST(previous_move_2)];
    if (string[id].libs == 1 && check[0] != id) {
      lib = string[id].lib[0];
      update[(*update_num)++] = lib;
      game->tactical_features1[lib] |= po_tactical_features_mask[F_CAPTURE_AFTER_KO];
    }
    check[checked++] = id;
  }

  //  下
  if (board[SOUTH(previous_move_2)] == other) {
    id = string_id[SOUTH(previous_move_2)];
    if (string[id].libs == 1 && check[0] != id && check[1] != id) {
      lib = string[id].lib[0];
      update[(*update_num)++] = lib;
      game->tactical_features1[lib] |= po_tactical_features_mask[F_CAPTURE_AFTER_KO];
    }
    check[checked++] = id;
  }

  //  左
  if (board[WEST(previous_move_2)] == other) {
    id = string_id[WEST(previous_move_2)];
    if (string[id].libs == 1 && check[0] != id && check[1] != id && check[2] != id) {
      lib = string[id].lib[0];
      update[(*update_num)++] = lib;
      game->tactical_features1[lib] |= po_tactical_features_mask[F_CAPTURE_AFTER_KO];
    }
  }
}

//////////////////
//  自己アタリ  //
//////////////////
bool
PoCheckSelfAtari( game_info_t *game, int color, int pos )
{
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  int size = 0;
  int already[4] = { 0 };
  int already_num = 0;
  int lib, count = 0, libs = 0;
  int lib_candidate[10];
  int i;
  int id;
  bool flag;
  bool checked;

  // 上下左右が空点なら呼吸点の候補に入れる
  if (board[NORTH(pos)] == S_EMPTY) lib_candidate[libs++] = NORTH(pos);
  if (board[ WEST(pos)] == S_EMPTY) lib_candidate[libs++] =  WEST(pos);
  if (board[ EAST(pos)] == S_EMPTY) lib_candidate[libs++] =  EAST(pos);
  if (board[SOUTH(pos)] == S_EMPTY) lib_candidate[libs++] = SOUTH(pos);

  //  空点
  if (libs >= 2) return true;

  // 上を調べる
  if (board[NORTH(pos)] == color) {
    id = string_id[NORTH(pos)];
    if (string[id].libs > 2) return true;
    lib = string[id].lib[0];
    count = 0;
    while (lib != LIBERTY_END) {
      if (lib != pos) {
      	checked = false;
      	for (i = 0; i < libs; i++) {
      	  if (lib_candidate[i] == lib) {
      	    checked = true;
      	    break;
      	  }
      	}
      	if (!checked) {
      	  lib_candidate[libs + count] = lib;
      	  count++;
      	}
      }
      lib = string[id].lib[lib];
    }
    libs += count;
    size += string[id].size;
    already[already_num++] = id;
    if (libs >= 2) return true;
  } else if (board[NORTH(pos)] == other &&
	     string[string_id[NORTH(pos)]].libs == 1) {
    return true;
  }

  // 左を調べる
  if (board[WEST(pos)] == color) {
    id = string_id[WEST(pos)];
    if (already[0] != id) {
      if (string[id].libs > 2) return true;
      lib = string[id].lib[0];
      count = 0;
      while (lib != LIBERTY_END) {
      	if (lib != pos) {
      	  checked = false;
      	  for (i = 0; i < libs; i++) {
      	    if (lib_candidate[i] == lib) {
      	      checked = true;
      	      break;
      	    }
      	  }
      	  if (!checked) {
      	    lib_candidate[libs + count] = lib;
      	    count++;
      	  }
      	}
	      lib = string[id].lib[lib];
      }
      libs += count;
      size += string[id].size;
      already[already_num++] = id;
      if (libs >= 2) return true;
    }
  } else if (board[WEST(pos)] == other &&
	     string[string_id[WEST(pos)]].libs == 1) {
    return true;
  }

  // 右を調べる
  if (board[EAST(pos)] == color) {
    id = string_id[EAST(pos)];
    if (already[0] != id && already[1] != id) {
      if (string[id].libs > 2) return true;
      lib = string[id].lib[0];
      count = 0;
      while (lib != LIBERTY_END) {
      	if (lib != pos) {
      	  checked = false;
      	  for (i = 0; i < libs; i++) {
      	    if (lib_candidate[i] == lib) {
      	      checked = true;
      	      break;
      	    }
      	  }
      	  if (!checked) {
      	    lib_candidate[libs + count] = lib;
      	    count++;
      	  }
      	}
      	lib = string[id].lib[lib];
      }
      libs += count;
      size += string[id].size;
      already[already_num++] = id;
      if (libs >= 2) return true;
    }
  } else if (board[EAST(pos)] == other &&
	     string[string_id[EAST(pos)]].libs == 1) {
    return true;
  }

  // 下を調べる
  if (board[SOUTH(pos)] == color) {
    id = string_id[SOUTH(pos)];
    if (already[0] != id && already[1] != id && already[2] != id) {
      if (string[id].libs > 2) return true;
      lib = string[id].lib[0];
      count = 0;
      while (lib != LIBERTY_END) {
      	if (lib != pos) {
      	  checked = false;
      	  for (i = 0; i < libs; i++) {
      	    if (lib_candidate[i] == lib) {
      	      checked = true;
      	      break;
      	    }
      	  }
      	  if (!checked) {
      	    lib_candidate[libs + count] = lib;
      	    count++;
      	  }
      	}
      	lib = string[id].lib[lib];
      }
      libs += count;
      size += string[id].size;
      already[already_num++] = id;
      if (libs >= 2) return true;
    }
  } else if (board[SOUTH(pos)] == other &&
	     string[string_id[SOUTH(pos)]].libs == 1) {
    return true;
  }

  // 自己アタリになる連の大きさが2以下,
  // または大きさが5以下でナカデの形になる場合は
  // 打っても良いものとする
  if (size < 2) {
    game->tactical_features2[pos] |= po_tactical_features_mask[F_SELF_ATARI_SMALL];
    flag = true;
  } else if (size < 5) {
    if (IsNakadeSelfAtari(game, pos, color)) {
      game->tactical_features2[pos] |= po_tactical_features_mask[F_SELF_ATARI_NAKADE];
      flag = true;
    } else {
      game->tactical_features2[pos] |= po_tactical_features_mask[F_SELF_ATARI_LARGE];
      flag = false;
    }
  } else {
    game->tactical_features2[pos] |= po_tactical_features_mask[F_SELF_ATARI_LARGE];
    flag = false;
  }

  return flag;
}

//////////////////
//  トリの判定  //
//////////////////
void
PoCheckCaptureAndAtari( game_info_t *game, int color, int pos )
{
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  int libs;

  // 上を調べる
  // 1. 敵の石
  // 2. 呼吸点が1つ
  if (board[NORTH(pos)] == other) {
    libs = string[string_id[NORTH(pos)]].libs;
    if (libs == 1) {
      game->tactical_features1[pos] |= po_tactical_features_mask[F_CAPTURE];
    } else if (libs == 2) {
      game->tactical_features2[pos] |= po_tactical_features_mask[F_ATARI];
    }
  }

  //  左を調べる
  // 1. 敵の石
  // 2. 呼吸点が1つ
  if (board[WEST(pos)] == other) {
    libs = string[string_id[WEST(pos)]].libs;
    if (libs == 1) {
      game->tactical_features1[pos] |= po_tactical_features_mask[F_CAPTURE];
    } else if (libs == 2) {
      game->tactical_features2[pos] |= po_tactical_features_mask[F_ATARI];
    }
  }

  //  右を調べる
  // 1. 敵の石
  // 2. 呼吸点が1つ
  if (board[EAST(pos)] == other) {
    libs = string[string_id[EAST(pos)]].libs;
    if (libs == 1) {
      game->tactical_features1[pos] |= po_tactical_features_mask[F_CAPTURE];
    } else if (libs == 2) {
      game->tactical_features2[pos] |= po_tactical_features_mask[F_ATARI];
    }
  }

  //  下を調べる
  // 1. 敵の石
  // 2. 呼吸点が1つ
  if (board[SOUTH(pos)] == other) {
    libs = string[string_id[SOUTH(pos)]].libs;
    if (libs == 1) {
      game->tactical_features1[pos] |= po_tactical_features_mask[F_CAPTURE];
    } else if (libs == 2) {
      game->tactical_features2[pos] |= po_tactical_features_mask[F_ATARI];
    }
  }
}

///////////////////////////////////
//  2目抜かれたときのホウリコミ  //
///////////////////////////////////
void
PoCheckRemove2Stones( game_info_t *game, int color, int *update, int *update_num )
{
  int i, rm1, rm2, check;
  int other = FLIP_COLOR(color);

  if (game->capture_num[other] != 2) {
    return;
  }

  rm1 = game->capture_pos[other][0];
  rm2 = game->capture_pos[other][1];

  if (rm1 - rm2 != 1 &&
      rm2 - rm1 != 1 &&
      rm1 - rm2 != board_size &&
      rm2 - rm1 != board_size) {
    return;
  }

  for (i = 0, check = 0; i < 4; i++) {
    if ((game->board[rm1 + cross[i]] & color) == color) {
      check++;
    }
  }

  if (check >= 2) {
    game->tactical_features2[rm1] |= po_tactical_features_mask[F_THROW_IN_2];
    update[(*update_num)++] = rm1;
  }

  for (i = 0, check = 0; i < 4; i++) {
    if ((game->board[rm2 + cross[i]] & color) == color) {
      check++;
    }
  }

  if (check >= 2) {
    game->tactical_features2[rm2] |= po_tactical_features_mask[F_THROW_IN_2];
    update[(*update_num)++] = rm2;
  }
}

//////////////////
//  γ読み込み  //
//////////////////
static void
InputPOGamma( void )
{
  int i;
  string po_parameters_path = po_params_path;
  string path;

  po_md2 = new float[MD2_MAX];
  po_pattern = new float[MD2_MAX];

#if defined (_WIN32)
  po_parameters_path += '\\';
#else
  po_parameters_path += '/';
#endif

  // 戦術的特徴の読み込み
  path = po_parameters_path + "TacticalFeature.txt";
  InputTxtFLT(path.c_str(), po_tactical_features, TACTICAL_FEATURE_MAX);

  // 直前の着手からの距離の読み込み
  path = po_parameters_path + "PreviousDistance.txt";
  InputTxtFLT(path.c_str(), po_neighbor_orig, PREVIOUS_DISTANCE_MAX);

  // 直前の着手からの距離のγを補正して出力
  for (i = 0; i < PREVIOUS_DISTANCE_MAX - 1; i++) {
    po_previous_distance[i] = (float)(po_neighbor_orig[i] * neighbor_bias);
  }
  po_previous_distance[2] = (float)(po_neighbor_orig[2] * jump_bias);

  // 3x3のパターンの読み込み
  path = po_parameters_path + "Pat3.txt";
  InputTxtFLT(path.c_str(), po_pat3, PAT3_MAX);

  // マンハッタン距離2のパターンの読み込み
  path = po_parameters_path + "MD2.txt";
  InputMD2(path.c_str(), po_md2);

  // 3x3とMD2のパターンをまとめる
  for (i = 0; i < MD2_MAX; i++){
    po_pattern[i] = (float)(po_md2[i] * po_pat3[i & 0xFFFF] * 100.0);
  }
}

//////////////////////
//  γ読み込み MD2  //
//////////////////////
static void
InputMD2( const char *filename, float *ap )
{
  FILE *fp;
  int i;
  int index;
  float rate;

  for (i = 0; i < MD2_MAX; i++) ap[i] = 1.0;

#if defined (_WIN32)
  errno_t err;

  err = fopen_s(&fp, filename, "r");
  if (err != 0) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  while (fscanf_s(fp, "%d%f", &index, &rate) != EOF) {
    ap[index] = rate;
  }
#else
  fp = fopen(filename, "r");
  if (fp == NULL) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  while (fscanf(fp, "%d%f", &index, &rate) != EOF) {
    ap[index] = rate;
  }
#endif
}

void
AnalyzePoRating( game_info_t *game, int color, double rate[] )
{
  int i, pos;
  int moves = game->moves;
  int pm1 = PASS;
  float gamma;
  int update_pos[BOARD_MAX], update_num = 0;
  int *string_id = game->string_id;
  string_t *string = game->string;
  char *board = game->board;
  
  for (i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    game->tactical_features1[pos] = 0;
    game->tactical_features2[pos] = 0;
  }
  
  pm1 = game->record[moves - 1].pos;
  
  PoCheckFeatures(game, color, update_pos, &update_num);
  PoCheckRemove2Stones(game, color, update_pos, &update_num);
  if (game->ko_move == moves - 2) {
    PoCheckCaptureAfterKo(game, color, update_pos, &update_num);
  }
  
  for (i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];

    //cerr << "CHECK " << pos << ":" << (int)board[pos] <<  " " << color << endl;
    if (board[pos] == color) {
      int id = string_id[pos];
      //cerr << "CHECK " << pos << " " << color << endl;
      //if (id != check[0] && id != check[1] && id != check[2]) {//TODO
      //}
      update_num = 0;
      if (string[id].libs == 1) {
	      PoCheckFeaturesLib1(game, color, id, update_pos, &update_num);
      } else if (string[id].libs == 2) {
	      PoCheckFeaturesLib2(game, color, id, update_pos, &update_num);
      } else if (string[id].libs == 3) {
	      PoCheckFeaturesLib3(game, color, id, update_pos, &update_num);
      }
    }

    if (!IsLegal(game, pos, color)) {
      rate[i] = 0;
      continue;
    }
    
    PoCheckSelfAtari(game, color, pos);
    PoCheckCaptureAndAtari(game, color, pos);
    
    gamma = 1.0;
    
    if (pm1 != PASS) {
      if (DIS(pos, pm1) == 2) {
	      gamma *= po_previous_distance[0];
      } else if (DIS(pos, pm1) == 3) {
	      gamma *= po_previous_distance[1];
      } else if (DIS(pos, pm1) == 4) {
	      gamma *= po_previous_distance[2];
      }
    }
    
    gamma *= po_tactical_set1[game->tactical_features1[pos]];
    gamma *= po_tactical_set2[game->tactical_features2[pos]];
    gamma *= po_pattern[MD2(game->pat, pos)];
    
    rate[i] = (long long int)gamma + 1;
  }
}

