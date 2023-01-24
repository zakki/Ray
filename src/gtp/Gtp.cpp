#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <cctype>
#include <iostream>
#include <sstream>
#include <algorithm>

#include "board/DynamicKomi.hpp"
#include "board/GoBoard.hpp"
#include "board/Point.hpp"
#include "board/ZobristHash.hpp"
#include "common/Message.hpp"
#include "feature/Nakade.hpp"
#include "gtp/Gtp.hpp"
#include "mcts/Rating.hpp"
#include "mcts/Simulation.hpp"
#include "mcts/UctSearch.hpp"
#include "mcts/UctRating.hpp"
#include "sgf/SgfExtractor.hpp"


////////////
//  変数  //
////////////

//  コマンドの処理用のバッファ
char input[BUF_SIZE], input_copy[BUF_SIZE];
char *next_token;
int command_id;

//  応答用の文字列
char brank[] = "";
char err_command[] = "? unknown command";
char err_genmove[] = "gemmove color";
char err_play[] = "play color point";
char err_komi[] = "komi float";

//  自分の石の色
int player_color = 0;

//  盤面の情報
game_info_t *game;


////////////
//  関数  //
////////////

//  gtpの出力用関数
static void GTP_response( const char *res, bool success );
//  boardsizeコマンドを処理
static void GTP_boardsize( void );
//  clearboardコマンドを処理
static void GTP_clearboard( void );
//  nameコマンドを処理
static void GTP_name( void );
//  protocolversionコマンドを処理
static void GTP_protocolversion( void );
//  genmoveコマンドを処理
static void GTP_genmove( void );
//  playコマンドを処理
static void GTP_play( void );
//  knowncommandコマンドを処理
static void GTP_knowncommand( void );
//  listcommandsコマンドを処理
static void GTP_listcommands( void );
//  quitコマンドを処理
static void GTP_quit( void );
//  komiコマンドを処理
static void GTP_komi( void );
//  getkomiコマンドを処理
static void GTP_getkomi( void );
//  finalscoreコマンドを処理
static void GTP_finalscore( void );
//  timesettingsコマンドを処理
static void GTP_timesettings( void );
//  timeleftコマンドを処理
static void GTP_timeleft( void );
//  versionコマンドを処理
static void GTP_version( void );
//  showboardコマンドを処理
static void GTP_showboard( void );
//  kgs-genmove_cleanupコマンドを処理
static void GTP_kgs_genmove_cleanup( void );
//  final_status_listコマンドを処理
static void GTP_final_status_list( void );
//  set_free_handicapコマンドを処理
static void GTP_set_free_handicap( void );
//  fixed_handicapコマンドを処理
static void GTP_fixed_handicap( void );
//  loadsgfコマンドを処理
static void GTP_loadsgf( void );
//  cgos-genmove_analyzeコマンドを処理
static void GTP_cgos_genmove_analyze( void );

////////////
//  定数  //
////////////

//  GTPコマンド
const GTP_command_t gtpcmd[] = {
  { "boardsize",           GTP_boardsize           },
  { "clear_board",         GTP_clearboard          },
  { "final_score",         GTP_finalscore          },
  { "final_status_list",   GTP_final_status_list   },
  { "fixed_handicap",      GTP_fixed_handicap      },
  { "genmove",             GTP_genmove             },
  { "cgos-genmove_analyze",GTP_cgos_genmove_analyze},
  { "get_komi",            GTP_getkomi             },
  { "kgs-genmove_cleanup", GTP_kgs_genmove_cleanup },
  { "known_command",       GTP_knowncommand        },
  { "komi",                GTP_komi                },
  { "list_commands",       GTP_listcommands        },
  { "loadsgf",             GTP_loadsgf             },
  { "name",                GTP_name                },
  { "place_free_handicap", GTP_fixed_handicap      },
  { "play",                GTP_play                },
  { "protocol_version",    GTP_protocolversion     },
  { "quit",                GTP_quit                },
  { "set_free_handicap",   GTP_set_free_handicap   },
  { "showboard",           GTP_showboard           },
  { "time_left",           GTP_timeleft            },
  { "time_settings",       GTP_timesettings        },
  { "version",             GTP_version             },
};


