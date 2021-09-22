#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <sstream>

#include "Message.h"
#include "Point.h"
#include "UctSearch.h"

using namespace std;


bool debug_message = true;


////////////////////////////////////
//  エラーメッセージの出力の設定  //
////////////////////////////////////
void
SetDebugMessageMode( const bool flag )
{
  debug_message = flag;
}

bool
IsDebugMessageMode()
{
  return debug_message;
}

//////////////////
//  盤面の表示  //
//////////////////
void
PrintBoard( const game_info_t *game )
{
  const char stone[S_MAX] = { '+', 'B', 'W', '#' };

  if (!debug_message) return ;

  cerr << "Prisoner(Black) : " << game->prisoner[S_BLACK] << endl;
  cerr << "Prisoner(White) : " << game->prisoner[S_WHITE] << endl;
  cerr << "Move : " << game->moves << endl;

  cerr << "    ";
  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    cerr << " " << gogui_x[i];
  }
  cerr << endl;

  cerr << "   +";
  for (int i = 0; i < pure_board_size * 2 + 1; i++) {
    cerr << "-";
  }
  cerr << "+" << endl;

  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    cerr << setw(2) << (pure_board_size + 1 - i) << ":|";
    for (int x = board_start; x <= board_end; x++) {
      cerr << " " << stone[(int)game->board[POS(x, y)]];
    }
    cerr << " |" << endl;
  }

  cerr << "   +";
  for (int i = 1; i <= pure_board_size * 2 + 1; i++) {
    cerr << "-";
  }
  cerr << "+" << endl;
}


/////////////////////////////////
//  連の情報の表示              //
//    呼吸点の数, 座標          //
//    連を構成する石の数, 座標  //
/////////////////////////////////
void
PrintString( const game_info_t *game )
{
  const string_t *string = game->string;
  int pos, neighbor;

  if (!debug_message) return ;

  cerr << "  :: :: String :: ::" << endl;

  for (int i = 0; i < MAX_STRING; i++){
    if (string[i].flag) {
      if (game->board[string[i].origin] == S_BLACK) {
        cerr << "Black String   ";
      } else {
        cerr << "White String   ";
      }
      cerr << "ID : " << i << " (libs : " << string[i].libs << ", size : " << string[i].size << ")" << endl;
      pos = string[i].lib[0];

      cerr << "  Liberty : " << endl;
      cerr << "  ";
      while (pos != STRING_END) {
        cerr << GOGUI_X(pos) << GOGUI_Y(pos) << " ";
        pos = string[i].lib[pos];
      }
      cerr << endl;

      pos = string[i].origin;
      cerr << "  Stone : " << endl;
      cerr << "    ";
      while (pos != STRING_END) {
        cerr << GOGUI_X(pos) << GOGUI_Y(pos) << " ";
        pos = game->string_next[pos];
        if (pos == game->string_next[pos]) getchar();
      }
      cerr << endl;

      neighbor = string[i].neighbor[0];
      if (neighbor == 0) getchar();
      cerr << "  Neighbor : " << endl;
      cerr << "    ";
      while (neighbor < NEIGHBOR_END) {
        cerr << neighbor << " ";
        neighbor = string[i].neighbor[neighbor];
      }
      cerr << endl;
    }
  }

  cerr << endl;
}


//////////////////////////
//  各座標の連IDの表示  //
//////////////////////////
void
PrintStringID( const game_info_t *game )
{
  if (!debug_message) return ;

  cerr << "    ";
  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    cerr << "   " << gogui_x[i];
  }
  cerr << endl;

  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    cerr << setw(3) << (pure_board_size + 1 - i) << ":";
    for (int x = board_start; x <= board_end; x++) {
      const int pos = x + y * board_size;
      if (game->string[game->string_id[pos]].flag) {
        cerr << " " << setw(3) << game->string_id[pos];
      } else {
        cerr << "   -";
      }
    }
    cerr << endl;
  }
  cerr << endl;
}


///////////////////////////////////////
//  連リストの繋がりを表示(Debug用)  //
///////////////////////////////////////
void
PrintStringNext( const game_info_t *game )
{
  if (!debug_message) return ;

  cerr << "    ";
  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    cerr << "   " << gogui_x[i];
  }
  cerr << endl;

  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    cerr << setw(3) << (pure_board_size + 1 - i);
    for (int x = board_start; x <= board_end; x++) {
      const int pos = x + y * board_size;
      if (game->string[game->string_id[pos]].flag) {
        if (game->string_next[pos] != STRING_END) {
          cerr << setw(3) << game->string_next[pos];
        } else {
          cerr << " END";
        }
      } else {
        cerr << "   -";
      }
    }
    cerr << endl;
  }

  cerr << endl;
}


