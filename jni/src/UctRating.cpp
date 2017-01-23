#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cstdio>
#include <iostream>

#include "Ladder.h"
#include "Message.h"
#include "Nakade.h"
#include "PatternHash.h"
#include "Point.h"
#include "Semeai.h"
#include "Utility.h"
#include "UctRating.h"

using namespace std;

// 各種レートが格納されているディレクトリへのパス
char uct_params_path[1024];

// w_0
static double weight_zero;
// 戦術的特徴のレート
static latent_factor_t uct_tactical_features[UCT_TACTICAL_FEATURE_MAX];
// 盤上の位置のレート
static latent_factor_t uct_pos_id[POS_ID_MAX];
// パスのレート
static latent_factor_t uct_pass[UCT_PASS_MAX];
// 直前の着手からの距離のレート
static latent_factor_t uct_move_distance_1[MOVE_DISTANCE_MAX];
// 2手前の着手からの距離のレート
static latent_factor_t uct_move_distance_2[MOVE_DISTANCE_MAX];
// 3x3パターンのレート
static latent_factor_t uct_pat3[PAT3_LIMIT];
// マンハッタン距離2のパターンのレート
// latent_factor_t uct_md2[MD2_LIMIT];
static latent_factor_t *uct_md2;
// マンハッタン距離3のパターンのレート
static latent_factor_t uct_md3[LARGE_PAT_MAX];
// マンハッタン距離4のパターンのレート
static latent_factor_t uct_md4[LARGE_PAT_MAX];
// マンハッタン距離5のパターンのレート
static latent_factor_t uct_md5[LARGE_PAT_MAX];
// オーナーのレート
double uct_owner[OWNER_MAX];
// クリティカリティのレート
double uct_criticality[CRITICALITY_MAX];

// index_hash_t md3_index[HASH_MAX];
// index_hash_t md4_index[HASH_MAX];
// index_hash_t md5_index[HASH_MAX];
index_hash_t *md3_index;
index_hash_t *md4_index;
index_hash_t *md5_index;

static int pat3_index[PAT3_MAX];
// int md2_index[MD2_MAX];
static int *md2_index;

static game_info_t snapback_game;

// 戦術的特徴のビットマスク
const unsigned long long uct_mask[UCT_MASK_MAX] = {
  0x0000000000000001, 0x0000000000000002, 0x0000000000000004, 0x0000000000000008,   
  0x0000000000000010, 0x0000000000000020, 0x0000000000000040, 0x0000000000000080,   
  0x0000000000000100, 0x0000000000000200, 0x0000000000000400, 0x0000000000000800,   
  0x0000000000001000, 0x0000000000002000, 0x0000000000004000, 0x0000000000008000,   
  0x0000000000010000, 0x0000000000020000, 0x0000000000040000, 0x0000000000080000,   
  0x0000000000100000, 0x0000000000200000, 0x0000000000400000, 0x0000000000800000,   
  0x0000000001000000, 0x0000000002000000, 0x0000000004000000, 0x0000000008000000,   
  0x0000000010000000, 0x0000000020000000, 0x0000000040000000, 0x0000000080000000,   
  0x0000000100000000, 0x0000000200000000, 0x0000000400000000, 0x0000000800000000,   
  0x0000001000000000, 0x0000002000000000, 0x0000004000000000, 0x0000008000000000,   
  0x0000010000000000, 0x0000020000000000, 0x0000040000000000, 0x0000080000000000,   
  0x0000100000000000, 0x0000200000000000, 0x0000400000000000, 0x0000800000000000,   
  0x0001000000000000, 0x0002000000000000, 0x0004000000000000, 0x0008000000000000,   
  0x0010000000000000, 0x0020000000000000, 0x0040000000000000, 0x0080000000000000,   
  0x0100000000000000, 0x0200000000000000, 0x0400000000000000, 0x0800000000000000,   
  0x1000000000000000, 0x2000000000000000, 0x4000000000000000, 0x8000000000000000,   
};

//static double criticality_init = CRITICALITY_INIT;
static double criticality_bias = CRITICALITY_BIAS;
static double owner_bias = OWNER_BIAS;
static double owner_k = OWNER_K;

unsigned long long atari_mask, capture_mask;

//  γ読み込み
static void InputUCTParameter( void );
//  読み込み 
static void InputLatentFactor( const char *filename, latent_factor_t *lf, int n );
//  読み込み Pat3
static void InputPat3( const char *filename, latent_factor_t *lf );
//  読み込み MD2
static void InputMD2( const char *filename, latent_factor_t *lf );
//  読み込み
static void InputLargePattern( const char *filename, latent_factor_t *lf, index_hash_t *pat_index );

//////////////////////
//  γ値の初期設定  //
//////////////////////
void
InitializeUctRating()
{
  int i;
  //  γ読み込み
  InputUCTParameter();

  for (i = UCT_SAVE_CAPTURE_1_1; i <= UCT_SEMEAI_CAPTURE; i++) {
    capture_mask |= uct_mask[i];
  }

  for (i = UCT_ATARI; i <= UCT_3POINT_C_ATARI_L_L; i++) {
    atari_mask |= uct_mask[i];
  }
}

/////////////////////////////////////////
//  呼吸点が1つの連に対する特徴の判定  //
/////////////////////////////////////////
void
UctCheckFeaturesLib1( game_info_t *game, int color, int id, bool ladder, uct_features_t *uct_features )
{
  string_t *string = game->string;
  int lib, neighbor;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;

  // 呼吸点が1つになった連の呼吸点を取り出す
  lib = string[id].lib[0];

  // シチョウを逃げる手かどうかで特徴を判定
  if (ladder) {
    tactical_features1[lib] |= uct_mask[UCT_LADDER_EXTENSION];
  } else {
    if (string[id].size == 1) {
      tactical_features1[lib] |= uct_mask[UCT_SAVE_EXTENSION_1];
    } else if (string[id].size == 2) {
      tactical_features1[lib] |= uct_mask[UCT_SAVE_EXTENSION_2];
    } else {
      tactical_features1[lib] |= uct_mask[UCT_SAVE_EXTENSION_3];
    }
  }

  // 敵連を取ることによって連を助ける手の特徴の判定
  // 自分の連の大きさと敵の連の大きさで特徴を判定
  neighbor = string[id].neighbor[0];
  while (neighbor != NEIGHBOR_END) {
    if (string[neighbor].libs == 1) {
      lib = string[neighbor].lib[0];
      if (string[id].size == 1) {
      	if (string[neighbor].size == 1) {
      	  tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_1_1];
      	} else if (string[neighbor].size == 2) {
      	  tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_1_2];
      	} else {
      	  tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_1_3];
      	}
      } else if (string[id].size == 2){
      	if (string[neighbor].size == 1) {
      	  if (IsSelfAtariCapture(game, lib, color, id)) {
      	    tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_SELF_ATARI];
      	  } else {
      	    tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_2_1];
      	  }
      	} else if (string[neighbor].size == 2) {
      	  tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_2_2];
      	} else {
      	  tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_2_3];
      	}
      } else {
      	if (string[neighbor].size == 1) {
      	  if (IsSelfAtariCapture(game, lib, color, id)) {
      	    tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_SELF_ATARI];
      	  } else {
      	    tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_3_1];
      	  }
      	} else if (string[neighbor].size == 2) {
      	  tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_3_2];
      	} else {
      	  tactical_features1[lib] |= uct_mask[UCT_SAVE_CAPTURE_3_3];
      	}
      }
    }
    neighbor = string[id].neighbor[neighbor];
  }
}

