#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <cstring>
#include <cctype>
#include <iostream>

#include "DynamicKomi.h"
#include "Gtp.h"
#include "GoBoard.h"
#include "Nakade.h"
#include "UctSearch.h"
#include "UctRating.h"
#include "Message.h"
#include "Point.h"
#include "Rating.h"
#include "Simulation.h"
#include "ZobristHash.h"

using namespace std;

static GTP_command_t gtpcmd[GTP_COMMAND_NUM];

static char input[BUF_SIZE], input_copy[BUF_SIZE];
//static char *next_token;

static char *brank, *err_command, *err_genmove, *err_play, *err_komi;

static int player_color = 0;

static game_info_t *game;

///////////////////////
//  void GTP_main()  //
///////////////////////
void
GTP_main( void )
{
  int i;

  game = AllocateGame();
  InitializeBoard(game);

  GTP_setCommand();
  GTP_message();

  while (fgets(input, sizeof(input), stdin) != NULL) {
    char *command;
    bool nocommand = true;

    STRCPY(input_copy, BUF_SIZE, input);
    command = STRTOK(input, DELIM, &next_token);
    CHOMP(command);

    for (i = 0; i < GTP_COMMAND_NUM; i++) {
      if (!strcmp(command, gtpcmd[i].command)) {
      	StopPondering();
      	(*gtpcmd[i].function)();
      	nocommand = false;
      	break;
      }
    }

    if (nocommand) {
      cout << err_command << endl << endl;
    }

    fflush(stdin);
    fflush(stdout);
  }
}

///////////////////////
//  GTP??????  //
///////////////////////
void
GTP_message( void )
{
  brank = STRDUP("");
  err_command = STRDUP("? unknown command");
  err_genmove = STRDUP("genmove color");
  err_play = STRDUP("play color point");
  err_komi = STRDUP("komi float");
}

/////////////////////////////
//  void GTP_setcommand()  //
/////////////////////////////
void
GTP_setCommand( void )
{
  gtpcmd[ 0].command = STRDUP("boardsize");
  gtpcmd[ 1].command = STRDUP("clear_board");
  gtpcmd[ 2].command = STRDUP("name");
  gtpcmd[ 3].command = STRDUP("protocol_version");
  gtpcmd[ 4].command = STRDUP("genmove");
  gtpcmd[ 5].command = STRDUP("play");
  gtpcmd[ 6].command = STRDUP("known_command");
  gtpcmd[ 7].command = STRDUP("list_commands");
  gtpcmd[ 8].command = STRDUP("quit");
  gtpcmd[ 9].command = STRDUP("komi");
  gtpcmd[10].command = STRDUP("get_komi");
  gtpcmd[11].command = STRDUP("final_score");
  gtpcmd[12].command = STRDUP("time_settings");
  gtpcmd[13].command = STRDUP("time_left");
  gtpcmd[14].command = STRDUP("version");
  gtpcmd[15].command = STRDUP("genmove_black");
  gtpcmd[16].command = STRDUP("genmove_white");
  gtpcmd[17].command = STRDUP("black");
  gtpcmd[18].command = STRDUP("white");
  gtpcmd[19].command = STRDUP("showboard");
  gtpcmd[20].command = STRDUP("final_status_list");
  gtpcmd[21].command = STRDUP("fixed_handicap");
  gtpcmd[22].command = STRDUP("place_free_handicap");
  gtpcmd[23].command = STRDUP("set_free_handicap");
  gtpcmd[24].command = STRDUP("kgs-genmove_cleanup");

  gtpcmd[ 0].function = GTP_boardsize;
  gtpcmd[ 1].function = GTP_clearboard;
  gtpcmd[ 2].function = GTP_name;
  gtpcmd[ 3].function = GTP_protocolversion;
  gtpcmd[ 4].function = GTP_genmove;
  gtpcmd[ 5].function = GTP_play;
  gtpcmd[ 6].function = GTP_knowncommand;
  gtpcmd[ 7].function = GTP_listcommands;
  gtpcmd[ 8].function = GTP_quit;
  gtpcmd[ 9].function = GTP_komi;
  gtpcmd[10].function = GTP_getkomi;
  gtpcmd[11].function = GTP_finalscore;
  gtpcmd[12].function = GTP_timesettings;
  gtpcmd[13].function = GTP_timeleft;
  gtpcmd[14].function = GTP_version;
  gtpcmd[15].function = GTP_genmove;
  gtpcmd[16].function = GTP_genmove;
  gtpcmd[17].function = GTP_play;
  gtpcmd[18].function = GTP_play;
  gtpcmd[19].function = GTP_showboard;
  gtpcmd[20].function = GTP_final_status_list;
  gtpcmd[21].function = GTP_fixed_handicap;
  gtpcmd[22].function = GTP_fixed_handicap;
  gtpcmd[23].function = GTP_set_free_handicap;
  gtpcmd[24].function = GTP_kgs_genmove_cleanup;
}

