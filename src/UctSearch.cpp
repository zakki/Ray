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
#include <iterator>
#include <mutex>
#include <condition_variable>
#include <numeric>
#include <thread>
#include <random>
#include <queue>

#include "DynamicKomi.h"
#include "GoBoard.h"
#include "Ladder.h"
#include "Message.h"
#include "PatternHash.h"
#include "Point.h"
#include "Rating.h"
#include "Seki.h"
#include "Simulation.h"
#include "UctRating.h"
#include "UctSearch.h"
#include "Utility.h"
#include "lz/Leela.h"

#if defined (_WIN32)
#define NOMINMAX
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

#define ASYNC_NN 0


enum search_mode_t { PO, NN };

struct uct_search_context_t {
  int move_count;
  int depth;
  search_mode_t search_mode;
  std::vector<int> path;
};

struct nn_eval_req {
  int index;
  int moves;
  std::vector<int> path;
  record_t record[MAX_RECORDS];
};

#if ASYNC_NN
void EvalNode();
#endif
void EvalValue(const std::shared_ptr<nn_eval_req>& req);
//void EvalUctNode(std::vector<int>& indices, std::vector<int>& color, std::vector<int>& trans, std::vector<float>& data, std::vector<int>& path);


////////////
//  定数  //
////////////

// ノード展開の閾値
const int EXPAND_THRESHOLD_9 = 20;
const int EXPAND_THRESHOLD_13 = 25;
const int EXPAND_THRESHOLD_19 = 40;


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

static double value_evaluation_threshold = 0;

// ノードを展開しない
static bool no_expand = false;

// 試行時間を延長するかどうかのフラグ
static bool extend_time = false;

int current_root; // 現在のルートのインデックス
mutex mutex_nodes[MAX_NODES];
mutex mutex_expand;       // ノード展開を排他処理するためのmutex
mutex mutex_rating;

mutex mutex_queue;
condition_variable cond_queue;

// 探索の設定
static enum SEARCH_MODE mode = TIME_SETTING_MODE;
// 使用するスレッド数
int threads = 1;
int nn_threads = 1;
// 1手あたりの試行時間
double const_thinking_time = CONST_TIME;
// 1手当たりのプレイアウト数
int playout = CONST_PLAYOUT;
// デフォルトの持ち時間
double default_remaining_time = ALL_THINKING_TIME;
// 1手当たりのNNプレイアウト数
int nn_playout = -1;

// 各スレッドに渡す引数
vector<thread_arg_t> t_arg;

// プレイアウトの統計情報
statistic_t statistic[BOARD_MAX];  
// 盤上の各点のCriticality
double criticality[BOARD_MAX];  
// 盤上の各点のOwner(0-100%)
static double owner[BOARD_MAX];

// 現在のオーナーのインデックス
int owner_index[BOARD_MAX];   
// 現在のクリティカリティのインデックス
int criticality_index[BOARD_MAX];  

// 候補手のフラグ
bool candidates[BOARD_MAX];  

// 投了する勝率の閾値
double resign_threshold = 0.10;
double resign_threshold2 = 0.30;

bool pondering_mode = false;

bool ponder = false;

bool pondering_stop = false;

bool pondered = false;

double time_limit;

std::vector<std::unique_ptr<std::thread>> handle;    // スレッドのハンドル

static volatile bool running;

// UCB Bonusの等価パラメータ
double bonus_equivalence = BONUS_EQUIVALENCE;
// UCB Bonusの重み
double bonus_weight = BONUS_WEIGHT;

// 乱数生成器
std::vector<std::unique_ptr<std::mt19937_64>> mt;

// Search context
std::vector<uct_search_context_t> ctx;

// Criticalityの上限値
int criticality_max = CRITICALITY_MAX;

// 
bool reuse_subtree = false;

// 自分の手番の色
int my_color;

// Scoring Rule
SCORING_MODE scoring_mode = SCORING_MODE::CHINESE;

//
static bool live_best_sequence = false;

double policy_temperature = 0.49;
double policy_temperature_inc = 0.056;
double c_puct = 0.8;
double value_scale = 0.95;
int custom_expand_threshold = -1;

double policy_top_rate_max = 0.90;
double search_threshold_policy_rate = 0.005;

double pass_po_limit = 0.5;
int value_batch_size = 64;

ray_clock::time_point begin_time;

static bool early_pass = true;

static bool use_nn = true;
static vector<int> device_ids = { -2 };
static std::queue<std::shared_ptr<nn_eval_req>> eval_nn_queue;
static std::atomic<int> eval_count_value;
static std::atomic<int> eval_count_eog;
static double owner_nn[BOARD_MAX];
static std::atomic<double> depth_count;
static std::atomic<double> depth_sum;


//template<double>
double atomic_fetch_add(std::atomic<double> *obj, double arg) {
  double expected = obj->load();
  while (!atomic_compare_exchange_weak(obj, &expected, expected + arg))
    ;
  return expected;
}

static void
ClearEvalQueue()
{
  lock_guard<mutex> lock(mutex_queue);
  queue<shared_ptr<nn_eval_req>> empty_value;
  eval_nn_queue.swap(empty_value);
  cond_queue.notify_all();
}


////////////
//  関数  //
////////////

// Virtual Lossを加算
static int AddVirtualLoss( child_node_t *child, int current );

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
static int ExpandNode( const game_info_t *game, int color, int current );
static void RateExpandedNode( unsigned int index, const game_info_t *game, int color, int current, const std::vector<int>& path );

// ルートの展開
static int ExpandRoot( game_info_t *game, int color );

// 思考時間を延長する処理
static bool ExtendTime( void );

// 候補手の初期化
static void InitializeCandidate( child_node_t *uct_child, int pos, bool ladder );

// 探索打ち切りの確認
static bool InterruptionCheck( void );

// UCT探索
static void ParallelUctSearch( thread_arg_t *arg );
static void ParallelUctSearchNN( thread_arg_t *arg );

// UCT探索(予測読み)
static void ParallelUctSearchPondering( thread_arg_t *arg );
static void ParallelUctSearchPonderingNN( thread_arg_t *arg );

// ノードのレーティング
static void RatingNode( const game_info_t *game, int color, int index, const std::vector<int>& path );

static int RateComp( const void *a, const void *b );

// UCB値が最大の子ノードを返す
static int SelectMaxUcbChild( const uct_search_context_t& ctx, const game_info_t *game, int current, int color );

// 各座標の統計処理
static void Statistic( game_info_t *game, int winner );

// UCT探索(1回の呼び出しにつき, 1回の探索)
static int UctSearchPO( uct_search_context_t& ctx, game_info_t *game, int color, mt19937_64 *mt, int current, int *winner );
static void UctSearchNN( uct_search_context_t& ctx, game_info_t *game, int color, mt19937_64 *mt, int current );

// 各ノードの統計情報の更新
static void UpdateNodeStatistic( game_info_t *game, int winner, statistic_t *node_statistic );

// 結果の更新
static void UpdateResult( child_node_t *child, int result, int current );

// 乱数の初期化
static void InitRand();


static inline double
MoveSelectionWeight(int node, int child)
{
  return ValueMoveCount(node, child) + static_cast<double>(uct_node[node].child[child].move_count) / uct_node[node].move_count;
}

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

void
SetNNPlayout( int po )
{
  nn_playout = po;
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

  ctx.resize(threads + nn_threads);

  InitRand();
}

