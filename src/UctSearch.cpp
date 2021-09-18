#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <thread>
#include <random>

#include "DynamicKomi.h"
#include "GoBoard.h"
#include "Ladder.h"
#include "Message.h"
#include "PatternHash.h"
#include "Seki.h"
#include "Simulation.h"
#include "UctRating.h"
#include "UctSearch.h"
#include "Utility.h"

#if defined (_WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#endif

using namespace std;

#define LOCK_NODE(var) mutex_nodes[(var)].lock()
#define UNLOCK_NODE(var) mutex_nodes[(var)].unlock()
#define LOCK_EXPAND mutex_expand.lock();
#define UNLOCK_EXPAND mutex_expand.unlock();

////////////////
//  大域変数  //
////////////////

// 持ち時間
double remaining_time[S_MAX];

// UCTのノード
uct_node_t *uct_node;

// プレイアウト情報
static po_info_t po_info;

// Progressive Widening の閾値
static int pw[PURE_BOARD_MAX + 1];  

// ノード展開の閾値
static int expand_threshold = EXPAND_THRESHOLD_19;

// 試行時間を延長するかどうかのフラグ
static bool extend_time = false;

int current_root; // 現在のルートのインデックス
static mutex mutex_nodes[MAX_NODES];
static mutex mutex_expand;       // ノード展開を排他処理するためのmutex

// 探索の設定
static enum SEARCH_MODE mode = CONST_TIME_MODE;
// 使用するスレッド数
static int threads = 1;
// 1手あたりの試行時間
static double const_thinking_time = CONST_TIME;
// 1手当たりのプレイアウト数
static int playout = CONST_PLAYOUT;
// デフォルトの持ち時間
static double default_remaining_time = ALL_THINKING_TIME;

// 各スレッドに渡す引数
static thread_arg_t t_arg[THREAD_MAX];

// プレイアウトの統計情報
static statistic_t statistic[BOARD_MAX];  
// 盤上の各点のCriticality
static double criticality[BOARD_MAX];  
// 盤上の各点のOwner(0-100%)
static double owner[BOARD_MAX];  

// 現在のオーナーのインデックス
static int owner_index[BOARD_MAX];   
// 現在のクリティカリティのインデックス
static int criticality_index[BOARD_MAX];  

// 候補手のフラグ
static bool candidates[BOARD_MAX];  

// 投了する勝率の閾値
double resign_threshold = RESIGN_THRESHOLD;

static bool pondering_mode = false;

static bool ponder = false;

static bool pondering_stop = false;

static bool pondered = false;

static double time_limit;

static std::thread *handle[THREAD_MAX];    // スレッドのハンドル

// UCB Bonusの等価パラメータ
static double bonus_equivalence = BONUS_EQUIVALENCE;
// UCB Bonusの重み
static double bonus_weight = BONUS_WEIGHT;

// 乱数生成器
static std::mt19937_64 *mt[THREAD_MAX];

// Criticalityの上限値
static int criticality_max = CRITICALITY_MAX;

// サブツリーを再利用するフラグ
static bool reuse_subtree = false;

// 自分の手番の色
static int my_color;

static ray_clock::time_point begin_time;


////////////
//  関数  //
////////////

// Virtual Lossを加算
static void AddVirtualLoss( child_node_t *child, int current );

// 次のプレイアウト回数の設定
static void CalculateNextPlayouts( game_info_t *game, int color, double best_wp, double finish_time );

// Criticaliityの計算
static void CalculateCriticality( int color );

// Criticality
static void CalculateCriticalityIndex( uct_node_t *node, statistic_t *node_statistic, int color, int *index );

// Ownershipの計算
static void CalculateOwner( int color, int count );

// Ownership
static void CalculateOwnerIndex( uct_node_t *node, statistic_t *node_statistc, int color, int *index );

// 現局面の子ノードのインデックスの導出
static void CorrectDescendentNodes( vector<int> &indexes, int index );

// ノードの展開
static int ExpandNode( game_info_t *game, int color, int current );

// ルートの展開
static int ExpandRoot( game_info_t *game, int color );

// 思考時間を延長する処理
static bool ExtendTime( const int moves );

// 候補手の初期化
static void InitializeCandidate( child_node_t *uct_child, int pos, bool ladder );

// 探索打ち切りの確認
static bool InterruptionCheck( void );

// UCT探索
static void ParallelUctSearch( thread_arg_t *arg );

// UCT探索(予測読み)
static void ParallelUctSearchPondering( thread_arg_t *arg );

// ノードのレーティング
static void RatingNode( game_info_t *game, int color, int index );

static int RateComp( const void *a, const void *b );

// UCB値が最大の子ノードを返す
static int SelectMaxUcbChild( int current, int color );

// 各座標の統計処理
static void Statistic( game_info_t *game, int winner );

// UCT探索(1回の呼び出しにつき, 1回の探索)
static int UctSearch( game_info_t *game, int color, std::mt19937_64 *mt, int current, int *winner );

// 各ノードの統計情報の更新
static void UpdateNodeStatistic( game_info_t *game, int winner, statistic_t *node_statistic );

// 結果の更新
static void UpdateResult( child_node_t *child, int result, int current );



/////////////////////
//  予測読みの設定  //
/////////////////////
void
SetPonderingMode( bool flag )
{
  pondering_mode = flag;
}


////////////////////////
//  探索モードの指定  //
////////////////////////
void
SetMode( enum SEARCH_MODE new_mode )
{
  mode = new_mode;
}


///////////////////////////////////////
//  1手あたりのプレイアウト数の指定  //
///////////////////////////////////////
void
SetPlayout( int po )
{
  playout = po;
}


/////////////////////////////////
//  1手にかける試行時間の設定  //
/////////////////////////////////
void
SetConstTime( double time )
{
  const_thinking_time = time;
}


////////////////////////////////
//  使用するスレッド数の指定  //
////////////////////////////////
void
SetThread( int new_thread )
{
  threads = new_thread;
}


//////////////////////
//  持ち時間の設定  //
//////////////////////
void
SetTime( double time )
{
  default_remaining_time = time;
}


//////////////////////////
//  ノード再利用の設定  //
//////////////////////////
void
SetReuseSubtree( bool flag )
{
  reuse_subtree = flag;
}