/////////////////////////////////////////////////
//  void GTP_response(char *res, int success)  //
/////////////////////////////////////////////////
void
GTP_response( const char *res, bool success )
{
  if (success){
    cout << "= " << res << endl << endl;
  } else {
    if (res != NULL) {
      cerr << res << endl;
    }
    cout << "?" << endl << endl;
  }
}

/////////////////////////////
//?@ void GTP_boardsize()  //
/////////////////////////////
void
GTP_boardsize( void )
{
  char *command;
  int size;
  char buf[1024];
  
  command = STRTOK(NULL, DELIM, &next_token);

#if defined (_WIN32)
  sscanf_s(command, "%d", &size);
  sprintf_s(buf, 1024, " ");
#else
  sscanf(command, "%d", &size);
  sprintf(buf, " ");
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
void
GTP_clearboard( void )
{
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
void
GTP_name( void )
{
  GTP_response(PROGRAM_NAME, true);
}

//////////////////////////////////
//  void GTP_protocolversion()  //
//////////////////////////////////
void
GTP_protocolversion( void )
{
  GTP_response(PROTOCOL_VERSION, true);
}

//////////////////////////
//  void GTP_genmove()  //
//////////////////////////
void
GTP_genmove( void )
{
  char *command;
  char c;
  char pos[10];
  int color;
  int point = PASS;
  
  command = STRTOK(input_copy, DELIM, &next_token);

  CHOMP(command);
  if (!strcmp("genmove_black", command)) {
    color = S_BLACK;
  } else if (!strcmp("genmove_white", command)) {
    color = S_WHITE;
  } else {
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
  }

  player_color = color;

  point = UctSearchGenmove(game, color);
  if (point != RESIGN) {
    PutStone(game, point, color);
  }
  IntegerToString(point, pos);
  
  GTP_response(pos, true);
  UctSearchPondering(game, FLIP_COLOR(color));
}

///////////////////////
//  void GTP_play()  //
///////////////////////
void
GTP_play( void )
{
  char *command;
  char c;
  int color, pos = 0;
  
  command = STRTOK(input_copy, DELIM, &next_token);

  if (!strcmp("black", command)){
    color = S_BLACK;
  } else if (!strcmp("white", command)){
    color = S_WHITE;
  } else{
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
  PrintBoard(game);
  
  
  GTP_response(brank, true);
}
 
/////////////////////////////
// void GTP_knowncommand() //
/////////////////////////////
void
GTP_knowncommand( void )
{
  int i;
  char *command;
  
  command = STRTOK(NULL, DELIM, &next_token);
  
  if (command == NULL){
    GTP_response("known_command command", false);
    return;
  }
  CHOMP(command);
  for (i = 0; i < GTP_COMMAND_NUM; i++){
    if (!strcmp(command, gtpcmd[i].command)) {
      GTP_response("true", true);
      return;
    }
  }
  GTP_response("false", false);
}
 
///////////////////////////////
//  void GTP_listcommands()  //
///////////////////////////////
void
GTP_listcommands( void )
{
  char list[2048];
  int i, j;
  unsigned int k;

  i = 0;
  list[i++] = '\n';
  for (j = 0; j < GTP_COMMAND_NUM; j++) {
    for (k = 0; k < strlen(gtpcmd[j].command); k++){
      list[i++] = gtpcmd[j].command[k];
    }
    list[i++] = '\n';
  }
  list[i++] = '\0';

  GTP_response(list, true);
}
 
//////////////////////
// void GTP_quit()  //
//////////////////////
void
GTP_quit( void )
{
  FinalizeUctSearch();
  GTP_response(brank, true);
  exit(0);
}

///////////////////////
//  void GTP_komi()  //
///////////////////////
void
GTP_komi( void )
{
  char* c_komi;
  
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
void
GTP_getkomi( void )
{
  char buf[256];
  
#if defined(_WIN32)
  sprintf_s(buf, 4, "%lf", komi[0]);
#else
  sprintf(buf, "%lf", komi[0]);
#endif
  GTP_response(buf, true);
}

/////////////////////////////
//  void GTP_finalscore()  //
/////////////////////////////
void
GTP_finalscore( void )
{
  char buf[10];
  double score = 0;
  
  score = UctAnalyze(game, S_BLACK) - komi[0];

#if defined(_WIN32)  
  if (score > 0) {
    sprintf_s(buf, 10, "B+%.1lf", score);
  } else {
    sprintf_s(buf, 10, "W+%.1lf", abs(score));
  }
#else
  if (score > 0) {
    sprintf(buf, "B+%.1lf", score);
  } else {
    sprintf(buf, "W+%.1lf", abs(score));
  }
#endif

  GTP_response(buf, true);
}

///////////////////////////////
//  void GTP_timesettings()  //
///////////////////////////////
void
GTP_timesettings( void )
{
  GTP_response(brank, true);
}

///////////////////////////
//  void GTP_timeleft()  //
///////////////////////////
void
GTP_timeleft( void )
{
  char *str1, *str2;

  str1 = STRTOK(NULL, DELIM, &next_token);
  str2 = STRTOK(NULL, DELIM, &next_token);

  if (str1[0] == 'B' || str1[0] == 'b'){
    remaining_time[S_BLACK] = atof(str2);
  } else if (str1[0] == 'W' || str1[0] == 'w'){
    remaining_time[S_WHITE] = atof(str2);
  }
  
  fprintf(stderr, "%f\n", remaining_time[S_BLACK]);
  fprintf(stderr, "%f\n", remaining_time[S_WHITE]);
  GTP_response(brank, true);
}

//////////////////////////
//  void GTP_version()  //
//////////////////////////
void
GTP_version( void )
{
  GTP_response(PROGRAM_VERSION, true);
}
 
////////////////////////////
//  void GTP_showboard()  //
////////////////////////////
void
GTP_showboard( void )
{
  PrintBoard(game);
  GTP_response(brank, true);
}

/////////////////////////////////
//  void GTP_fixed_handicap()  //
/////////////////////////////////
void
GTP_fixed_handicap( void )
{
  char *command;
  int num;
  char buf[1024];
  int handi[9];

  command = STRTOK(NULL, DELIM, &next_token);
  
#if defined (_WIN32)
  sscanf_s(command, "%d", &num);
  sprintf_s(buf, 1024, " ");
#else
  sscanf(command, "%d", &num);
  sprintf(buf, " ");
#endif

  handi[0] = POS(board_start + 3, board_start + 3);
  handi[1] = POS(board_start + 9, board_start + 3);
  handi[2] = POS(board_start + 15, board_start + 3);
  handi[3] = POS(board_start + 3, board_start + 9);
  handi[4] = POS(board_start + 9, board_start + 9);
  handi[5] = POS(board_start + 15, board_start + 9);
  handi[6] = POS(board_start + 3, board_start + 15);
  handi[7] = POS(board_start + 9, board_start + 15);
  handi[8] = POS(board_start + 15, board_start + 15);
  
  switch (num) {
    case 2:
      PutStone(game, handi[2], S_BLACK);
      PutStone(game, handi[6], S_BLACK);
#if defined (_WIN32)
      sprintf_s(buf, 1024, "%c%d %c%d",
		GOGUI_X(handi[2]), GOGUI_Y(handi[2]), GOGUI_X(handi[6]), GOGUI_Y(handi[6]));
#else
      sprintf(buf, "%c%d %c%d",
	      GOGUI_X(handi[2]), GOGUI_Y(handi[2]), GOGUI_X(handi[6]), GOGUI_Y(handi[6]));
#endif
      break;
    case 3:
      PutStone(game, handi[0], S_BLACK);
      PutStone(game, handi[2], S_BLACK);
      PutStone(game, handi[6], S_BLACK);
#if defined (_WIN32)
      sprintf_s(buf, 1024, "%c%d %c%d %c%d",
		GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[2]), GOGUI_Y(handi[2]),
		GOGUI_X(handi[6]), GOGUI_Y(handi[6]));
#else
      sprintf(buf, "%c%d %c%d %c%d",
	      GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[2]), GOGUI_Y(handi[2]),
	      GOGUI_X(handi[6]), GOGUI_Y(handi[6]));
#endif
      break;
    case 4:
      PutStone(game, handi[0], S_BLACK);
      PutStone(game, handi[2], S_BLACK);
      PutStone(game, handi[6], S_BLACK);
      PutStone(game, handi[8], S_BLACK);
#if defined (_WIN32)
      sprintf_s(buf, 1024, "%c%d %c%d %c%d %c%d",
		GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[2]), GOGUI_Y(handi[2]),
		GOGUI_X(handi[6]), GOGUI_Y(handi[6]), GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#else
      sprintf(buf, "%c%d %c%d %c%d %c%d",
	      GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[2]), GOGUI_Y(handi[2]),
	      GOGUI_X(handi[6]), GOGUI_Y(handi[6]), GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#endif
      break;
    case 5:
      PutStone(game, handi[0], S_BLACK);
      PutStone(game, handi[2], S_BLACK);
      PutStone(game, handi[4], S_BLACK);
      PutStone(game, handi[6], S_BLACK);
      PutStone(game, handi[8], S_BLACK);
#if defined (_WIN32)
      sprintf_s(buf, 1024, "%c%d %c%d %c%d %c%d %c%d", GOGUI_X(handi[0]), GOGUI_Y(handi[0]),
		GOGUI_X(handi[2]), GOGUI_Y(handi[2]), GOGUI_X(handi[4]), GOGUI_Y(handi[4]),
		GOGUI_X(handi[6]), GOGUI_Y(handi[6]), GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#else
      sprintf(buf, "%c%d %c%d %c%d %c%d %c%d", GOGUI_X(handi[0]), GOGUI_Y(handi[0]),
	      GOGUI_X(handi[2]), GOGUI_Y(handi[2]), GOGUI_X(handi[4]), GOGUI_Y(handi[4]),
	      GOGUI_X(handi[6]), GOGUI_Y(handi[6]), GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#endif
      break;
    case 6:
      PutStone(game, handi[0], S_BLACK);
      PutStone(game, handi[2], S_BLACK);
      PutStone(game, handi[3], S_BLACK);
      PutStone(game, handi[5], S_BLACK);
      PutStone(game, handi[6], S_BLACK);
      PutStone(game, handi[8], S_BLACK);
#if defined (_WIN32)
      sprintf_s(buf, 1024, "%c%d %c%d %c%d %c%d %c%d %c%d",
		GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[2]), GOGUI_Y(handi[2]),
		GOGUI_X(handi[3]), GOGUI_Y(handi[3]), GOGUI_X(handi[5]), GOGUI_Y(handi[5]),
		GOGUI_X(handi[6]), GOGUI_Y(handi[6]), GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#else
      sprintf(buf, "%c%d %c%d %c%d %c%d %c%d %c%d",
	      GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[2]), GOGUI_Y(handi[2]),
	      GOGUI_X(handi[3]), GOGUI_Y(handi[3]), GOGUI_X(handi[5]), GOGUI_Y(handi[5]),
	      GOGUI_X(handi[6]), GOGUI_Y(handi[6]), GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#endif
      break;
    case 7:
      PutStone(game, handi[0], S_BLACK);
      PutStone(game, handi[2], S_BLACK);
      PutStone(game, handi[3], S_BLACK);
      PutStone(game, handi[4], S_BLACK);
      PutStone(game, handi[5], S_BLACK);
      PutStone(game, handi[6], S_BLACK);
      PutStone(game, handi[8], S_BLACK);
#if defined (_WIN32)
      sprintf_s(buf, 1024, "%c%d %c%d %c%d %c%d %c%d %c%d %c%d",
		GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[2]), GOGUI_Y(handi[2]),
		GOGUI_X(handi[3]), GOGUI_Y(handi[3]), GOGUI_X(handi[4]), GOGUI_Y(handi[4]),
		GOGUI_X(handi[5]), GOGUI_Y(handi[5]), GOGUI_X(handi[6]), GOGUI_Y(handi[6]),
		GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#else
      sprintf(buf, "%c%d %c%d %c%d %c%d %c%d %c%d %c%d",
	      GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[2]), GOGUI_Y(handi[2]),
	      GOGUI_X(handi[3]), GOGUI_Y(handi[3]), GOGUI_X(handi[4]), GOGUI_Y(handi[4]),
	      GOGUI_X(handi[5]), GOGUI_Y(handi[5]), GOGUI_X(handi[6]), GOGUI_Y(handi[6]),
	      GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#endif
      break;
    case 8:
      PutStone(game, handi[0], S_BLACK);
      PutStone(game, handi[1], S_BLACK);
      PutStone(game, handi[2], S_BLACK);
      PutStone(game, handi[3], S_BLACK);
      PutStone(game, handi[5], S_BLACK);
      PutStone(game, handi[6], S_BLACK);
      PutStone(game, handi[7], S_BLACK);
      PutStone(game, handi[8], S_BLACK);
#if defined (_WIN32)
      sprintf_s(buf, 1024, "%c%d %c%d %c%d %c%d %c%d %c%d %c%d %c%d",
		GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[1]), GOGUI_Y(handi[2]),
		GOGUI_X(handi[2]), GOGUI_Y(handi[2]), GOGUI_X(handi[3]), GOGUI_Y(handi[3]),
		GOGUI_X(handi[5]), GOGUI_Y(handi[5]), GOGUI_X(handi[6]), GOGUI_Y(handi[6]),
		GOGUI_X(handi[7]), GOGUI_Y(handi[7]), GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#else
      sprintf(buf, "%c%d %c%d %c%d %c%d %c%d %c%d %c%d %c%d",
	      GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[1]), GOGUI_Y(handi[2]),
	      GOGUI_X(handi[2]), GOGUI_Y(handi[2]), GOGUI_X(handi[3]), GOGUI_Y(handi[3]),
	      GOGUI_X(handi[5]), GOGUI_Y(handi[5]), GOGUI_X(handi[6]), GOGUI_Y(handi[6]),
	      GOGUI_X(handi[7]), GOGUI_Y(handi[7]), GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#endif
      break;
    case 9:
      PutStone(game, handi[0], S_BLACK);
      PutStone(game, handi[1], S_BLACK);
      PutStone(game, handi[2], S_BLACK);
      PutStone(game, handi[3], S_BLACK);
      PutStone(game, handi[4], S_BLACK);
      PutStone(game, handi[5], S_BLACK);
      PutStone(game, handi[6], S_BLACK);
      PutStone(game, handi[7], S_BLACK);
      PutStone(game, handi[8], S_BLACK);
#if defined (_WIN32)
      sprintf_s(buf, 1024, "%c%d %c%d %c%d %c%d %c%d %c%d %c%d %c%d %c%d",
		GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[1]), GOGUI_Y(handi[1]),
		GOGUI_X(handi[2]), GOGUI_Y(handi[2]), GOGUI_X(handi[3]), GOGUI_Y(handi[3]),
		GOGUI_X(handi[4]), GOGUI_Y(handi[4]), GOGUI_X(handi[5]), GOGUI_Y(handi[5]),
		GOGUI_X(handi[6]), GOGUI_Y(handi[6]), GOGUI_X(handi[7]), GOGUI_Y(handi[7]),
		GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#else
      sprintf(buf, "%c%d %c%d %c%d %c%d %c%d %c%d %c%d %c%d %c%d",
	      GOGUI_X(handi[0]), GOGUI_Y(handi[0]), GOGUI_X(handi[1]), GOGUI_Y(handi[1]),
	      GOGUI_X(handi[2]), GOGUI_Y(handi[2]), GOGUI_X(handi[3]), GOGUI_Y(handi[3]),
	      GOGUI_X(handi[4]), GOGUI_Y(handi[4]), GOGUI_X(handi[5]), GOGUI_Y(handi[5]),
	      GOGUI_X(handi[6]), GOGUI_Y(handi[6]), GOGUI_X(handi[7]), GOGUI_Y(handi[7]),
	      GOGUI_X(handi[8]), GOGUI_Y(handi[8]));
#endif
      break;
  }
  SetKomi(0.5);
  SetHandicapNum(num);
  GTP_response(buf, true);
}

////////////////////////////////////
//  void GTP_set_free_handicap()  //
////////////////////////////////////
void
GTP_set_free_handicap( void )
{
  char *command;
  int pos, num = 0;
  
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
void
GTP_final_status_list( void )
{
  char dead[2048] = { 0 };
  char pos[5];
  int owner[BOARD_MAX]; 
  int x, y;
  char *command;
  
  OwnerCopy(owner);
  
  command = STRTOK(NULL, DELIM, &next_token);

  CHOMP(command);
  
  if (!strcmp(command, "dead")){
    for (y = board_start; y <= board_end; y++) {
      for (x = board_start; x <= board_end; x++) {
      	if ((game->board[POS(x, y)] == player_color && owner[POS(x, y)] <= 30) ||
      	    (game->board[POS(x, y)] == FLIP_COLOR(player_color) && owner[POS(x, y)] >= 70)) {
      #if defined (_WIN32)
      	  sprintf_s(pos, 5, "%c%d ", GOGUI_X(POS(x, y)), GOGUI_Y(POS(x, y)));
      	  strcat_s(dead, 2048, pos);
      #else
      	  sprintf(pos, "%c%d ", GOGUI_X(POS(x, y)), GOGUI_Y(POS(x, y)));
      	  strcat(dead, pos);
      #endif
      	}
      }
    }
  } else if (!strcmp(command, "alive")){
    for (y = board_start; y <= board_end; y++) {
      for (x = board_start; x <= board_end; x++) {
      	if ((game->board[POS(x, y)] == player_color && owner[POS(x, y)] >= 70) ||
      	    (game->board[POS(x, y)] == FLIP_COLOR(player_color) && owner[POS(x, y)] <= 30)) {
      #if defined (_WIN32)
      	  sprintf_s(pos, 5, "%c%d ", GOGUI_X(POS(x, y)), GOGUI_Y(POS(x, y)));
      	  strcat_s(dead, 2048, pos);
      #else
      	  sprintf(pos, "%c%d ", GOGUI_X(POS(x, y)), GOGUI_Y(POS(x, y)));
      	  strcat(dead, pos);
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
void
GTP_kgs_genmove_cleanup( void )
{
  char *command;
  char c;
  char pos[10];
  int color;
  int point = PASS;
  
  command = STRTOK(input_copy, DELIM, &next_token);
  
  CHOMP(command);
  if (!strcmp("genmove_black", command)) {
    color = S_BLACK;
  } else if (!strcmp("genmove_white", command)) {
    color = S_WHITE;
  } else {
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
  }
  
  player_color = color;
  
  point = UctSearchGenmoveCleanUp(game, color);
  if (point != RESIGN) {
    PutStone(game, point, color);
  }
  
  IntegerToString(point, pos);
  
  GTP_response(pos, true);
}