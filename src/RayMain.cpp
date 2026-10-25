/**
 * @file src/RayMain.cpp
 * @author Yuki Kobayashi
 * @~english
 * @brief Entry point for Ray.
 * @~japanese
 * @brief Rayのエントリーポイント
 */
#include <cstring>
#include <cstdio>
#if defined (_WIN32)
#include <windows.h>
#endif

#include "board/GoBoard.hpp"
#include "board/ZobristHash.hpp"
#include "feature/Semeai.hpp"
#include "gtp/Gtp.hpp"
#include "learn/FactorizationMachines.hpp"
#include "learn/MinorizationMaximization.hpp"
#include "learn/PatternAnalyzer.hpp"
#include "pattern/PatternHash.hpp"
#include "mcts/Rating.hpp"
#include "mcts/UctRating.hpp"
#include "mcts/UctSearch.hpp"
#include "util/Command.hpp"


/**
 * @~english
 * @brief Main function.
 * @param[in] argc The number of arguments.
 * @param[in] argv Command line arguments.
 * @return Return code.
 * @~japanese
 * @brief メイン関数
 * @param[in] argc コマンドライン引数の個数
 * @param[in] argv コマンドライン引数
 * @return 終了コード
 */
int
main( int argc, char **argv )
{
  char program_path[1024];
  int last;

  // 実行ファイルのあるディレクトリのパスを抽出
#if defined (_WIN32)
  HMODULE hModule = GetModuleHandle(NULL);
  GetModuleFileNameA(hModule, program_path, 1024);
#else
  strcpy(program_path, argv[0]);
#endif
  last = (int)strlen(program_path);
  while (last--){
#if defined (_WIN32)
    if (program_path[last] == '\\' || program_path[last] == '/') {
      program_path[last] = '\0';
      break;
    }
#else
    if (program_path[last] == '/') {
      program_path[last] = '\0';
      break;
    }
#endif
  }

  // ワーキングディレクトリの設定
  SetWorkingDirectory(program_path);

  // コマンドライン引数の解析  
  AnalyzeCommand(argc, argv);

  // 各種初期化
  InitializeConst();
  InitializeRating();
  InitializeUctRating();
  InitializeUctSearch();
  InitializeSearchSetting();
  InitializeHash();
  InitializeUctHash();
  SetNeighbor();

  //AnalyzePattern();

  //TrainBTModelByMinorizationMaximization();

  //TrainBTModelWithFactorizationMachines();

  // GTP
#if defined (RAY_WEB)
  GTP_initialize();
#else
  GTP_main();
#endif

  return 0;
}