////////////////////////////////////////////
//  盤の大きさに合わせたパラメータの設定  //
////////////////////////////////////////////
void
SetParameter( void )
{
  if (pure_board_size < 11) {
    expand_threshold = EXPAND_THRESHOLD_9;
  } else if (pure_board_size < 16) {
    expand_threshold = EXPAND_THRESHOLD_13;
  } else {
    expand_threshold = EXPAND_THRESHOLD_19;
  }
}

//////////////////////////////////////
//  time_settingsコマンドによる設定  //
//////////////////////////////////////
void
SetTimeSettings( int main_time, int byoyomi, int stone )
{
  if (mode == CONST_PLAYOUT_MODE ||
      mode == CONST_TIME_MODE) {
    return ;
  }
  
  if (main_time == 0) {
    const_thinking_time = (double)byoyomi * 0.85;
    mode = CONST_TIME_MODE;
    cerr << "Const Thinking Time Mode" << endl;
  } else {
    if (byoyomi == 0) {
      default_remaining_time = main_time;
      mode = TIME_SETTING_MODE;
      cerr << "Time Setting Mode" << endl;
    } else {
      default_remaining_time = main_time;
      const_thinking_time = ((double)byoyomi) / stone;
      mode = TIME_SETTING_WITH_BYOYOMI_MODE;
      cerr << "Time Setting Mode (byoyomi)" << endl;
    }
  }
}

/////////////////////////
//  UCT探索の初期設定  //
/////////////////////////
void
InitializeUctSearch( void )
{
  int i;

  // Progressive Wideningの初期化  
  pw[0] = 0;
  for (i = 1; i <= PURE_BOARD_MAX; i++) {  
    pw[i] = pw[i - 1] + (int)(40 * pow(PROGRESSIVE_WIDENING, i - 1));
    if (pw[i] > 10000000) break;
  }
  for (i = i + 1; i <= PURE_BOARD_MAX; i++) { 
    pw[i] = INT_MAX;
  }

  // UCTのノードのメモリを確保
  uct_node = new uct_node_t[uct_hash_size];

  if (IsDebugMessageMode()) {
    cerr << "Require " << uct_hash_size * sizeof(uct_node_t) / 1024 / 1024 << " Mbytes for Uct Node" << endl << endl;
    cerr << sizeof(uct_node_t) << endl;
    cerr << sizeof(child_node_t) * UCT_CHILD_MAX << endl;
  }
  
  if (uct_node == NULL) {
    cerr << "Cannot allocate memory !!" << endl;
    cerr << "You must reduce tree size !!" << endl;
    exit(1);
  }

}


////////////////////////
//  探索設定の初期化  //
////////////////////////
void
InitializeSearchSetting( void )
{
  // Ownerの初期化
  for (int i = 0; i < board_max; i++){
    owner[i] = 50;
    owner_index[i] = 5;
    candidates[i] = true;
  }

  // 乱数の初期化
  for (int i = 0; i < THREAD_MAX; i++) {
    if (mt[i]) {
      delete mt[i];
    }
    mt[i] = new mt19937_64((unsigned int)(time(NULL) + i));
  }

  // 持ち時間の初期化
  for (int i = 0; i < 3; i++) {
    remaining_time[i] = default_remaining_time;
  }

  // 制限時間を設定
  // プレイアウト回数の初期化
  if (mode == CONST_PLAYOUT_MODE) {
    time_limit = 100000.0;
    po_info.num = playout;
    extend_time = false;
  } else if (mode == CONST_TIME_MODE) {
    time_limit = const_thinking_time;
    po_info.num = 100000000;
    extend_time = false;
  } else if (mode == TIME_SETTING_MODE ||
             mode == TIME_SETTING_WITH_BYOYOMI_MODE) {
    if (pure_board_size < 11) {
      time_limit = remaining_time[0] / TIME_RATE_9;
      po_info.num = (int)(PLAYOUT_SPEED * time_limit);
      extend_time = true;
    } else if (pure_board_size < 13) {
      time_limit = remaining_time[0] / (TIME_MAXPLY_13 + TIME_C_13);
      po_info.num = (int)(PLAYOUT_SPEED * time_limit);
      extend_time = true;
    } else {
      time_limit = remaining_time[0] / (TIME_MAXPLY_19 + TIME_C_19);
      po_info.num = (int)(PLAYOUT_SPEED * time_limit);
      extend_time = true;
    }
  }

  pondered = false;
  pondering_stop = true;
}


void
StopPondering( void )
{
  if (!pondering_mode) {
    return ;
  }

  if (ponder) {
    pondering_stop = true;
    for (int i = 0; i < threads; i++) {
      handle[i]->join();
      delete handle[i];
    }
    ponder = false;
    pondered = true;
    PrintPonderingCount(po_info.count);
  }
}