/////////////////////////////////////////
//  呼吸点が2つの連に対する特徴の判定  //
/////////////////////////////////////////
void
UctCheckFeaturesLib2( game_info_t *game, int color, int id, uct_features_t *uct_features )
{
  string_t *string = game->string;
  int lib1, lib2, neighbor;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int lib1_state, lib2_state;

  // 呼吸点が2つになった連の呼吸点を取り出す
  lib1 = string[id].lib[0];
  lib2 = string[id].lib[lib1];

  // 呼吸点に打つ特徴
  lib1_state = CheckLibertyState(game, lib1, color, id);
  lib2_state = CheckLibertyState(game, lib2, color, id);
  switch (lib1_state) {
  case L_DECREASE :
    tactical_features1[lib1] |= uct_mask[UCT_2POINT_EXTENSION_DECREASE];
    break;
  case L_EVEN :
    tactical_features1[lib1] |= uct_mask[UCT_2POINT_EXTENSION_EVEN];
    break;
  case L_INCREASE:
    tactical_features1[lib1] |= uct_mask[UCT_2POINT_EXTENSION_INCREASE];
    break;
  default :
    break;
  }
  switch (lib2_state) {
  case L_DECREASE :
    tactical_features1[lib2] |= uct_mask[UCT_2POINT_EXTENSION_DECREASE];
    break;
  case L_EVEN :
    tactical_features1[lib2] |= uct_mask[UCT_2POINT_EXTENSION_EVEN];
    break;
  case L_INCREASE:
    tactical_features1[lib2] |= uct_mask[UCT_2POINT_EXTENSION_INCREASE];
    break;
  default :
    break;
  }

  // 呼吸点が2つになった連の周囲の敵連を調べる
  // 1. 呼吸点が1つの敵連
  // 2. 呼吸点が2つの敵連
  // それぞれに対して, 特徴を判定する
  // さらに2.に関しては1手で取れるかどうかも考慮する
  neighbor = string[id].neighbor[0];
  if (string[id].size <= 2) {
    while (neighbor != NEIGHBOR_END) {
      if (string[neighbor].libs == 1) {
      	lib1 = string[neighbor].lib[0];
      	if (string[neighbor].size <= 2) {
      	  tactical_features1[lib1] |= uct_mask[UCT_2POINT_CAPTURE_S_S];
      	} else {
      	  tactical_features1[lib1] |= uct_mask[UCT_2POINT_CAPTURE_S_L];
      	}
      } else if (string[neighbor].libs == 2) {
      	lib1 = string[neighbor].lib[0];
      	lib2 = string[neighbor].lib[lib1];
      	if (string[neighbor].size <= 2) {
      	  if (IsCapturableAtari(game, lib1, color, string[neighbor].origin)) {
      	    tactical_features1[lib1] |= uct_mask[UCT_2POINT_C_ATARI_S_S];
      	  } else {
      	    tactical_features1[lib1] |= uct_mask[UCT_2POINT_ATARI_S_S];
      	  }
      	  if (IsCapturableAtari(game, lib2, color, string[neighbor].origin)) {
      	    tactical_features1[lib2] |= uct_mask[UCT_2POINT_C_ATARI_S_S];
      	  } else {
      	    tactical_features1[lib2] |= uct_mask[UCT_2POINT_ATARI_S_S];
      	  }
      	} else {
      	  if (IsCapturableAtari(game, lib1, color, string[neighbor].origin)) {
      	    tactical_features1[lib1] |= uct_mask[UCT_2POINT_C_ATARI_S_L];
      	  } else {
      	    tactical_features1[lib1] |= uct_mask[UCT_2POINT_ATARI_S_L];
      	  }
      	  if (IsCapturableAtari(game, lib2, color, string[neighbor].origin)) {
      	    tactical_features1[lib2] |= uct_mask[UCT_2POINT_C_ATARI_S_L];
      	  } else {
      	    tactical_features1[lib2] |= uct_mask[UCT_2POINT_ATARI_S_L];
      	  }
      	}
      }
      neighbor = string[id].neighbor[neighbor];
    }
  } else {
    while (neighbor != NEIGHBOR_END) {
      if (string[neighbor].libs == 1) {
      	lib1 = string[neighbor].lib[0];
      	if (string[neighbor].size <= 2) {
      	  tactical_features1[lib1] |= uct_mask[UCT_2POINT_CAPTURE_L_S];
      	} else {
      	  tactical_features1[lib1] |= uct_mask[UCT_2POINT_CAPTURE_L_L];
      	}
      } else if (string[neighbor].libs == 2) {
      	lib1 = string[neighbor].lib[0];
      	lib2 = string[neighbor].lib[lib1];
      	if (string[neighbor].size <= 2) {
      	  if (IsCapturableAtari(game, lib1, color, string[neighbor].origin)) {
      	    tactical_features1[lib1] |= uct_mask[UCT_2POINT_C_ATARI_L_S];
      	  } else {
      	    tactical_features1[lib1] |= uct_mask[UCT_2POINT_ATARI_L_S];
      	  }
      	  if (IsCapturableAtari(game, lib2, color, string[neighbor].origin)) {
      	    tactical_features1[lib2] |= uct_mask[UCT_2POINT_C_ATARI_L_S];
      	  } else {
      	    tactical_features1[lib2] |= uct_mask[UCT_2POINT_ATARI_L_S];
      	  }
      	} else {
      	  if (IsCapturableAtari(game, lib1, color, string[neighbor].origin)) {
      	    tactical_features1[lib1] |= uct_mask[UCT_2POINT_C_ATARI_L_L];
      	  } else {
      	    tactical_features1[lib1] |= uct_mask[UCT_2POINT_ATARI_L_L];
      	  }
      	  if (IsCapturableAtari(game, lib2, color, string[neighbor].origin)) {
      	    tactical_features1[lib2] |= uct_mask[UCT_2POINT_C_ATARI_L_L];
      	  } else {
      	    tactical_features1[lib2] |= uct_mask[UCT_2POINT_ATARI_L_L];
      	  }
      	}
      }
      neighbor = string[id].neighbor[neighbor];
    }
  }
}