///////////////////////
//  void GTP_main()  //
///////////////////////
void
GTP_main( void )
{
  game = AllocateGame();
  InitializeBoard(game);

  while (fgets(input, sizeof(input), stdin) != NULL) {
    char *command;
    bool nocommand = true;
    command_id = -1;

    if (isdigit(input[0])) {
      char buf[BUF_SIZE];
      STRCPY(buf, BUF_SIZE, input);
      char *strid = STRTOK(buf, DELIM, &next_token);
      command_id = atoi(strid);
      command = STRTOK(nullptr, DELIM, &next_token);
      int offset = command - buf;
      STRCPY(input_copy, BUF_SIZE, input + offset);
    } else {
      STRCPY(input_copy, BUF_SIZE, input);
      command = STRTOK(input, DELIM, &next_token);
    }
    CHOMP(command);

    for (const GTP_command_t& cmd : gtpcmd) {
      if (!strcmp(command, cmd.command)) {
        (*cmd.function)();
        nocommand = false;
        break;
      }
    }

    if (nocommand) {
      std::cout << err_command << std::endl << std::endl;
    }

    fflush(stdin);
    fflush(stdout);
  }
}


/////////////////////////////////////////////////
//  void GTP_response(char *res, int success)  //
/////////////////////////////////////////////////
static void
GTP_response( const char *res, bool success )
{
  if (success){
    if (command_id >= 0) {
      std::cout << "=" << command_id << " " << res << std::endl << std::endl;
    } else {
      std::cout << "= " << res << std::endl << std::endl;
    }
  } else {
    if (res != NULL) {
      std::cerr << res << std::endl;
    }
    std::cout << "?" << std::endl << std::endl;
  }
}


/////////////////////////////
//　 void GTP_boardsize()  //
/////////////////////////////
static void
GTP_boardsize( void )
{
  char *command;
  int size;
  char buf[1024];

  StopPondering();

  command = STRTOK(NULL, DELIM, &next_token);

#if defined (_WIN32)
  sscanf_s(command, "%d", &size);
  sprintf_s(buf, 1024, " ");
#else
  sscanf(command, "%d", &size);
  snprintf(buf, 1024, " ");
#endif

  if (pure_board_size != size &&
      size <= PURE_BOARD_SIZE && size > 0) {
    SetBoardSize(size);
    SetParameter();
    SetNeighbor();
    InitializeNakadeHash();
  }

  FreeGame(game);
  game = AllocateGame();
  InitializeBoard(game);
  InitializeSearchSetting();
  InitializeUctHash();

  GTP_response(brank, true);
}


/////////////////////////////
//  void GTP_clearboard()  //
/////////////////////////////
static void
GTP_clearboard( void )
{
  StopPondering();

  player_color = 0;
  SetHandicapNum(0);
  FreeGame(game);
  game = AllocateGame();
  InitializeBoard(game);
  InitializeSearchSetting();
  InitializeUctHash();

  GTP_response(brank, true);
}


///////////////////////
//  void GTP_name()  //
///////////////////////
static void
GTP_name( void )
{
  GTP_response(PROGRAM_NAME, true);
}


//////////////////////////////////
//  void GTP_protocolversion()  //
//////////////////////////////////
static void
GTP_protocolversion( void )
{
  GTP_response(PROTOCOL_VERSION, true);
}


//////////////////////////
//  void GTP_genmove()  //
//////////////////////////
static void
GTP_genmove( void )
{
  char *command;
  char c;
  char pos[10];
  int color;
  int point = PASS;

  StopPondering();

  command = STRTOK(input_copy, DELIM, &next_token);

  CHOMP(command);

  command = STRTOK(NULL, DELIM, &next_token);
  if (command == NULL){
    GTP_response(err_genmove, true);
    return;
  }
  CHOMP(command);
  c = (char)tolower((int)command[0]);
  if (c == 'w') {
    color = S_WHITE;
  } else if (c == 'b') {
    color = S_BLACK;
  } else {
    GTP_response(err_genmove, true);
    return;
  }

  player_color = color;

  point = UctSearchGenmove(game, color);
  if (point != RESIGN) {
    PutStone(game, point, color);
  }

  IntegerToString(point, pos);

  GTP_response(pos, true);

  UctSearchPondering(game, GetOppositeColor(color));
}


std::string FormatMove(int pos)
{
  if (pos == PASS) {
    return "PASS";
  } else if (pos == RESIGN) {
    return "RESIGN";
  } else {
    std::ostringstream o;
    o << GOGUI_X(pos) << GOGUI_Y(pos);
    return o.str();
  }
}


