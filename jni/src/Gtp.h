#ifndef _GTP_H_
#define _GTP_H_

const int GTP_COMMAND_NUM = 25;

const int BUF_SIZE = 256;

#define DELIM  " "
#define PROGRAM_NAME  "Ray"
#define PROGRAM_VERSION  "8.0.1"
#define PROTOCOL_VERSION  "2"

#if defined (_WIN32)
#define STRDUP(var) _strdup((var))
#define STRCPY(dst, size, src) strcpy_s((dst), (size), (src))
#define STRTOK(src, token, next) strtok_s((src), (token), (next))
#else
#define STRDUP(var) strdup((var))
#define STRCPY(dst, size, src) strcpy((dst), (src))
#define STRTOK(src, token, next) strtok((src), (token))
#endif

typedef struct {
  void (*function)();
  char *type;
  char *label;
  char *command;
} GTP_command_t;

#define CHOMP(command) if(command[strlen(command)-1] == '\n') command[strlen(command)-1] = '\0'

// gtp本体
void GTP_main( void );
// gtpの出力
void GTP_message( void );
// gtpコマンドを設定する
void GTP_setCommand( void );
// gtpの出力用関数
void GTP_response( const char *res, bool success );
// boardsizeコマンドを処理
void GTP_boardsize( void );
// clearboardコマンドを処理
void GTP_clearboard( void );
// nameコマンドを処理
void GTP_name( void );
// protocolversionコマンドを処理
void GTP_protocolversion( void );
// genmoveコマンドを処理
void GTP_genmove( void );
// playコマンドを処理
void GTP_play( void );
// knowncommandコマンドを処理
void GTP_knowncommand( void );
// listcommandsコマンドを処理
void GTP_listcommands( void );
// quitコマンドを処理
void GTP_quit( void );
// komiコマンドを処理
void GTP_komi( void );
// getkomiコマンドを処理
void GTP_getkomi( void );
// finalscoreコマンドを処理
void GTP_finalscore( void );
// timesettingsコマンドを処理
void GTP_timesettings( void );
// timeleftコマンドを処理
void GTP_timeleft( void );
// versionコマンドを処理
void GTP_version( void );
// showboardコマンドを処理
void GTP_showboard( void );
// kgs-genmove_cleanupコマンドを処理
void GTP_kgs_genmove_cleanup( void );
// final_status_listコマンドを処理
void GTP_final_status_list( void );
// set_free_handicapコマンドを処理
void GTP_set_free_handicap( void );
// fixed_handicapコマンドを処理
void GTP_fixed_handicap( void );

#endif
