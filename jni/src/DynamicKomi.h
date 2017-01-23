#ifndef _DYNAMICKOMI_H_
#define _DYNAMICKOMI_H_

#include "GoBoard.h"
#include "UctSearch.h"

////////////////
//    定数    //
////////////////

// コミを緩める勝率の閾値
const double RED = 0.35;
// コミを厳しくする勝率の閾値
const double GREEN = 0.75;

// コミの値を収束させる手数
const int LINEAR_THRESHOLD = 200;
// 置き石1子あたりの重み
const int HANDICAP_WEIGHT = 8;

enum DYNAMIC_KOMI_MODE {
  DK_OFF,
  DK_LINEAR,
  DK_VALUE,
};

////////////////
//    関数    //
////////////////

// 置き石の個数の設定
void SetHandicapNum( int num );

// 置き石の個数の設定(テスト対局用)
void SetConstHandicapNum( int num );

// Dynamic Komi
void DynamicKomi( game_info_t *game, uct_node_t *root, int color );

// コミの値を直線的に減らすDynamic Komi
void LinearHandicap( game_info_t *game );

// 勝率に基づいてコミの値を変更するDynamic Komi
void ValueSituational( uct_node_t *root, int color );

#endif
