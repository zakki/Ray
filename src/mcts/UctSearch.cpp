/**
 * @file src/mcts/UctSearch.cpp
 * @author Yuki Kobayashi
 * @~english
 * @brief Monte-Carlo tree search with upper confidence bound.
 * @~japanese
 * @brief UCBを利用したモンテカルロ木探索
 */
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

#include "board/DynamicKomi.hpp"
#include "board/GoBoard.hpp"
#include "common/Message.hpp"
#include "pattern/PatternHash.hpp"
#include "feature/Ladder.hpp"
#include "feature/Seki.hpp"
#include "feature/Semeai.hpp"
#include "mcts/MoveSelection.hpp"
#include "mcts/Simulation.hpp"
#include "mcts/UctRating.hpp"
#include "mcts/UctSearch.hpp"
#include "mcts/ucb/UCBEvaluation.hpp"
#include "util/Utility.hpp"

#if defined (_WIN32)
#include <Windows.h>
#else
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/time.h>
#endif


/**
 * @~english
 * @brief UCT nodes.
 * @~japanese
 * @brief UCTノード
 */
uct_node_t *uct_node;

/**
 * @~english
 * @brief Progressive widening threshold for node pruning.
 * @~japanese
 * @brief Progressive Widening の枝刈り探索回数閾値
 */
static int pw[PURE_BOARD_MAX + 1];  

/**
 * @~english
 * @brief Node expansion threshold.
 * @~japanese
 * @brief ノード展開の閾値
 */
static int expand_threshold = EXPAND_THRESHOLD_19;

/**
 * @~english
 * @brief Current root's index.
 * @~japanese
 * @brief 現在のルートのインデックス
 */
int current_root;

/**
 * @~english
 * @brief Mutex variable for UCT nodes.
 * @~japanese
 * @brief UCTノード用のミューテックス変数
 */
static std::mutex *mutex_nodes;

/**
 * @~english
 * @brief Mutex variable for node expansion.
 * @~japanese
 * @brief ノード展開を排他処理するためのミューテックス変数
 */
static std::mutex mutex_expand;       

/**
 * @~english
 * @brief The number of search worker threads.
 * @~japanese
 * @brief 探索スレッド数
 */
static int threads = 1;

/**
 * @~english
 * @brief Arguments for search worker threads.
 * @~japanese
 * @brief 各スレッドに渡す引数
 */
static thread_arg_t t_arg[THREAD_MAX];

/**
 * @~english
 * @brief Statistic information of Monte-Carlo simulation.
 * @~japanese
 * @brief プレイアウトの統計情報
 */
static statistic_t statistic[BOARD_MAX];  

/**
 * @~english
 * @brief Counter for statistic information of Monte-Carlo simulation.
 * @~japanese
 * @brief プレイアウトの統計情報を収集した回数
 */
static std::atomic<int> statistic_count;

/**
 * @~english
 * @brief Criticality value.
 * @~japanese
 * @brief 盤上の各点のCriticality
 */
static double criticality[BOARD_MAX];  

/**
 * @~english
 * @brief Ownership value.
 * @~japanese
 * @brief 盤上の各点のOwner(0-100%)
 */
static double owner[BOARD_MAX];  

/**
 * @~english
 * @brief Indices for ownership feature.
 * @~japanese
 * @brief 現在のオーナーのインデックス
 */
static int owner_index[BOARD_MAX];   

/**
 * @~english
 * @brief Indices for criticality feature.
 * @~japanese
 * @brief 現在のクリティカリティのインデックス
 */
static int criticality_index[BOARD_MAX];  

/**
 * @~english
 * @brief Upper bound value for criticality.
 * @~japanese
 * @brief Criticalityの上限値
 */
static int criticality_max = CRITICALITY_MAX;

/**
 * @~english
 * @brief Move candidate flags.
 * @~japanese
 * @brief 候補手のフラグ
 */
static bool candidates[BOARD_MAX];  

/**
 * @~english
 * @brief Pondering activation flag.
 * @~japanese
 * @brief 予測読みするかどうかのフラグ
 */
bool pondering_mode = false;

/**
 * @~english
 * @brief Pondering flag.
 * @~japanese
 * @brief 予測読み中であることを表すフラグ
 */
static bool ponder = false;

/**
 * @~english
 * @brief Pondering stopping flag.
 * @~japanese
 * @brief 予測読みを止めるためのフラグ
 */
static bool pondering_stop = false;

/**
 * @~english
 * @brief Pondering is executed.
 * @~japanese
 * @brief 予測読みを実行したかどうかを表すフラグ
 */
static bool pondered = false;

/**
 * @~english
 * @brief Search worker thread.
 * @~japanese
 * @brief 探索ワーカスレッド
 */
static std::thread *worker[THREAD_MAX];

/**
 * @~english
 * @brief Random number generator for Monte-Carlo simulation.
 * @~japanese
 * @brief 乱数生成器
 */
static std::mt19937_64 mt[THREAD_MAX];

/**
 * @~english
 * @brief Reuse subtree flag.
 * @~japanese
 * @brief 探索木の再利用のフラグ
 */
static bool reuse_subtree = false;

/**
* @~english
* @brief Ray's stone color.
* @~japanese
* @brief 自分の手番の色
*/
static int my_color;


// Criticaliityの計算
static void CalculateCriticality( int color );

// Criticality
static void CalculateCriticalityIndex( uct_node_t *node, statistic_t *node_statistic, int color, int *index );

// Ownershipの計算
static void CalculateOwner( int color );

// Ownership
static void CalculateOwnerIndex( uct_node_t *node, statistic_t *node_statistc, int color, int *index );