///////////////////
//  Ownerの表示  //
///////////////////
void
PrintOwner( const uct_node_t *root, const int color, double *own )
{
  const statistic_t *statistic = root->statistic;
  int player = 0, opponent = 0;
  double owner, score;

  if (!debug_message) return ;

  cerr << "   ";
  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    cerr << "   " << gogui_x[i];
  }
  cerr << endl;

  cerr << "   +";
  for (int i = 0; i < pure_board_size * 4; i++) {
    cerr << "-";
  }
  cerr << "+" << endl;
  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    cerr << setw(2) << (pure_board_size + 1 - i) << ":|";
    for (int x = board_start; x <= board_end; x++) {
      const int pos = POS(x, y);
      owner = (double)statistic[pos].colors[color] / root->move_count;
      if (owner > 0.5) {
        player++;
      } else {
        opponent++;
      }
      own[pos] = owner * 100.0;
      cerr << setw(3) << (int)(owner * 100) << " ";
    }
    cerr << "|" << endl;
  }

  cerr << "   +";
  for (int i = 0; i < pure_board_size * 4; i++) {
    cerr << "-";
  }
  cerr << "+" << endl;

  if (color == S_BLACK) {
    if (player - opponent > komi[0]) {
      score = player - opponent - komi[0];
      cerr << "BLACK+" << score << endl;
    } else {
      score = -(player - opponent - komi[0]);
      cerr << "WHITE+" << score << endl;
    }
  } else {
    if (player - opponent > -komi[0]) {
      score = player - opponent + komi[0];
      cerr << "WHITE+" << score << endl;
    } else {
      score = -(player - opponent + komi[0]);
      cerr << "BLACK+" << score << endl;
    }
  }
}


///////////////////////
//  最善応手列の出力  //
///////////////////////
void
PrintBestSequence( const game_info_t *game, const uct_node_t *uct_node, const int root, const int start_color )
{
  game_info_t *search_result;
  int current = root;
  int index = -1;
  int max = 0;
  int color = start_color;
  int child_num;
  const child_node_t *uct_child;

  if (!debug_message) return ;

  search_result = AllocateGame();
  CopyGame(search_result, game);

  cerr << "Best Sequence : ";

  uct_child = uct_node[current].child;
  child_num = uct_node[current].child_num;

  for (int i = 0; i < child_num; i++) {
    if (uct_child[i].move_count > max) {
      max = uct_child[i].move_count;
      index = i;
    }
  }

  if (uct_child[index].pos == PASS) cerr << "PASS";
  else cerr << GOGUI_X(uct_child[index].pos) << GOGUI_Y(uct_child[index].pos);
  if (node_hash[current].color == S_BLACK) cerr << "(BLACK : ";
  else if (node_hash[current].color == S_WHITE) cerr << "(WHITE : ";


  PutStone(search_result, uct_child[index].pos, color);
  color = FLIP_COLOR(color);

  cerr << uct_child[index].win << "/" << uct_child[index].move_count << ")";

  current = uct_child[index].index;
  
  while (current != NOT_EXPANDED) {
    uct_child = uct_node[current].child;
    child_num = uct_node[current].child_num;

    max = 50;
    index = -1;

    for (int i = 0; i < child_num; i++) {
      if (uct_child[i].move_count > max) {
        max = uct_child[i].move_count;
        index = i;
      }
    }

    if (index == -1) break;

    cerr << "->";

    if (uct_child[index].pos == PASS) cerr << "PASS";
    else cerr << GOGUI_X(uct_child[index].pos) << GOGUI_Y(uct_child[index].pos);

    if (node_hash[current].color == S_BLACK) cerr << "(BLACK : ";
    else if (node_hash[current].color == S_WHITE) cerr << "(WHITE : ";

    PutStone(search_result, uct_child[index].pos, color);

    color = FLIP_COLOR(color);

    cerr << uct_child[index].win << "/" << uct_child[index].move_count << ")";

    current = uct_child[index].index;

  }

  cerr << endl;

  PrintBoard(search_result);
  FreeGame(search_result);
}


static void
PrintBestSequence(std::ostream& out, const game_info_t *game, const uct_node_t *uct_node, int current_root)
{
  int current = current_root;

  auto root = &uct_node[current_root];
  auto uct_child = uct_node[current].child;
  int child_num = uct_node[current].child_num;

  if (root->move_count == 0)
    return;

  int index = -1;
  double max = 0;
  for (int i = 0; i < child_num; i++) {
    double m = uct_child[i].move_count;
    if (m > max) {
      max = m;
      index = i;
    }
  }
  if (index == -1)
    return;
  if (node_hash[current].color == S_BLACK) {
    out << "B ";
  } else if (node_hash[current].color == S_WHITE) {
    out << "W ";
  }
  out << FormatMove(uct_child[index].pos) << " ";

  current = uct_child[index].index;

  while (current != NOT_EXPANDED) {
    uct_child = uct_node[current].child;
    child_num = uct_node[current].child_num;

    max = 0;
    index = -1;

    for (int i = 0; i < child_num; i++) {
      double m = uct_child[i].move_count;
      if (m > max) {
        max = m;
        index = i;
      }
    }

    if (index == -1) break;

    if (node_hash[current].color == S_BLACK) {
      out << "B ";
    } else if (node_hash[current].color == S_WHITE) {
      out << "W ";
    }
    out << FormatMove(uct_child[index].pos) << " ";

    current = uct_child[index].index;
  }
}