/////////////////////////////////////
//  UCTアルゴリズムによる着手生成  //
/////////////////////////////////////
int
UctSearchGenmove( game_info_t *game, int color )
{
  int pos, select_index, max_count, pre_simulated;
  double finish_time, pass_wp, best_wp;
  child_node_t *uct_child;

  // 探索情報をクリア
  if (!pondered) {
    for (int i = 0; i < board_max; i++) {
      statistic[i].clear();
    }
    fill_n(criticality_index, board_max, 0);
    for (int i = 0; i < board_max; i++) {
      criticality[i] = 0.0;
    }
  }
  po_info.count = 0;

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    owner[pos] = 50;
    owner_index[pos] = 5;
    candidates[pos] = true;
  }

  if (!reuse_subtree) {
    ClearUctHash();
  }
  
  // 探索開始時刻の記録
  begin_time = ray_clock::now();
  
  // UCTの初期化
  current_root = ExpandRoot(game, color);

  // 前回から持ち込んだ探索回数を記録
  pre_simulated = uct_node[current_root].move_count;

  // 子ノードが1つ(パスのみ)ならPASSを返す
  if (uct_node[current_root].child_num <= 1) {
    return PASS;
  }

  // 探索回数の閾値を設定
  po_info.halt = po_info.num;

  // 自分の手番を設定
  my_color = color;

  // Dynamic Komiの算出(置碁のときのみ)
  DynamicKomi(game, &uct_node[current_root], color);

  // 探索時間とプレイアウト回数の予定値を出力
  PrintPlayoutLimits(time_limit, po_info.halt);

  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
  }

  const double mag[3] = { 1.0, 1.5, 2.0 };
  const double search_time_limit = time_limit;
  int mag_count = 0;

  // 着手が41手以降で, 
  // 時間延長を行う設定になっていて,
  // 探索時間延長をすべきときは
  // 探索回数を1.5倍, 2.0倍に増やす
  do {
    po_info.halt = (int)(mag[mag_count] * po_info.num);
    time_limit = mag[mag_count] * search_time_limit;
    for (int i = 0; i < threads; i++) {
      handle[i] = new thread(ParallelUctSearch, &t_arg[i]);
    }
    for (int i = 0; i < threads; i++) {
      handle[i]->join();
      delete handle[i];
    }
    mag_count++;
  } while (mag_count < 3 && ExtendTime(game->moves));

  uct_child = uct_node[current_root].child;

  select_index = PASS_INDEX;
  max_count = uct_child[PASS_INDEX].move_count;

  // 探索回数最大の手を見つける
  for (int i = 1; i < uct_node[current_root].child_num; i++){
    if (uct_child[i].move_count > max_count) {
      select_index = i;
      max_count = uct_child[i].move_count;
    }
  }

  // 探索にかかった時間を求める
  finish_time = GetSpendTime(begin_time);

  // パスの勝率の算出
  if (uct_child[PASS_INDEX].move_count != 0) {
    pass_wp = (double)uct_child[PASS_INDEX].win / uct_child[PASS_INDEX].move_count;
  } else {
    pass_wp = 0;
  }

  // 選択した着手の勝率の算出(Dynamic Komi)
  best_wp = (double)uct_child[select_index].win / uct_child[select_index].move_count;

  // 各地点の領地になる確率の出力
  PrintOwner(&uct_node[current_root], color, owner);

  // パスをするときは
  // 1. 直前の着手がパスで, パスした時の勝率がPASS_THRESHOLD以上
  // 2. 着手数がMAX_MOVES以上
  // 投了するときは
  //    Dynamic Komiでの勝率がRESIGN_THRESHOLD以下
  // それ以外は選ばれた着手を返す
  if (pass_wp >= PASS_THRESHOLD &&
      (game->record[game->moves - 1].pos == PASS)){
    pos = PASS;
  } else if (game->moves >= MAX_MOVES) {
    pos = PASS;
  } else if (game->moves > 3 &&
             game->record[game->moves - 1].pos == PASS &&
             game->record[game->moves - 3].pos == PASS) {
    pos = PASS;
  } else if (best_wp <= resign_threshold) {
    pos = RESIGN;
  } else {
    pos = uct_child[select_index].pos;
  }

  // 最善応手列を出力
  PrintBestSequence(game, uct_node, current_root, color);
  // 探索の情報を出力(探索回数, 勝敗, 思考時間, 勝率, 探索速度)
  PrintPlayoutInformation(&uct_node[current_root], &po_info, finish_time, pre_simulated);
  // 次の探索でのプレイアウト回数の算出
  CalculateNextPlayouts(game, color, best_wp, finish_time);

  return pos;
}


///////////////
//  予測読み  //
///////////////
void
UctSearchPondering( game_info_t *game, int color )
{
  if (!pondering_mode) {
    return ;
  }

  // 探索情報をクリア
  for (int i = 0; i < board_max; i++) {
    statistic[i].clear();
  }
  fill_n(criticality_index, board_max, 0);  
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;    
  }

  po_info.count = 0;

  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    owner[pos] = 50;
    owner_index[pos] = 5;
    candidates[pos] = true;
  }

  // UCTの初期化
  current_root = ExpandRoot(game, color);

  pondered = false;

  // 子ノードが1つ(パスのみ)ならPASSを返す
  if (uct_node[current_root].child_num <= 1) {
    ponder = false;
    pondering_stop = true;
    return ;
  }

  ponder = true;
  pondering_stop = false;

  // Dynamic Komiの算出(置碁のときのみ)
  DynamicKomi(game, &uct_node[current_root], color);

  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    handle[i] = new thread(ParallelUctSearchPondering, &t_arg[i]);
  }

  return ;
}

/////////////////////
//  候補手の初期化  //
/////////////////////
static void
InitializeCandidate( child_node_t *uct_child, int pos, bool ladder )
{
  uct_child->pos = pos;
  uct_child->move_count = 0;
  uct_child->win = 0;
  uct_child->index = NOT_EXPANDED;
  uct_child->rate = 0.0;
  uct_child->pw = false;
  uct_child->open = false;
  uct_child->ladder = ladder;
}