static void
PrintCgosMoves(std::ostringstream& out, const uct_node_t *uct_node, const int root, const int color)
{
  const child_node_t *uct_child = uct_node[current_root].child;
  const int child_num = uct_node[current_root].child_num;

  std::vector<size_t> idx(child_num);
  std::iota(idx.begin(), idx.end(), 0);

  auto idxComp = [=](size_t i1, size_t i2) {
    return uct_child[i1].move_count > uct_child[i2].move_count;
  };

  std::sort(idx.begin(), idx.end(), idxComp);

  double owner[BOARD_MAX];
  double score = GetCurrentScore(color, owner);
  out << "\"moves\":[";
  for (int j = 0; j < child_num; j++) {
    auto i = idx[j];

    double win = uct_child[i].win;
    double move_count = uct_child[i].move_count;
    double p0 = win / move_count;
    if (move_count == 0)
      continue;

    if (j != 0) out << ",";
    out << "{\"move\":\"" << FormatMove(uct_child[i].pos)
      << "\",\"winrate\":" << p0
      << ",\"score\":" << score;

    if (uct_child[i].index > 0) {
      int current = uct_child[i].index;
      bool first = true;
      while (current != NOT_EXPANDED) {
        auto uct_child = uct_node[current].child;
        auto child_num = uct_node[current].child_num;

        int max = 50;
        int index = -1;

        for (int i = 0; i < child_num; i++) {
          if (uct_child[i].move_count > max) {
            max = uct_child[i].move_count;
            index = i;
          }
        }

        if (index == -1) break;
        if (first)
          out << ",\"pv\":\"";
        else
          out << " ";
        first = false;
        if (uct_child[index].pos == PASS) out << "PASS";
        else out << GOGUI_X(uct_child[index].pos) << GOGUI_Y(uct_child[index].pos);

        current = uct_child[index].index;
      }
      if (!first)
      out << "\"";
    }
    //  PrintBestSequence(out, game, uct_node, uct_child[i].index);
    out << "}";
  }
  out << "],";
  out << "\"ownership\":\"";


  const auto CHARS = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for (int i = 1, y = board_start; y <= board_end; y++, i++) {
    for (int x = board_start; x <= board_end; x++) {
      const int pos = x + y * board_size;
      out << CHARS[(int)std::round(owner[pos] / 100 * 62)];
    }
    // out << "\n";
  }
  out << "\"";
}


///////////////////////////////////////
//  void GTP_cgos_genmove_analyze()  //
///////////////////////////////////////
static void
GTP_cgos_genmove_analyze( void )
{
  char *command;
  char c;
  char pos[10];
  int color;
  int point = PASS;

  StopPondering();

  command = STRTOK(input_copy, DELIM, &next_token);

  CHOMP(command);

  command = STRTOK(NULL, DELIM, &next_token);
  if (command == NULL){
    GTP_response(err_genmove, true);
    return;
  }
  CHOMP(command);
  c = (char)tolower((int)command[0]);
  if (c == 'w') {
    color = S_WHITE;
  } else if (c == 'b') {
    color = S_BLACK;
  } else {
    GTP_response(err_genmove, true);
    return;
  }

  player_color = color;

  point = UctSearchGenmove(game, color);
  if (point != RESIGN) {
    PutStone(game, point, color);
  }

  IntegerToString(point, pos);

  std::ostringstream out;
  out << "\n";
  out << "{";
  PrintCgosMoves(out, uct_node, current_root, color);
  out << "}\n";
  out << "play " << pos;

  GTP_response(out.str().c_str(), true);

  UctSearchPondering(game, GetOppositeColor(color));
}


///////////////////////
//  void GTP_play()  //
///////////////////////
static void
GTP_play( void )
{
  char *command;
  char c;
  int color, pos = 0;

  StopPondering();

  command = STRTOK(input_copy, DELIM, &next_token);

  command = STRTOK(NULL, DELIM, &next_token);
  if (command == NULL){
    GTP_response(err_play, false);
    return;
  }
  CHOMP(command);
  c = (char)tolower((int)command[0]);
  if (c == 'w') {
    color = S_WHITE;
  } else{
    color = S_BLACK;
  }

  command = STRTOK(NULL, DELIM, &next_token);

  CHOMP(command);
  if (command == NULL){
    GTP_response(err_play, false);
    return;
  } else {
    pos = StringToInteger(command);
  }

  if (pos != RESIGN) {
    PutStone(game, pos, color);
  }

  GTP_response(brank, true);
}