// 現局面の子ノードのインデックスの導出
static void CorrectDescendentNodes( std::vector<int> &indexes, int index );

// ノードの展開
static int ExpandNode( game_info_t *game, int color, int current );

// ルートの展開
static int ExpandRoot( game_info_t *game, int color );

// UCT探索
static void ParallelUctSearch( thread_arg_t *arg );

// UCT探索(予測読み)
static void ParallelUctSearchPondering( thread_arg_t *arg );

// ノードのレーティング
static void RatingNode( game_info_t *game, int color, int index );

static int RateComp( const void *a, const void *b );

// UCB値が最大の子ノードを返す
static int SelectMaxUcbChild( int current, int color, std::mt19937_64 &mt );

// 各座標の統計処理
static void Statistic( game_info_t *game, int winner );

// UCT探索(1回の呼び出しにつき, 1回の探索)
static int UctSearch( game_info_t *game, int color, std::mt19937_64 &mt, int current, int &winner );

// ノード展開の閾値を取得
static int GetExpandThreshold( const game_info_t *game );


/**
 * @~english
 * @brief Get node's data.
 * @param[in] index Index for node.
 * @return Node's data.
 * @~japanese
 * @brief 指定したインデックスのノードのデータの取得
 * @param[in] index ノードのインデックス
 * @return 指定したインデックスのノードのデータ
 */
uct_node_t&
GetNode( const int index )
{
  return uct_node[index];
}


/**
 * @~english
 * @brief Get current root node's data.
 * @return Current root node's data.
 * @~japanese
 * @brief 現在のルートノードのデータの取得
 * @return 現在のルートノードのデータ
 */
uct_node_t&
GetRootNode( void )
{
  return uct_node[current_root];
}


/**
 * @~english
 * @brief Set pondering mode.
 * @param[in] flag Pondering mode.
 * @~japanese
 * @brief 予測読みの設定
 * @param[in] flag 予測読みのモード
 */
void
SetPonderingMode( const bool flag )
{
#if defined (RAY_WEB)
  (void)flag;
  pondering_mode = false;
#else
  pondering_mode = flag;
#endif
}


/**
 * @~english
 * @brief Set the number of search worker threads.
 * @param[in] new_threads The number of search worker threads.
 * @~japanese
 * @brief 使用するスレッド数の指定
 * @param[in] new_threads 使用するスレッド数
 */
void
SetThread( const int new_threads )
{
#if defined (RAY_WEB)
  (void)new_threads;
  threads = 1;
#else
  threads = new_threads;
#endif
}


/**
 * @~english
 * @brief Set reuse subtre mode.
 * @param[in] flag Reuse subtree mode.
 * @~japanese
 * @brief 探索木再利用の設定
 * @param[in] flag 探索木再利用のモード
 */
void
SetReuseSubtree( bool flag )
{
  reuse_subtree = flag;
}


/**
 * @~english
 * @brief Set parameters for search settings.
 * @~japanese
 * @brief 探索用パラメータの設定
 */
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

  SetTimeManagementParameter();
}


/**
 * @~english
 * @brief Initialization for tree search.
 * @~japanese
 * @brief 木探索の初期設定
 */
void
InitializeUctSearch( void )
{
  int i;

  // Progressive Wideningの初期化  
  pw[0] = 0;
  for (i = 1; i <= PURE_BOARD_MAX; i++) {  
    pw[i] = pw[i - 1] + static_cast<int>(40 * pow(PROGRESSIVE_WIDENING, i - 1));
    if (pw[i] > 10000000) break;
  }
  for (i = i + 1; i <= PURE_BOARD_MAX; i++) { 
    pw[i] = INT_MAX;
  }

  mutex_nodes = new std::mutex[uct_hash_size];

  // UCTのノードのメモリを確保
  uct_node = new uct_node_t[uct_hash_size];

  std::cerr << "Require " << uct_hash_size * sizeof(uct_node_t) / 1024 / 1024 << " Mbytes for Uct Node" << std::endl << std::endl;
  std::cerr << sizeof(uct_node_t) << std::endl;
  std::cerr << sizeof(child_node_t) * UCT_CHILD_MAX << std::endl;
  
  if (uct_node == NULL) {
    std::cerr << "Cannot allocate memory !!" << std::endl;
    std::cerr << "You must reduce tree size !!" << std::endl;
    exit(1);
  }

}


/**
 * @~english
 * @brief Initialization for search settings.
 * @~japanese
 * @brief 探索設定の初期化
 */
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
  std::random_device rand;
  for (int i = 0; i < THREAD_MAX; i++) {
    mt[i].seed(rand());
  }


  SetTimeManagementParameter();
  InitializeTimeSetting();

  pondered = false;
  pondering_stop = true;
}


/**
 * @~english
 * @brief Stop pondering.
 * @~japanese
 * @brief 予測読みの停止
 */
void
StopPondering( void )
{
  if (!pondering_mode) {
    return ;
  }

  if (ponder) {
    pondering_stop = true;
    for (int i = 0; i < threads; i++) {
      worker[i]->join();
      delete worker[i];
    }
    ponder = false;
    pondered = true;
    PrintPonderingCount(GetPoCount());
  }
}


/**
 * @~english
 * @brief Generate next move by UCT search.
 * @param[in] game Current board position data.
 * @param[in] color Player's color.
 * @param[in] lz_analysis_cs Update interval for lz-analyze.
 * @return Coordinate of next move.
 * @~japanese
 * @brief UCT探索による着手生成
 * @param[in] game 現在の局面情報
 * @param[in] color 手番の色
 * @param[in] lz_analysis_cs lz-analyzeコマンドの更新間隔
 * @return 次の着手の座標
 */