/////////////////////////
//  ルートノードの展開  //
/////////////////////////
static int
ExpandRoot( game_info_t *game, int color )
{
  const int moves = game->moves;
  const unsigned long long hash = game->move_hash;
  unsigned int index = FindSameHashIndex(hash, color, moves);
  int pos, child_num = 0, pm1 = PASS, pm2 = PASS;
  bool ladder[BOARD_MAX] = { false };
  child_node_t *uct_child;

  // 直前の着手の座標を取り出す
  pm1 = game->record[moves - 1].pos;
  // 2手前の着手の座標を取り出す
  if (moves > 1) pm2 = game->record[moves - 2].pos;

  // 9路盤でなければシチョウを調べる  
  if (pure_board_size != 9) {
    LadderExtension(game, color, ladder);
  }

  // 既に展開されていた時は, 探索結果を再利用する
  if (index != uct_hash_size) {
    vector<int> indexes;

    // 現局面の子ノード以外を削除する
    CorrectDescendentNodes(indexes, index);
    std::sort(indexes.begin(), indexes.end());
    ClearNotDescendentNodes(indexes);
    
    // 直前と2手前の着手を更新
    uct_node[index].previous_move1 = pm1;
    uct_node[index].previous_move2 = pm2;

    uct_child = uct_node[index].child;

    child_num = uct_node[index].child_num;

    for (int i = 0; i < child_num; i++) {
      pos = uct_child[i].pos;
      uct_child[i].rate = 0.0;
      uct_child[i].pw = false;
      uct_child[i].open = false;
      if (ladder[pos]) {
        uct_node[index].move_count -= uct_child[i].move_count;
        uct_node[index].win -= uct_child[i].win;
        uct_child[i].move_count = 0;
        uct_child[i].win = 0;
      }
      uct_child[i].ladder = ladder[pos];
    }

    // 展開されたノード数を1に初期化
    uct_node[index].width = 1;

    // 候補手のレーティング
    RatingNode(game, color, index);

    PrintReuseCount(uct_node[index].move_count);

    return index;
  } else {
    // 全ノードのクリア
    ClearUctHash();
    
    // 空のインデックスを探す
    index = SearchEmptyIndex(hash, color, moves);

    assert(index != uct_hash_size);    
    
    // ルートノードの初期化
    uct_node[index].previous_move1 = pm1;
    uct_node[index].previous_move2 = pm2;
    uct_node[index].move_count = 0;
    uct_node[index].win = 0;
    uct_node[index].width = 0;
    uct_node[index].child_num = 0;
    for (int i = 0; i < board_max; i++) {
      uct_node[index].statistic[i].clear();
    }
    fill_n(uct_node[index].seki, BOARD_MAX, false);
    
    uct_child = uct_node[index].child;
    
    // パスノードの展開
    InitializeCandidate(&uct_child[PASS_INDEX], PASS, ladder[PASS]);
    child_num++;
    
    // 候補手の展開
    if (moves == 1) {
      for (int i = 0; i < first_move_candidates; i++) {
        pos = first_move_candidate[i];
        // 探索候補かつ合法手であれば探索対象にする
        if (candidates[pos] && IsLegal(game, pos, color)) {
          InitializeCandidate(&uct_child[child_num], pos, ladder[pos]);
          child_num++;
        }
      }
    } else {
      for (int i = 0; i < pure_board_max; i++) {
        pos = onboard_pos[i];
        // 探索候補かつ合法手であれば探索対象にする
        if (candidates[pos] && IsLegal(game, pos, color)) {
          InitializeCandidate(&uct_child[child_num], pos, ladder[pos]);
          child_num++;
        }
      }
    }
    
    // 子ノード個数の設定
    uct_node[index].child_num = child_num;
    
    // 候補手のレーティング
    RatingNode(game, color, index);

    // セキの確認
    CheckSeki(game, uct_node[index].seki);
    
    uct_node[index].width++;
  }

  return index;
}



///////////////////
//  ノードの展開  //
///////////////////
static int
ExpandNode( game_info_t *game, int color, int current )
{
  const int moves = game->moves;
  const unsigned long long hash = game->move_hash;
  unsigned int index = FindSameHashIndex(hash, color, moves);
  int child_num = 0, max_pos = PASS, sibling_num, pm1 = PASS, pm2 = PASS;
  double max_rate = 0.0;
  child_node_t *uct_child, *uct_sibling;
  
  // 合流先が検知できれば, それを返す
  if (index != uct_hash_size) {
    return index;
  }

  // 空のインデックスを探す
  index = SearchEmptyIndex(hash, color, moves);

  assert(index != uct_hash_size);    

  // 直前の着手の座標を取り出す
  pm1 = game->record[moves - 1].pos;
  // 2手前の着手の座標を取り出す
  if (moves > 1) pm2 = game->record[moves - 2].pos;

  // 現在のノードの初期化
  uct_node[index].previous_move1 = pm1;
  uct_node[index].previous_move2 = pm2;
  uct_node[index].move_count = 0;
  uct_node[index].win = 0;
  uct_node[index].width = 0;
  uct_node[index].child_num = 0;
  for (int i = 0; i < board_max; i++) {
    uct_node[index].statistic[i].clear();
  }
  fill_n(uct_node[index].seki, BOARD_MAX, false);
  uct_child = uct_node[index].child;

  // パスノードの展開
  InitializeCandidate(&uct_child[PASS_INDEX], PASS, false);
  child_num++;

  // 候補手の展開
  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    // 探索候補でなければ除外
    if (candidates[pos] && IsLegal(game, pos, color)) {
      InitializeCandidate(&uct_child[child_num], pos, false);
      child_num++;
    }
  }

  // 子ノードの個数を設定
  uct_node[index].child_num = child_num;

  // 候補手のレーティング
  RatingNode(game, color, index);

  // セキの確認
  CheckSeki(game, uct_node[index].seki);
  
  // 探索幅を1つ増やす
  uct_node[index].width++;

  // 兄弟ノードで一番レートの高い手を求める
  uct_sibling = uct_node[current].child;
  sibling_num = uct_node[current].child_num;
  for (int i = 0; i < sibling_num; i++) {
    if (uct_sibling[i].pos != pm1) {
      if (uct_sibling[i].rate > max_rate) {
        max_rate = uct_sibling[i].rate;
        max_pos = uct_sibling[i].pos;
      }
    }
  }

  // 兄弟ノードで一番レートの高い手を展開する
  for (int i = 0; i < child_num; i++) {
    if (uct_child[i].pos == max_pos) {
      if (!uct_child[i].pw) {
        uct_child[i].open = true;
      }
      break;
    }
  }

  return index;
}