/////////////////////////////////////////
//  呼吸点が3つの連に対する特徴の判定  //
/////////////////////////////////////////
void
UctCheckFeaturesLib3( game_info_t *game, int color, int id, uct_features_t *uct_features )
{
  string_t *string = game->string;
  int lib1, lib2, lib3, neighbor;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int lib1_state, lib2_state, lib3_state;

  // 呼吸点が3つになった連の呼吸点を取り出す
  lib1 = string[id].lib[0];
  lib2 = string[id].lib[lib1];
  lib3 = string[id].lib[lib2];

  // 呼吸点に打つ特徴
  lib1_state = CheckLibertyState(game, lib1, color, id);
  lib2_state = CheckLibertyState(game, lib2, color, id);
  lib3_state = CheckLibertyState(game, lib3, color, id);
  switch (lib1_state) {
    case L_DECREASE :
      tactical_features1[lib1] |= uct_mask[UCT_3POINT_EXTENSION_DECREASE];
      break;
    case L_EVEN :
      tactical_features1[lib1] |= uct_mask[UCT_3POINT_EXTENSION_EVEN];
      break;
    case L_INCREASE:
      tactical_features1[lib1] |= uct_mask[UCT_3POINT_EXTENSION_INCREASE];
      break;
    default :
      break;
  }
  switch (lib2_state) {
    case L_DECREASE :
      tactical_features1[lib2] |= uct_mask[UCT_3POINT_EXTENSION_DECREASE];
      break;
    case L_EVEN :
      tactical_features1[lib2] |= uct_mask[UCT_3POINT_EXTENSION_EVEN];
      break;
    case L_INCREASE:
      tactical_features1[lib2] |= uct_mask[UCT_3POINT_EXTENSION_INCREASE];
      break;
    default :
      break;
  }
  switch (lib3_state) {
    case L_DECREASE :
      tactical_features1[lib3] |= uct_mask[UCT_3POINT_EXTENSION_DECREASE];
      break;
    case L_EVEN :
      tactical_features1[lib3] |= uct_mask[UCT_3POINT_EXTENSION_EVEN];
      break;
    case L_INCREASE:
      tactical_features1[lib3] |= uct_mask[UCT_3POINT_EXTENSION_INCREASE];
      break;
    default :
      break;
  }

  // 呼吸点が3つになった連の周囲の敵連を調べる
  // 1. 呼吸点が1つの敵連
  // 2. 呼吸点が2つの敵連
  // 3. 呼吸点が3つの敵連
  // それぞれに対して, 特徴を判定する
  // さらに2に関しては1手で取れるかを考慮する

  neighbor = string[id].neighbor[0];
  if (string[id].size <= 2) {
    while (neighbor != NEIGHBOR_END) {
      if (string[neighbor].libs == 1) {
      	lib1 = string[neighbor].lib[0];
      	if (string[neighbor].size <= 2) {
      	  tactical_features1[lib1] |= uct_mask[UCT_3POINT_CAPTURE_S_S];
      	} else {
      	  tactical_features1[lib1] |= uct_mask[UCT_3POINT_CAPTURE_S_L];
      	}
      } else if (string[neighbor].libs == 2) {
      	lib1 = string[neighbor].lib[0];
      	lib2 = string[neighbor].lib[lib1];
      	if (string[neighbor].size <= 2) {
      	  if (IsCapturableAtari(game, lib1, color, string[neighbor].origin)) {
      	    tactical_features1[lib1] |= uct_mask[UCT_3POINT_C_ATARI_S_S];
      	  } else {
      	    tactical_features1[lib1] |= uct_mask[UCT_3POINT_ATARI_S_S];
      	  }
      	  if (IsCapturableAtari(game, lib2, color, string[neighbor].origin)) {
      	    tactical_features1[lib2] |= uct_mask[UCT_3POINT_C_ATARI_S_S];
      	  } else {
      	    tactical_features1[lib2] |= uct_mask[UCT_3POINT_ATARI_S_S];
      	  }
      	} else {
      	  if (IsCapturableAtari(game, lib1, color, string[neighbor].origin)) {
      	    tactical_features1[lib1] |= uct_mask[UCT_3POINT_C_ATARI_S_L];
      	  } else {
      	    tactical_features1[lib1] |= uct_mask[UCT_3POINT_ATARI_S_L];
      	  }
      	  if (IsCapturableAtari(game, lib2, color, string[neighbor].origin)) {
      	    tactical_features1[lib2] |= uct_mask[UCT_3POINT_C_ATARI_S_L];
      	  } else {
      	    tactical_features1[lib2] |= uct_mask[UCT_3POINT_ATARI_S_L];
      	  }
      	}
      } else if (string[neighbor].libs == 3) {
      	lib1 = string[neighbor].lib[0];
      	lib2 = string[neighbor].lib[lib1];
      	lib3 = string[neighbor].lib[lib2];
      	if (string[neighbor].size <= 2) {
      	  tactical_features1[lib1] |= uct_mask[UCT_3POINT_DAME_S_S];
      	  tactical_features1[lib2] |= uct_mask[UCT_3POINT_DAME_S_S];
      	  tactical_features1[lib3] |= uct_mask[UCT_3POINT_DAME_S_S];
      	} else {
      	  tactical_features1[lib1] |= uct_mask[UCT_3POINT_DAME_S_L];
      	  tactical_features1[lib2] |= uct_mask[UCT_3POINT_DAME_S_L];
      	  tactical_features1[lib3] |= uct_mask[UCT_3POINT_DAME_S_L];
      	}
      }
      neighbor = string[id].neighbor[neighbor];
    }
  } else {
    while (neighbor != NEIGHBOR_END) {
      if (string[neighbor].libs == 1) {
      	lib1 = string[neighbor].lib[0];
      	if (string[neighbor].size <= 2) {
      	  tactical_features1[lib1] |= uct_mask[UCT_3POINT_CAPTURE_L_S];
      	} else {
      	  tactical_features1[lib1] |= uct_mask[UCT_3POINT_CAPTURE_L_L];
      	}
      } else if (string[neighbor].libs == 2) {
      	lib1 = string[neighbor].lib[0];
      	lib2 = string[neighbor].lib[lib1];
      	if (string[neighbor].size <= 2) {
      	  if (IsCapturableAtari(game, lib1, color, string[neighbor].origin)) {
      	    tactical_features1[lib1] |= uct_mask[UCT_3POINT_C_ATARI_L_S];
      	  } else {
      	    tactical_features1[lib1] |= uct_mask[UCT_3POINT_ATARI_L_S];
      	  }
      	  if (IsCapturableAtari(game, lib2, color, string[neighbor].origin)) {
      	    tactical_features1[lib2] |= uct_mask[UCT_3POINT_C_ATARI_L_S];
      	  } else {
      	    tactical_features1[lib2] |= uct_mask[UCT_3POINT_ATARI_L_S];
      	  }
      	} else {
      	  if (IsCapturableAtari(game, lib1, color, string[neighbor].origin)) {
      	    tactical_features1[lib1] |= uct_mask[UCT_3POINT_C_ATARI_L_L];
      	  } else {
      	    tactical_features1[lib1] |= uct_mask[UCT_3POINT_ATARI_L_L];
      	  }
      	  if (IsCapturableAtari(game, lib2, color, string[neighbor].origin)) {
      	    tactical_features1[lib2] |= uct_mask[UCT_3POINT_C_ATARI_L_L];
      	  } else {
      	    tactical_features1[lib2] |= uct_mask[UCT_3POINT_ATARI_L_L];
      	  }
      	}
      } else if (string[neighbor].libs == 3) {
      	lib1 = string[neighbor].lib[0];
      	lib2 = string[neighbor].lib[lib1];
      	lib3 = string[neighbor].lib[lib2];
      	if (string[neighbor].size <= 2) {
      	  tactical_features1[lib1] |= uct_mask[UCT_3POINT_DAME_L_S];
      	  tactical_features1[lib2] |= uct_mask[UCT_3POINT_DAME_L_S];
      	  tactical_features1[lib3] |= uct_mask[UCT_3POINT_DAME_L_S];
      	} else {
      	  tactical_features1[lib1] |= uct_mask[UCT_3POINT_DAME_L_L];
      	  tactical_features1[lib2] |= uct_mask[UCT_3POINT_DAME_L_L];
      	  tactical_features1[lib3] |= uct_mask[UCT_3POINT_DAME_L_L];
      	}
      }
      neighbor = string[id].neighbor[neighbor];
    }
  }
}