int
UctSearchGenmove( game_info_t *game, int color, int lz_analysis_cs )
{
  double best_wp;

  // 探索情報をクリア
  if (!pondered) {
    for (int i = 0; i < board_max; i++) {
      statistic[i].clear();
    }
    statistic_count = 0;
    std::fill_n(criticality_index, board_max, 0);
    for (int i = 0; i < board_max; i++) {
      criticality[i] = 0.0;
    }
  }

  ResetPoCount();

  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    owner[pos] = 50;
    owner_index[pos] = 5;
    candidates[pos] = true;
  }

  if (!reuse_subtree) {
    ClearUctHash();
  }
  
  // 探索開始時刻の記録
  StartTimer();
  
  // UCTの初期化
  current_root = ExpandRoot(game, color);

  // 前回から持ち込んだ探索回数を記録
  const int pre_simulated = uct_node[current_root].move_count;

  // 子ノードが1つ(パスのみ)ならPASSを返す
  if (uct_node[current_root].child_num <= 1) {
    return PASS;
  }

  // 探索回数の閾値を設定
  SetPoHalt(GetPoNum());

  // 自分の手番を設定
  my_color = color;

  // Dynamic Komiの算出(置碁のときのみ)
  DynamicKomi(game, &uct_node[current_root], color);

  // 探索時間とプレイアウト回数の予定値を出力
  PrintPlayoutLimits(GetTimeLimit(), GetPoHalt());

  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    t_arg[i].lz_analysis_cs = lz_analysis_cs;
  }

  const double mag[3] = { 1.0, 1.5, 2.0 };
  int mag_count = 0;

  // 着手が41手以降で, 
  // 時間延長を行う設定になっていて,
  // 探索時間延長をすべきときは
  // 探索回数を1.5倍, 2.0倍に増やす

  do {
    ExtendSearchTime(mag[mag_count]);
#if defined (RAY_WEB)
    ParallelUctSearch(&t_arg[0]);
#else
    for (int i = 0; i < threads; i++) {
      worker[i] = new std::thread(ParallelUctSearch, &t_arg[i]);
    }
    for (int i = 0; i < threads; i++) {
      worker[i]->join();
      delete worker[i];
    }
#endif
    mag_count++;
  } while (mag_count < 3 && ExtendTime(uct_node[current_root], game->moves));

  const int pos = SelectMove(game, uct_node[current_root], color, best_wp);
  
  // 探索にかかった時間を求める
  const double finish_time = CalculateElapsedTime();

  // 各地点の領地になる確率の出力
  PrintOwner(&uct_node[current_root], statistic, color, statistic_count.load(), owner);
  
  const int po_speed = static_cast<int>(CalculatePlayoutSpeed(finish_time, threads));

  // 最善応手列を出力
  PrintBestSequence(game, uct_node, current_root, color);
  // 探索の情報を出力(探索回数, 勝敗, 思考時間, 勝率, 探索速度)
  PrintPlayoutInformation(&uct_node[current_root], po_speed, finish_time, pre_simulated);
  // 次の探索でのプレイアウト回数の算出
  CalculateNextPlayouts(game, color, best_wp, finish_time, threads);

  return pos;
}


/**
 * @~english
 * @brief Ponder by UCT search.
 * @param[in] game Current board position data.
 * @param[in] color Player's color.
 * @param[in] lz_analysis_cs Update interval for lz-analyze.
 * @return Coordinate of next move.
 * @~japanese
 * @brief UCT探索による予測読み
 * @param[in] game 現在の局面情報
 * @param[in] color 手番の色
 * @param[in] lz_analysis_cs lz-analyzeコマンドの更新間隔
 * @return 次の着手の座標
 */
void
UctSearchPondering( game_info_t *game, int color, int lz_analysis_cs )
{
  if (!pondering_mode) {
    return;
  }

  // 探索情報をクリア
  for (int i = 0; i < board_max; i++) {
    statistic[i].clear();
  }
  statistic_count = 0;
  std::fill_n(criticality_index, board_max, 0);  
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;    
  }

  ResetPoCount();

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
    return;
  }

  ponder = true;
  pondering_stop = false;

  // Dynamic Komiの算出(置碁のときのみ)
  DynamicKomi(game, &uct_node[current_root], color);

  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    t_arg[i].lz_analysis_cs = lz_analysis_cs;
    worker[i] = new std::thread(ParallelUctSearchPondering, &t_arg[i]);
  }

  return;
}


/**
 * @~english
 * @brief Expand a root node.
 * @param[in] game Current board position data.
 * @param[in] color Player's color.
 * @return Index of a root node.
 * @~japanese
 * @brief ルートノードの展開
 * @param[in] game 現在の局面情報
 * @param[in] color 手番の色
 * @return ルートノードのインデックス
 */
