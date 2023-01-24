#ifndef _UCT_SEARCH_HPP_
#define _UCT_SEARCH_HPP_

#include <atomic>
#include <random>

#include "board/GoBoard.hpp"
#include "board/ZobristHash.hpp"
#include "mcts/MCTSNode.hpp"
#include "mcts/SearchManager.hpp"

////////////
//  定数  //
////////////

const int THREAD_MAX = 32;              // 使用するスレッド数の最大値

// CriticalityとOwnerを計算する間隔
const int CRITICALITY_INTERVAL = 100;

// Progressive Widening
const double PROGRESSIVE_WIDENING = 1.8;

// ノード展開の閾値
const int EXPAND_THRESHOLD_9  = 20;
const int EXPAND_THRESHOLD_13 = 25;
const int EXPAND_THRESHOLD_19 = 40;

//////////////
//  構造体  //
//////////////
struct thread_arg_t {
  game_info_t *game; // 探索対象の局面
  int thread_id;   // スレッド識別番号
  int color;       // 探索する手番
};

struct statistic_t {
  std::atomic<int> colors[3];  // その箇所を領地にした回数

  void clear() {
    for (int i = 0; i < 3; i++) {
      colors[i] = 0;
    }
  }

  statistic_t& operator=(const statistic_t& v) {
    for (int i = 0; i < 3; i++) {
      colors[i] = v.colors[i].load();
    }
    return *this;
  }
};

struct rate_order_t {
  int index;    // ノードのインデックス
  double rate;  // その手のレート
};


//////////////////////
//  グローバル変数  //
//////////////////////

// UCTのノード
extern uct_node_t *uct_node;

// 現在のルートのインデックス
extern int current_root;


////////////
//  関数  //
////////////

// 予測読みを止める
void StopPondering( void );

// 予測読みのモードの設定
void SetPonderingMode( bool flag );

// 使用するスレッド数の指定
void SetThread( int new_thread );

// パラメータの設定
void SetParameter( void );

// UCT探索の初期設定
void InitializeUctSearch( void ); 

// 探索設定の初期化
void InitializeSearchSetting( void );

// UCT探索による着手生成
int UctSearchGenmove( game_info_t *game, int color );

// 予測よみ
void UctSearchPondering( game_info_t *game, int color );

// UCT探索による着手生成
int UctAnalyze( game_info_t *game, int color );

// 領地になる確率をdestにコピーする
void OwnerCopy( int *dest );

// Criticaltyをdestに
void CopyCriticality( double *dest );

void CopyStatistic( statistic_t *dest );

// UCT探索による着手生成(Clean Upモード)
int UctSearchGenmoveCleanUp( game_info_t *game, int color );

// 探索の再利用の設定
void SetReuseSubtree( bool flag );

// スコア予想
double GetCurrentScore( int color, double *own );

#endif