//////////////////
//  特徴の判定  //
//////////////////
void
UctCheckFeatures( game_info_t *game, int color, uct_features_t *uct_features )
{
  string_t *string = game->string;
  char *board = game->board;
  int *string_id = game->string_id;
  int previous_move = PASS;
  int id;
  int check[4] = { 0 };
  int checked = 0;
  bool ladder;
  int neighbor4[4];
  int i, j;
  bool already_checked;

  if (game->moves > 1) previous_move = game->record[game->moves - 1].pos;

  if (previous_move == PASS) return;

  GetNeighbor4(neighbor4, previous_move);

  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == color) {
      id = string_id[neighbor4[i]];
      already_checked = false;
      for (j = 0; j < checked; j++) {
      	if (check[j] == id) {
      	  already_checked = true;
      	  break;
      	}
      }
      if (already_checked) continue;
      if (string[id].libs == 1) {
      	ladder = CheckLadderExtension(game, color, neighbor4[i]);
      	UctCheckFeaturesLib1(game, color, id, ladder, uct_features);
      } else if (string[id].libs == 2) {
	      UctCheckFeaturesLib2(game, color, id, uct_features);
      } else if (string[id].libs == 3) {
	      UctCheckFeaturesLib3(game, color, id, uct_features);
      }
      check[checked++] = id;
    }
  }
}

////////////////////////
//  劫を解消するトリ  //
////////////////////////
void
UctCheckCaptureAfterKo( game_info_t *game, int color, uct_features_t *uct_features )
{
  string_t *string = game->string;
  char *board = game->board;
  int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  int previous_move_2 = game->record[game->moves - 2].pos;
  int id, lib, i;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int neighbor4[4];

  GetNeighbor4(neighbor4, previous_move_2);

  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == other) {
      id = string_id[neighbor4[i]];
      if (string[id].libs == 1) {
	lib = string[id].lib[0];
	tactical_features1[lib] |= uct_mask[UCT_CAPTURE_AFTER_KO];
      }
    }
  }
}

//////////////////
//  自己アタリ  //
//////////////////
bool
UctCheckSelfAtari( game_info_t *game, int color, int pos, uct_features_t *uct_features )
{
  bool flag;
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  int size = 0;
  int already[4] = { 0 };
  int already_num = 0;
  int id;
  int lib, count, libs = 0;
  int i, j;
  int lib_candidate[PURE_BOARD_MAX];   
  bool checked;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int neighbor4[4];
  bool already_checked;

  GetNeighbor4(neighbor4, pos);

  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == S_EMPTY) {
      lib_candidate[libs++] = neighbor4[i];
    }
  }

  //  空点
  if (libs >= 2) return true;

  //  上下左右の確認
  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == color) {
      id = string_id[neighbor4[i]];
      already_checked = false;
      for (j = 0; j < already_num; j++) {
      	if (already[j] == id) {
      	  already_checked = true;
      	}
      }
      if (already_checked) continue;

      if (string[id].libs > 2) return true;
      lib = string[id].lib[0];
      count = 0;
      while (lib != LIBERTY_END) {
      	if (lib != pos) {
      	  checked = false;
      	  for (j = 0; j < libs; j++) {
      	    if (lib_candidate[j] == lib) {
      	      checked = true;
      	      break;
      	    }
      	  }
      	  if (!checked) {
      	    lib_candidate[libs + count] = lib;
      	    count++;
      	  }
      	}
	      lib = string[id].lib[lib];
      }
      libs += count;
      size += string[id].size;
      already[already_num++] = id;
      if (libs >= 2) return true;
    } else if (board[neighbor4[i]] == other &&
	       string[string_id[neighbor4[i]]].libs == 1) {
      return true;
    }
  }

  // 自己アタリの分類
  // 1.大きさが2以下の自己アタリ
  // 2.大きさが6以下でナカデの形になる自己アタリ
  // 3.それ以外の自己アタリ 
  if (size < 2) {
    tactical_features1[pos] |= uct_mask[UCT_SELF_ATARI_SMALL];
    flag = true;
  } else if (size < 6) {
    if (IsUctNakadeSelfAtari(game, pos, color)) {
      tactical_features1[pos] |= uct_mask[UCT_SELF_ATARI_NAKADE];
      flag = true;
    } else {
      tactical_features1[pos] |= uct_mask[UCT_SELF_ATARI_LARGE];
      flag = false;
    }
  } else {
    tactical_features1[pos] |= uct_mask[UCT_SELF_ATARI_LARGE];
    flag = false;
  }
  return flag;
}