void
PrintMoveStat( std::ostream& out, const game_info_t *game, const uct_node_t *uct_node, int current_root )
{
  const child_node_t *uct_child = uct_node[current_root].child;
  const int child_num = uct_node[current_root].child_num;

  vector<size_t> idx(child_num);
  iota(idx.begin(), idx.end(), 0);

  auto idxComp = [=](size_t i1, size_t i2) {
    return uct_child[i1].move_count > uct_child[i2].move_count;
  };

  sort(idx.begin(), idx.end(), idxComp);

  out << "|Move|#PO  |Simulation|Win       |Best Sequence" << endl;
  // UCB値最大の手を求める
  //for (int j = 0; j < std::min(10, child_num); j++) {
  for (int j = 0; j < child_num; j++) {
    size_t i = idx[j];
    if (uct_child[i].move_count == 0)
      continue;
    //if (!uct_child[i].flag && !uct_child[i].open)
    //continue;
    //double p2 = -1;

    double win = uct_child[i].win;
    double move_count = uct_child[i].move_count;
    double p0 = win / move_count;

    out << "|" << setw(4) << FormatMove(uct_child[i].pos);
    out << "|" << setw(5) << (int) move_count;

    auto precision = out.precision();
    out.precision(4);
    out << "|" << setw(10) << fixed << (p0 * 100);
    out.precision(precision);
    out << "|";
    if (uct_child[i].index > 0)
      PrintBestSequence(out, game, uct_node, uct_child[i].index);
    out << endl;
  }
}


///////////////////////
//  探索の情報の表示  //
///////////////////////
void
PrintPlayoutInformation( const uct_node_t *root, const po_info_t *po_info, const double finish_time, const int pre_simulated )
{
  const double winning_percentage = (double)root->win / root->move_count;

  if (!debug_message) return ;

  cerr << "All Playouts       :  " << setw(7) << root->move_count << endl;
  cerr << "Pre Simulated      :  " << setw(7) << pre_simulated << endl;
  cerr << "Win                :  " << setw(7) << root->win << endl;
  cerr << "Thinking Time      :  " << setw(7) << finish_time << " sec" << endl;
  cerr << "Winning Percentage :  " << setw(7) << (winning_percentage * 100) << "%" << endl;
  if (finish_time != 0.0) {
    cerr << "Playout Speed      :  " << setw(7) << (int)(po_info->count / finish_time) << " PO/sec " << endl;
  }
}


//////////////////
//  座標の出力  //
//////////////////
void
PrintPoint( const int pos )
{
  if (!debug_message) return ;

  if (pos == PASS) {
    cerr << "PASS" << endl;
  } else if (pos == RESIGN) {
    cerr << "RESIGN" << endl;
  } else {
    cerr << GOGUI_X(pos) << GOGUI_Y(pos) << endl;
  }
}


std::string FormatMove(int pos)
{
  if (pos == PASS) {
    return "PASS";
  } else if (pos == RESIGN) {
    return "RESIGN";
  } else {
    ostringstream o;
    o << GOGUI_X(pos) << GOGUI_Y(pos);
    return o.str();
  }

}


/////////////////////
//  コミの値の出力  //
/////////////////////
void
PrintKomiValue( void )
{
  if (!debug_message) return ;

  cerr << "Dynamic Komi : " << setw(4) << dynamic_komi[0] << endl;
}


////////////////////////////////////////
//  Ponderingのプレイアウト回数の出力  //
////////////////////////////////////////
void
PrintPonderingCount( const int count )
{
  if (!debug_message) return ;

  cerr << "Ponder : " << count << " Playouts" << endl;
}


//////////////////////
//  探索時間の出力  //
/////////////////////
void
PrintPlayoutLimits( const double time_limit, const int playout_limit )
{
  if (!debug_message) return ;

  cerr << "Time Limit    : " << time_limit << " Sec" << endl;
  cerr << "Playout Limit : " << playout_limit << " PO" << endl; 
}

////////////////////////////////////////
//  Ponderingのプレイアウト回数の出力  //
////////////////////////////////////////
void
PrintReuseCount( const int count )
{
  if (!debug_message) return ;

  cerr << "Reuse : " << count << " Playouts" << endl;
}
