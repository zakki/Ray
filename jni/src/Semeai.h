#ifndef _SEMEAI_H_
#define _SEMEAI_H_

#include "GoBoard.h"

enum LIBERTY_STATE {
  L_DECREASE,
  L_EVEN,
  L_INCREASE,
};

//  1手で取れるアタリの判定
bool IsCapturableAtari( game_info_t *game, int pos, int color, int opponent_pos );

//  オイオトシの確認
int CheckOiotoshi( game_info_t *game, int pos, int color, int opponent_pos );

//  ウッテガエシ用の判定
int CapturableCandidate( game_info_t *game, int id );

//  すぐに捕まる手かどうかを判定  
bool IsDeadlyExtension( game_info_t *game, int color, int id );

//  隣接する敵連が取れるかを判定  
bool IsCapturableNeighborNone( game_info_t *game, int id );

//  呼吸点がどのように変化するかを確認
int CheckLibertyState( game_info_t *game, int pos, int color, int id );

//  自己アタリになるトリかどうか判定
bool IsSelfAtariCapture( game_info_t *game, int pos, int color, int id );

//  1手で取れるアタリ(シミュレーション用)
bool IsCapturableAtariForSimulation( game_info_t *game, int pos, int color, int id );

//  自己アタリになるトリかどうか判定
bool IsSelfAtariCaptureForSimulation( game_info_t *game, int pos, int color, int lib );

#endif