//////////////////
//  トリの判定  //
//////////////////
void
UctCheckCapture( game_info_t *game, int color, int pos, uct_features_t *uct_features )
{
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int other = FLIP_COLOR(color);
  bool check;
  int neighbor;
  int id;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int neighbor4[4];
  int i;

  GetNeighbor4(neighbor4, pos);

  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == other) {
      if (string[string_id[neighbor4[i]]].libs == 1) {
	check = false;
	id = string_id[neighbor4[i]];
	neighbor = string[id].neighbor[0];
	while (neighbor != NEIGHBOR_END) {
	  if (string[neighbor].libs == 1) {
	    check = true;
	    break;
	  }
	  neighbor = string[id].neighbor[neighbor];
	}
	if (check) {
	  tactical_features1[pos] |= uct_mask[UCT_SEMEAI_CAPTURE];
	  return;
	} else {
	  tactical_features1[pos] |= uct_mask[UCT_CAPTURE];
	}
      }
    }
  }
}

////////////////////
//  アタリの判定  //
////////////////////
void
UctCheckAtari( game_info_t *game, int color, int pos, uct_features_t *uct_features )
{
  char *board = game->board;
  string_t *string = game->string;
  int other = FLIP_COLOR(color);
  int *string_id = game->string_id;
  int id;
  int size;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int neighbor4[4], i;

  GetNeighbor4(neighbor4, pos);

  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == other) {
      id = string_id[neighbor4[i]];
      if (string[id].libs == 2) {
      	size = CheckOiotoshi(game, pos, color, neighbor4[i]);
      	if (size > 4) {
      	  tactical_features1[pos] |= uct_mask[UCT_OIOTOSHI];
      	  return;
      	} else if (size > 0) {
      	  tactical_features1[pos] |= uct_mask[UCT_CAPTURABLE_ATARI];
      	} else {
      	  tactical_features1[pos] |= uct_mask[UCT_ATARI];
      	}
      }
    }
  }
}

////////////////
//  劫の解消  //
////////////////
void
UctCheckKoConnection( game_info_t *game, uct_features_t *uct_features )
{
  int ko_pos = game->ko_pos;

  if (game->ko_move == game->moves - 2) {
    uct_features->tactical_features1[ko_pos] |= uct_mask[UCT_KO_CONNECTION];
  }
}

////////////////////////////////
// 2目取られた後のホウリコミ  //
////////////////////////////////
void
UctCheckRemove2Stones( game_info_t *game, int color, uct_features_t *uct_features )
{
  int i, rm1, rm2, connect;
  int other = FLIP_COLOR(color);
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int cross[4];

  cross[0] = - board_size - 1;
  cross[1] = - board_size + 1;
  cross[2] =   board_size - 1;
  cross[3] =   board_size + 1;

  if (game->capture_num[other] != 2) {
    return;
  }

  rm1 = game->capture_pos[other][0];
  rm2 = game->capture_pos[other][1];

  if (rm1 - rm2 != 1 &&
      rm2 - rm1 != 1 &&
      rm1 - rm2 != board_size &&
      rm2 - rm1 != board_size) {
    return;
  }

  for (i = 0, connect = 0; i < 4; i++) {
    if ((game->board[rm1 + cross[i]] & color) == color) {
      connect++;
    }
  }

  if (connect >= 2) {
    tactical_features1[rm1] |= uct_mask[UCT_THROW_IN_2];
  }


  for (i = 0, connect = 0; i < 4; i++) {
    if ((game->board[rm2 + cross[i]] & color) == color) {
      connect++;
    }
  }

  if (connect >= 2) {
    tactical_features1[rm2] |= uct_mask[UCT_THROW_IN_2];
  }
}

/////////////////////////////
//  3目抜かれた後のナカデ  //
/////////////////////////////
void
UctCheckRemove3Stones( game_info_t *game, int color, uct_features_t *uct_features )
{
  int rm1, rm2, rm3;
  int other = FLIP_COLOR(color);
  unsigned long long *tactical_features1 = uct_features->tactical_features1;

  if (game->capture_num[other] != 3) {
    return;
  }

  rm1 = game->capture_pos[other][0];
  rm2 = game->capture_pos[other][1];
  rm3 = game->capture_pos[other][2];

  if (DIS(rm1, rm2) == 2 && DIS(rm1, rm3) == 2){
    tactical_features1[rm1] |= uct_mask[UCT_NAKADE_3];
  } else if (DIS(rm1, rm2) == 2 && DIS(rm2, rm3) == 2){
    tactical_features1[rm2] |= uct_mask[UCT_NAKADE_3];
  } else if (DIS(rm1, rm3) == 2 && DIS(rm2, rm3) == 2){
    tactical_features1[rm3] |= uct_mask[UCT_NAKADE_3];
  }
}