//////////////////////////////////////
//  ノードのレーティング             //
//  (Progressive Wideningのために)  //
//////////////////////////////////////
static void
RatingNode( game_info_t *game, int color, int index )
{
  const int child_num = uct_node[index].child_num;
  const int moves = game->moves;
  int pos, max_index;
  int pat_index[3] = {0};
  double score = 0.0, max_score, dynamic_parameter;
  bool self_atari_flag;
  pattern_hash_t hash_pat;
  child_node_t *uct_child = uct_node[index].child;
  uct_features_t uct_features;

  memset(&uct_features, 0, sizeof(uct_features_t));

  // パスのレーティング
  uct_child[PASS_INDEX].rate = CalculateLFRScore(game, PASS, pat_index, &uct_features);

  // 直前の着手で発生した特徴の確認
  UctCheckFeatures(game, color, &uct_features);
  // 直前の着手で石を2つ取られたか確認
  UctCheckRemove2Stones(game, color, &uct_features);
  // 直前の着手で石を3つ取られたか確認
  UctCheckRemove3Stones(game, color, &uct_features);
  // 2手前で劫が発生していたら, 劫を解消するトリの確認
  if (game->ko_move == moves - 2) {
    UctCheckCaptureAfterKo(game, color, &uct_features);
    UctCheckKoConnection(game, &uct_features);
  }

  max_index = 0;
  max_score = uct_child[0].rate;

  for (int i = 1; i < child_num; i++) {
    pos = uct_child[i].pos;

    // 自己アタリの確認
    self_atari_flag = UctCheckSelfAtari(game, color, pos, &uct_features);
    // ウッテガエシの確認
    UctCheckSnapBack(game, color, pos, &uct_features);
    // トリの確認
    if ((uct_features.tactical_features1[pos] & capture_mask)== 0) {
      UctCheckCapture(game, color, pos, &uct_features);
    }
    // アタリの確認
    if ((uct_features.tactical_features1[pos] & atari_mask) == 0) {
      UctCheckAtari(game, color, pos, &uct_features);
    }
    // 両ケイマの確認
    UctCheckDoubleKeima(game, color, pos, &uct_features);
    // ケイマのツケコシの確認
    UctCheckKeimaTsukekoshi(game, color, pos, &uct_features);

    // 自己アタリが無意味だったらスコアを0.0にする
    // 逃げられないシチョウならスコアを-1.0にする
    if (!self_atari_flag) {
      score = 0.0;
    } else if (uct_child[i].ladder) {
      score = -1.0;
    } else {
      // MD3, MD4, MD5のパターンのハッシュ値を求める
      PatternHash(&game->pat[pos], &hash_pat);
      // MD3のパターンのインデックスを探す
      pat_index[0] = SearchIndex(md3_index, hash_pat.list[MD_3]);
      // MD4のパターンのインデックスを探す
      pat_index[1] = SearchIndex(md4_index, hash_pat.list[MD_4]);
      // MD5のパターンのインデックスを探す
      pat_index[2] = SearchIndex(md5_index, hash_pat.list[MD_5 + MD_MAX]);

      score = CalculateLFRScore(game, pos, pat_index, &uct_features);
    }

    // その手のγを記録
    uct_child[i].rate = score;

    // 現在見ている箇所のOwnerとCriticalityの補正値を求める
    dynamic_parameter = uct_owner[owner_index[pos]] + uct_criticality[criticality_index[pos]];

    // 最もγが大きい着手を記録する
    if (score + dynamic_parameter > max_score) {
      max_index = i;
      max_score = score + dynamic_parameter;
    }
  }

  // 最もγが大きい着手を探索できるようにする
  uct_child[max_index].pw = true;
}




//////////////////////////
//  探索打ち止めの確認  //
//////////////////////////
static bool
InterruptionCheck( void )
{
  const int child_num = uct_node[current_root].child_num;
  const int rest = po_info.halt - po_info.count;
  int max = 0, second = 0;
  child_node_t *uct_child = uct_node[current_root].child;

  if (mode != CONST_PLAYOUT_MODE && 
      GetSpendTime(begin_time) * 10.0 < time_limit) {
      return false;
  }

  // 探索回数が最も多い手と次に多い手を求める
  for (int i = 0; i < child_num; i++) {
    if (uct_child[i].move_count > max) {
      second = max;
      max = uct_child[i].move_count;
    } else if (uct_child[i].move_count > second) {
      second = uct_child[i].move_count;
    }
  }

  // 残りの探索を全て次善手に費やしても
  // 最善手を超えられない場合は探索を打ち切る
  if (max - second > rest) {
    return true;
  } else {
    return false;
  }
}


///////////////////////////
//  思考時間延長の確認   //
///////////////////////////
static bool
ExtendTime( const int moves )
{
  const int child_num = uct_node[current_root].child_num;
  const child_node_t *uct_child = uct_node[current_root].child;
  int max = 0, second = 0;

  if (moves < pure_board_size * 3 - 17 || !extend_time) {
    return false;
  }
  
  // 探索回数が最も多い手と次に多い手を求める
  for (int i = 0; i < child_num; i++) {
    if (uct_child[i].move_count > max) {
      second = max;
      max = uct_child[i].move_count;
    } else if (uct_child[i].move_count > second) {
      second = uct_child[i].move_count;
    }
  }

  // 最善手の探索回数がが次善手の探索回数の
  // 1.2倍未満なら探索延長
  if (max < second * 1.2) {
    return true;
  } else {
    return false;
  }
}



/////////////////////////////////
//  並列処理で呼び出す関数     //
//  UCTアルゴリズムを反復する  //
/////////////////////////////////
static void
ParallelUctSearch( thread_arg_t *arg )
{
  const thread_arg_t *targ = (thread_arg_t *)arg;
  const int color = targ->color;
  bool interruption = false, enough_size = true;
  int winner = 0, interval = CRITICALITY_INTERVAL;
  game_info_t *game = AllocateGame();

  // スレッドIDが0のスレッドだけ別の処理をする
  // 探索回数が閾値を超える, または探索が打ち切られたらループを抜ける
  if (targ->thread_id == 0) {
    do {
      // 探索回数を1回増やす
      atomic_fetch_add(&po_info.count, 1);
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      UctSearch(game, color, mt[targ->thread_id], current_root, &winner);
      // 探索を打ち切るか確認
      interruption = InterruptionCheck();
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
      // OwnerとCriticalityを計算する
      if (po_info.count > interval) {
        CalculateOwner(color, po_info.count);
        CalculateCriticality(color);
        interval += CRITICALITY_INTERVAL;
      }
      if (GetSpendTime(begin_time) > time_limit) break;
    } while (po_info.count < po_info.halt && !interruption && enough_size);
  } else {
    do {
      // 探索回数を1回増やす
      atomic_fetch_add(&po_info.count, 1);
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      UctSearch(game, color, mt[targ->thread_id], current_root, &winner);
      // 探索を打ち切るか確認
      interruption = InterruptionCheck();
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
      if (GetSpendTime(begin_time) > time_limit) break;
    } while (po_info.count < po_info.halt && !interruption && enough_size);
  }

  // メモリの解放
  FreeGame(game);
}


