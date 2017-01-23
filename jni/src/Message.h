#ifndef _MESSAGE_H_
#define _MESSAGE_H_

#include "GoBoard.h"
#include "UctSearch.h"

//  エラーメッセージの出力の設定
void SetDebugMessageMode( bool flag );

//  盤面の表示
void PrintBoard( game_info_t *game );

//  連の情報の表示              
//    呼吸点の数, 座標          
//    連を構成する石の数, 座標  
//    隣接する敵の連のID
void PrintString( game_info_t *game );

//  各座標の連IDの表示  
void PrintStringID( game_info_t *game );

//  連リストの繋がりを表示(Debug用)
void PrintStringNext( game_info_t *game );

//  合法手である候補手を表示 
void PrintLegal( game_info_t *game, int color );

//  オーナーの表示
void PrintOwner( uct_node_t *root, int color, double *own );

//  最善応手列の表示
void PrintBestSequence( game_info_t *game, uct_node_t *uct_node, int root, int start_color );

//  探索の情報の表示
void PrintPlayoutInformation( uct_node_t *root, po_info_t *po_info, double finish_time, int pre_simulated );

//  座標の出力
void PrintPoint( int pos );

//  コミの値の出力
void PrintKomiValue( void );

//  Ponderingのプレイアウト回数の出力
void PrintPonderingCount( int count );

//  探索時間の出力
void PrintPlayoutLimits( double time_limit, int playout_limit );

//  再利用した探索回数の出力
void PrintReuseCount( int count );

#endif