//////////////////////////////
//  ケイマのツケコシの判定  //
//////////////////////////////
void
UctCheckKeimaTsukekoshi(game_info_t *game, int color, int pos, uct_features_t *uct_features)
{
  char *board = game->board;
  int other = FLIP_COLOR(color);
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int keima_pos[8], opponent_pos[8];

  keima_pos[0] = -2 * board_size - 1;
  keima_pos[1] = -2 * board_size + 1;
  keima_pos[2] = - board_size - 2;
  keima_pos[3] = - board_size + 2;
  keima_pos[4] = board_size - 2;
  keima_pos[5] = board_size + 2;
  keima_pos[6] = 2 * board_size - 1;
  keima_pos[7] = 2 * board_size + 1;

  opponent_pos[0] = - board_size - 1;
  opponent_pos[1] = - board_size;
  opponent_pos[2] = - board_size + 1;
  opponent_pos[3] = - 1;
  opponent_pos[4] = 1;
  opponent_pos[5] = board_size - 1;
  opponent_pos[6] = board_size;
  opponent_pos[7] = board_size + 1;

  // Pattern No.1
  // ?O+?
  // ?+X+
  // P+O+
  // ?+++
  if (board[pos + opponent_pos[0]] == other &&
      board[pos + opponent_pos[6]] == other &&
      board[pos + keima_pos[4]] == color &&
      board[NORTH(pos)] != other &&
      Pat3(game->pat, opponent_pos[6]) == 0x0000) {
    tactical_features1[pos] |= uct_mask[UCT_KEIMA_TSUKEKOSHI];
    return;
  }

  // Pattern No.2
  // ??P?
  // O+++
  // +XO+
  // ?+++
  if (board[pos + opponent_pos[0]] == other &&
      board[pos + opponent_pos[4]] == other &&
      board[pos + keima_pos[1]] == color &&
      board[WEST(pos)] != other &&
      Pat3(game->pat, pos + opponent_pos[4]) == 0x0000) {
    tactical_features1[pos] |= uct_mask[UCT_KEIMA_TSUKEKOSHI];
    return;
  }

  // Pattern No.3
  // ?+++
  // P+O+
  // ?+X+
  // ?O+?
  if (board[pos + opponent_pos[1]] == other &&
      board[pos + opponent_pos[5]] == other &&
      board[pos + keima_pos[2]] == color &&
      board[SOUTH(pos)] != other &&
      Pat3(game->pat, pos + opponent_pos[1]) == 0x0000) {
    tactical_features1[pos] |= uct_mask[UCT_KEIMA_TSUKEKOSHI];
    return;
  }

  // Pattern No.4
  // +++?
  // +O+P
  // +X+?
  // ?+O?
  if (board[pos + opponent_pos[1]] == other &&
      board[pos + opponent_pos[7]] == other &&
      board[pos + keima_pos[3]] == color &&
      board[SOUTH(pos)] != other &&
      Pat3(game->pat, pos + opponent_pos[1]) == 0x0000) {
    tactical_features1[pos] |= uct_mask[UCT_KEIMA_TSUKEKOSHI];
    return;
  }

  // Pattern No.5
  // +P??
  // +++O
  // +OX+
  // +++?
  if (board[pos + opponent_pos[2]] == other &&
      board[pos + opponent_pos[3]] == other &&
      board[pos + keima_pos[0]] == color &&
      board[EAST(pos)] != other &&
      Pat3(game->pat, pos + opponent_pos[3]) == 0x0000) {
    tactical_features1[pos] |= uct_mask[UCT_KEIMA_TSUKEKOSHI];
    return;
  }

  // Pattern No.6
  // ?+O? 
  // +X+?
  // +O+P
  // +++?
  if (board[pos + opponent_pos[2]] == other &&
      board[pos + opponent_pos[6]] == other &&
      board[pos + keima_pos[5]] == color &&
      board[NORTH(pos)] != other &&
      Pat3(game->pat, pos + opponent_pos[6]) == 0x0000){
    tactical_features1[pos] |= uct_mask[UCT_KEIMA_TSUKEKOSHI];
    return;
  }

  // Pattern No.7
  // ?+++
  // +XO+
  // O+++
  // ??P?
  if (board[pos + opponent_pos[4]] == other &&
      board[pos + opponent_pos[5]] == other &&
      board[pos + keima_pos[7]] == color &&
      board[WEST(pos)] != other &&
      Pat3(game->pat, pos + opponent_pos[4]) == 0x0000){
    tactical_features1[pos] |= uct_mask[UCT_KEIMA_TSUKEKOSHI];
    return;
  }

  // Pattern No.8
  // +++?
  // +OX+
  // +++O
  // ?P??
  if (board[pos + opponent_pos[3]] == other &&
      board[pos + opponent_pos[7]] == other &&
      board[pos + keima_pos[6]] == color &&
      board[EAST(pos)] != other &&
      Pat3(game->pat, pos + opponent_pos[3]) == 0x0000){
    tactical_features1[pos] |= uct_mask[UCT_KEIMA_TSUKEKOSHI];
    return;
  }
}

//////////////////////
//  両ケイマの判定  //
//////////////////////
void
UctCheckDoubleKeima( game_info_t *game, int color, int pos, uct_features_t *uct_features )
{
  // ++O+O++
  // +O+++O+
  // +++X+++
  // +O+++O+
  // ++O+O++
  // Oのうち自分と相手の石が1個ずつ以上ある時の特徴
  char *board = game->board;
  int other = FLIP_COLOR(color);
  int keima_pos[8];
  int i, p = 0, o = 0;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;

  if (Pat3(game->pat, pos) != 0x0000) {
    return;
  }

  keima_pos[0] = -2 * board_size - 1;
  keima_pos[1] = -2 * board_size + 1;
  keima_pos[2] = - board_size - 2;
  keima_pos[3] = - board_size + 2;
  keima_pos[4] = board_size - 2;
  keima_pos[5] = board_size + 2;
  keima_pos[6] = 2 * board_size - 1;
  keima_pos[7] = 2 * board_size + 1;

  for (i = 0; i < 8; i++) {
    if (board[pos + keima_pos[i]] == color) p++;
    if (board[pos + keima_pos[i]] == other) o++;
  }

  if (p > 0 && o > 0){
    tactical_features1[pos] |= uct_mask[UCT_DOUBLE_KEIMA];
  }
}

////////////////////
//  ウッテガエシ  //
////////////////////
void
UctCheckSnapBack( game_info_t *game, int color, int pos, uct_features_t *uct_features )
{
  string_t *string = snapback_game.string;
  int *string_id = snapback_game.string_id;
  char *board = snapback_game.board;
  int id, capturable_pos, lib;
  bool put = false;
  int other = FLIP_COLOR(color);
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  int i, neighbor4[4];

  GetNeighbor4(neighbor4, pos);

  for (i = 0; i < 4; i++) {
    if (board[neighbor4[i]] == other) {
      id = string_id[neighbor4[i]];
      if ((!put && string[id].libs == 2) ||
	  (put && string[id].libs == 1)) {
      	if (!put) {
      	  CopyGame(&snapback_game, game);
      	  PutStone(&snapback_game, pos, color);
      	  put = true;
      	}
      	lib = snapback_game.string[id].lib[0];
      	capturable_pos = CapturableCandidate(&snapback_game, id);
      	if (lib == capturable_pos) {
      	  tactical_features1[pos] |= uct_mask[UCT_SNAPBACK];
      	  return;
      	}
      }
    }
  }
}