static int
ExpandRoot( game_info_t *game, int color )
{
  const int moves = game->moves;
  const unsigned long long hash = game->move_hash;
  unsigned int index = FindSameHashIndex(hash, color, moves);
  int pm1 = PASS, pm2 = PASS;
  bool ladder[BOARD_MAX] = { false };

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
    std::vector<int> indexes;

    // 現局面の子ノード以外を削除する
    CorrectDescendentNodes(indexes, index);
    std::sort(indexes.begin(), indexes.end());
    ClearNotDescendentNodes(indexes);
    
    // 直前と2手前の着手を更新
    uct_node[index].previous_move1 = pm1;
    uct_node[index].previous_move2 = pm2;

    ReuseRootCandidateWithoutLadderMove(uct_node[index], ladder);

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
    InitializeNode(uct_node[index], pm1, pm2);

    child_node_t *uct_child = uct_node[index].child;
    int child_num = 0;

    // パスノードの展開
    InitializeCandidate(uct_child[PASS_INDEX], child_num, PASS, ladder[PASS]);
    
    // 候補手の展開
    if (moves == 1) {
      for (int i = 0; i < first_move_candidates; i++) {
        const int pos = first_move_candidate[i];
        // 探索候補かつ合法手であれば探索対象にする
        if (candidates[pos] && IsLegal(game, pos, color)) {
          InitializeCandidate(uct_child[child_num], child_num, pos, ladder[pos]);
        }
      }
    } else {
      for (int i = 0; i < pure_board_max; i++) {
        const int pos = onboard_pos[i];
        // 探索候補かつ合法手であれば探索対象にする
        if (candidates[pos] &&
            IsLegal(game, pos, color) &&
            IsMeaningfulSelfAtari(game, color, pos)) {
          InitializeCandidate(uct_child[child_num], child_num, pos, ladder[pos]);
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


/**
 * @~english
 * @brief Expand a node.
 * @param[in] game Current board position data.
 * @param[in] color Player's color.
 * @param[in] current Current index of MCTS node.
 * @return Index of a node.
 * @~japanese
 * @brief ノードの展開
 * @param[in] game 現在の局面情報
 * @param[in] color 手番の色
 * @param[in] current 現在のノードのインデックス
 * @return ノードのインデックス
 */
static int
ExpandNode( game_info_t *game, int color, int current )
{
  const int moves = game->moves;
  const unsigned long long hash = game->move_hash;
  unsigned int index = FindSameHashIndex(hash, color, moves);
  int pm1 = PASS, pm2 = PASS;
  
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
  InitializeNode(uct_node[index], pm1, pm2);

  child_node_t *uct_child = uct_node[index].child;
  int child_num = 0;

  // パスノードの展開
  InitializeCandidate(uct_child[PASS_INDEX], child_num, PASS, false);

  // 候補手の展開
  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    // 探索候補でなければ除外
    if (candidates[pos] &&
        IsLegal(game, pos, color) &&
        IsMeaningfulSelfAtari(game, color, pos)) {
      InitializeCandidate(uct_child[child_num], child_num, pos, false);
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
  const int sibling_num = uct_node[current].child_num;
  child_node_t *uct_sibling = uct_node[current].child;
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


/**
 * @~english
 * @brief Calculate all candidates' move score.
 * @param[in] game Board position data.
 * @param[in] color Player's color.
 * @param[in] index Node index.
 * @~japanese
 * @brief 着手のスコアの計算
 * @param[in] game 局面情報
 * @param[in] color 手番の色
 * @param[in] index ノードのインデックス
 */
static void
RatingNode( game_info_t *game, int color, int index )
{
  const int child_num = uct_node[index].child_num;
  const int moves = game->moves;
  int max_index;
  double score = 0.0, max_score, dynamic_parameter, total_score = 0.0;
  child_node_t *uct_child = uct_node[index].child;
  unsigned int tactical_features[BOARD_MAX * UCT_INDEX_MAX] = {0};
  int distance_index = 0;

  // パスのレーティング
  uct_child[PASS_INDEX].rate = CalculateMoveScoreWithBTFM(game, PASS, tactical_features, distance_index);
  // 直前の着手で発生した特徴の確認
  distance_index = CheckFeaturesForTree(game, color, tactical_features);
  // 直前の着手で石を2つ取られたか確認
  CheckRemove2StonesForTree(game, color, tactical_features);
  // 2手前で劫が発生していたら, 劫を解消するトリの確認
  if (game->ko_move == moves - 2) {
    CheckCaptureAfterKoForTree(game, color, tactical_features);
    CheckKoConnectionForTree(game, tactical_features);
  } else if (game->ko_move == moves - 3) {
    CheckKoRecaptureForTree(game, color, tactical_features);
  }

  max_index = 0;
  max_score = uct_child[0].rate;

  for (int i = 1; i < child_num; i++) {
    const int pos = uct_child[i].pos;
  
    // 自己アタリの確認
    CheckSelfAtariForTree(game, color, pos, tactical_features);
    // トリの確認
    CheckCaptureForTree(game, color, pos, tactical_features);
    // アタリの確認
    CheckAtariForTree(game, color, pos, tactical_features);

    // 自己アタリが無意味だったらスコアを0.0にする
    // 逃げられないシチョウならスコアを-1.0にする
    if (uct_child[i].ladder) {
      score = 0.0;
    } else {
      score = CalculateMoveScoreWithBTFM(game, pos, tactical_features, distance_index);
    }

    // その手のγを記録
    uct_child[i].rate = score;
    total_score += score;

    // 現在見ている箇所のOwnerとCriticalityの補正値を求める
    //dynamic_parameter = 1.0;
    dynamic_parameter = uct_owner[owner_index[pos]] * uct_criticality[criticality_index[pos]];

    // 最もγが大きい着手を記録する
    if (score * dynamic_parameter > max_score) {
      max_index = i;
      max_score = score * dynamic_parameter;
    }
  }

  const double inv_total = 1.0 / total_score;

  for (int i = 0; i < child_num; i++) {
    uct_child[i].rate *= inv_total;
  }
  
  // 最もγが大きい着手を探索できるようにする
  uct_child[max_index].pw = true;
}


/**
 * @~english
 * @brief Search worker.
 * @param[in] arg Arguments for a search worker thread.
 * @~japanese
 * @brief 探索ワーカ
 * @param[in] arg 探索ワーカスレッドの引数
 */
static void
ParallelUctSearch( thread_arg_t *arg )
{
  const thread_arg_t *targ = (thread_arg_t *)arg;
  const int color = targ->color;
  bool interruption = false, enough_size = true;
  bool use_analysis = targ->lz_analysis_cs > 0 ? true : false;
  int winner = 0, interval = CRITICALITY_INTERVAL;
  game_info_t *game = AllocateGame();
  ray_clock::time_point analysis_timer;

  // スレッドIDが0のスレッドだけ別の処理をする
  // 探索回数が閾値を超える, または探索が打ち切られたらループを抜ける
  if (targ->thread_id == 0) {
    analysis_timer = ray_clock::now();

    do {
      // 探索回数を1回増やす
      IncrementPoCount();
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      UctSearch(game, color, mt[targ->thread_id], current_root, winner);
      // 探索を打ち切るか確認
      interruption = CheckInterruption(uct_node[current_root]);
      //interruption = InterruptionCheck();
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
      // OwnerとCriticalityを計算する
      if (GetPoCount() > interval) {
        CalculateOwner(color);
        CalculateCriticality(color);
        interval += CRITICALITY_INTERVAL;
      }

      if (use_analysis &&
          static_cast<int>(100 * GetSpendTime(analysis_timer)) > targ->lz_analysis_cs) {
        analysis_timer = ray_clock::now();
        PrintLeelaZeroAnalyze(&uct_node[current_root]);
      }

      if (IsTimeOver()) break;
    } while (IsSearchContinue() && !interruption && enough_size);
  } else {
    do {
      // 探索回数を1回増やす
      IncrementPoCount();
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      UctSearch(game, color, mt[targ->thread_id], current_root, winner);
      // 探索を打ち切るか確認
      interruption = CheckInterruption(uct_node[current_root]);
      //interruption = InterruptionCheck();
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();

      if (IsTimeOver()) break;
    } while (IsSearchContinue() && !interruption && enough_size);
  }

  // メモリの解放
  FreeGame(game);
}


/**
 * @~english
 * @brief Pondering worker.
 * @param[in] arg Arguments for a search worker thread.
 * @~japanese
 * @brief 予測読みワーカ
 * @param[in] arg 予測読みワーカスレッドの引数
 */
static void
ParallelUctSearchPondering( thread_arg_t *arg )
{
  const thread_arg_t *targ = (thread_arg_t *)arg;
  const int color = targ->color;
  int winner = 0, interval = CRITICALITY_INTERVAL;
  bool enough_size = true;
  bool use_analysis = targ->lz_analysis_cs > 0 ? true : false;
  game_info_t *game = AllocateGame();
  ray_clock::time_point analysis_timer;

  // スレッドIDが0のスレッドだけ別の処理をする
  // 探索回数が閾値を超える, または探索が打ち切られたらループを抜ける
  if (targ->thread_id == 0) {
    analysis_timer = ray_clock::now();
    do {
      // 探索回数を1回増やす
      IncrementPoCount();
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      UctSearch(game, color, mt[targ->thread_id], current_root, winner);
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
      // OwnerとCriticalityを計算する
      if (GetPoCount() > interval) {
        CalculateOwner(color);
        CalculateCriticality(color);
        interval += CRITICALITY_INTERVAL;
      }

      if (use_analysis &&
          static_cast<int>(100 * GetSpendTime(analysis_timer)) > targ->lz_analysis_cs) {
        analysis_timer = ray_clock::now();
        PrintLeelaZeroAnalyze(&uct_node[current_root]);
      }

    } while (!pondering_stop && enough_size);
  } else {
    do {
      // 探索回数を1回増やす
      IncrementPoCount();
      // 盤面のコピー
      CopyGame(game, targ->game);
      // 1回プレイアウトする
      UctSearch(game, color, mt[targ->thread_id], current_root, winner);
      // ハッシュに余裕があるか確認
      enough_size = CheckRemainingHashSize();
    } while (!pondering_stop && enough_size);
  }

  // メモリの解放
  FreeGame(game);
}


/**
 * @~english
 * @brief Search by UCT algorithm.
 * @param[in] game Current board position data.
 * @param[in] color Player's color.
 * @param[in] mt Random number generator.
 * @param[in] current Index of current node.
 * @param[in] winner Search result data.
 * @return Monte-Carlo simulation's result.
 * @~japanese
 * @brief UCTアルゴリズムによる探索
 * @param[in] game 現在の局面情報
 * @param[in] color 手番の色
 * @param[in] mt 乱数生成器
 * @param[in] current 現在のノードのインデックス
 * @param[in] winner 探索結果
 * @return モンテカルロ・シミュレーションの結果
 */
static int 
UctSearch( game_info_t *game, int color, std::mt19937_64 &mt, int current, int &winner )
{
  int result = 0, next_index;
  double score;
  child_node_t *uct_child = uct_node[current].child;  

  // 現在見ているノードをロック
  mutex_nodes[current].lock();
  // UCB値最大の手を求める
  next_index = SelectMaxUcbChild(current, color, mt);
  // 選んだ手を着手
  PutStone(game, uct_child[next_index].pos, color);
  // 色を入れ替える
  color = GetOppositeColor(color);

  if ((uct_child[next_index].move_count + uct_child[next_index].virtual_loss) < GetExpandThreshold(game)) {
    AddVirtualLoss(uct_node[current], uct_child[next_index]);

    memcpy(game->seki, uct_node[current].seki, sizeof(bool) * BOARD_MAX);
    
    // 現在見ているノードのロックを解除
    mutex_nodes[current].unlock();

    // 終局まで対局のシミュレーション
    Simulation(game, color, mt);

    // 隅の曲がり四目の確認
    CheckBentFourInTheCorner(game);
    
    // コミを含めない盤面のスコアを求める
    score = static_cast<double>(CalculateScore(game));
    
    // コミを考慮した勝敗
    if (my_color == S_BLACK) {
      if (score - dynamic_komi[my_color] >= 0) {
        result = (color == S_BLACK ? 0 : 1);
        winner = S_BLACK;
      } else {
        result = (color == S_WHITE ? 0 : 1);
        winner = S_WHITE;
      }
    } else {
      if (score - dynamic_komi[my_color] > 0) {
        result = (color == S_BLACK ? 0 : 1);
        winner = S_BLACK;
      } else {
        result = (color == S_WHITE ? 0 : 1);
        winner = S_WHITE;
      }
    }
    // 統計情報の記録
    Statistic(game, winner);
  } else {
    // Virtual Lossを加算
    AddVirtualLoss(uct_node[current], uct_child[next_index]);
    // ノードの展開の確認
    if (uct_child[next_index].index == -1) {
      // ノードの展開中はロック
      mutex_expand.lock();
      // ノードの展開
      uct_child[next_index].index = ExpandNode(game, color, current);
      // ノード展開のロックの解除
      mutex_expand.unlock();
    }
    // 現在見ているノードのロックを解除
    mutex_nodes[current].unlock();
    // 手番を入れ替えて1手深く読む
    result = UctSearch(game, color, mt, uct_child[next_index].index, winner);
  }

  // 探索結果の反映
  UpdateResult(uct_node[current], uct_child[next_index], result);

  mutex_nodes[current].lock();
  UpdateOwnership(uct_node[current], game, GetOppositeColor(color));
  mutex_nodes[current].unlock();

  return 1 - result;
}


/**
 * @~english
 * @brief Comparator by move evaluation value.
 * @param[in] a Left-hand value.
 * @param[in] b Right-hand value.
 * @return Order judgment.
 * @~japanese
 * @brief 着手評価の大小比較
 * @param[in] a 左辺値
 * @param[in] b 右辺値
 * @return 並び順の判定
 */
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


/**
 * @~english
 * @brief Select next move.
 * @param[in] current MCTS node index.
 * @param[in] color Player's color.
 * @param[in] mt Random number generator.
 * @return Node index for next move.
 * @~japanese
 * @brief 次の着手の選択
 * @param[in] current MCTSノードインデックス
 * @param[in] color 手番の色
 * @param[in] mt 乱数生成器
 * @return 次の手に対応するノードのインデックス
 */
static int
SelectMaxUcbChild( int current, int color, std::mt19937_64 &mt )
{
  const int child_num = uct_node[current].child_num;
  const int sum = uct_node[current].move_count;
  child_node_t *uct_child = uct_node[current].child;
  rate_order_t order[PURE_BOARD_MAX + 1];  
  
  // 128回ごとにOwnerとCriticalityでソートし直す  
  if ((sum & 0x7f) == 0 && sum != 0) {
    int o_index[UCT_CHILD_MAX], c_index[UCT_CHILD_MAX];
    CalculateCriticalityIndex(&uct_node[current], statistic, color, c_index);
    CalculateOwnerIndex(&uct_node[current], statistic, color, o_index);
    for (int i = 0; i < child_num; i++) {
      const int pos = uct_child[i].pos;
      const double dynamic_parameter = (pos == PASS) ? 1.0 : uct_owner[o_index[i]] * uct_criticality[c_index[i]];

      order[i].rate = uct_child[i].rate * dynamic_parameter;
      order[i].index = i;
      uct_child[i].pw = false;
    }
    qsort(order, child_num, sizeof(rate_order_t), RateComp);

    // 子ノードの数と探索幅の最小値を取る
    const int width = ((uct_node[current].width > child_num) ? child_num : uct_node[current].width);

    // 探索候補の手を展開し直す
    for (int i = 0; i < width; i++) {
      uct_child[order[i].index].pw = true;
    }
  }

  // Progressive Wideningの閾値を超えたら, 
  // レートが最大の手を読む候補を1手追加
  if (sum > pw[uct_node[current].width]) {
    int max_index = -1;
    double max_rate = -10000.0;
    for (int i = 0; i < child_num; i++) {
      if (!uct_child[i].pw) {
        const int pos = uct_child[i].pos;
        const double dynamic_parameter = (pos == PASS) ? 1.0 : uct_owner[owner_index[pos]] * uct_criticality[criticality_index[pos]];
        const double rate = uct_child[i].rate * dynamic_parameter;

        if (rate > max_rate) {
          max_index = i;
          max_rate = rate;
        }
      }
    }
    if (max_index != -1) {
      uct_child[max_index].pw = true;
    }
    uct_node[current].width++;  
  }

  return SelectBestChildIndexByUCB1(uct_node[current], mt);
}


/**
 * @~english
 * @brief Update statistic information.
 * @param[in] game Board position data.
 * @param[in] winner Winner's color.
 * @~japanese
 * @brief 統計情報の更新
 * @param[in] game 局面情報
 * @param[in] winner 勝った手番の色
 */
static void
Statistic( game_info_t *game, int winner )
{
  const char *board = game->board;

  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    int color = board[pos];

    if (color == S_EMPTY) {
      color = territory[Pat3(game->pat, pos)];
    }

    std::atomic_fetch_add(&statistic[pos].colors[color], 1);
    if (color == winner) {
      std::atomic_fetch_add(&statistic[pos].colors[static_cast<int>(StatisticInformation::Win)], 1);
    }
  }
  std::atomic_fetch_add(&statistic_count, 1);
}


/**
 * @~english
 * @brief Calculate criticality feature index.
 * @param[in] node UCT node.
 * @param[in] node_statistic Statistic information for UCT node.
 * @param[in] color Player's color.
 * @param[in] index Criticality feature index.
 * @~japanese
 * @brief Criticalityの特徴インデックスの計算
 * @param[in] node UCTノード
 * @param[in] node_statistic UCTノードの統計情報
 * @param[in] color 手番の色
 * @param[in] index Criticalityの特徴インデックス
 */
static void
CalculateCriticalityIndex( uct_node_t *node, statistic_t *node_statistic, int color, int *index )
{
  const int other = GetOppositeColor(color);
  const double inv_count = (statistic_count.load() > 0) ? 1.0 / statistic_count.load() : 1.0;
  const int child_num = node->child_num;
  const double win = static_cast<double>(node->win) / node->move_count;
  const double lose = 1.0 - win;
  double tmp;

  index[0] = 0;

  for (int i = 1; i < child_num; i++) {
    const int pos = node->child[i].pos;

    tmp = node_statistic[pos].colors[static_cast<int>(StatisticInformation::Win)] * inv_count
      - ((node_statistic[pos].colors[color] * inv_count) * win
         + (node_statistic[pos].colors[other] * inv_count) * lose);
    if (tmp < 0) tmp = 0;
    index[i] = static_cast<int>(tmp * CRITICALITY_TERM);
    if (index[i] > criticality_max - 1) index[i] = criticality_max - 1;
  }
}


/**
 * @~english
 * @brief Calculate criticality.
 * @param[in] color Player's color.
 * @~japanese
 * @brief Criticalityの計算
 * @param[in] color 手番の色
 */
static void
CalculateCriticality( int color )
{
  const int other = GetOppositeColor(color);
  const double inv_count = (statistic_count.load() > 0) ? 1.0 / statistic_count.load() : 1.0;
  const double win = static_cast<double>(uct_node[current_root].win) / uct_node[current_root].move_count;
  const double lose = 1.0 - win;
  double tmp;

  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];

    tmp = statistic[pos].colors[static_cast<int>(StatisticInformation::Win)] * inv_count
      - ((statistic[pos].colors[color] * inv_count) * win
         + (statistic[pos].colors[other] * inv_count) * lose);

    criticality[pos] = tmp;
    if (tmp < 0) tmp = 0;
    criticality_index[pos] = static_cast<int>(tmp * CRITICALITY_TERM);
    if (criticality_index[pos] > criticality_max - 1) criticality_index[pos] = criticality_max - 1;
  }
}


/**
 * @~english
 * @brief Calculate ownership feature index.
 * @param[in] node UCT node.
 * @param[in] node_statistic Statistic information for UCT node.
 * @param[in] color Player's color
 * @param[in] index Ownership feature index.
 * @~japanese
 * @brief Ownershipの特徴インデックスの計算
 * @param[in] node UCTノード
 * @param[in] node_statistic UCTノードの統計情報
 * @param[in] color 手番の色
 * @param[in] index Ownerの特徴インデックス
 */
static void
CalculateOwnerIndex( uct_node_t *node, statistic_t *node_statistic, int color, int *index )
{
  const double inv_count = (statistic_count.load() > 0) ? 1.0 / statistic_count.load() : 1.0;
  const int child_num = node->child_num;

  index[0] = 0;

  for (int i = 1; i < child_num; i++){
    const int pos = node->child[i].pos;
    index[i] = static_cast<int>(static_cast<double>(node_statistic[pos].colors[color]) * 10.0 * inv_count + 0.5);
    if (index[i] > OWNER_MAX - 1) index[i] = OWNER_MAX - 1;
    if (index[i] < 0)             index[i] = 0;
  }
}


/**
 * @~english
 * @brief Calculate ownership feature index.
 * @param[in] color Player's color.
 * @~japanese
 * @brief Ownershipの特徴インデックスの計算
 * @param[in] color 手番の色
 */
static void
CalculateOwner( int color )
{
  const double inv_count = (statistic_count.load() > 0) ? 1.0 / statistic_count.load() : 1.0;

  for (int i = 0; i < pure_board_max; i++){
    const int pos = onboard_pos[i];
    owner_index[pos] = static_cast<int>(static_cast<double>(statistic[pos].colors[color]) * 10.0 * inv_count + 0.5);
    if (owner_index[pos] > OWNER_MAX - 1) owner_index[pos] = OWNER_MAX - 1;
    if (owner_index[pos] < 0)             owner_index[pos] = 0;
  }
}


/**
 * @~english
 * @brief Analyze current position by UCT algorithm.
 * @param[in] game Current board position data.
 * @param[in] color Player's color.
 * @return Current black player's score.
 * @~japanese
 * @brief UCTアルゴリズムによる局面解析
 * @param[in] game 現在の局面情報
 * @param[in] color 手番の色
 * @return 黒番の地
 */
int
UctAnalyze( game_info_t *game, int color )
{
  std::thread *worker[THREAD_MAX];

  // 探索情報をクリア
  for (int i = 0; i < board_max; i++) {
    statistic[i].clear();
  }
  statistic_count = 0;
  std::fill_n(criticality_index, board_max, 0);  
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;
  }
  ResetPoCount();

  ClearUctHash();

  current_root = ExpandRoot(game, color);

  SetPoHalt(10000);

  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    t_arg[i].lz_analysis_cs = -1;
#if defined (RAY_WEB)
    ParallelUctSearch(&t_arg[i]);
#else
    worker[i] = new std::thread(ParallelUctSearch, &t_arg[i]);
#endif
  }

#if !defined (RAY_WEB)
  for (int i = 0; i < threads; i++) {
    worker[i]->join();
    delete worker[i];
  }
#endif

  int black = 0, white = 0;

  const int inv_count = 1.0 / statistic_count.load();
  for (int y = board_start; y <= board_end; y++) {
    for (int x = board_start; x <= board_end; x++) {
      const int pos = POS(x, y);
      const double ownership_value = static_cast<double>(statistic[pos].colors[S_BLACK]) * inv_count;
      if (ownership_value > 0.5) {
        black++;
      } else {
        white++;
      }
    }
  }

  PrintOwner(&uct_node[current_root], statistic, color, statistic_count.load(), owner);

  return black - white;
}


/**
 * @~english
 * @brief Copy ownership values.
 * @param[out] dest Copy destination.
 * @~japanese
 * @brief Ownerの値のコピー
 * @param[out] dest コピー先
 */
void
OwnerCopy( int *dest )
{
  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    dest[pos] = static_cast<int>(static_cast<double>(statistic[pos].colors[my_color]) / uct_node[current_root].move_count * 100);
  }
}


