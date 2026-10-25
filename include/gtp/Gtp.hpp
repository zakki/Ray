/**
 * @file include/gtp/Gtp.hpp
 * @author Yuki Kobayashi
 * @~english
 * @brief Go text protocol client.
 * @~japanese
 * @brief Go Text Protocolクライアント
 */
#ifndef _GTP_HPP_
#define _GTP_HPP_


/**
 * @~english
 * @brief Maximum size of GTP command.
 * @~japanese
 * @brief GTPコマンドの文字数の上限
 */
constexpr int GTP_COMMAND_SIZE = 64;

/**
 * @~english
 * @brief Maximum length of GTP command input string.
 * @~japanese
 * @brief GTPコマンド入力文字列の上限
 */
constexpr int BUF_SIZE= 256;

/**
 * @~english
 * @brief Delimiter.
 * @~japanese
 * @brief コマンドの区切り文字(空白文字)
 */
constexpr char DELIM[] = " ";

/**
 * @~english
 * @brief Program name.
 * @~japanese
 * @brief プログラム名
 */
constexpr char PROGRAM_NAME[] = "Ray";

/**
 * @~english
 * @brief Program version.
 * @~japanese
 * @brief プログラムのバージョン
 */
constexpr char PROGRAM_VERSION[] = "11.2.0";

/**
 * @~english
 * @brief Go Text Protocol version.
 * @~japanese
 * @brief Go Text Protocolのバージョン
 */
constexpr char PROTOCOL_VERSION[] = "2";


/**
 * @struct GTP_command_t
 * @~english
 * @brief GTP command string and function.
 * @~japanese
 * @brief GTPコマンド文字列と関数
 */
struct GTP_command_t {
  /**
   * @~english
   * @brief GTP command string.
   * @~japanese
   * @brief GTPコマンド文字列
   */
  char command[GTP_COMMAND_SIZE];

  /**
   * @~english
   * @brief Function pointer to GTP command function.
   * @~japanese
   * @brief GTPコマンドに対応する関数へのポインタ
   */
  void (*function)();
};


/**
 * @def STRCPY(dst, size, src)
 * @~english
 * @brief Wrapper for strcpy function.
 * @~japanese
 * @brief strcpy関数のラッパー
 */
#if defined (_WIN32)
#define STRCPY(dst, size, src) strcpy_s((dst), (size), (src))
#else
#define STRCPY(dst, size, src) strcpy((dst), (src))
#endif

/**
 * @def STRTOK(src, token, next)
 * @~english
 * @brief Wrapper for strtok function
 * @~japanese
 * @brief strtok関数のラッパー
 */
#if defined (_WIN32)
#define STRTOK(src, token, next) strtok_s((src), (token), (next))
#else
#define STRTOK(src, token, next) strtok((src), (token))
#endif

/**
 * @def CHOMP(command)
 * @~english
 * @brief Remove last new line charactor.
 * @~japanese
 * @brief 末尾の改行の削除
 */
#define CHOMP(command) if(command[strlen(command)-1] == '\n') command[strlen(command)-1] = '\0'

// gtp本体
void GTP_main( void );

//  GTP initialization and single-command entry point.
void GTP_initialize( void );
extern "C" void GTP_exec( const char *line );

#endif