double
CalculateLFRScore( game_info_t *game, int pos, int index[3], uct_features_t *uct_features )
{
  struct pattern *pat = game->pat;
  int pm1 = PASS, pm2 = PASS;
  int moves = game->moves;
  int i, j, f, dis1 = -1, dis2 = -1;
  double score = weight_zero;
  double tmp_score;
  unsigned long long *tactical_features1 = uct_features->tactical_features1;
  unsigned int pat3, md2;
  latent_factor_t *all_feature[UCT_TACTICAL_FEATURE_MAX + 6];
  int feature_num = 0;

  if (moves > 1) pm1 = game->record[moves - 1].pos;
  if (moves > 2) pm2 = game->record[moves - 2].pos;

  // パスの時の分岐
  if (pos == PASS) {
    if (moves > 1 && pm1 == PASS) {
      score += uct_pass[UCT_PASS_AFTER_PASS].w;
    } else {
      score += uct_pass[UCT_PASS_AFTER_MOVE].w;
    }
    return score;
  }

  if (moves > 1 && pm1 != PASS) {
    dis1 = DIS(pm1, pos);
    if (dis1 >= MOVE_DISTANCE_MAX - 1) {
      dis1 = MOVE_DISTANCE_MAX - 1;
    }
  }

  if (moves > 2 && pm2 != PASS) {
    dis2 = DIS(pm2, pos);
    if (dis2 >= MOVE_DISTANCE_MAX - 1) {
      dis2 = MOVE_DISTANCE_MAX - 1;
    }
  }

  pat3 = pat3_index[Pat3(pat, pos)];
  md2 = md2_index[MD2(pat, pos)];

  // 特徴を
  for (i = 0; i < UCT_TACTICAL_FEATURE_MAX; i++) {
    if ((tactical_features1[pos] & uct_mask[i]) != 0) {
      all_feature[feature_num++] = &uct_tactical_features[i];
    }
  }
  // 盤上の位置
  all_feature[feature_num++] = &uct_pos_id[board_pos_id[pos]];
  // 1手前からの距離
  if (dis1 != -1) {
    all_feature[feature_num++] = &uct_move_distance_1[dis1];
  }
  // 2手前からの距離
  if (dis2 != -1) {
    all_feature[feature_num++] = &uct_move_distance_2[dis2];  
  }
  // パターン
  if (index[2] != -1) {
    all_feature[feature_num++] = &uct_md5[index[2]];
  } else if (index[1] != -1) {
    all_feature[feature_num++] = &uct_md4[index[1]];
  } else if (index[0] != -1) {
    all_feature[feature_num++] = &uct_md3[index[0]];
  } else if (uct_md2[md2].w != 0.0) {
    all_feature[feature_num++] = &uct_md2[md2];
  } else {
    all_feature[feature_num++] = &uct_pat3[pat3];
  }

  // wの足し算
  for (i = 0; i < feature_num; i++) {
    score += all_feature[i]->w;
  }

  // vの計算
  for (f = 0; f < LFR_DIMENSION; f++) {
    for (i = 0; i < feature_num; i++) {
      tmp_score = 0.0;
      for (j = i + 1; j < feature_num; j++) {
	tmp_score += all_feature[j]->v[f];
      }
      score += tmp_score * all_feature[i]->v[f];
    }
  }

  return score;
}

//////////////////////////////////////////
//  着手予想の精度を確認するための関数  //
//////////////////////////////////////////
void
AnalyzeUctRating( game_info_t *game, int color, double rate[] )
{
  int i;
  int pos;
  int moves = game->moves;
  float gamma;
  pattern_hash_t hash_pat;
  int pat_index[3];
  uct_features_t uct_features;

  memset(&uct_features, 0, sizeof(uct_features_t));

  UctCheckFeatures(game, color, &uct_features);
  UctCheckRemove2Stones(game, color, &uct_features);
  UctCheckRemove3Stones(game, color, &uct_features);
  if (game->ko_move == moves - 2) {
    UctCheckCaptureAfterKo(game, color, &uct_features);
    UctCheckKoConnection(game, &uct_features);
  }

  for (i = 0; i < pure_board_max; i++) {
    pos = onboard_pos[i];
    if (!game->candidates[pos] || !IsLegal(game, pos, color)) {
      rate[i] = 0;
      continue;
    }

    UctCheckSelfAtari(game, color, pos, &uct_features);
    UctCheckSnapBack(game, color, pos, &uct_features);
    if ((uct_features.tactical_features1[pos] & capture_mask)== 0) UctCheckCapture(game, color, pos, &uct_features);
    if ((uct_features.tactical_features1[pos] & atari_mask) == 0) UctCheckAtari(game, color, pos, &uct_features);
    UctCheckDoubleKeima(game, color, pos, &uct_features);
    UctCheckKeimaTsukekoshi(game, color, pos, &uct_features);

    //  Pattern
    PatternHash(&game->pat[pos], &hash_pat);
    pat_index[0] = SearchIndex(md3_index, hash_pat.list[MD_3]);
    pat_index[1] = SearchIndex(md4_index, hash_pat.list[MD_4]);
    pat_index[2] = SearchIndex(md5_index, hash_pat.list[MD_5 + MD_MAX]);

    gamma = CalculateLFRScore(game, pos, pat_index, &uct_features);

    rate[i] = gamma;
  }
}

//////////////////
//  γ読み込み  //
//////////////////
void
InputUCTParameter(void)
{
  string uct_parameters_path = uct_params_path;
  string path;

  md2_index = new int[MD2_MAX];
  md3_index = new index_hash_t[HASH_MAX];
  md4_index = new index_hash_t[HASH_MAX];
  md5_index = new index_hash_t[HASH_MAX];
  uct_md2 = new latent_factor_t[MD2_LIMIT];

#if defined (_WIN32)
  uct_parameters_path += '\\';
#else
  uct_parameters_path += '/';
#endif

  path = uct_parameters_path + "WeightZero.txt";

  //  W_0
  InputTxtDBL(path.c_str(), &weight_zero, 1);

  //  戦術的特徴
  path = uct_parameters_path + "TacticalFeature.txt";
  InputLatentFactor(path.c_str(), uct_tactical_features, UCT_TACTICAL_FEATURE_MAX);

  // 盤上の位置
  path = uct_parameters_path + "PosID.txt";
  InputLatentFactor(path.c_str(), uct_pos_id, POS_ID_MAX);

  // パス
  path = uct_parameters_path + "Pass.txt";
  InputLatentFactor(path.c_str(), uct_pass, UCT_PASS_MAX);

  //  直前の手との距離
  path = uct_parameters_path + "MoveDistance1.txt";
  InputLatentFactor(path.c_str(), uct_move_distance_1, MOVE_DISTANCE_MAX);

  //  2手前の手との距離
  path = uct_parameters_path + "MoveDistance2.txt";
  InputLatentFactor(path.c_str(), uct_move_distance_2, MOVE_DISTANCE_MAX);

  //  3x3パターン
  path = uct_parameters_path + "Pat3.txt";
  InputPat3(path.c_str(), uct_pat3);

  //  マンハッタン距離2のパターン
  path = uct_parameters_path + "MD2.txt";
  InputMD2(path.c_str(), uct_md2);

  //  マンハッタン距離3のパターン
  path = uct_parameters_path + "MD3.txt";
  InputLargePattern(path.c_str(), uct_md3, md3_index);

  //  マンハッタン距離4のパターン
  path = uct_parameters_path + "MD4.txt";
  InputLargePattern(path.c_str(), uct_md4, md4_index);

  //  マンハッタン距離5のパターン
  path = uct_parameters_path + "MD5.txt";
  InputLargePattern(path.c_str(), uct_md5, md5_index);

  //  Owner
  for (int i = 0; i < OWNER_MAX; i++) {
    uct_owner[i] = owner_k * exp(-pow(i - 5, 2) / owner_bias);
  }

  //  Criticality
  for (int i = 0; i < CRITICALITY_MAX; i++) {
    uct_criticality[i] = exp(criticality_bias * i) - 1;
  }
}

