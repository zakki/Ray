#include <cstring>
#include <random>

#include "GoBoard.h"
#include "Message.h"
#include "Point.h"
#include "Rating.h"
#include "Simulation.h"

using namespace std;

////////////////////////////////
//  終局までシミュレーション  //
////////////////////////////////
void
Simulation( game_info_t *game, int starting_color, std::mt19937_64 *mt )
{
  int color = starting_color;
  int pos = -1;
  int length;
  int pass_count;

  // シミュレーション打ち切り手数を設定
  length = MAX_MOVES - game->moves;
  if (length < 0) {
    return;
  }

  // レートの初期化  
  game->sum_rate[0] = game->sum_rate[1] = 0;
  memset(game->sum_rate_row, 0, sizeof(long long) * 2 * BOARD_SIZE);  
  memset(game->rate, 0, sizeof(long long) * 2 * BOARD_MAX);           

  pass_count = (game->record[game->moves - 1].pos == PASS && game->moves > 1);

  // 黒番のレートの計算
  Rating(game, S_BLACK, &game->sum_rate[0], game->sum_rate_row[0], game->rate[0]);
  // 白番のレートの計算
  Rating(game, S_WHITE, &game->sum_rate[1], game->sum_rate_row[1], game->rate[1]);

  // 終局まで対局をシミュレート
  while (length-- && pass_count < 2) {
    // 着手を生成する
    pos = RatingMove(game, color, mt);
    // 石を置く
    PoPutStone(game, pos, color);
    // パスの確認
    pass_count = (pos == PASS) ? (pass_count + 1) : 0;
    // 手番の入れ替え
    color = FLIP_COLOR(color);
  }
}
