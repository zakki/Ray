#ifndef _SIMULATION_H_
#define _SIMULATION_H_

#include <random>

#include "GoBoard.h"
#include "UctSearch.h"

// 対局のシミュレーション(知識あり)
void Simulation( game_info_t *game, int color, std::mt19937_64 *mt );

#endif