/////////////////////////////////
//  並列処理で呼び出す関数     //
//  UCTアルゴリズムを反復する  //
/////////////////////////////////
static void
ParallelUctSearchPondering( thread_arg_t *arg )
{
  const thread_arg_t *targ = (thread_arg_t *)arg;
  const int color = targ->color;
  int winner = 0, interval = CRITICALITY_INTERVAL;
  bool enough_size = true;
  game_info_t *game = AllocateGame();

  // スレッドIDが0のスレッドだけ別の処理をする
  // 探索回数が閾値を超える, または探索が打ち切られたらループを抜ける
  if (targ->thread_id == 0) {
    do {
      // 探索回数を1回増やす
      atomic_fetch_add(&po_info.count, 1);
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      UctSearch(game, color, mt[targ->thread_id], current_root, &winner);
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
      // OwnerとCriticalityを計算する
      if (po_info.count > interval) {
        CalculateOwner(color, po_info.count);
        CalculateCriticality(color);
        interval += CRITICALITY_INTERVAL;
      }
    } while (!pondering_stop && enough_size);
  } else {
    do {
      // 探索回数を1回増やす
      atomic_fetch_add(&po_info.count, 1);
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      UctSearch(game, color, mt[targ->thread_id], current_root, &winner);
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
    } while (!pondering_stop && enough_size);
  }

  // メモリの解放
  FreeGame(game);
}


//////////////////////////////////////////////
//  UCT探索を行う関数                        //
//  1回の呼び出しにつき, 1プレイアウトする    //
//////////////////////////////////////////////
static int 
UctSearch( game_info_t *game, int color, mt19937_64 *mt, int current, int *winner )
{
  int result = 0, next_index;
  double score;
  child_node_t *uct_child = uct_node[current].child;  

  // 現在見ているノードをロック
  LOCK_NODE(current);
  // UCB値最大の手を求める
  next_index = SelectMaxUcbChild(current, color);
  // 選んだ手を着手
  PutStone(game, uct_child[next_index].pos, color);
  // 色を入れ替える
  color = FLIP_COLOR(color);

  if (uct_child[next_index].move_count < expand_threshold) {
    // Virtual Lossを加算
    AddVirtualLoss(&uct_child[next_index], current);

    memcpy(game->seki, uct_node[current].seki, sizeof(bool) * BOARD_MAX);
    
    // 現在見ているノードのロックを解除
    UNLOCK_NODE(current);

    // 終局まで対局のシミュレーション
    Simulation(game, color, mt);

    // 隅の曲がり四目の確認
    CheckBentFourInTheCorner(game);
    
    // コミを含めない盤面のスコアを求める
    score = (double)CalculateScore(game);
    
    // コミを考慮した勝敗
    if (my_color == S_BLACK) {
      if (score - dynamic_komi[my_color] >= 0) {
        result = (color == S_BLACK ? 0 : 1);
        *winner = S_BLACK;
      } else {
        result = (color == S_WHITE ? 0 : 1);
        *winner = S_WHITE;
      }
    } else {
      if (score - dynamic_komi[my_color] > 0) {
        result = (color == S_BLACK ? 0 : 1);
        *winner = S_BLACK;
      } else {
        result = (color == S_WHITE ? 0 : 1);
        *winner = S_WHITE;
      }
    }
    // 統計情報の記録
    Statistic(game, *winner);
  } else {
    // Virtual Lossを加算
    AddVirtualLoss(&uct_child[next_index], current);
    // ノードの展開の確認
    if (uct_child[next_index].index == -1) {
      // ノードの展開中はロック
      LOCK_EXPAND;
      // ノードの展開
      uct_child[next_index].index = ExpandNode(game, color, current);
      // ノード展開のロックの解除
      UNLOCK_EXPAND;
    }
    // 現在見ているノードのロックを解除
    UNLOCK_NODE(current);
    // 手番を入れ替えて1手深く読む
    result = UctSearch(game, color, mt, uct_child[next_index].index, winner);
  }

  // 探索結果の反映
  UpdateResult(&uct_child[next_index], result, current);

  // 統計情報の更新
  UpdateNodeStatistic(game, *winner, uct_node[current].statistic);

  return 1 - result;
}


//////////////////////////
//  Virtual Lossの加算  //
//////////////////////////
static void
AddVirtualLoss( child_node_t *child, int current )
{
  atomic_fetch_add(&uct_node[current].move_count, VIRTUAL_LOSS);
  atomic_fetch_add(&child->move_count, VIRTUAL_LOSS);
}


//////////////////////
//  探索結果の更新  //
/////////////////////
static void
UpdateResult( child_node_t *child, int result, int current )
{
  atomic_fetch_add(&uct_node[current].win, result);
  atomic_fetch_add(&uct_node[current].move_count, 1 - VIRTUAL_LOSS);
  atomic_fetch_add(&child->win, result);
  atomic_fetch_add(&child->move_count, 1 - VIRTUAL_LOSS);
}


//////////////////////////
//  ノードの並び替え用  //
//////////////////////////
static int
RateComp( const void *a, const void *b )
{
  rate_order_t *ro1 = (rate_order_t *)a;
  rate_order_t *ro2 = (rate_order_t *)b;
  if (ro1->rate < ro2->rate) {
    return 1;
  } else if (ro1->rate > ro2->rate) {
    return -1;
  } else {
    return 0;
  }
}


