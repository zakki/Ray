#include "GoBoard.h"
#include "Message.h"
#include "Point.h"
#include "Pattern.h"
#include "Semeai.h"
#include "UctRating.h"

// IsCapturableAtari関数用
game_info_t capturable_game;
// CheckOiotoshi関数用
game_info_t oiotoshi_game;
// CheckLibertyState関数用
game_info_t liberty_game;
// IsSelfAtariCapture関数用
game_info_t capture_game;

/////////////////////////
//  1手で取れるか確認  //
/////////////////////////
bool
IsCapturableAtari( game_info_t *game, int pos, int color, int opponent_pos )
{
  string_t *string;
  int *string_id;
  int other = FLIP_COLOR(color);
  int neighbor;
  int id;
  int libs;

  if (!IsLegal(game, pos, color)) {
    return false;
  }

  // 局面をコピー
  CopyGame(&capturable_game, game);
  // とりあえず石を置く
  PutStone(&capturable_game, pos, color);

  string = capturable_game.string;
  string_id = capturable_game.string_id;
  id = string_id[opponent_pos];

  // 周囲に取り返せる石があれば安全
  neighbor = string[id].neighbor[0];
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      return false;
    }
    neighbor = string[id].neighbor[neighbor];
  }

  if (!IsLegal(&capturable_game, string[string_id[opponent_pos]].lib[0], other)) {
    return true;
  }
  // 逃げるつもりでダメに打つ
  PutStone(&capturable_game, string[string_id[opponent_pos]].lib[0], other);

  libs = string[string_id[opponent_pos]].libs;

  // 逃げても呼吸点が1つなら捕獲可能と判定
  if (libs == 1) {
    return true;
  } else {
    return false;
  }
}

////////////////////////////////
//  オイオトシかどうかを確認  //
////////////////////////////////
// 返り値がintとboolの違いだけでIsCapturableAtari関数と同じ
int
CheckOiotoshi( game_info_t *game, int pos, int color, int opponent_pos )
{
  string_t *string;
  int *string_id;
  int other = FLIP_COLOR(color);
  int neighbor;
  int id, num = -1;

  if (!IsLegal(game, pos, color)) {
    return -1;
  }

  CopyGame(&oiotoshi_game, game);
  PutStone(&oiotoshi_game, pos, color);
  string = oiotoshi_game.string;
  string_id = oiotoshi_game.string_id;
  id = string_id[opponent_pos];

  neighbor = string[id].neighbor[0];
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      return -1;
    }
    neighbor = string[id].neighbor[neighbor];
  }

  if (!IsLegal(&oiotoshi_game, string[string_id[opponent_pos]].lib[0], other)) {
    return -1;
  }
  PutStone(&oiotoshi_game, string[string_id[opponent_pos]].lib[0], other);

  if (string[string_id[opponent_pos]].libs == 1) {
    num = string[string_id[opponent_pos]].size;
  }

  return num;
}

//////////////////////////////////////////////
//  石をすぐに捕獲できそうな候補手を求める  //
//////////////////////////////////////////////
int
CapturableCandidate( game_info_t *game, int id )
{
  string_t *string = game->string;
  int neighbor = string[id].neighbor[0];
  bool flag = false;
  int capturable_pos = -1;

  // 隣接する呼吸点が1つの敵連が1つだけの時, 候補を返す
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      if (string[neighbor].size >= 2) {
	return -1;
      } else {
	if (flag) {
	  return -1;
	}
	capturable_pos = string[neighbor].lib[0];
	flag = true;
      }
    }
    neighbor = string[id].neighbor[neighbor];
  }

  return capturable_pos;
}

////////////////////////////////////
//  すぐに捕まる手かどうかを判定  //
////////////////////////////////////
bool
IsDeadlyExtension( game_info_t *game, int color, int id )
{
  game_info_t search_game;
  int other = FLIP_COLOR(color);
  int pos = game->string[id].lib[0];
  bool flag = false;

  if (nb4_empty[Pat3(game->pat, pos)] == 0 &&
      IsSuicide(game, game->string, other, pos)) {
    return true;
  }

  CopyGame(&search_game, game);
  PutStone(&search_game, pos, other);

  if (search_game.string[search_game.string_id[pos]].libs == 1) {
    flag = true;
  }

  return flag;
}

////////////////////////////////////
//  隣接する敵連が取れるかを判定  //
////////////////////////////////////
bool
IsCapturableNeighborNone(game_info_t *game, int id)
{
  string_t *string = game->string;
  int neighbor = string[id].neighbor[0];

  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      return false;
    }
    neighbor = string[id].neighbor[neighbor];
  }

  return true;
}

/////////////////////////////////
//  自己アタリになるトリか判定  //
/////////////////////////////////
bool
IsSelfAtariCapture( game_info_t *game, int pos, int color, int id )
{
  string_t *string;
  int string_pos = game->string[id].origin;
  int *string_id;

  if (!IsLegal(game, pos, color)) {
    return false;
  }

  CopyGame(&capture_game, game);
  PutStone(&capture_game, pos, color);

  string = capture_game.string;
  string_id = capture_game.string_id;

  if (string[string_id[string_pos]].libs == 1) {
    return true;
  } else {
    return false;
  }
}