/////////////////////////////
// void GTP_knowncommand() //
/////////////////////////////
static void
GTP_knowncommand( void )
{
  char *command;

  command = STRTOK(NULL, DELIM, &next_token);

  if (command == NULL){
    GTP_response("known_command command", false);
    return;
  }
  CHOMP(command);
  for (const auto& cmd : gtpcmd) {
    if (!strcmp(command, cmd.command)) {
      GTP_response("true", true);
      return;
    }
  }
  GTP_response("false", false);
}


///////////////////////////////
//  void GTP_listcommands()  //
///////////////////////////////
static void
GTP_listcommands( void )
{
  char list[2048];
  int i;

  i = 0;
  list[i++] = '\n';
  for (const auto& cmd : gtpcmd) {
    for (unsigned int k = 0; k < strlen(cmd.command); k++){
      list[i++] = cmd.command[k];
    }
    list[i++] = '\n';
  }
  list[i-1] = '\0';

  GTP_response(list, true);
}


//////////////////////
// void GTP_quit()  //
//////////////////////
static void
GTP_quit( void )
{
  GTP_response(brank, true);
  exit(0);
}


///////////////////////
//  void GTP_komi()  //
///////////////////////
static void
GTP_komi( void )
{
  char* c_komi;

  StopPondering();

  c_komi = STRTOK(NULL, DELIM, &next_token);

  if (c_komi != NULL) {
    SetKomi(atof(c_komi));
    PrintKomiValue();
    GTP_response(brank, true);
  } else {
    GTP_response(err_komi, false);
  }
}


//////////////////////////
//  void GTP_getkomi()  //
//////////////////////////
static void
GTP_getkomi( void )
{
  char buf[256];

#if defined(_WIN32)
  sprintf_s(buf, 4, "%lf", komi[0]);
#else
  snprintf(buf, 4, "%lf", komi[0]);
#endif
  GTP_response(buf, true);
}


/////////////////////////////
//  void GTP_finalscore()  //
/////////////////////////////
static void
GTP_finalscore( void )
{
  char buf[10];

  StopPondering();

  double score = UctAnalyze(game, S_BLACK) - komi[0];

#if defined(_WIN32)
  if (abs(score) < 0.1) {
    sprintf_s(buf, 10, "0");
  } else if (score > 0) {
    sprintf_s(buf, 10, "B+%.1lf", score);
  } else {
    sprintf_s(buf, 10, "W+%.1lf", -score);
  }
#else
  if (abs(score) < 0.1) {
    snprintf(buf, 10, "0");
  } else if (score > 0) {
    snprintf(buf, 10, "B+%.1lf", score);
  } else {
    snprintf(buf, 10, "W+%.1lf", -score);
  }
#endif

  GTP_response(buf, true);
}


///////////////////////////////
//  void GTP_timesettings()  //
///////////////////////////////
static void
GTP_timesettings( void )
{
  char *str1, *str2, *str3;
  double main_time, byoyomi, stone;

  str1 = STRTOK(NULL, DELIM, &next_token);
  str2 = STRTOK(NULL, DELIM, &next_token);
  str3 = STRTOK(NULL, DELIM, &next_token);

  main_time = atoi(str1);
  byoyomi = atoi(str2);
  stone = atoi(str3);

  std::cerr << main_time << "," << byoyomi << "," << stone << std::endl;

  SetTimeSettings(main_time, byoyomi, stone);
  InitializeSearchSetting();

  GTP_response(brank, true);
}


///////////////////////////
//  void GTP_timeleft()  //
///////////////////////////
static void
GTP_timeleft( void )
{
  char *str1, *str2;

  str1 = STRTOK(NULL, DELIM, &next_token);
  str2 = STRTOK(NULL, DELIM, &next_token);

  if (str1[0] == 'B' || str1[0] == 'b'){
    SetCurrentRemainingTime(S_BLACK, atof(str2));
  } else if (str1[0] == 'W' || str1[0] == 'w'){
    SetCurrentRemainingTime(S_WHITE, atof(str2));
  }

  fprintf(stderr, "%f\n", GetRemainingTime(S_BLACK));
  fprintf(stderr, "%f\n", GetRemainingTime(S_WHITE));
  GTP_response(brank, true);
}


//////////////////////////
//  void GTP_version()  //
//////////////////////////
static void
GTP_version( void )
{
  GTP_response(PROGRAM_VERSION, true);
}