/////////////////////////////////////////////////////
//  UCBが最大となる子ノードのインデックスを返す関数  //
/////////////////////////////////////////////////////
static int
SelectMaxUcbChild( int current, int color )
{
  const int child_num = uct_node[current].child_num;
  const int sum = uct_node[current].move_count;
  const double ucb_bonus_weight = bonus_weight * sqrt(bonus_equivalence / (sum + bonus_equivalence));
  child_node_t *uct_child = uct_node[current].child;
  int max_child = 0, pos, width;
  double p, max_value, ucb_value, dynamic_parameter;
  rate_order_t order[PURE_BOARD_MAX + 1];  
  
  // 128回ごとにOwnerとCriticalityでソートし直す  
  if ((sum & 0x7f) == 0 && sum != 0) {
    int o_index[UCT_CHILD_MAX], c_index[UCT_CHILD_MAX];
    CalculateCriticalityIndex(&uct_node[current], uct_node[current].statistic, color, c_index);
    CalculateOwnerIndex(&uct_node[current], uct_node[current].statistic, color, o_index);
    for (int i = 0; i < child_num; i++) {
      pos = uct_child[i].pos;
      if (pos == PASS) {
        dynamic_parameter = 0.0;
      } else {
        dynamic_parameter = uct_owner[o_index[i]] + uct_criticality[c_index[i]];
      }
      order[i].rate = uct_child[i].rate + dynamic_parameter;
      order[i].index = i;
      uct_child[i].pw = false;
    }
    qsort(order, child_num, sizeof(rate_order_t), RateComp);

    // 子ノードの数と探索幅の最小値を取る
    width = ((uct_node[current].width > child_num) ? child_num : uct_node[current].width);

    // 探索候補の手を展開し直す
    for (int i = 0; i < width; i++) {
      uct_child[order[i].index].pw = true;
    }
  }

  // Progressive Wideningの閾値を超えたら, 
  // レートが最大の手を読む候補を1手追加
  if (sum > pw[uct_node[current].width]) {
    int max_index = -1;
    double max_rate = 0.0;
    for (int i = 0; i < child_num; i++) {
      if (uct_child[i].pw == false) {
        pos = uct_child[i].pos;
        dynamic_parameter = uct_owner[owner_index[pos]] + uct_criticality[criticality_index[pos]];
        if (uct_child[i].rate + dynamic_parameter > max_rate) {
          max_index = i;
          max_rate = uct_child[i].rate + dynamic_parameter;
        }
      }
    }
    if (max_index != -1) {
      uct_child[max_index].pw = true;
    }
    uct_node[current].width++;  
  }

  max_value = -1;
  max_child = 0;

  // UCB値最大の手を求める  
  for (int i = 0; i < child_num; i++) {
    if (uct_child[i].pw || uct_child[i].open) {
      if (uct_child[i].move_count == 0) {
        ucb_value = FPU;
      } else {
        double div, v;
        // UCB1-TUNED value
        p = (double)uct_child[i].win / uct_child[i].move_count;
        div = log(sum) / uct_child[i].move_count;
        v = p - p * p + sqrt(2.0 * div);
        ucb_value = p + sqrt(div * ((0.25 < v) ? 0.25 : v));

        // UCB Bonus
        ucb_value += ucb_bonus_weight * uct_child[i].rate;
      }

      if (ucb_value > max_value) {
        max_value = ucb_value;
        max_child = i;
      }
    }
  }

  return max_child;
}


///////////////////////////////////////////////////////////
//  OwnerやCriiticalityを計算するための情報を記録する関数  //
///////////////////////////////////////////////////////////
static void
Statistic( game_info_t *game, int winner )
{
  const char *board = game->board;

  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    int color = board[pos];

    if (color == S_EMPTY) color = territory[Pat3(game->pat, pos)];

    std::atomic_fetch_add(&statistic[pos].colors[color], 1);
    if (color == winner) {
      std::atomic_fetch_add(&statistic[pos].colors[0], 1);
    }
  }
}


///////////////////////////////
//  各ノードの統計情報の更新  //
///////////////////////////////
static void
UpdateNodeStatistic( game_info_t *game, int winner, statistic_t *node_statistic )
{
  const char *board = game->board;

  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    int color = board[pos];

    if (color == S_EMPTY) color = territory[Pat3(game->pat, pos)];

    std::atomic_fetch_add(&node_statistic[pos].colors[color], 1);
    if (color == winner) {
      std::atomic_fetch_add(&node_statistic[pos].colors[0], 1);
    }
  }
}


//////////////////////////////////
//  各ノードのCriticalityの計算  //
//////////////////////////////////
static void
CalculateCriticalityIndex( uct_node_t *node, statistic_t *node_statistic, int color, int *index )
{
  const int other = FLIP_COLOR(color);
  const int count = node->move_count;
  const int child_num = node->child_num;
  const double win = (double)node->win / node->move_count;
  const double lose = 1.0 - win;
  double tmp;

  index[0] = 0;

  for (int i = 1; i < child_num; i++) {
    const int pos = node->child[i].pos;

    tmp = ((double)node_statistic[pos].colors[0] / count) -
      ((((double)node_statistic[pos].colors[color] / count) * win)
       + (((double)node_statistic[pos].colors[other] / count) * lose));
    if (tmp < 0) tmp = 0;
    index[i] = (int)(tmp * 40);
    if (index[i] > criticality_max - 1) index[i] = criticality_max - 1;
  }
}

////////////////////////////////////
//  Criticalityの計算をする関数   // 
////////////////////////////////////
static void
CalculateCriticality( int color )
{
  const int other = FLIP_COLOR(color);
  const double win = (double)uct_node[current_root].win / uct_node[current_root].move_count;
  const double lose = 1.0 - win;
  double tmp;

  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];

    tmp = ((float)statistic[pos].colors[0] / po_info.count) -
      ((((float)statistic[pos].colors[color] / po_info.count)*win)
       + (((float)statistic[pos].colors[other] / po_info.count)*lose));
    criticality[pos] = tmp;
    if (tmp < 0) tmp = 0;
    criticality_index[pos] = (int)(tmp * 40);
    if (criticality_index[pos] > criticality_max - 1) criticality_index[pos] = criticality_max - 1;
  }
}


//////////////////////////////
//  Ownerの計算をする関数   //
//////////////////////////////
static void
CalculateOwnerIndex( uct_node_t *node, statistic_t *node_statistic, int color, int *index )
{
  const int count = node->move_count;
  const int child_num = node->child_num;

  index[0] = 0;

  for (int i = 1; i < child_num; i++){
    const int pos = node->child[i].pos;
    index[i] = (int)((double)node_statistic[pos].colors[color] * 10.0 / count + 0.5);
    if (index[i] > OWNER_MAX - 1) index[i] = OWNER_MAX - 1;
    if (index[i] < 0)             index[i] = 0;
  }
}


//////////////////////////////
//  Ownerの計算をする関数   //
//////////////////////////////
static void
CalculateOwner( int color, int count )
{
  for (int i = 0; i < pure_board_max; i++){
    const int pos = onboard_pos[i];
    owner_index[pos] = (int)((double)statistic[pos].colors[color] * 10.0 / count + 0.5);
    if (owner_index[pos] > OWNER_MAX - 1) owner_index[pos] = OWNER_MAX - 1;
    if (owner_index[pos] < 0)             owner_index[pos] = 0;
  }
}