///////////////////////////
//  γ読み込み 着手距離  //
///////////////////////////
static void
InputLatentFactor( const char *filename, latent_factor_t *lf, int n )
{
  FILE *fp;
  int i, j;

#if defined (_WIN32)
  errno_t err;

  err = fopen_s(&fp, filename, "r");
  if (err != 0) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  for (i = 0; i < n; i++) {
    if (fscanf_s(fp, "%lf", &lf[i].w) == EOF) {
      cerr << "Read Error : " << filename << endl;
      exit(1);
    }
    for (j = 0; j < LFR_DIMENSION; j++) {
      if (fscanf_s(fp, "%lf", &lf[i].v[j]) == EOF) {
        cerr << "Read Error : " << filename << endl;
	exit(1);
      }
    }
  }
#else
  fp = fopen(filename, "r");
  if (fp == NULL) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  for (i = 0; i < n; i++) {
    if (fscanf(fp, "%lf", &lf[i].w) == EOF) {
      cerr << "Read Error : " << filename << endl;
      exit(1);
    }
    for (j = 0; j < LFR_DIMENSION; j++) {
      if (fscanf(fp, "%lf", &lf[i].v[j]) == EOF) {
        cerr << "Read Error : " << filename << endl;
        exit(1);
      }
    }
  }
#endif
}

//////////////////////
//  γ読み込み Pat3  //
//////////////////////
static void
InputPat3( const char *filename, latent_factor_t *lf )
{
  FILE *fp;
  int i, idx = 0;
  double weight;
  unsigned int pat3, pat3_transp16[16];

  for (pat3 = 0; pat3 < (unsigned int)PAT3_MAX; pat3++) {
    pat3_index[pat3] = -1;
  }

  for (pat3 = 0; pat3 < (unsigned int)PAT3_MAX; pat3++) {
    if (pat3_index[pat3] == -1) {
      Pat3Transpose16(pat3, pat3_transp16);
      for (i = 0; i < 16; i++) {
	pat3_index[pat3_transp16[i]] = idx;
      }
      idx++;
    }
  }

#if defined (_WIN32)
  errno_t err;

  err = fopen_s(&fp, filename, "r");
  if (err != 0) {
    cerr << "can not open -" << filename << "-" << endl;
    exit(1);
  }
  for (pat3 = 0; pat3 < (unsigned int)PAT3_MAX; pat3++) {
    if (fscanf_s(fp, "%lf", &weight) == EOF) {
      cerr << "Read Error : " << filename << endl;
      exit(1);
    }
    idx = pat3_index[pat3];
    lf[idx].w = weight;   
    for (i = 0; i < LFR_DIMENSION; i++) {
      if (fscanf_s(fp, "%lf", &lf[idx].v[i]) == EOF) {
        cerr << "Read Error : " << filename << endl;
        exit(1);
      }
    }
  }
#else
  fp = fopen(filename, "r");
  if (fp == NULL) {
    cerr << "can not open -" << filename << "-" << endl;
    exit(1);
  }
  for (pat3 = 0; pat3 < (unsigned int)PAT3_MAX; pat3++) {
    if (fscanf(fp, "%lf", &weight) == EOF) {
      cerr << "Read Error : " << filename << endl;
      exit(1);
    }
    idx = pat3_index[pat3];
    lf[idx].w = weight;
    for (i = 0; i < LFR_DIMENSION; i++) {
      if (fscanf(fp, "%lf", &lf[idx].v[i]) == EOF) {
        cerr << "Read Error : " << filename << endl;
        exit(1);
      }
    }
  }
#endif
}

//////////////////////
//  γ読み込み MD2  //
//////////////////////
static void
InputMD2( const char *filename, latent_factor_t *lf )
{
  FILE *fp;
  int i;
  int index, idx = 0;
  double weight;
  unsigned int md2, md2_transp16[16];

  for (md2 = 0; md2 < (unsigned int)MD2_MAX; md2++) {
    md2_index[md2] = -1;
  }

  for (md2 = 0; md2 < (unsigned int)MD2_MAX; md2++) {
    if (md2_index[md2] == -1) {
      MD2Transpose16(md2, md2_transp16);
      for (i = 0; i < 16; i++) {
	md2_index[md2_transp16[i]] = idx;
      }
      idx++;
    }
  }

#if defined (_WIN32)
  errno_t err;

  err = fopen_s(&fp, filename, "r");
  if (err != 0) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  while (fscanf_s(fp, "%d%lf", &index, &weight) != EOF) {
    idx = md2_index[index];
    lf[idx].w = weight;
    for (i = 0; i < LFR_DIMENSION; i++) {
      if (fscanf_s(fp, "%lf", &lf[idx].v[i]) == EOF) {
        cerr << "Read Error : " << filename << endl;
        exit(1);
      }
    }
  }
#else
  fp = fopen(filename, "r");
  if (fp == NULL) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  while (fscanf(fp, "%d%lf", &index, &weight) != EOF) {
    idx = md2_index[index];
    lf[idx].w = weight;
    for (i = 0; i < LFR_DIMENSION; i++) {
      if (fscanf(fp, "%lf", &lf[idx].v[i]) == EOF) {
        cerr << "Read Error : " << filename << endl;
        exit(1);
      }
    }
  }
#endif
}

//  読み込み
static void 
InputLargePattern( const char *filename, latent_factor_t *lf, index_hash_t *pat_index )
{
  FILE *fp;
  int i;
  int index, idx = 0;
  unsigned long long hash;
  double weight;

  for (i = 0; i < HASH_MAX; i++) {
    pat_index[i].hash = 0;
    pat_index[i].index = -1;
  }

#if defined (_WIN32)
  errno_t err;

  err = fopen_s(&fp, filename, "r");
  if (err != 0) {
    cerr << "can not open -" << filename << "-" << endl;
    exit(1);
  }
  while (fscanf_s(fp, "%d%llu%lf", &index, &hash, &weight) != EOF) {
    pat_index[index].hash = hash;
    pat_index[index].index = idx;
    lf[idx].w = weight;
    for (i = 0; i < LFR_DIMENSION; i++) {
      if (fscanf_s(fp, "%lf", &lf[idx].v[i]) == EOF) {
        cerr << "Read Error : " << filename << endl;
        exit(1);
      }
    }
    idx++;
  }
#else
  fp = fopen(filename, "r");
  if (fp == NULL) {
    cerr << "can not open -" << filename << "-" << endl;
    exit(1);
  }
  while (fscanf(fp, "%d%llu%lf", &index, &hash, &weight) != EOF) {
    pat_index[index].hash = hash;
    pat_index[index].index = idx;
    lf[idx].w = weight;
    for (i = 0; i < LFR_DIMENSION; i++) {
      if (fscanf(fp, "%lf", &lf[idx].v[i]) == EOF) {
        cerr << "Read Error : " << filename << endl;
        exit(1);
      }
    }
    idx++;
  }
#endif
}