////////////////////////////
//  void GTP_showboard()  //
////////////////////////////
static void
GTP_showboard( void )
{
  PrintBoard(game);
  GTP_response(brank, true);
}


/////////////////////////////////
//  void GTP_fixed_handicap()  //
/////////////////////////////////
static void
GTP_fixed_handicap( void )
{
  char *command;
  int num;
  char buf[1024];
  char pos[5];
  int handicap[9];
  const int place_index[8][9] = {
    {2, 6},
    {0, 2, 6},
    {0, 2, 6, 8},
    {0, 2, 4, 6, 8},
    {0, 2, 3, 5, 6, 8},
    {0, 2, 3, 4, 5, 6, 8},
    {0, 1, 2, 3, 5, 6, 7, 8},
    {0, 1, 2, 3, 4, 5, 6, 7, 8},
  };

  StopPondering();

  command = STRTOK(NULL, DELIM, &next_token);

#if defined (_WIN32)
  sscanf_s(command, "%d", &num);
  sprintf_s(buf, 1024, " ");
#else
  sscanf(command, "%d", &num);
  snprintf(buf, 1024, " ");
#endif

  if (num < 2 || 9 < num) {
    GTP_response(brank, false);
    return ;
  }

  handicap[0] = POS(board_start +  3, board_start +  3);
  handicap[1] = POS(board_start +  9, board_start +  3);
  handicap[2] = POS(board_start + 15, board_start +  3);
  handicap[3] = POS(board_start +  3, board_start +  9);
  handicap[4] = POS(board_start +  9, board_start +  9);
  handicap[5] = POS(board_start + 15, board_start +  9);
  handicap[6] = POS(board_start +  3, board_start + 15);
  handicap[7] = POS(board_start +  9, board_start + 15);
  handicap[8] = POS(board_start + 15, board_start + 15);

  for (int i = 0; i < num; i++) {
    PutStone(game, handicap[place_index[num - 2][i]], S_BLACK);
#if defined (_WIN32)
    sprintf_s(pos, 5, "%c%d ", GOGUI_X(handicap[place_index[num - 2][i]]), GOGUI_Y(handicap[place_index[num - 2][i]]));
    strcat_s(buf, 1024, pos);
#else
    snprintf(pos, 5, "%c%d ", GOGUI_X(handicap[place_index[num - 2][i]]), GOGUI_Y(handicap[place_index[num - 2][i]]));
    strncat(buf, pos, 5);
#endif
  }

  SetKomi(0.5);
  SetHandicapNum(num);
  GTP_response(buf, true);
}


////////////////////////////////////
//  void GTP_set_free_handicap()  //
////////////////////////////////////
static void
GTP_set_free_handicap( void )
{
  char *command;
  int pos, num = 0;

  StopPondering();

  while (1){
    command = STRTOK(NULL, DELIM, &next_token);

    if (command == NULL){
      SetHandicapNum(num);
      SetKomi(0.5);
      GTP_response(brank, true);
      return;
    }

    pos = StringToInteger(command);

    if (pos > 0 && pos < board_max && IsLegal(game, pos, S_BLACK)) {
      PutStone(game, pos, S_BLACK);
      num++;
    }
  }
}


////////////////////////////////////
//  void GTP_final_status_list()  //
////////////////////////////////////
static void
GTP_final_status_list( void )
{
  char dead[2048] = { 0 };
  char pos[5];
  int owner[BOARD_MAX];
  char *command;

  StopPondering();

  OwnerCopy(owner);

  command = STRTOK(NULL, DELIM, &next_token);

  CHOMP(command);

  if (!strcmp(command, "dead")){
    for (int y = board_start; y <= board_end; y++) {
      for (int x = board_start; x <= board_end; x++) {
        if ((game->board[POS(x, y)] == player_color && owner[POS(x, y)] <= 30) ||
            (game->board[POS(x, y)] == GetOppositeColor(player_color) && owner[POS(x, y)] >= 70)) {
#if defined (_WIN32)
          sprintf_s(pos, 5, "%c%d ", GOGUI_X(POS(x, y)), GOGUI_Y(POS(x, y)));
          strcat_s(dead, 2048, pos);
#else
          snprintf(pos, 5, "%c%d ", GOGUI_X(POS(x, y)), GOGUI_Y(POS(x, y)));
          strncat(dead, pos, 5);
#endif
        }
      }
    }
  } else if (!strcmp(command, "alive")){
    for (int y = board_start; y <= board_end; y++) {
      for (int x = board_start; x <= board_end; x++) {
        if ((game->board[POS(x, y)] == player_color && owner[POS(x, y)] >= 70) ||
            (game->board[POS(x, y)] == GetOppositeColor(player_color) && owner[POS(x, y)] <= 30)) {
#if defined (_WIN32)
          sprintf_s(pos, 5, "%c%d ", GOGUI_X(POS(x, y)), GOGUI_Y(POS(x, y)));
          strcat_s(dead, 2048, pos);
#else
          snprintf(pos, 5, "%c%d ", GOGUI_X(POS(x, y)), GOGUI_Y(POS(x, y)));
          strncat(dead, pos, 5);
#endif
        }
      }
    }
  }

  GTP_response(dead, true);
}