/////////////////////////////////
//  次のプレイアウト回数の設定  //
/////////////////////////////////
static void
CalculateNextPlayouts( game_info_t *game, int color, double best_wp, double finish_time )
{
  double po_per_sec;

  if (finish_time != 0.0) {
    po_per_sec = po_info.count / finish_time;
  } else {
    po_per_sec = PLAYOUT_SPEED * threads;
  }

  // 次の探索の時の探索回数を求める
  if (mode == CONST_TIME_MODE) {
    if (best_wp > 0.90) {
      po_info.num = (int)(po_info.count / finish_time * const_thinking_time / 2);
    } else {
      po_info.num = (int)(po_info.count / finish_time * const_thinking_time);
    }
  } else if (mode == TIME_SETTING_MODE ||
             mode == TIME_SETTING_WITH_BYOYOMI_MODE) {
    remaining_time[color] -= finish_time;
    if (pure_board_size < 11) {
      time_limit = remaining_time[color] / TIME_RATE_9;
    } else if (pure_board_size < 16) {
      time_limit = remaining_time[color] / (TIME_C_13 + ((TIME_MAXPLY_13 - (game->moves + 1) > 0) ? TIME_MAXPLY_13 - (game->moves + 1) : 0));
    } else {
      time_limit = remaining_time[color] / (TIME_C_19 + ((TIME_MAXPLY_19 - (game->moves + 1) > 0) ? TIME_MAXPLY_19 - (game->moves + 1) : 0));
    }
    if (mode == TIME_SETTING_WITH_BYOYOMI_MODE &&
      time_limit < (const_thinking_time * 0.5)) {
      time_limit = const_thinking_time * 0.5;
    }
    po_info.num = (int)(po_per_sec * time_limit);
  } 
}


/////////////////////////////////////
//  UCTアルゴリズムによる局面解析  //
/////////////////////////////////////
int
UctAnalyze( game_info_t *game, int color )
{
  thread *handle[THREAD_MAX];

  // 探索情報をクリア
  for (int i = 0; i < board_max; i++) {
    statistic[i].clear();
  }
  fill_n(criticality_index, board_max, 0);  
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;
  }
  po_info.count = 0;

  ClearUctHash();

  current_root = ExpandRoot(game, color);

  po_info.halt = 10000;

  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    handle[i] = new std::thread(ParallelUctSearch, &t_arg[i]);
  }

  for (int i = 0; i < threads; i++) {
    handle[i]->join();
    delete handle[i];
  }

  int black = 0, white = 0;

  for (int y = board_start; y <= board_end; y++) {
    for (int x = board_start; x <= board_end; x++) {
      const int pos = POS(x, y);
      const double ownership_value = (double)statistic[pos].colors[S_BLACK] / uct_node[current_root].move_count;
      if (ownership_value > 0.5) {
        black++;
      } else {
        white++;
      }
    }
  }

  PrintOwner(&uct_node[current_root], color, owner);

  return black - white;
}


/////////////////////////
//  Ownerをコピーする  //
/////////////////////////
void
OwnerCopy( int *dest )
{
  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    dest[pos] = (int)((double)uct_node[current_root].statistic[pos].colors[my_color] / uct_node[current_root].move_count * 100);
  }
}


///////////////////////////////
//  Criticalityをコピーする  //
///////////////////////////////
void
CopyCriticality( double *dest )
{
  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    dest[pos] = criticality[pos];
  }
}

void
CopyStatistic( statistic_t *dest )
{
  for (int i = 0; i < board_max; i++) {
    dest[i] = statistic[i];
  }
}


////////////////////////////////////////////////////////
//  UCTアルゴリズムによる着手生成(KGS Clean Up Mode)  //
////////////////////////////////////////////////////////
int
UctSearchGenmoveCleanUp( game_info_t *game, int color )
{
  int pos, select_index, max_count, count;
  double finish_time, wp;
  child_node_t *uct_child;
  thread *handle[THREAD_MAX];

  for (int i = 0; i < board_max; i++) {
    statistic[i].clear();
  }
  fill_n(criticality_index, board_max, 0); 
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;
  }

  begin_time = ray_clock::now();

  po_info.count = 0;

  current_root = ExpandRoot(game, color);

  if (uct_node[current_root].child_num <= 1) {
    return PASS;
  }

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    owner[pos] = 50.0;
  }

  po_info.halt = po_info.num;

  DynamicKomi(game, &uct_node[current_root], color);

  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    handle[i] = new std::thread(ParallelUctSearch, &t_arg[i]);
  }

  for (int i = 0; i < threads; i++) {
    handle[i]->join();
    delete handle[i];
  }

  uct_child = uct_node[current_root].child;

  select_index = 0;
  max_count = uct_child[0].move_count;

  for (int i = 0; i < uct_node[current_root].child_num; i++){
    if (uct_child[i].move_count > max_count) {
      select_index = i;
      max_count = uct_child[i].move_count;
    }
  }

  finish_time = GetSpendTime(begin_time);

  wp = (double)uct_node[current_root].win / uct_node[current_root].move_count;

  PrintPlayoutInformation(&uct_node[current_root], &po_info, finish_time, 0);
  PrintOwner(&uct_node[current_root], color, owner);

  PrintBestSequence(game, uct_node, current_root, color);

  CalculateNextPlayouts(game, color, wp, finish_time);

  count = 0;

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];

    if (owner[pos] >= 5 && owner[pos] <= 95) {
      candidates[pos] = true;
      count++;
    } else {
      candidates[pos] = false;
    }
  }

  if (count == 0) {
    pos = PASS;
  } else {
    pos = uct_child[select_index].pos;
  }

  if ((double)uct_child[select_index].win / uct_child[select_index].move_count < resign_threshold) {
    return PASS;
  } else {
    return pos;
  }
}


///////////////////////////////////
//  子ノードのインデックスの収集  //
///////////////////////////////////
static void
CorrectDescendentNodes( vector<int> &indexes, int index )
{
  child_node_t *uct_child = uct_node[index].child;
  const int child_num = uct_node[index].child_num;

  indexes.push_back(index);

  for (int i = 0; i < child_num; i++) {
    if (uct_child[i].index != NOT_EXPANDED) {
      CorrectDescendentNodes(indexes, uct_child[i].index);
    }
  }   
}