///////////////////////////////////////////////
//  1手で取れるかを判定(シミュレーション用)  //
///////////////////////////////////////////////
int
CheckLibertyState( game_info_t *game, int pos, int color, int id )
{
  string_t *string;
  int string_pos = game->string[id].origin;
  int *string_id;
  int libs = game->string[id].libs;
  int new_libs;

  if (!IsLegal(game, pos, color)) {
    return L_DECREASE;
  }

  CopyGame(&liberty_game, game);
  PutStone(&liberty_game, pos, color);

  string = liberty_game.string;
  string_id = liberty_game.string_id;

  new_libs = string[string_id[string_pos]].libs;

  if (new_libs > libs + 1) {
    return L_INCREASE;
  } else if (new_libs > libs) {
    return L_EVEN;
  } else {
    return L_DECREASE;
  }
}

///////////////////////////////////////////////
//  1Žè‚ÅŽæ‚ê‚é‚©‚ð”»’è(ƒVƒ~ƒ…ƒŒ[ƒVƒ‡ƒ“—p)  //
///////////////////////////////////////////////
bool
IsCapturableAtariForSimulation( game_info_t *game, int pos, int color, int id )
{
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  int lib;
  bool neighbor = false;
  int index_distance;
  int pat3;
  int empty;
  int connect_libs = 0;
  int tmp_id;

  lib = string[id].lib[0];

  if (lib == pos) {
    lib = string[id].lib[lib];
  }

  index_distance = lib - pos;

  // 詰める方とは逆のダメの周囲の空点数を調べる
  pat3 = Pat3(game->pat, lib);
  empty = nb4_empty[pat3];

  // 逆のダメの周囲の空点数が3なら取れないのでfalse
  if (empty == 3) {
    return false;
  }

  if (index_distance ==           1) neighbor = true;
  if (index_distance ==          -1) neighbor = true;
  if (index_distance ==  board_size) neighbor = true;
  if (index_distance == -board_size) neighbor = true;

  // ダメが隣り合っている時と
  // ダメが離れている時の分岐
  if (( neighbor && empty >= 3) ||
      (!neighbor && empty >= 2)) {
    return false;
  }

  // 隣接する連がlib以外に持つ呼吸点の合計数が
  // 2以上なら無条件で1手で取れるアタリではないのでfalse
  
  // 上の確認
  if (board[NORTH(lib)] == other && 
      string_id[NORTH(lib)] != id) {
    tmp_id = string_id[NORTH(lib)];
    if (string[tmp_id].libs > 2) {
      return false;
    } else {
      connect_libs += string[tmp_id].libs - 1;
    }
  } 

  // 左の確認
  if (board[WEST(lib)] == other && 
      string_id[WEST(lib)] != id) {
    tmp_id = string_id[WEST(lib)];
    if (string[tmp_id].libs > 2) {
      return false;
    } else {
      connect_libs += string[tmp_id].libs - 1;
    }
  }

  // 右の確認
  if (board[EAST(lib)] == other && 
      string_id[EAST(lib)] != id) {
    tmp_id = string_id[EAST(lib)];
    if (string[tmp_id].libs > 2) {
      return false;
    } else {
      connect_libs += string[tmp_id].libs - 1;
    }
  }

  // 下の確認
  if (board[SOUTH(lib)] == other && 
      string_id[SOUTH(lib)] != id) {
    tmp_id = string_id[SOUTH(lib)];
    if (string[tmp_id].libs > 2) {
      return false;
    } else {
      connect_libs += string[tmp_id].libs - 1;
    }
  }

  // ダメに打っても増える呼吸点数が1以下なら
  // 1手で取れるアタリ
  if (( neighbor && connect_libs < 2) ||
      (!neighbor && connect_libs < 1)) {
    return true;
  } else {
    return false;
  }
}

bool
IsSelfAtariCaptureForSimulation( game_info_t *game, int pos, int color, int lib )
{
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  int id;
  int size = 0;

  if (lib != pos || 
      nb4_empty[Pat3(game->pat, pos)] != 0) {
    return false;
  }

  if (board[NORTH(pos)] == color) {
    id = string_id[NORTH(pos)];
    if (string[id].libs > 1) {
      return false;
    }
  } else if (board[NORTH(pos)] == other) {
    id = string_id[NORTH(pos)];
    size += string[id].size;
    if (size > 1) {
      return false;
    }
  }

  if (board[WEST(pos)] == color) {
    id = string_id[WEST(pos)];
    if (string[id].libs > 1) {
      return false;
    }
  } else if (board[WEST(pos)] == other) {
    id = string_id[WEST(pos)];
    size += string[id].size;
    if (size > 1) {
      return false;
    }
  }

  if (board[EAST(pos)] == color) {
    id = string_id[EAST(pos)];
    if (string[id].libs > 1) {
      return false;
    }
  } else if (board[EAST(pos)] == other) {
    id = string_id[EAST(pos)];
    size += string[id].size;
    if (size > 1) {
      return false;
    }
  }

  if (board[SOUTH(pos)] == color) {
    id = string_id[SOUTH(pos)];
    if (string[id].libs > 1) {
      return false;
    }
  } else if (board[SOUTH(pos)] == other) {
    id = string_id[SOUTH(pos)];
    size += string[id].size;
    if (size > 1) {
      return false;
    }
  }

  return true;
}