//////////////////////////////////////
//  void GTP_kgs_genmove_cleanup()  //
//////////////////////////////////////
static void
GTP_kgs_genmove_cleanup( void )
{
  char *command;
  char c;
  char pos[10];
  int color;
  int point = PASS;

  StopPondering();

  command = STRTOK(input_copy, DELIM, &next_token);

  CHOMP(command);
  if (!strcmp("genmove_black", command)) {
    color = S_BLACK;
  } else if (!strcmp("genmove_white", command)) {
    color = S_WHITE;
  } else {
    command = STRTOK(NULL, DELIM, &next_token);
    if (command == NULL){
      GTP_response(err_genmove, false);
      return;
    }
    CHOMP(command);
    c = (char)tolower((int)command[0]);
    if (c == 'w') {
      color = S_WHITE;
    } else if (c == 'b') {
      color = S_BLACK;
    } else {
      GTP_response(err_genmove, false);
      return;
    }
  }

  player_color = color;

  point = UctSearchGenmoveCleanUp(game, color);
  if (point != RESIGN) {
    PutStone(game, point, color);
  }

  IntegerToString(point, pos);

  GTP_response(pos, true);
}


//////////////////////////
//  void GTP_loadsgf()  //
//////////////////////////
static void
GTP_loadsgf( void )
{
  SGF_record_t sgf;
  char *command, *filename, *move;
  int pos, color, size, target_move = 0;

  StopPondering();

  // コマンドの抽出
  command = STRTOK(input_copy, DELIM, &next_token);
  CHOMP(command);

  // ファイル名の抽出
  filename = STRTOK(NULL, DELIM, &next_token);
  CHOMP(filename);
  std::cerr << filename << std::endl;

  // 着手数の抽出
  move = STRTOK(NULL, DELIM, &next_token);
  if (move != NULL) {
    CHOMP(move);
    target_move = atoi(move);
  }

  // 棋譜の読み込み
  if (ExtractKifu(filename, &sgf) != 0) {
    char errmsg[2048];
#if defined (_WIN32)
    sprintf_s(errmsg, 2048, "cannot read \"%s\"", filename);
#else
    snprintf(errmsg, 2048, "cannot read \"%s\"", filename);
#endif
    GTP_response(errmsg, false);
    return;
  }

  // 碁盤のサイズを設定
  size = sgf.board_size;

  // 碁盤の初期化処理
  if (pure_board_size != size &&
      size <= PURE_BOARD_SIZE && size > 0) {
    SetBoardSize(size);
    SetParameter();
    SetNeighbor();
    InitializeNakadeHash();
  }
  FreeGame(game);
  game = AllocateGame();
  InitializeBoard(game);
  InitializeSearchSetting();
  InitializeUctHash();
  SetKomi(sgf.komi);

  // あらかじめ置いてある石を配置
  for (int i = 0; i < sgf.handicap_stones; i++) {
    pos = GetHandicapStone(&sgf, i);
    PutStone(game, pos, sgf.handicap_color[i]);
  }

  // 置き石の個数の設定
  if (sgf.handicaps != 0) {
    SetHandicapNum(sgf.handicaps);
  }

  color = sgf.start_color;

  if (target_move < 1 || target_move > sgf.moves) {
    target_move = sgf.moves;
  } else {
    target_move--;
  }

  // 石を配置
  for (int i = 0; i < target_move; i++) {
    pos = GetKifuMove(&sgf, i);
    PutStone(game, pos, color);
    color = GetOppositeColor(color);
  }

  if (color == S_BLACK) {
    GTP_response("black", true);
  } else {
    GTP_response("white", true);
  }
}
