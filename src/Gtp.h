#ifndef _GTP_H_
#define _GTP_H_

/////////////
//  定数  //
/////////////

//  GTPコマンドの文字数の上限
const int GTP_COMMAND_SIZE = 64;

//  GTPコマンド入力文字列の上限
const int BUF_SIZE = 256;

//  コマンドの区切り文字(空白文字)
#define DELIM  " "

//  プログラム名
#define PROGRAM_NAME  "Ray"

//  プログラムのバージョン
#define PROGRAM_VERSION  "9.0.1"

//  GTPのバージョン
#define PROTOCOL_VERSION  "2"


//////////////
//  構造体  //
//////////////

//  GTPコマンド処理用の構造体
struct GTP_command_t {
  char command[GTP_COMMAND_SIZE];
  void (*function)();
};


////////////
//  関数  //
////////////

#if defined (_WIN32)
#define STRCPY(dst, size, src) strcpy_s((dst), (size), (src))
#define STRTOK(src, token, next) strtok_s((src), (token), (next))
#else
#define STRCPY(dst, size, src) strcpy((dst), (src))
#define STRTOK(src, token, next) strtok((src), (token))
#endif

#define CHOMP(command) while (true) {\
    if(command[strlen(command)-1] == '\n' || command[strlen(command)-1] == '\r') command[strlen(command)-1] = '\0'; else break; \
  }

// gtp本体
void GTP_main( void );

#endif