void
SetNNThread( int new_thread )
{
  nn_threads = new_thread;

  ctx.resize(threads + nn_threads);

  InitRand();
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

//////////////////
//  パスの設定  //
//////////////////
void
SetEarlyPass(bool pass)
{
  early_pass = pass;
}

//////////////////////////////
// Toggle Live Best Sequece //
//////////////////////////////
void
ToggleLiveBestSequence()
{
  live_best_sequence = !live_best_sequence;
}

////////////////////////////////////////////
//  盤の大きさに合わせたパラメータの設定  //
////////////////////////////////////////////
void
SetParameter( void )
{
  if (custom_expand_threshold > 0) {
    expand_threshold = custom_expand_threshold;
  }  else if (pure_board_size < 11) {
    expand_threshold = EXPAND_THRESHOLD_9;
  } else if (pure_board_size < 16) {
    expand_threshold = EXPAND_THRESHOLD_13;
  } else {
    expand_threshold = EXPAND_THRESHOLD_19;
  }
}

////////////////////
//  NN利用の設定  //
////////////////////
void
SetUseNN(bool flag)
{
  use_nn = flag;
}

void
SetDeviceIds(const vector<int>& ids)
{
  device_ids = ids;
}

const vector<int>&
GetDeviceIds()
{
  return device_ids;
}

void
SetNoExpand(bool flag)
{
  no_expand = flag;
}

//////////////////////////////////////
//  time_settingsコマンドによる設定  //
//////////////////////////////////////
void
SetTimeSettings( int main_time, int byoyomi, int stone )
{
  if (mode == CONST_PLAYOUT_MODE ||
      mode == CONST_TIME_MODE) {
    cerr << "Ignore time_setting " << mode << endl;
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

////////////////
// 乱数の初期化 //
////////////////
static void
InitRand()
{
  mt.clear();
  random_device rd;
  for (int i = 0; i < threads + nn_threads; i++) {
    mt.push_back(make_unique<mt19937_64>(rd()));
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
  InitRand();

  ctx.resize(threads + nn_threads);

  // 持ち時間の初期化
  for (int i = 0; i < 3; i++) {
    remaining_time[i] = default_remaining_time;
  }

  // 制限時間を設定
  // プレイアウト回数の初期化
  if (mode == CONST_PLAYOUT_MODE) {
    time_limit = std::numeric_limits<double>::max();
    if (nn_playout > 0) {
      po_info.num = std::numeric_limits<int>::max();
      po_info.nn_num = nn_playout;
    } else {
      po_info.num = playout;
      po_info.nn_num = std::numeric_limits<int>::max();
    }
    extend_time = false;
  } else if (mode == CONST_TIME_MODE) {
    time_limit = const_thinking_time;
    po_info.num = std::numeric_limits<int>::max();
    po_info.nn_num = std::numeric_limits<int>::max();
    extend_time = false;
  } else if (mode == TIME_SETTING_MODE ||
	     mode == TIME_SETTING_WITH_BYOYOMI_MODE) {
    if (pure_board_size < 11) {
      time_limit = remaining_time[0] / TIME_RATE_9;
      extend_time = true;
    } else if (pure_board_size < 13) {
      time_limit = remaining_time[0] / (TIME_MAXPLY_13 + TIME_C_13);
      extend_time = true;
    } else {
      time_limit = remaining_time[0] / (TIME_MAXPLY_19 + TIME_C_19);
      extend_time = true;
    }
    po_info.num = std::numeric_limits<int>::max();
    po_info.nn_num = std::numeric_limits<int>::max();
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
    for (auto& t : handle) {
      t->join();
    }
    handle.clear();

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
  int pos, select_index;
  double finish_time, pass_wp, best_wp;
  child_node_t *uct_child;

  // 探索情報をクリア
  if (!pondered) {
    memset(statistic, 0, sizeof(statistic_t) * board_max);
    fill_n(criticality_index, board_max, 0);
    for (int i = 0; i < board_max; i++) {
      criticality[i] = 0.0;
    }
  }
  po_info.count = 0;
  po_info.nn_count = 0;

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    owner[pos] = 50;
    owner_index[pos] = 5;
    candidates[pos] = true;

    owner_nn[pos] = 50;
  }

  if (!reuse_subtree) {
    ClearUctHash();
  }

  ClearEvalQueue();

  eval_count_value = 0;
  eval_count_eog = 0;

  depth_sum = 0;
  depth_count = 0;

  // 探索開始時刻の記録
  begin_time = ray_clock::now();
  
  // UCTの初期化
  current_root = ExpandRoot(game, color);

  // 前回から持ち込んだ探索回数を記録
  int pre_simulated = uct_node[current_root].move_count;
  int pre_value_move_count = uct_node[current_root].value_move_count;

  // 子ノードが1つ(パスのみ)ならPASSを返す
  if (uct_node[current_root].child_num <= 1) {
    return PASS;
  }

  // 探索回数の閾値を設定
  po_info.halt = po_info.num;
  po_info.nn_halt = po_info.nn_num;

  // 自分の手番を設定
  my_color = color;

  // Dynamic Komiの算出(置碁のときのみ)
  DynamicKomi(game, &uct_node[current_root], color);

  // 探索時間とプレイアウト回数の予定値を出力
  PrintPlayoutLimits(time_limit, po_info.halt, po_info.nn_halt);

  t_arg.resize(threads + nn_threads);
  running = true;
  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    handle.push_back(make_unique<thread>(ParallelUctSearch, &t_arg[i]));
  }
  for (int i = 0; i < nn_threads; i++) {
    t_arg[threads + i].thread_id = threads + i;
    t_arg[threads + i].game = game;
    t_arg[threads + i].color = color;
    handle.push_back(make_unique<thread>(ParallelUctSearchNN, &t_arg[threads + i]));
  }

#if ASYNC_NN
  if (use_nn) {
    for (int i = 0; i < min(4, nn_threads); i++) {
      handle.push_back(make_unique<thread>(EvalNode));
    }
  }
#endif

  for (auto &t : handle) {
    t->join();
  }
  handle.clear();

  // 着手が41手以降で, 
  // 時間延長を行う設定になっていて,
  // 探索時間延長をすべきときは
  // 探索回数を1.5倍に増やす
  if (game->moves > pure_board_size * 3 - 17 &&
      extend_time &&
      ExtendTime()) {
    po_info.halt = (int)(1.5 * po_info.halt);
    po_info.nn_halt = (int)(1.5 * po_info.nn_halt);
    time_limit *= 1.5;
    running = true;
    for (int i = 0; i < threads; i++) {
      handle.push_back(make_unique<thread>(ParallelUctSearch, &t_arg[i]));
    }
    for (int i = 0; i < nn_threads; i++) {
      handle.push_back(make_unique<thread>(ParallelUctSearchNN, &t_arg[threads + i]));
    }

#if ASYNC_NN
    if (use_nn) {
      for (int i = 0; i < min(4, nn_threads); i++) {
        handle.push_back(make_unique<thread>(EvalNode));
      }
    }
#endif

    for (auto &t : handle) {
      t->join();
    }
    handle.clear();
  }

  uct_child = uct_node[current_root].child;

  select_index = PASS_INDEX;
  auto max_count = early_pass ? MoveSelectionWeight(current_root, PASS_INDEX) : 0;

  // 探索回数最大の手を見つける
  for (int i = 1; i < uct_node[current_root].child_num; i++){
    auto c = MoveSelectionWeight(current_root, i);
    if (c > max_count) {
      select_index = i;
      max_count = c;
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
  double best_wpv = (double)uct_node[current_root].value_win / uct_node[current_root].value_move_count;

  // コミを含めない盤面のスコアを求める
  game_info_t game_copy;
  CopyGame(&game_copy, game);
  double score = (double)CalculateScore(&game_copy);
  // コミを考慮した勝敗
  score -= komi[my_color];

  // 各地点の領地になる確率の出力
  PrintOwner(&uct_node[current_root], color, owner);

  // 取れている石を数える
  int count = 0;
  for (int i = 0; i < pure_board_max; i++) {
    int pos = onboard_pos[i];

    if (game->board[pos] == FLIP_COLOR(color) && owner[pos] > 70) {
      count++;
    }
  }

  // パスをするときは
  // 1. 直前の着手がパスで, パスした時の勝率がPASS_THRESHOLD以上
  //    early_pass か有効か死石をすべて打ち上げ済み
  // 2. 着手数がMAX_MOVES以上
  // 投了するときは
  //    Dynamic Komiでの勝率がresign_threshold以下
  // それ以外は選ばれた着手を返す
  if (pass_wp >= PASS_THRESHOLD &&
      (early_pass || count == 0) &&
      (game->record[game->moves - 1].pos == PASS)){
    pos = PASS;
  } else if (game->moves >= MAX_MOVES) {
    pos = PASS;
  } else if (game->moves > 3 &&
             early_pass &&
	     game->record[game->moves - 1].pos == PASS &&
	     game->record[game->moves - 3].pos == PASS) {
    pos = PASS;
  } else if (!early_pass && count == 0 && best_wp < pass_wp && max_count < MoveSelectionWeight(current_root, PASS_INDEX)) {
    pos = PASS;
#if 0
  } else if (best_wp <= resign_threshold && (!use_nn || best_wpv <= resign_threshold)) {
    pos = RESIGN;
  } else if (use_nn && best_wp <= resign_threshold2 && best_wpv <= resign_threshold) {
    pos = RESIGN;
#else
  } else if (best_wpv < resign_threshold) {
    pos = RESIGN;
#endif
  } else {
    pos = uct_child[select_index].pos;
  }

  // 最善応手列を出力
  PrintBestSequence(game, uct_node, current_root, color);
  // 探索の情報を出力(探索回数, 勝敗, 思考時間, 勝率, 探索速度)
  PrintPlayoutInformation(&uct_node[current_root], &po_info, finish_time, pre_simulated);
  // 次の探索でのプレイアウト回数の算出
  CalculateNextPlayouts(game, color, best_wp, finish_time);

  ClearEvalQueue();
 
  if (use_nn && GetDebugMessageMode()) {
    cerr << "Eval NN Value      :  " << setw(7) << (eval_count_value + eval_nn_queue.size()) << endl;
    cerr << "Eval NN            :  " << setw(7) << eval_count_value
         << "/" << eval_count_eog
         << "/" << value_evaluation_threshold << endl;
    cerr << "NN Speed           :  " << setw(7) << (int)(eval_count_value / finish_time) << " PO/sec " << endl;
    cerr << "Count Captured     :  " << setw(7) << count << endl;
    cerr << "Score              :  " << setw(7) << score << endl;
    cerr << "Average Depth      :  " << (double)depth_sum / depth_count << endl;
    PrintMoveStat(cerr, game, uct_node, current_root);
    if (uct_child[select_index].index >= 0) {
      cerr << "Opponent Moves" << endl;
      PrintMoveStat(cerr, game, uct_node, uct_child[select_index].index);

    }
    //PrintOwnerNN(S_BLACK, owner_nn);
  }

  return pos;
}


///////////////
//  予測読み  //
///////////////
void
UctSearchPondering(game_info_t *game, int color)
{
  int pos;

  if (!pondering_mode) {
    return;
  }

  // 探索情報をクリア
  memset(statistic, 0, sizeof(statistic_t) * board_max);
  fill_n(criticality_index, board_max, 0);
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;
  }

  po_info.count = 0;
  po_info.nn_count = 0;

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
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
    return;
  }

  ponder = true;
  pondering_stop = false;

  // Dynamic Komiの算出(置碁のときのみ)
  DynamicKomi(game, &uct_node[current_root], color);

  t_arg.resize(threads + nn_threads);
  running = true;
  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    handle.push_back(make_unique<thread>(ParallelUctSearchPondering, &t_arg[i]));
  }
  for (int i = 0; i < nn_threads; i++) {
    t_arg[threads + i].thread_id = threads + i;
    t_arg[threads + i].game = game;
    t_arg[threads + i].color = color;
    handle.push_back(make_unique<thread>(ParallelUctSearchPonderingNN, &t_arg[threads + i]));
  }

#if ASYNC_NN
  if (use_nn) {
    for (int i = 0; i < min(4, nn_threads); i++) {
      handle.push_back(make_unique<thread>(EvalNode));
    }
  }
#endif

  return ;
}

/////////////////////////////////////
// 統計
/////////////////////////////////////
void
UctSearchStat(game_info_t *game, int color, int num)
{
  int i, pos;
  double finish_time;
  int select_index;
  int max_count;
  double pass_wp;
  double best_wp;
  child_node_t *uct_child;


  // 探索情報をクリア
  memset(statistic, 0, sizeof(statistic_t) * board_max); 
  memset(criticality_index, 0, sizeof(int) * board_max); 
  memset(criticality, 0, sizeof(double) * board_max);    
  po_info.count = 0;
  po_info.nn_count = 0;

  for (i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    owner[pos] = 50;
    owner_index[pos] = 5;
    candidates[pos] = true;
  }

  if (reuse_subtree) {
    DeleteOldHash(game);
  } else {
    ClearUctHash();
  }

  bool org_use_nn = use_nn;
  use_nn = false;

  // 探索開始時刻の記録
  begin_time = ray_clock::now();

  // UCTの初期化
  current_root = ExpandRoot(game, color);

  // 前回から持ち込んだ探索回数を記録
  int pre_simulated = uct_node[current_root].move_count;
  int pre_value_move_count = uct_node[current_root].value_move_count;

  // 子ノードが1つ(パスのみ)ならPASSを返す
  if (uct_node[current_root].child_num <= 1) {
    return;
  }

  // 探索回数の閾値を設定
  po_info.halt = num;

  // 自分の手番を設定
  my_color = color;

  // Dynamic Komiの算出(置碁のときのみ)
  DynamicKomi(game, &uct_node[current_root], color);

  t_arg.resize(threads);
  running = true;
  for (i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    handle.push_back(make_unique<thread>(ParallelUctSearch, &t_arg[i]));
  }

  for (auto &t : handle) {
    t->join();
  }
  handle.clear();

  use_nn = org_use_nn;

  uct_child = uct_node[current_root].child;

  select_index = PASS_INDEX;
  max_count = uct_child[PASS_INDEX].move_count;

  // 探索回数最大の手を見つける
  for (i = 1; i < uct_node[current_root].child_num; i++){
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

  cerr << (color == S_BLACK ? "BLACK" : "WHITE") << endl;
  // 各地点の領地になる確率の出力
  PrintOwner(&uct_node[current_root], color, owner);

  // 探索の情報を出力(探索回数, 勝敗, 思考時間, 勝率, 探索速度)
  PrintPlayoutInformation(&uct_node[current_root], &po_info, finish_time, 0);
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
  uct_child->flag = false;
  uct_child->open = false;
  uct_child->ladder = ladder;
  uct_child->nnrate = 0;
}


/////////////////////////
//  ルートノードの展開  //
/////////////////////////
static int
ExpandRoot( game_info_t *game, int color )
{
  unsigned long long hash = game->move_hash;
  unsigned int index = FindSameHashIndex(hash, color, game->moves);
  child_node_t *uct_child;
  int i, pos, child_num = 0;
  bool ladder[BOARD_MAX] = { false };  
  int pm1 = PASS, pm2 = PASS;
  int moves = game->moves;

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

    for (i = 0; i < child_num; i++) {
      pos = uct_child[i].pos;
      uct_child[i].rate = 0.0;
      uct_child[i].flag = false;
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

    atomic_fetch_add(&uct_node[index].value_move_count, VIRTUAL_LOSS);

    // 候補手のレーティング
    RatingNode(game, color, index, {});

    PrintReuseCount(uct_node[index].move_count);

    return index;
  } else {
    // 全ノードのクリア
    ClearUctHash();
    
    // 空のインデックスを探す
    index = SearchEmptyIndex(hash, color, game->moves);

    assert(index != uct_hash_size);    
    
    // ルートノードの初期化
    uct_node[index].previous_move1 = pm1;
    uct_node[index].previous_move2 = pm2;
    uct_node[index].move_count = 0;
    uct_node[index].win = 0;
    uct_node[index].width = 0;
    uct_node[index].child_num = 0;
    uct_node[index].state = NODE_STATE::INIT;
    uct_node[index].value_move_count = 0;
    uct_node[index].value_win = 0;
    memset(uct_node[index].statistic, 0, sizeof(statistic_t) * BOARD_MAX); 
    fill_n(uct_node[index].seki, BOARD_MAX, false);
    
    uct_child = uct_node[index].child;
    
    // パスノードの展開
    InitializeCandidate(&uct_child[PASS_INDEX], PASS, ladder[PASS]);
    child_num++;
    
    // 候補手の展開
    if (game->moves == 1) {
      for (i = 0; i < first_move_candidates; i++) {
	pos = first_move_candidate[i];
	// 探索候補かつ合法手であれば探索対象にする
	if (candidates[pos] && IsLegal(game, pos, color)) {
	  InitializeCandidate(&uct_child[child_num], pos, ladder[pos]);
	  child_num++;
	}	
      }
    } else {
      for (i = 0; i < pure_board_max; i++) {
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

    atomic_fetch_add(&uct_node[index].value_move_count, VIRTUAL_LOSS);

    // 候補手のレーティング
    RatingNode(game, color, index, {});

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
ExpandNode( const game_info_t *game, int color, int current )
{
  // ノードの展開中はロック
  unique_lock<mutex> lock(mutex_expand);

  unsigned long long hash = game->move_hash;
  unsigned int index = FindSameHashIndex(hash, color, game->moves);

  // 合流先が検知できれば, それを返す
  if (index != uct_hash_size) {
    lock.unlock();
    while (uct_node[index].child_num.load() == 0) {
      this_thread::sleep_for(chrono::milliseconds(1));
    }
    return index;
  }

  // 空のインデックスを探す
  index = SearchEmptyIndex(hash, color, game->moves);

  assert(index != uct_hash_size);    

  int moves = game->moves;
  // 直前の着手の座標を取り出す
  int pm1 = game->record[moves - 1].pos;
  // 2手前の着手の座標を取り出す
  int pm2 = PASS;
  if (moves > 1) pm2 = game->record[moves - 2].pos;

  // 現在のノードの初期化
  uct_node[index].previous_move1 = pm1;
  uct_node[index].previous_move2 = pm2;
  uct_node[index].move_count = 0;
  uct_node[index].win = 0;
  uct_node[index].width = 0;
  uct_node[index].child_num = 0;
  uct_node[index].state = NODE_STATE::INIT;
  uct_node[index].value_move_count = 0;
  uct_node[index].value_win = 0;
  memset(uct_node[index].statistic, 0, sizeof(statistic_t) * BOARD_MAX);  
  fill_n(uct_node[index].seki, BOARD_MAX, false);

  lock.unlock();

  // 現在見ているノードをロック
  LOCK_NODE(index);

  // 9路盤でなければシチョウを調べる
  bool ladder[BOARD_MAX] = { false };
  if (pure_board_size != 9) {
    LadderExtension(game, color, ladder);
  }

  child_node_t *uct_child = uct_node[index].child;
  int child_num = 0;

  // パスノードの展開
  InitializeCandidate(&uct_child[PASS_INDEX], PASS, ladder[PASS]);
  child_num++;

  // 候補手の展開
  for (int i = 0; i < pure_board_max; i++) {
    int pos = onboard_pos[i];
    // 探索候補でなければ除外
    if (candidates[pos] && IsLegal(game, pos, color)) {
      InitializeCandidate(&uct_child[child_num], pos, ladder[pos]);
      child_num++;
    }
  }

  // 子ノードの個数を設定
  uct_node[index].child_num = child_num;

  // 現在見ているノードのロックを解除
  UNLOCK_NODE(index);

  return index;
}


static void
RateExpandedNode( unsigned int index, const game_info_t *game, int color, int current, const std::vector<int>& path )
{
  int child_num = uct_node[index].child_num;
  child_node_t *uct_child = uct_node[index].child;
  int moves = game->moves;
  int pm1 = game->record[moves - 1].pos;

  // 候補手のレーティング
  RatingNode(game, color, index, path);

  // セキの確認
  CheckSeki(game, uct_node[index].seki);
  
  // 探索幅を1つ増やす
  uct_node[index].width++;

  // 兄弟ノードで一番レートの高い手を求める
  child_node_t *uct_sibling = uct_node[current].child;
  int sibling_num = uct_node[current].child_num;
  double max_rate = 0.0;
  int max_pos = PASS;
  for (int i = 0; i < sibling_num; i++) {
    if (uct_sibling[i].pos != pm1) {
      if (uct_sibling[i].rate > max_rate) {
        max_rate = uct_sibling[i].rate;
        max_pos = uct_sibling[i].pos;
      }
    }
  }

  // 兄弟ノードで一番レートの高い手を展開する
  for (int i = 1; i < child_num; i++) {
    if (uct_child[i].pos == max_pos) {
      if (!uct_child[i].flag) {
        uct_child[i].open = true;
      }
      break;
    }
  }
}


//////////////////////////////////////
//  ノードのレーティング             //
//  (Progressive Wideningのために)  //
//////////////////////////////////////
static void
RatingNode( const game_info_t *game, int color, int index, const std::vector<int>& path )
{
  int child_num = uct_node[index].child_num;
  int pos;
  int moves = game->moves;
  double score = 0.0;
  int max_index;
  double max_score;
  pattern_hash_t hash_pat;
  int pat_index[3] = {0};
  double dynamic_parameter;
  bool self_atari_flag;
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

  if (use_nn && path.size() < 1) {
  //if (use_nn) {
    auto req = make_shared<nn_eval_req>();
    req->index = index;
    req->moves = game->moves;
    memcpy(req->record, game->record, sizeof(record_t) * MAX_RECORDS);
    copy(path.begin(), path.end(), back_inserter(req->path));
    req->path.push_back(index);

    for (int n : req->path)
      atomic_fetch_add(&uct_node[n].value_move_count, VIRTUAL_LOSS_NN);

#if ASYNC_NN
    lock_guard<mutex> lock(mutex_queue);
    eval_nn_queue.push(req);
    cond_queue.notify_all();
#else
    EvalValue(req);
#endif
  }

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
#if 1
      // MD3, MD4, MD5のパターンのハッシュ値を求める
      PatternHash(&game->pat[pos], &hash_pat);
      // MD3のパターンのインデックスを探す
      pat_index[0] = SearchIndex(md3_index, hash_pat.list[MD_3]);
      // MD4のパターンのインデックスを探す
      pat_index[1] = SearchIndex(md4_index, hash_pat.list[MD_4]);
      // MD5のパターンのインデックスを探す
      pat_index[2] = SearchIndex(md5_index, hash_pat.list[MD_5 + MD_MAX]);

      score = CalculateLFRScore(game, pos, pat_index, &uct_features);
#else
      pos = uct_child[i].pos;
      int x = X(pos) - OB_SIZE;
      int y = Y(pos) - OB_SIZE;
      int n = x + y * pure_board_size;
      score = outputs[n] / sum;
      if (score > 0)
        uct_child[i].flag = true;
#endif
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
  uct_child[max_index].flag = true;
}




//////////////////////////
//  探索打ち止めの確認  //
//////////////////////////
static bool
InterruptionCheck( void )
{
  const int child_num = uct_node[current_root].child_num;
  //const int rest = po_info.halt - po_info.count;
  const int nn_rest = po_info.nn_halt - po_info.nn_count;
  child_node_t *uct_child = uct_node[current_root].child;

  if (mode != CONST_PLAYOUT_MODE &&
      GetSpendTime(begin_time) * 2.0 < time_limit) {
      return false;
  }

  if (mode == CONST_PLAYOUT_MODE) {
    return (po_info.count >= po_info.halt) || (po_info.nn_count >= po_info.nn_halt);
  } else {
    if (GetSpendTime(begin_time) > time_limit)
      return true;
  }

  // 探索回数が最も多い手と次に多い手を求める
  int max = 0, second = 0;
  for (int i = 0; i < child_num; i++) {
    int m = ValueMoveCount(current_root, i);
    if (m > max) {
      second = max;
      max = m;
    } else if (m > second) {
      second = m;
    }
  }

  // 残りの探索を全て次善手に費やしても
  // 最善手を超えられない場合は探索を打ち切る
  if (max - second > nn_rest) {
    return true;
  } else {
    return false;
  }
}


///////////////////////////
//  思考時間延長の確認   //
///////////////////////////
static bool
ExtendTime( void )
{
  // 持ち時間が少ないときは延長しない
  if (time_limit < 2.0)
    return false;

  double max = 0, second = 0;
  int max_index = 0;
  const int child_num = uct_node[current_root].child_num;
  child_node_t *uct_child = uct_node[current_root].child;

  // 探索回数が最も多い手と次に多い手を求める
  for (int i = 0; i < child_num; i++) {
    auto w = MoveSelectionWeight(current_root, i);
    if (w > max) {
      second = max;
      max = w;
      max_index = i;
    } else if (w > second) {
      second = w;
    }
  }

  // 最善手の探索回数がが次善手の探索回数の
  // 1.2倍未満なら探索延長
  if (max < second * 1.2) {
    if (GetDebugMessageMode()) {
      cerr << "Extend time "
        << FormatMove(uct_child[max_index].pos) << " max:" << max
        << " second:" << second << endl;
    }
    return true;
  }

  // Extend time if policy value is too low 
  if (uct_node[current_root].state == NODE_STATE::EVALUATED) {
    if (uct_child[max_index].nnrate < 0.02) {
      if (GetDebugMessageMode()) {
        cerr << "Extend time "
          << FormatMove(uct_child[max_index].pos) << " policy:" << uct_child[max_index].nnrate << endl;
      }
      return true;
    } else {
      if (GetDebugMessageMode()) {
        cerr << "Stop policy:" << uct_child[max_index].nnrate << endl;
      }
    }
  }

  return false;
}


static void
WaitForEvaluationQueue(bool ponderingmode)
{
#if ASYNC_NN
  static std::atomic<int> queue_full;

  value_evaluation_threshold = max(0.0, value_evaluation_threshold - 0.01);

  // Wait if dcnn queue is full
  mutex_queue.lock();
  while (eval_nn_queue.size() > value_batch_size * 3) {
    if (!running) break;
    if (ponderingmode) {
      if (pondering_stop) break;
    } else {
      if (GetSpendTime(begin_time) > time_limit) break;
    }
    std::atomic_fetch_add(&queue_full, 1);
    value_evaluation_threshold = min(0.5, value_evaluation_threshold + 0.01);
    mutex_queue.unlock();
    this_thread::sleep_for(chrono::milliseconds(10));
    if (queue_full % 1000 == 0)
      cerr << "EVAL QUEUE FULL" << endl;
    mutex_queue.lock();
  }
  mutex_queue.unlock();
#endif
}

/////////////////////////////////
//  並列処理で呼び出す関数     //
//  UCTアルゴリズムを反復する  //
/////////////////////////////////
static void
ParallelUctSearch( thread_arg_t *arg )
{
  thread_arg_t *targ = (thread_arg_t *)arg;
  game_info_t *game;
  int color = targ->color;
  bool enough_size = true;
  int winner = 0;
  uct_search_context_t& c = ctx[targ->thread_id];
  c.search_mode = PO;

  game = AllocateGame();

  // スレッドIDが0のスレッドだけ別の処理をする
  // 探索回数が閾値を超える, または探索が打ち切られたらループを抜ける
  if (targ->thread_id == 0) {
    int interval = CRITICALITY_INTERVAL;

    do {
      // Wait if dcnn queue is full
      WaitForEvaluationQueue(false);
      // 探索回数を1回増やす
      c.move_count = atomic_fetch_add(&po_info.count, 1);
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      c.path.clear();
      UctSearchPO(c, game, color, mt[targ->thread_id].get(), current_root, &winner);
      // 探索を打ち切るか確認
      bool interruption = InterruptionCheck();
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
      // OwnerとCriticalityを計算する
      if (po_info.count > interval) {
        CalculateOwner(color, po_info.count);
        CalculateCriticality(color);
        interval += CRITICALITY_INTERVAL;
      }
      if (!enough_size) cerr << "HASH TABLE FULL" << endl;
      if (interruption || !enough_size) {
        cerr << "interrupted" << endl;
        break;
      }
    } while (true);
    lock_guard<mutex> lock(mutex_queue);
    running = false;
    cond_queue.notify_all();
  } else {
    do {
      // Wait if dcnn queue is full
      WaitForEvaluationQueue(false);
      // 探索回数を1回増やす
      c.move_count = atomic_fetch_add(&po_info.count, 1);
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      c.path.clear();
      UctSearchPO(c, game, color, mt[targ->thread_id].get(), current_root, &winner);
      // 探索を打ち切るか確認
      bool interruption = InterruptionCheck();
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
      if (!enough_size) cerr << "HASH TABLE FULL" << endl;
      if (interruption || !enough_size)
        break;
    } while (true);
  }

  // メモリの解放
  FreeGame(game);
  return;
}

static void
ParallelUctSearchNN( thread_arg_t *arg )
{
  thread_arg_t *targ = (thread_arg_t *)arg;
  game_info_t *game;
  int color = targ->color;
  bool enough_size = true;
  uct_search_context_t& c = ctx[targ->thread_id];
  c.search_mode = NN;

  game = AllocateGame();

  // 探索回数が閾値を超える, または探索が打ち切られたらループを抜ける
  do {
    // Wait if dcnn queue is full
    WaitForEvaluationQueue(false);
    // 探索回数を1回増やす
    c.move_count = atomic_fetch_add(&po_info.nn_count, 1);
    // 盤面のコピー
    CopyGame(game, targ->game);
    // 1回プレイアウトする
    c.path.clear();
    UctSearchNN(c, game, color, mt[targ->thread_id].get(), current_root);
    atomic_fetch_add(&depth_sum, c.depth);
    atomic_fetch_add(&depth_count, 1);
    // 探索を打ち切るか確認
    bool interruption = InterruptionCheck();
    // ハッシュに余裕があるか確認
    enough_size = CheckRemainingHashSize();
    if (!enough_size) cerr << "HASH TABLE FULL" << endl;
    if (interruption || !enough_size)
      break;
  } while (true);

  // メモリの解放
  FreeGame(game);
  return;
}


/////////////////////////////////
//  並列処理で呼び出す関数     //
//  UCTアルゴリズムを反復する  //
/////////////////////////////////
static void
ParallelUctSearchPondering( thread_arg_t *arg )
{
  thread_arg_t *targ = (thread_arg_t *)arg;
  game_info_t *game;
  int color = targ->color;
  bool enough_size = true;
  int winner = 0;
  uct_search_context_t& c = ctx[targ->thread_id];
  c.search_mode = PO;

  game = AllocateGame();

  // スレッドIDが0のスレッドだけ別の処理をする
  // 探索回数が閾値を超える, または探索が打ち切られたらループを抜ける
  if (targ->thread_id == 0) {
    int interval = CRITICALITY_INTERVAL;

    do {
      // Wait if dcnn queue is full
      WaitForEvaluationQueue(true);
      // 探索回数を1回増やす
      c.move_count = atomic_fetch_add(&po_info.count, 1);
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      c.path.clear();
      UctSearchPO(c, game, color, mt[targ->thread_id].get(), current_root, &winner);
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
      // OwnerとCriticalityを計算する
      if (po_info.count > interval) {
        CalculateOwner(color, po_info.count);
        CalculateCriticality(color);
        interval += CRITICALITY_INTERVAL;
      }
    } while (!pondering_stop && enough_size);

    lock_guard<mutex> lock(mutex_queue);
    running = false;
    cond_queue.notify_all();
  } else {
    do {
      // Wait if dcnn queue is full
      WaitForEvaluationQueue(true);
      // 探索回数を1回増やす
      c.move_count = atomic_fetch_add(&po_info.count, 1);
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      c.path.clear();
      UctSearchPO(c, game, color, mt[targ->thread_id].get(), current_root, &winner);
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
    } while (!pondering_stop && enough_size);
  }

  // メモリの解放
  FreeGame(game);
  return;
}

static void
ParallelUctSearchPonderingNN( thread_arg_t *arg )
{
  thread_arg_t *targ = (thread_arg_t *)arg;
  game_info_t *game;
  int color = targ->color;
  bool enough_size = true;
  uct_search_context_t& c = ctx[targ->thread_id];
  c.search_mode = NN;

  game = AllocateGame();

  // 探索回数が閾値を超える, または探索が打ち切られたらループを抜ける
  do {
    // Wait if dcnn queue is full
    WaitForEvaluationQueue(true);
    // 探索回数を1回増やす
    c.move_count = atomic_fetch_add(&po_info.nn_count, 1);
    // 盤面のコピー
    CopyGame(game, targ->game);
    // 1回プレイアウトする
    c.path.clear();
    UctSearchNN(c, game, color, mt[targ->thread_id].get(), current_root);
    // ハッシュに余裕があるか確認
    enough_size = CheckRemainingHashSize();
  } while (!pondering_stop && enough_size);

  // メモリの解放
  FreeGame(game);
  return;
}

static int
GetResult( game_info_t *game, int color, mt19937_64 *mt, int *winner )
{
  // 終局まで対局のシミュレーション
  Simulation(game, color, mt);

  // コミを含めない盤面のスコアを求める
  double score = CalculateScore(game);

  int result;
  // コミを考慮した勝敗
  if (scoring_mode == SCORING_MODE::JAPANESE) {
    if (my_color == S_BLACK) {
      if (score - dynamic_komi[my_color] + 0.1 >= 0) {
          result = (color == S_BLACK ? 0 : 1);
          *winner = S_BLACK;
      } else {
        result = (color == S_WHITE ? 0 : 1);
        *winner = S_WHITE;
      }
    } else {
      if (score - dynamic_komi[my_color] - 0.1 > 0) {
        result = (color == S_BLACK ? 0 : 1);
        *winner = S_BLACK;
      } else {
        result = (color == S_WHITE ? 0 : 1);
        *winner = S_WHITE;
      }
    }
  } else {
    if (my_color == S_BLACK) {
      if (score - dynamic_komi[0] + 0.1 >= 0) {
        result = (color == S_BLACK ? 0 : 1);
        *winner = S_BLACK;
      } else {
        result = (color == S_WHITE ? 0 : 1);
        *winner = S_WHITE;
      }
    } else {
      if (score - dynamic_komi[0] - 0.1 > 0) {
        result = (color == S_BLACK ? 0 : 1);
        *winner = S_BLACK;
      } else {
        result = (color == S_WHITE ? 0 : 1);
        *winner = S_WHITE;
      }
    }
  }

  return result;
}

//////////////////////////////////////////////
//  UCT探索を行う関数                        //
//  1回の呼び出しにつき, 1プレイアウトする    //
//////////////////////////////////////////////
static int
UctSearchPO( uct_search_context_t& ctx, game_info_t *game, int color, mt19937_64 *mt, int current, int *winner )
{
  int result = 0;
  child_node_t *uct_child = uct_node[current].child;
  const int child_num = uct_node[current].child_num;

  if (uct_node[current].state == NODE_STATE::EVALUATING) {
    //cerr << "<";
    for (int i = 0; i < 1000; i++) {
      this_thread::sleep_for(chrono::milliseconds(1));
      if (uct_node[current].state == NODE_STATE::EVALUATED)
        break;
    }
  }

  // 現在見ているノードをロック
  LOCK_NODE(current);
  // LFR
  if (uct_node[current].width == 0) {
    int parent = ctx.path[ctx.path.size() - 1];
    if (uct_node[current].state == NODE_STATE::EVALUATED) {
      // 探索幅を1つ増やす
      uct_node[current].width++;

      // 最もγが大きい着手を探索できるようにする
      int max_index = 0;
      double max_score = uct_child[0].nnrate;
      for (int i = 1; i < child_num; i++) {
        if (uct_child[i].nnrate > max_score) {
          max_score = uct_child[i].nnrate;
          max_index = i;
        }
      }
      uct_child[max_index].flag = true;
    } else {
      lock_guard<mutex> lock(mutex_rating);
      RateExpandedNode(current, game, color, parent, ctx.path);
    }
  }
  // UCB値最大の手を求める
  int next_index = SelectMaxUcbChild(ctx, game, current, color);
  // 選んだ手を着手
  PutStone(game, uct_child[next_index].pos, color);
  // 色を入れ替える
  color = FLIP_COLOR(color);

  ctx.path.push_back(current);

  bool end_of_game = game->moves > 2 &&
    game->record[game->moves - 1].pos == PASS &&
    game->record[game->moves - 2].pos == PASS;

  if (uct_child[next_index].index == -1 && (uct_child[next_index].move_count < expand_threshold || end_of_game)) {
    int start = game->moves;

    // Virtual Lossを加算
    int n = AddVirtualLoss(&uct_child[next_index], current);

    memcpy(game->seki, uct_node[current].seki, sizeof(bool) * BOARD_MAX);
    
    // 現在見ているノードのロックを解除
    UNLOCK_NODE(current);

    // 終局まで対局のシミュレーション
    result = GetResult(game, color, mt, winner);

    // 統計情報の記録
    Statistic(game, *winner);
  } else {
    // Virtual Lossを加算
    AddVirtualLoss(&uct_child[next_index], current);
    // ノードの展開の確認
    if (uct_child[next_index].index == -1) {
      // ノードの展開
      uct_child[next_index].index = ExpandNode(game, color, current);
      //cerr << "value evaluated " << result << " " << v << " " << *value_result << endl;
    }
    // 現在見ているノードのロックを解除
    UNLOCK_NODE(current);
    // 手番を入れ替えて1手深く読む
    result = UctSearchPO(ctx, game, color, mt, uct_child[next_index].index, winner);
  }

  // 探索結果の反映
  UpdateResult(&uct_child[next_index], result, current);

  // 統計情報の更新
  UpdateNodeStatistic(game, *winner, uct_node[current].statistic);

  return 1 - result;
}


static void
UctSearchNN( uct_search_context_t& ctx, game_info_t *game, int color, mt19937_64 *mt, int current )
{
  child_node_t *uct_child = uct_node[current].child;

  /*
  static int block_count[10] = { 0 };
  if (!mutex_nodes[(current)].try_lock()) {
    atomic_fetch_add(&count_uct_block, 1);
    cerr << "+" << (int) uct_node[current].state.load();
    LOCK_NODE(current);
  }
  */
  if (uct_node[current].state != NODE_STATE::EVALUATED) {
    for (int i = 0; i < 1000; i++) {
      this_thread::sleep_for(chrono::milliseconds(1));
      if (uct_node[current].state == NODE_STATE::EVALUATED)
        break;
    }
    if (uct_node[current].state != NODE_STATE::EVALUATED) {
      cerr << "#";
      return;
    }
  }

  // 現在見ているノードをロック
  LOCK_NODE(current);
  // UCB値最大の手を求める
  int next_index = SelectMaxUcbChild(ctx, game, current, color);
  // 選んだ手を着手
  PutStone(game, uct_child[next_index].pos, color);
  // 色を入れ替える
  color = FLIP_COLOR(color);

  ctx.path.push_back(current);

  bool end_of_game = game->moves > 2 &&
    game->record[game->moves - 1].pos == PASS &&
    game->record[game->moves - 2].pos == PASS;

  if (end_of_game) {
    // 現在見ているノードのロックを解除
    UNLOCK_NODE(current);

    // 終局まで対局のシミュレーション
    int winner;
    double r = GetResult(game, color, mt, &winner);
    atomic_fetch_add(&eval_count_eog, 1);
    //cerr << "End " << winner << endl;
    double value = r;

    for (int i = ctx.path.size() - 1; i >= 0; i--) {
      int node = ctx.path[i];

      atomic_fetch_add(&uct_node[node].value_move_count, 1);
      atomic_fetch_add(&uct_node[node].value_win, value);
      value = 1 - value;
    }
    ctx.depth = ctx.path.size();
  } else {
    /*
    cerr << "Serach "
      << uct_child[next_index].index
      << "\t" << uct_node[uct_child[next_index].index].evaled
      << "\t" << uct_child[next_index].eval_value << endl;
    */
    // ノードの展開の確認
    if (uct_child[next_index].index == -1) {
      // ノードの展開
      uct_child[next_index].index = ExpandNode(game, color, current);
    }

    // 現在見ているノードのロックを解除
    UNLOCK_NODE(current);

    int next_node_index = uct_child[next_index].index;

    NODE_STATE expected = NODE_STATE::INIT;
    if (atomic_compare_exchange_strong(&uct_node[next_node_index].state, &expected, NODE_STATE::EVALUATING)) {
      auto req = make_shared<nn_eval_req>();
      req->index = next_node_index;
      copy(ctx.path.begin(), ctx.path.end(), back_inserter(req->path));
      req->path.push_back(next_node_index);
      req->moves = game->moves;
      memcpy(req->record, game->record, sizeof(record_t) * MAX_RECORDS);

      for (int n : req->path)
        atomic_fetch_add(&uct_node[n].value_move_count, VIRTUAL_LOSS_NN);

#if ASYNC_NN
      lock_guard<mutex> lock(mutex_queue);
      eval_nn_queue.push(req);
      cond_queue.notify_all();
#else
      EvalValue(req);
#endif
      ctx.depth = ctx.path.size();

      return;
    }

    // 手番を入れ替えて1手深く読む
    UctSearchNN(ctx, game, color, mt, uct_child[next_index].index);
  }
}


//////////////////////////
//  Virtual Lossの加算  //
//////////////////////////
static int
AddVirtualLoss( child_node_t *child, int current )
{
  atomic_fetch_add(&uct_node[current].move_count, VIRTUAL_LOSS);
  int org = atomic_fetch_add(&child->move_count, VIRTUAL_LOSS);
  return org;
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

static void
UpdatePolicyRate(int current)
{
#if 0
  const int move_count = uct_node[current].move_count;
  child_node_t *uct_child = uct_node[current].child;
  const int child_num = uct_node[current].child_num;
  double t = policy_temperature + log(move_count + 1) * policy_temperature_inc;

  double sum;
  double rate;
  int n = 0;

  double max_nnrate0 = numeric_limits<double>::lowest();
  for (int i = 1; i < child_num; i++) {
    max_nnrate0 = max(uct_child[i].nnrate0, max_nnrate0);
  }
  double offset = - max_nnrate0;

  // TODO cleanup
  do {
    double max_rate = 0;
    double min_rate = numeric_limits<double>::max();

    sum = 0;

    for (int i = 1; i < child_num; i++) {
      double rate = exp((uct_child[i].nnrate0 + offset) / t);
      if (rate > max_rate)
        max_rate = rate;
      if (rate < min_rate)
        min_rate = rate;
      sum += rate;
    }
    rate = max_rate / sum;

    //if (n != 0) cerr << "#" << n << " max:" << (max_rate / sum) << " temperature:" << t << " " << tt << endl;
    if (rate > policy_top_rate_max)
      t *= 1.05;
    n++;
  } while (t > 0 && t < 10 && rate > policy_top_rate_max);

  for (int i = 1; i < child_num; i++) {
    double rate = exp((uct_child[i].nnrate0 + offset) / t) / sum;
    uct_child[i].nnrate = max(rate, 0.0);
  }
#endif
}


/////////////////////////////////////////////////////
//  UCBが最大となる子ノードのインデックスを返す関数  //
/////////////////////////////////////////////////////
static int
SelectMaxUcbChild( const uct_search_context_t& ctx, const game_info_t *game, int current, int color )
{
  bool evaled = uct_node[current].state == NODE_STATE::EVALUATED;
  child_node_t *uct_child = uct_node[current].child;
  const int child_num = uct_node[current].child_num;
  const int sum = uct_node[current].move_count;
  const int sum_value = uct_node[current].value_move_count;
  rate_order_t order[PURE_BOARD_MAX + 1];
  double child_ucb[UCT_CHILD_MAX];
  double child_lcb[UCT_CHILD_MAX];
  double ucb_bonus_weight = bonus_weight * sqrt(bonus_equivalence / (sum + bonus_equivalence));
  const bool debug = current == current_root
    && GetDebugMessageMode() && GetVerbose()
    && ((ctx.search_mode == PO && sum % 10000 == 0) || (ctx.search_mode == NN && sum_value % 1000 == 0));

  if (live_best_sequence && current == current_root && sum % 1000 == 0) {
    PrintBestSequenceGFX(cerr, game, uct_node, current_root, color);
  }
  //if (evaled) {
    //cerr << "use nn" << endl;
//  } else 
  {
    // 128回ごとにOwnerとCriticalityでソートし直す  
    if (((sum & 0x7f) == 0 && sum != 0) || (ctx.search_mode == NN && sum_value <= 1)) {
      int o_index[UCT_CHILD_MAX], c_index[UCT_CHILD_MAX];
      if (sum == 0) {
        fill_n(o_index, child_num, 0);
        fill_n(c_index, child_num, 0);
      } else {
        CalculateCriticalityIndex(&uct_node[current], uct_node[current].statistic, color, c_index);
        CalculateOwnerIndex(&uct_node[current], uct_node[current].statistic, color, o_index);
      }
      double thr = max(uct_child[0].nnrate, search_threshold_policy_rate);
      for (int i = 0; i < child_num; i++) {
        int pos = uct_child[i].pos;
        double dynamic_parameter = uct_owner[o_index[i]] + uct_criticality[c_index[i]];
        order[i].rate = uct_child[i].rate + dynamic_parameter + uct_child[i].nnrate * 10;
        order[i].index = i;
        uct_child[i].flag = uct_child[i].nnrate >= thr;
        //uct_child[i].flag = false;
      }
      qsort(order, child_num, sizeof(rate_order_t), RateComp);

      // 子ノードの数と探索幅の最小値を取る
      int width = ((uct_node[current].width > child_num) ? child_num : uct_node[current].width);

      // 探索候補の手を展開し直す
      for (int i = 0; i < width; i++) {
        uct_child[order[i].index].flag = true;
      }

      if (evaled && policy_temperature_inc > 0)
        UpdatePolicyRate(current);

#if 1
      if (current == current_root) {
        // Prune
        int max_value_move_count = 0;
        for (int i = 0; i < child_num; i++) {
          int m = ValueMoveCount(current, order[i].index);
          if (m > max_value_move_count)
            max_value_move_count = m;
        }
        int rest = po_info.nn_halt - po_info.nn_count;
        for (int i = 0; i < child_num; i++) {
          if (!uct_child[order[i].index].flag && !uct_child[order[i].index].open)
            continue;
          int m = ValueMoveCount(current, order[i].index);
          if (m + rest < max_value_move_count) {
            /*
            cerr << "prune  " << FormatMove(uct_child[order[i].index].pos)
                 << " " << m << " + " << rest << " < " << max_value_move_count << endl;
            */
            uct_child[order[i].index].flag = false;
          }
        }
      }
#endif
    }

    // Progressive Wideningの閾値を超えたら, 
    // レートが最大の手を読む候補を1手追加
    if (sum > pw[uct_node[current].width]) {
      int max_index = -1;
      double max_rate = numeric_limits<double>::lowest();
      for (int i = 0; i < child_num; i++) {
        if (uct_child[i].flag == false) {
          int pos = uct_child[i].pos;
          double dynamic_parameter = uct_owner[owner_index[pos]] + uct_criticality[criticality_index[pos]];
          double r = uct_child[i].rate + dynamic_parameter + uct_child[i].nnrate * 10;
          if (r > max_rate) {
            max_index = i;
            max_rate = r;
          }
        }
      }
      if (max_index != -1) {
        uct_child[max_index].flag = true;
      }
      uct_node[current].width++;
    }
  }

  double max_value = numeric_limits<double>::lowest();
  int max_child = 0;

  const double p_po0 = (double)uct_node[current].win / (uct_node[current].move_count + 1);
  const double p_vn0 = (double)uct_node[current].value_win / (uct_node[current].value_move_count + 1);
  const double scale = std::max(0.5, std::min(1.0, 1.0 - (game->moves - 250) / 200.0)) * value_scale;
  //const double scale = value_scale;

  int start_child = 0;
  if (!early_pass && current == current_root && child_num > 1) {
    if (uct_child[0].move_count > uct_node[current].move_count * pass_po_limit) {
      start_child = 1;
    }
  }

  double sum_visited_nnrate = 0;
  for (int i = start_child; i < child_num; i++) {
    if (uct_child[i].index >= 0) {

      sum_visited_nnrate += uct_child[i].nnrate;
    }
  }

  double cfg_fpu_reduction = 0.25f;
  double fpu_reduction0 = cfg_fpu_reduction * sqrt(sum_visited_nnrate);
  double fpu_reduction = current == current_root ? 0 : fpu_reduction0;

  int max_move_count = 0;
  int max_move_child = 0;

  double sqrt_sum = sqrt(sum + 1);
  double sqrt_sum_value = sqrt(sum_value + 1);

  // UCB値最大の手を求める  
  for (int i = start_child; i < child_num; i++) {
#if 0
    if (!uct_child[i].flag && !uct_child[i].open)
      continue;
#else
    if (ctx.search_mode == PO && !uct_child[i].flag && !uct_child[i].open)
      continue;
    //if (ctx.search_mode == NN && uct_child[i].nnrate < max(uct_child[0].nnrate, search_threshold_policy_rate))
    //continue;
    if (ctx.search_mode == NN && !uct_child[i].flag && !uct_child[i].open)
      continue;
#endif

    double value_win = 0;
    double value_move_count = 0;

    if (uct_child[i].index >= 0 ) {
      auto node = &uct_node[uct_child[i].index];
      value_win = node->value_win;
      value_move_count = node->value_move_count;
      value_win = value_move_count - value_win;
      //cerr << "VA:" << (value_win / value_move_count) << " VS:" << uct_child[i].value << endl;
    }

    double win = uct_child[i].win;
    double move_count = uct_child[i].move_count;
    double ucb_value, lcb_value;

    if (evaled) {
      if (debug) {
        cerr << uct_node[current].move_count << ".";
        cerr << setw(3) << FormatMove(uct_child[i].pos);
        cerr << ": move " << setw(5) << (int) move_count << " policy "
          << setw(10) << (uct_child[i].nnrate  * 100) << " ";
      }
      double p_po, p_vn;
      if (move_count == 0) {
        if (ctx.search_mode == PO)
          p_po = FPU;
        else
          p_po = p_po0;
      } else {
        p_po = win / move_count;
      }
      if (value_move_count == 0) {
        p_vn = p_vn0 - fpu_reduction;
      } else {
        p_vn = value_win / value_move_count;
      }
      double p = p_po * (1 - scale)  + p_vn * scale;
      if (debug) {
        cerr << "DP:" << setw(10) << (p_po * 100) << " DV:" << setw(10) << (p_vn * 100) << " => " << setw(10) << (p * 100);
        cerr << " LM:" << scale << " ";
      }

      const double rate = uct_child[i].nnrate;
#if 0
      double u_po = sqrt_sum / (1 + move_count);
      double ucb_po = p_po + c_puct * u_po * rate;

      double u_vn = sqrt_sum_value / (1 + value_move_count);
      double ucb_vn = p_vn + c_puct * u_vn * rate;

      if (ctx.search_mode == PO) {
        if (sum % 2 == 0) {
          ucb_value = (1 - scale) * ucb_po + scale * p_vn;
        } else {
          ucb_value = (1 - scale) * ucb_po + scale * ucb_vn;
        }
      } else {
        ucb_value = (1 - scale) * p_po + scale * ucb_vn;
      }
      if (debug) {
        cerr << " P:" << p << " UCB:" << ucb_value
          << endl;
      }
#elif 1
      double cp = 10.0 / (move_count + 10.0);
      double p_po2 = p_po0 * cp + p_po * (1.0 - cp);

      //double cv = 5.0 / (value_move_count + 5.0);
      //double p_vn2 = p_vn0 * cv + p_vn * (1.0 - cv);

      if (ctx.search_mode == PO) {
        if (sum % 2 == 0 && sum > sum_value * 2) {
          double u_po = sqrt_sum / (1 + move_count);
          double ucb_po = p_po2 + c_puct * u_po * rate;
          double diff = value_move_count / (sum_value + 1.0) - move_count / (sum + 1.0);

          //ucb_value = (1 - scale) * ucb_po + scale * p_vn + diff;
          ucb_value = diff + 1.0;
        } else {
          double u_po = sqrt(log(sum + 1) / (1 + move_count));
          double ucb_po = p_po2 + u_po;
          if (value_move_count == 0) {
            p_vn = p_vn0;
          }
          double u_vn = sqrt_sum_value / (1 + value_move_count);
          double lcb_vn = p_vn - c_puct * u_vn * rate;

          ucb_value = (1 - scale) * ucb_po + scale * lcb_vn;
        }
      } else {
        double u_vn = sqrt_sum_value / (1 + value_move_count);
        double ucb_vn = p_vn + c_puct * u_vn * rate;

        ucb_value = (1 - scale) * p_po2 + scale * ucb_vn;
      }
      if (debug) {
        cerr << " P:" << p << " UCB:" << ucb_value
          << endl;
      }
#else
      double u_po = sqrt_sum / (1 + uct_child[i].move_count);
      double ucb_po = p_po + c_puct * u_po * rate;
      double lcb_po = p_po - c_puct * u_po * rate;

      double u_vn = sqrt_sum_value / (1 + value_move_count);
      double ucb_vn = p_vn + c_puct * u_vn * rate;
      double lcb_vn = p_vn - c_puct * u_vn * rate;

      if (ctx.search_mode == PO)
        ucb_value = (1 - scale) * ucb_po + scale * lcb_vn;
      else
        ucb_value = (1 - scale) * lcb_po + scale * ucb_vn;
      if (debug) {
        cerr << " P:" << p << " UCB:" << ucb_value
          << endl;
      }
#endif
    } else {
      if (uct_child[i].move_count == 0) {
        ucb_value = FPU;
        lcb_value = FPU;
      } else {
        // UCB1-TUNED value
        double p = (double) uct_child[i].win / uct_child[i].move_count;
        //if (p2 >= 0) p = (p * 9 + p2) / 10;
        double div = log(sum) / uct_child[i].move_count;
        double v = p - p * p + sqrt(2.0 * div);
        ucb_value = p + sqrt(div * ((0.25 < v) ? 0.25 : v));
        lcb_value = p - sqrt(div * ((0.25 < v) ? 0.25 : v));

        // UCB Bonus
        ucb_value += ucb_bonus_weight * uct_child[i].rate;
        lcb_value += ucb_bonus_weight * uct_child[i].rate;
      }
    }

    child_ucb[i] = ucb_value;
    child_lcb[i] = lcb_value;

    if (ucb_value > max_value) {
      max_value = ucb_value;
      max_child = i;
    }
    if (ctx.search_mode == PO) {
      if (move_count > max_move_count) {
        max_move_count = move_count;
        max_move_child = i;
      }
    } else {
      if (value_move_count > max_move_count) {
        max_move_count = value_move_count;
        max_move_child = i;
      }
    }
  }

#if 0
  if (current == current_root && max_child == max_move_child) {
    if (ctx.search_mode == PO) {
      double next_ucb = -1;//child_lcb[max_child];
      int next_child = max_child;
      for (int i = 0; i < child_num; i++) {
        if (max_child == i)
          continue;
        if (uct_child[i].flag || uct_child[i].open) {
          if (child_ucb[i] > next_ucb) {
            next_ucb = child_ucb[i];
            next_child = i;
          }
        }
      }
      if (max_child != next_child
          && uct_child[max_child].move_count > uct_child[next_child].move_count * 1.2) {
        //cerr << "Replace " << FormatMove(uct_child[max_child].pos) << " -> " << FormatMove(uct_child[next_child].pos) << endl;
        max_child = next_child;
      }
    } else {
      double next_ucb = -1;//child_lcb[max_child];
      int next_child = max_child;
      for (int i = 0; i < child_num; i++) {
        if (max_child == i)
          continue;
        if (uct_child[i].flag || uct_child[i].open) {
          if (child_ucb[i] > next_ucb) {
            next_ucb = child_ucb[i];
            next_child = i;
          }
        }
      }
      if (max_child != next_child
          && ValueMoveCount(current, max_child) > ValueMoveCount(current,  next_child) * 1.2) {
        //cerr << "Replace " << FormatMove(uct_child[max_child].pos) << " -> " << FormatMove(uct_child[next_child].pos) << endl;
        max_child = next_child;
      }
    }
  }
#endif

  /*
  static ray_clock::time_point previous_time = ray_clock::now();
  static mutex mutex_log;
  if (current == current_root && sum > 0) {
    mutex_log.lock();
    if (GetSpendTime(previous_time) > 1.0) {
      for (int i = 0; i < child_num; i++) {
	if (i > 0 && !uct_child[i].flag && !uct_child[i].open)
	  continue;
	double win = uct_child[i].win;
	double move_count = uct_child[i].move_count;
	double p0 = win / move_count;

	cerr << "|" << setw(4) << FormatMove(uct_child[i].pos);
	cerr << "|" << setw(5) << (int) move_count;
	auto precision = cerr.precision();
	cerr.precision(4);
	cerr << "|" << setw(10) << fixed << (p0 * 100);
	cerr << "|" << setw(10) << fixed << (child_ucb[i] * 100);
	cerr << "|" << setw(10) << fixed << (child_lcb[i] * 100);
	cerr << endl;
	cerr.precision(precision);
      }
      previous_time = ray_clock::now();
    }
    mutex_log.unlock();
  }
  */

  return max_child;
}


///////////////////////////////////////////////////////////
//  OwnerやCriiticalityを計算するための情報を記録する関数  //
///////////////////////////////////////////////////////////
static void
Statistic( game_info_t *game, int winner )
{
  const char *board = game->board;
  int pos, color;

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    color = board[pos];
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
  int pos, color;

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    color = board[pos];
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
  double win, lose;
  const int other = FLIP_COLOR(color);
  const int count = node->move_count;
  const int child_num = node->child_num;
  int pos;
  double tmp;

  win = (double)node->win / node->move_count;
  lose = 1.0 - win;

  index[0] = 0;

  for (int i = 1; i < child_num; i++) {
    pos = node->child[i].pos;

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
  int pos;
  double tmp;
  const int other = FLIP_COLOR(color);
  double win, lose;

  win = (double)uct_node[current_root].win / uct_node[current_root].move_count;
  lose = 1.0 - win;

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];

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
  int pos;
  const int count = node->move_count;
  const int child_num = node->child_num;

  index[0] = 0;

  for (int i = 1; i < child_num; i++){
    pos = node->child[i].pos;
    index[i] = (int)((double)node_statistic[pos].colors[color] * 10.0 / count + 0.5);
    if (index[i] > OWNER_MAX - 1) index[i] = OWNER_MAX - 1;
    if (index[i] < 0)   index[pos] = 0;
  }
}


//////////////////////////////
//  Ownerの計算をする関数   //
//////////////////////////////
static void
CalculateOwner( int color, int count )
{
  int pos;

  for (int i = 0; i < pure_board_max; i++){
    pos = onboard_pos[i];
    owner_index[pos] = (int)((double)statistic[pos].colors[color] * 10.0 / count + 0.5);
    if (owner_index[pos] > OWNER_MAX - 1) owner_index[pos] = OWNER_MAX - 1;
    if (owner_index[pos] < 0)   owner_index[pos] = 0;
  }
}


/////////////////////////////////
//  次のプレイアウト回数の設定  //
/////////////////////////////////
static void
CalculateNextPlayouts( game_info_t *game, int color, double best_wp, double finish_time )
{
  double po_per_sec;
  double nn_per_sec;

  if (finish_time != 0.0) {
    po_per_sec = po_info.count / finish_time;
    nn_per_sec = po_info.nn_count / finish_time;
  } else {
    po_per_sec = PLAYOUT_SPEED * threads;
    nn_per_sec = PLAYOUT_SPEED;
  }

  // 次の探索の時の探索回数を求める
  if (mode == CONST_TIME_MODE) {
    if (best_wp > 0.90) {
      po_info.num = (int)(po_per_sec * const_thinking_time * 1.5 / 2);
      po_info.nn_num = (int)(nn_per_sec * const_thinking_time * 1.5 / 2);
    } else {
      po_info.num = (int)(po_per_sec * const_thinking_time * 1.5);
      po_info.nn_num = (int)(nn_per_sec * const_thinking_time * 1.5);
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
    po_info.num = (int)(po_per_sec * time_limit * 1.5);
    po_info.nn_num = (int)(nn_per_sec * time_limit * 1.5);
  } 
}


/////////////////////////////////////
//  UCTアルゴリズムによる局面解析  //
/////////////////////////////////////
int
UctAnalyze( game_info_t *game, int color )
{
  int pos;
  vector<unique_ptr<std::thread>> handle;

  // 探索情報をクリア
  memset(statistic, 0, sizeof(statistic_t) * board_max);  
  fill_n(criticality_index, board_max, 0);  
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;
  }
  po_info.count = 0;

  ClearUctHash();

  current_root = ExpandRoot(game, color);

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
  }

  bool org_use_nn = use_nn;
  use_nn = false;

  po_info.halt = 10000;

  t_arg.resize(threads);
  running = true;
  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    handle.push_back(make_unique<thread>(ParallelUctSearch, &t_arg[i]));
  }


  for (auto &t : handle) {
    t->join();
  }
  handle.clear();

  use_nn = org_use_nn;

  int x, y, black = 0, white = 0;
  double own;

  for (y = board_start; y <= board_end; y++) {
    for (x = board_start; x <= board_end; x++) {
      pos = POS(x, y);
      own = (double)statistic[pos].colors[S_BLACK] / uct_node[current_root].move_count;
      if (own > 0.5) {
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
  int pos;
  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    dest[pos] = (int)((double)uct_node[current_root].statistic[pos].colors[my_color] / uct_node[current_root].move_count * 100);
  }
}


///////////////////////////////
//  Criticalityをコピーする  //
///////////////////////////////
void
CopyCriticality( double *dest )
{
  int pos;
  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    dest[pos] = criticality[pos];
  }
}

void
CopyStatistic( statistic_t *dest )
{
  memcpy(dest, statistic, sizeof(statistic_t)* BOARD_MAX); 
}


////////////////////////////////////////////////////////
//  UCTアルゴリズムによる着手生成(KGS Clean Up Mode)  //
////////////////////////////////////////////////////////
int
UctSearchGenmoveCleanUp( game_info_t *game, int color )
{
  int pos;
  double finish_time;
  int select_index;
  int max_count;
  double wp;
  int count;
  child_node_t *uct_child;
  vector<unique_ptr<std::thread>> handle;

  memset(statistic, 0, sizeof(statistic_t)* board_max); 
  fill_n(criticality_index, board_max, 0); 
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;
  }

  begin_time = ray_clock::now();

  po_info.count = 0;

  current_root = ExpandRoot(game, color);

  if (uct_node[current_root].child_num <= 1) {
    pos = PASS;
    return pos;
  }

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    owner[pos] = 50.0;
  }

  po_info.halt = po_info.num;

  bool org_use_nn = use_nn;
  use_nn = false;

  DynamicKomi(game, &uct_node[current_root], color);

  t_arg.reserve(threads);
  running = true;
  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    handle.push_back(make_unique<thread>(ParallelUctSearch, &t_arg[i]));
  }

  for (auto &t : handle) {
    t->join();
  }
  handle.clear();

  use_nn = org_use_nn;

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

  pos = uct_child[select_index].pos;

  PrintBestSequence(game, uct_node, current_root, color);

  CalculateNextPlayouts(game, color, wp, finish_time);

  count = 0;

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];

    if (owner[pos] >= 5 || owner[pos] <= 95) {
      candidates[pos] = true;
      count++;
    } else {
      candidates[pos] = false;
    }
  }

  if (count == 0) pos = PASS;
  else pos = uct_child[select_index].pos;

  if ((double)uct_child[select_index].win / uct_child[select_index].move_count < resign_threshold) {
    pos = PASS;
  }

  return pos;
}

///////////////////////////////////
//  子ノードのインデックスの収集  //
///////////////////////////////////
static void
CorrectDescendentNodes(vector<int> &indexes, int index)
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


void
EvalValue(const std::shared_ptr<nn_eval_req>& req)
{
  const int index = req->index;
  const int child_num = uct_node[index].child_num;
  child_node_t *uct_child = uct_node[index].child;

  auto result = EvaluateLeela(req->moves, req->record);

  LOCK_NODE(index);

  float win = result.winrate;
  //cerr << "color:" << req->color << "\twin:" << win << endl;
  array<float, PURE_BOARD_MAX> &moves = result.policy;

  const int ofs = 0;

  double sum = 0;
  uct_child[0].nnrate = min(result.policy_pass, 0.1f);
  sum += uct_child[0].nnrate;
  for (int i = 1; i < child_num; i++) {
    int pos = uct_child[i].pos;

    int x = X(pos) - OB_SIZE;
    int y = Y(pos) - OB_SIZE;
    int n = x + y * pure_board_size;
    double score = moves[n + ofs];
    //if (depth == 1) cerr << "RAW POLICY " << uct_child[i].pos << " " << req->trans << " " << FormatMove(pos) << " " << x << "," << y << " " << ofs << " -> " << score << endl;
    if (uct_child[i].ladder) {
      score = min(score, 0.01);
    }

    uct_child[i].nnrate = score;
    // uct_child[i].flag = uct_child[i].nnrate > max(result.policy_pass, (float) search_threshold_policy_rate);
    sum += uct_child[i].nnrate;
  }
  //cerr << "sum:" << sum << endl;
  double thr = max(uct_child[0].nnrate, search_threshold_policy_rate);
  for (int i = 0; i < child_num; i++) {
    uct_child[i].nnrate /= sum;
    uct_child[i].flag = uct_child[i].nnrate >= thr;
  }

  UpdatePolicyRate(index);
  uct_node[index].state = NODE_STATE::EVALUATED;

  UNLOCK_NODE(index);

  double p = win;
  if (p < 0)
    p = 0;
  if (p > 1)
    p = 1;
  //cerr << "#" << index << "  " << sum << endl;

  double value = p;

  for (int i = req->path.size() - 1; i >= 0; i--) {
    int current = req->path[i];

    atomic_fetch_add(&uct_node[current].value_move_count, 1 - VIRTUAL_LOSS_NN);
    atomic_fetch_add(&uct_node[current].value_win, value);
    value = 1 - value;
  }

  atomic_fetch_add(&eval_count_value, 1);
}

#if ASYNC_NN
void EvalNode() {
  int num_eval = 0;
  bool allow_skip = (!reuse_subtree && !ponder) || time_limit <= 1.0;

  int num_wait = 0;
  double sum_wait = 0;

  while (true) {
    unique_lock<mutex> lock(mutex_queue);

    auto begin_time = ray_clock::now();
    cond_queue.wait(lock, [=] {
      bool abort = !running && (allow_skip || eval_nn_queue.empty());
      return abort || !eval_nn_queue.empty();
    });
    sum_wait += GetSpendTime(begin_time);
    num_wait++;

    if (!running
      && (allow_skip || eval_nn_queue.empty())) {
      cerr << "Eval " << num_eval << endl;
      cerr << (sum_wait / num_wait) << "sec " << num_wait << endl;
      break;
    }

    if (eval_nn_queue.size() > 0) {
      auto req = eval_nn_queue.front();
      eval_nn_queue.pop();
      lock.unlock();

      num_eval++;
      EvalValue(req);
    }
  }
}
#endif