/**
 * @~english
 * @brief Copy criticality value.
 * @param[out] dest Copy destination.
 * @~japanese
 * @brief Criticalityの値のコピー
 * @param[out] dest コピー先
 */
void
CopyCriticality( double *dest )
{
  for (int i = 0; i < pure_board_max; i++) {
    const int pos = onboard_pos[i];
    dest[pos] = criticality[pos];
  }
}


/**
 * @~english
 * @brief Copy statistic information of Monte-Carlo simulation.
 * @param[out] dest Copy destination.
 * @~japanese
 * @brief 統計情報のコピー
 * @param[out] dest コピー先
 */
void
CopyStatistic( statistic_t *dest )
{
  for (int i = 0; i < board_max; i++) {
    dest[i] = statistic[i];
  }
}


/**
 * @~english
 * @brief Generate next move by UCT search for KGS clean up mode.
 * @param[in] game Current board position data.
 * @param[in] color Player's color.
 * @return Coordinate of next move.
 * @~japanese
 * @brief UCT探索による着手生成 (KGSの終局処理用)
 * @param[in] game 現在の局面情報
 * @param[in] color 手番の色
 * @return 次の着手の座標
 */
int
UctSearchGenmoveCleanUp( game_info_t *game, int color )
{
  int pos;
  double wp;
  std::thread *worker[THREAD_MAX];

  for (int i = 0; i < board_max; i++) {
    statistic[i].clear();
  }
  statistic_count = 0;
  std::fill_n(criticality_index, board_max, 0); 
  for (int i = 0; i < board_max; i++) {
    criticality[i] = 0.0;
  }

  StartTimer();

  ResetPoCount();

  current_root = ExpandRoot(game, color);

  if (uct_node[current_root].child_num <= 1) {
    return PASS;
  }

  for (int i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    owner[pos] = 50.0;
  }

  SetPoHalt(GetPoNum());

  DynamicKomi(game, &uct_node[current_root], color);

  for (int i = 0; i < threads; i++) {
    t_arg[i].thread_id = i;
    t_arg[i].game = game;
    t_arg[i].color = color;
    t_arg[i].lz_analysis_cs = -1;
#if defined (RAY_WEB)
    ParallelUctSearch(&t_arg[i]);
#else
    worker[i] = new std::thread(ParallelUctSearch, &t_arg[i]);
#endif
  }

#if !defined (RAY_WEB)
  for (int i = 0; i < threads; i++) {
    worker[i]->join();
    delete worker[i];
  }
#endif

  child_node_t *uct_child = uct_node[current_root].child;

  const int select_index = SelectMaxVisitChild(uct_node[current_root]);

  const double finish_time = CalculateElapsedTime();

  wp = (double)uct_node[current_root].win / uct_node[current_root].move_count;

  const int po_speed = static_cast<int>(CalculatePlayoutSpeed(finish_time, threads));

  PrintPlayoutInformation(&uct_node[current_root], po_speed, finish_time, 0);
  PrintOwner(&uct_node[current_root], statistic, color, statistic_count.load(), owner);

  PrintBestSequence(game, uct_node, current_root, color);

  CalculateNextPlayouts(game, color, wp, finish_time, threads);

  int count = 0;

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
  
  if (static_cast<double>(uct_child[select_index].win) / uct_child[select_index].move_count < RESIGN_THRESHOLD) {
    return PASS;
  } else {
    return pos;
  }
}


/**
 * @~english
 * @brief Correct descendent node indices.
 * @param[in, out] indexes Descendent node indices.
 * @param[in] index Node index.
 * @~japanese
 * @brief 子孫ノードのインデックスの収集
 * @param[in, out] indexes 子孫ノードのインデックス
 * @param[in] index 現在のインデックス
 */
static void
CorrectDescendentNodes( std::vector<int> &indexes, int index )
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


/**
 * @~english
 * @brief Get visits threshold for node expansion.
 * @param[in] game Board position data.
 * @return Visit threshold for node expansion.
 * @~japanese
 * @brief ノードを展開する探索回数の閾値の取得
 * @param[in] game 局面情報
 * @return ノードを展開する探索回数の閾値
 */
static int
GetExpandThreshold( const game_info_t *game )
{
  if (game->moves > 2 &&
      game->record[game->moves - 1].pos == PASS &&
      game->record[game->moves - 2].pos == PASS) {
    return INT_MAX - 1;
  } else {
    return expand_threshold;
  }
}
