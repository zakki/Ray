#include <iostream>

#include "Message.h"
#include "Ladder.h"
#include "Point.h"

using namespace std;

#define ALIVE true
#define DEAD  false

// IsLadderCaptured関数用
game_info_t search_game[100];

void
LadderExtension( const game_info_t *game, int color, bool *ladder_pos )
{
  const string_t *string = game->string;
  int i, ladder = PASS;
  game_info_t *shicho_game = AllocateGame();
  bool checked[BOARD_MAX] = { false };  
  int neighbor;
  bool flag;

  for (i = 0; i < MAX_STRING; i++) {
    if (!string[i].flag ||
	string[i].color != color) {
      continue;
    }
    // アタリから逃げる着手箇所
    ladder = string[i].lib[0];

    flag = false;

    // アタリを逃げる手で未探索のものを確認
    if (!checked[ladder] && string[i].libs == 1) {
      // 隣接する敵連を取って助かるかを確認
      neighbor = string[i].neighbor[0];
      while (neighbor != NEIGHBOR_END) {
      	if (string[neighbor].libs == 1) {
      	  CopyGame(shicho_game, game);
      	  PutStone(shicho_game, string[neighbor].lib[0], color);
      	  if (IsLadderCaptured(0, shicho_game, string[i].origin, FLIP_COLOR(color)) == DEAD) {
      	    if (string[i].size >= 2) { 
      	      ladder_pos[string[neighbor].lib[0]] = true;
      	    }
      	  } else {
      	    flag = true;
      	    break;
      	  }
      	}
	      neighbor = string[i].neighbor[neighbor];
      }

      // 取って助からない時は逃げてみる
      if (!flag) {
      	if (IsLegal(game, ladder, color)) {
      	  CopyGame(shicho_game, game);
      	  PutStone(shicho_game, ladder, color);
      	  if (string[i].size >= 2 && 
      	      IsLadderCaptured(0, shicho_game, ladder, FLIP_COLOR(color)) == DEAD){
      	      ladder_pos[ladder] = true;
      	  }
      	}
      }
      checked[ladder] = true;
    }
  }
  
  FreeGame(shicho_game);
}

bool
IsLadderCaptured( int depth, const game_info_t *game, int ren_xy, int turn_color )
{
  const string_t *string = game->string;
  int str = game->string_id[ren_xy];
  int escape_color, capture_color;
  int escape_xy, capture_xy;
  const char *board = game->board;
  int neighbor;

  if (depth >= 100) {
    return ALIVE;
  }

  if (board[ren_xy] == S_EMPTY) {
    return DEAD;
  } else if (string[str].libs >= 3) {
    return ALIVE;
  }

  escape_color = board[ren_xy];
  capture_color = FLIP_COLOR(escape_color);

  if (turn_color == escape_color) {
    // 周囲の敵連が取れるか確認し,
    // 取れるなら取って探索を続ける
    neighbor = string[str].neighbor[0];
    while (neighbor != NEIGHBOR_END) {
      if (string[neighbor].libs == 1) {
      	if (IsLegal(game, string[neighbor].lib[0], escape_color)) {
      	  CopyGame(&search_game[depth], game);
      	  PutStone(&search_game[depth], string[neighbor].lib[0], escape_color);
      	  if (IsLadderCaptured(depth + 1, &search_game[depth], ren_xy, FLIP_COLOR(turn_color)) == ALIVE) {
      	    return ALIVE;
      	  }
      	}
      }
      neighbor = string[str].neighbor[neighbor];
    }

    // 逃げる手を打ってみて探索を続ける
    escape_xy = string[str].lib[0];
    while (escape_xy != LIBERTY_END) {
      if (IsLegal(game, escape_xy, escape_color)) {
      	CopyGame(&search_game[depth], game);
      	PutStone(&search_game[depth], escape_xy, escape_color);
      	if (IsLadderCaptured(depth + 1, &search_game[depth], ren_xy, FLIP_COLOR(turn_color)) == ALIVE) {
      	  return ALIVE;
      	}
      }
      escape_xy = string[str].lib[escape_xy];
    }
    return DEAD;
  } else {
    if (string[str].libs == 1) return DEAD;
    // 追いかける側なのでアタリにする手を打ってみる
    capture_xy = string[str].lib[0];
    while (capture_xy != LIBERTY_END) {
      if (IsLegal(game, capture_xy, capture_color)) {
      	CopyGame(&search_game[depth], game);
      	PutStone(&search_game[depth], capture_xy, capture_color);
      	if (IsLadderCaptured(depth + 1, &search_game[depth], ren_xy, FLIP_COLOR(turn_color)) == DEAD) {
      	  return DEAD;
      	}
      }
      capture_xy = string[str].lib[capture_xy];
    }
  }

  return ALIVE;
}

//////////////////////////////////////////
//  助からないシチョウを逃げる手か判定  //
//////////////////////////////////////////
bool
CheckLadderExtension( const game_info_t *game, int color, int pos )
{
  const char *board = game->board;
  const string_t *string = game->string;
  const int *string_id = game->string_id;
  int ladder = PASS;
  game_info_t *shicho_game = AllocateGame();
  bool flag = false;
  int id;

  if (board[pos] != color){
    FreeGame(shicho_game);
    return false;
  }

  id = string_id[pos];

  ladder = string[id].lib[0];

  if (string[id].libs == 1 && IsLegal(game, ladder, color)){
    CopyGame(shicho_game, game);
    PutStone(shicho_game, ladder, color);
    if (IsLadderCaptured(0, shicho_game, ladder, FLIP_COLOR(color)) == DEAD) {
      flag = true;
    } else {
      flag = false;
    }
  }

  FreeGame(shicho_game);

  return flag;
}
