#include <cstdio>
#include <cstring>

#include "GoBoard.h"
#include "Pattern.h"

//  ビット列操作
#define REV18(p) (((p) >> 36) | (((p) & 0x3) << 36))
#define REV16(p) (((p) >> 32) | (((p) & 0x3) << 32))
#define REV14(p) (((p) >> 28) | (((p) & 0x3) << 28))
#define REV12(p) (((p) >> 24) | (((p) & 0x3) << 24))
#define REV10(p) (((p) >> 20) | (((p) & 0x3) << 20))
#define REV8(p) (((p) >> 16) | (((p) & 0x3) << 16))
#define REV6(p) (((p) >> 12) | (((p) & 0x3) << 12))
#define REV4(p) (((p) >> 8) | (((p) & 0x3) << 8))
#define REV2(p) (((p) >> 4) | (((p) & 0x3) << 4))

#define REV3(p) (((p) >> 4) | ((p) & 0xC) | (((p) & 0x3) << 4))
#define REV(p) (((p) >> 2) | (((p) & 0x3) << 2))

//  近傍の座標
#define N   (-board_size)
#define S   (board_size)
#define E   (1)
#define W   (-1)
#define NN  (N+N)
#define NE  (N+E)
#define NW  (N+W)
#define SS  (S+S)
#define SE  (S+E)
#define SW  (S+W)
#define WW  (W+W)
#define EE  (E+E)

//////////////////////
//  グローバル変数  //
//////////////////////

//  更新用ビットマスク
static unsigned int update_mask[40][3] = {
  //  3x3
  { 0, 0x00004000, 0x00008000 }, //  1->8
  { 0, 0x00001000, 0x00002000 }, //  2->7
  { 0, 0x00000400, 0x00000800 }, //  3->6
  { 0, 0x00000100, 0x00000200 }, //  4->5
  { 0, 0x00000040, 0x00000080 }, //  5->4
  { 0, 0x00000010, 0x00000020 }, //  6->3
  { 0, 0x00000004, 0x00000008 }, //  7->2
  { 0, 0x00000001, 0x00000002 }, //  8->1

  //  md2
  { 0, 0x00100000, 0x00200000 }, //  9->11
  { 0, 0x00400000, 0x00800000 }, // 10->12
  { 0, 0x00010000, 0x00020000 }, // 11-> 9
  { 0, 0x00040000, 0x00080000 }, // 12->10

  //  md3
  { 0, 0x00001000, 0x00002000 }, // 13->19
  { 0, 0x00004000, 0x00008000 }, // 14->20
  { 0, 0x00010000, 0x00020000 }, // 15->21
  { 0, 0x00040000, 0x00080000 }, // 16->22
  { 0, 0x00100000, 0x00200000 }, // 17->23
  { 0, 0x00400000, 0x00800000 }, // 18->24
  { 0, 0x00000001, 0x00000002 }, // 19->13
  { 0, 0x00000004, 0x00000008 }, // 20->14
  { 0, 0x00000010, 0x00000020 }, // 21->15
  { 0, 0x00000040, 0x00000080 }, // 22->16
  { 0, 0x00000100, 0x00000200 }, // 23->17
  { 0, 0x00000400, 0x00000800 }, // 24->18

  // md4 
  { 0, 0x00010000, 0x00020000 }, // 25->33
  { 0, 0x00040000, 0x00080000 }, // 26->34
  { 0, 0x00100000, 0x00200000 }, // 27->35
  { 0, 0x00400000, 0x00800000 }, // 28->36
  { 0, 0x01000000, 0x02000000 }, // 29->37
  { 0, 0x04000000, 0x08000000 }, // 30->38
  { 0, 0x10000000, 0x20000000 }, // 31->39
  { 0, 0x40000000, 0x80000000 }, // 32->40
  { 0, 0x00000001, 0x00000002 }, // 33->25
  { 0, 0x00000004, 0x00000008 }, // 34->26
  { 0, 0x00000010, 0x00000020 }, // 35->27
  { 0, 0x00000040, 0x00000080 }, // 36->28
  { 0, 0x00000100, 0x00000200 }, // 37->29
  { 0, 0x00000400, 0x00000800 }, // 38->30
  { 0, 0x00001000, 0x00002000 }, // 39->31
  { 0, 0x00004000, 0x00008000 }, // 40->32
};

static unsigned long long large_mask[][3] = {
  // md5 
  { 0, 0x0000000000100000, 0x0000000000200000 },
  { 0, 0x0000000000400000, 0x0000000000800000 },
  { 0, 0x0000000001000000, 0x0000000002000000 },
  { 0, 0x0000000004000000, 0x0000000008000000 },
  { 0, 0x0000000010000000, 0x0000000020000000 },
  { 0, 0x0000000040000000, 0x0000000080000000 },
  { 0, 0x0000000100000000, 0x0000000200000000 },
  { 0, 0x0000000400000000, 0x0000000800000000 },
  { 0, 0x0000001000000000, 0x0000002000000000 },
  { 0, 0x0000004000000000, 0x0000008000000000 },
  { 0, 0x0000000000000001, 0x0000000000000002 },
  { 0, 0x0000000000000004, 0x0000000000000008 },
  { 0, 0x0000000000000010, 0x0000000000000020 },
  { 0, 0x0000000000000040, 0x0000000000000080 },
  { 0, 0x0000000000000100, 0x0000000000000200 },
  { 0, 0x0000000000000400, 0x0000000000000800 },
  { 0, 0x0000000000001000, 0x0000000000002000 },
  { 0, 0x0000000000004000, 0x0000000000008000 },
  { 0, 0x0000000000010000, 0x0000000000020000 },
  { 0, 0x0000000000040000, 0x0000000000080000 },
};

////////////////
//  初期設定  //
////////////////
void
ClearPattern( struct pattern *pat )
{
  int y;

  memset(pat, 0, sizeof(struct pattern) * board_max); 

  for (y = board_start; y <= board_end; y++){
    // 1線
    // 上
    pat[POS(y, board_start)].list[MD_2] |= 0x0003003F;  // 1 2 3 9
    pat[POS(y, board_start)].list[MD_3] |= 0x00F0003F;  // 13 14 15 23 24
    pat[POS(y, board_start)].list[MD_4] |= 0xFC0000FF;  // 25 26 27 28 38 39 40
    pat[POS(y, board_start)].large_list[MD_5] |= 0xFF000003FF;
    // 右
    pat[POS(board_end, y)].list[MD_2] |= 0x000CC330;  // 3 5 8 10
    pat[POS(board_end, y)].list[MD_3] |= 0x00000FFC;  // 14 15 16 17 18
    pat[POS(board_end, y)].list[MD_4] |= 0x0000FFFC;  // 26 27 28 29 30 31 32
    pat[POS(board_end, y)].large_list[MD_5] |= 0x00000FFFFC;
    // 下
    pat[POS(y, board_end)].list[MD_2] |= 0x0030FC00;  // 6 7 8 11
    pat[POS(y, board_end)].list[MD_3] |= 0x0003FF00;  // 17 18 19 20 21
    pat[POS(y, board_end)].list[MD_4] |= 0x00FFFC00;  // 30 31 32 33 34 35 36
    pat[POS(y, board_end)].large_list[MD_5] |= 0x003FFFF000;
    // 左
    pat[POS(board_start, y)].list[MD_2] |= 0x00C00CC3;  // 1 4 6 12
    pat[POS(board_start, y)].list[MD_3] |= 0x00FFC000;  // 20 21 22 23 24
    pat[POS(board_start, y)].list[MD_4] |= 0xFFFC0000;  // 34 35 36 37 38 39 40
    pat[POS(board_start, y)].large_list[MD_5] |= 0xFFFFC00000;

    // 2線
    // 上
    pat[POS(y, board_start + 1)].list[MD_2] |= 0x00030000;  // 9
    pat[POS(y, board_start + 1)].list[MD_3] |= 0x00C0000F;  // 13 14 24
    pat[POS(y, board_start + 1)].list[MD_4] |= 0xF000003F;  // 25 26 27 39 40
    pat[POS(y, board_start + 1)].large_list[MD_5] |= 0xFC000000FF;
    // 右
    pat[POS(board_end - 1, y)].list[MD_2] |= 0x000C0000;  // 10
    pat[POS(board_end - 1, y)].list[MD_3] |= 0x000003F0;  // 15 16 17
    pat[POS(board_end - 1, y)].list[MD_4] |= 0x00003FF0;  // 27 28 29 30 31
    pat[POS(board_end - 1, y)].large_list[MD_5] |= 0x000003FFF0;
    // 下
    pat[POS(y, board_end - 1)].list[MD_2] |= 0x00300000;  // 11
    pat[POS(y, board_end - 1)].list[MD_3] |= 0x0000FC00;  // 18 19 20
    pat[POS(y, board_end - 1)].list[MD_4] |= 0x003FF000;  // 31 32 33 34 35
    pat[POS(y, board_end - 1)].large_list[MD_5] |= 0x000FFFC000;
    // 左
    pat[POS(board_start + 1, y)].list[MD_2] |= 0x00C00000;  // 12
    pat[POS(board_start + 1, y)].list[MD_3] |= 0x003F0000;  // 21 22 23
    pat[POS(board_start + 1, y)].list[MD_4] |= 0x3FF00000;  // 35 36 37 38 39
    pat[POS(board_start + 1, y)].large_list[MD_5] |= 0x3FFF000000;

    // 3線
    // 上
    pat[POS(y, board_start + 2)].list[MD_3] |= 0x00000003;  // 13
    pat[POS(y, board_start + 2)].list[MD_4] |= 0xC000000F;  // 25 26 40
    pat[POS(y, board_start + 2)].large_list[MD_5] |= 0xF00000003F;
    // 右
    pat[POS(board_end - 2, y)].list[MD_3] |= 0x000000C0;  // 16
    pat[POS(board_end - 2, y)].list[MD_4] |= 0x00000FC0;  // 28 29 30
    pat[POS(board_end - 2, y)].large_list[MD_5] |= 0x000000FFC0;
    // 下
    pat[POS(y, board_end - 2)].list[MD_3] |= 0x00003000;  // 19
    pat[POS(y, board_end - 2)].list[MD_4] |= 0x000FC000;  // 32 33 34
    pat[POS(y, board_end - 2)].large_list[MD_5] |= 0x0003FF0000;
    // 左
    pat[POS(board_start + 2, y)].list[MD_3] |= 0x000C0000;  // 22
    pat[POS(board_start + 2, y)].list[MD_4] |= 0x0FC00000;  // 36 37 38
    pat[POS(board_start + 2, y)].large_list[MD_5] |= 0x0FFC000000;

    // 4線 
    // 上
    pat[POS(y, board_start + 3)].list[MD_4] |= 0x00000003;  // 25
    pat[POS(y, board_start + 3)].large_list[MD_5] |= 0xC00000000F;
    // 右
    pat[POS(board_end - 3, y)].list[MD_4] |= 0x00000300;  // 29
    pat[POS(board_end - 3, y)].large_list[MD_5] |= 0x0000003F00;
    // 下
    pat[POS(y, board_end - 3)].list[MD_4] |= 0x00030000;  // 33
    pat[POS(y, board_end - 3)].large_list[MD_5] |= 0x0000FC0000;
    // 左
    pat[POS(board_start + 3, y)].list[MD_4] |= 0x03000000;  // 37
    pat[POS(board_start + 3, y)].large_list[MD_5] |= 0x03F0000000;

    // 5線
    // 上
    pat[POS(y, board_start + 4)].large_list[MD_5] |= 0x0000000003;
    // 右
    pat[POS(board_end - 4, y)].large_list[MD_5] |= 0x0000000C00;
    // 下
    pat[POS(y, board_end - 4)].large_list[MD_5] |= 0x0000300000;
    // 左
    pat[POS(board_start + 4, y)].large_list[MD_5] |= 0x00C0000000;
  }
}

//////////////////////
//  パターンの更新  //
//////////////////////

//  3x3
void
UpdatePat3Empty( struct pattern *pat, int pos )
{
  pat[pos + NW].list[MD_2] &= 0xFF3FFF;
  pat[pos + N].list[MD_2] &= 0xFFCFFF;
  pat[pos + NE].list[MD_2] &= 0xFFF3FF;
  pat[pos + W].list[MD_2] &= 0xFFFCFF;
  pat[pos + E].list[MD_2] &= 0xFFFF3F;
  pat[pos + SW].list[MD_2] &= 0xFFFFCF;
  pat[pos + S].list[MD_2] &= 0xFFFFF3;
  pat[pos + SE].list[MD_2] &= 0xFFFFFC;
}

void
UpdatePat3Stone( struct pattern *pat, int color, int pos )
{
  pat[pos + NW].list[MD_2] |= update_mask[0][color];
  pat[pos + N].list[MD_2] |= update_mask[1][color];
  pat[pos + NE].list[MD_2] |= update_mask[2][color];
  pat[pos + W].list[MD_2] |= update_mask[3][color];
  pat[pos + E].list[MD_2] |= update_mask[4][color];
  pat[pos + SW].list[MD_2] |= update_mask[5][color];
  pat[pos + S].list[MD_2] |= update_mask[6][color];
  pat[pos + SE].list[MD_2] |= update_mask[7][color];
}

//  md2
void
UpdateMD2Empty( struct pattern *pat, int pos )
{
  pat[pos + NW].list[MD_2] &= 0xFF3FFF;
  pat[pos + N].list[MD_2] &= 0xFFCFFF;
  pat[pos + NE].list[MD_2] &= 0xFFF3FF;
  pat[pos + W].list[MD_2] &= 0xFFFCFF;
  pat[pos + E].list[MD_2] &= 0xFFFF3F;
  pat[pos + SW].list[MD_2] &= 0xFFFFCF;
  pat[pos + S].list[MD_2] &= 0xFFFFF3;
  pat[pos + SE].list[MD_2] &= 0xFFFFFC;
  pat[pos + NN].list[MD_2] &= 0xCFFFFF;
  pat[pos + EE].list[MD_2] &= 0x3FFFFF;
  pat[pos + SS].list[MD_2] &= 0xFCFFFF;
  pat[pos + WW].list[MD_2] &= 0xF3FFFF;
}

void
UpdateMD2Stone( struct pattern *pat, int color, int pos )
{
  pat[pos + NW].list[MD_2] |= update_mask[0][color];
  pat[pos + N].list[MD_2] |= update_mask[1][color];
  pat[pos + NE].list[MD_2] |= update_mask[2][color];
  pat[pos + W].list[MD_2] |= update_mask[3][color];
  pat[pos + E].list[MD_2] |= update_mask[4][color];
  pat[pos + SW].list[MD_2] |= update_mask[5][color];
  pat[pos + S].list[MD_2] |= update_mask[6][color];
  pat[pos + SE].list[MD_2] |= update_mask[7][color];
  pat[pos + NN].list[MD_2] |= update_mask[8][color];
  pat[pos + EE].list[MD_2] |= update_mask[9][color];
  pat[pos + SS].list[MD_2] |= update_mask[10][color];
  pat[pos + WW].list[MD_2] |= update_mask[11][color];
}

//  全部
void
UpdatePatternEmpty( struct pattern *pat, int pos )
{
  //  3x3 
  pat[pos + NW].list[MD_2] &= 0xFF3FFF;
  pat[pos + N].list[MD_2] &= 0xFFCFFF;
  pat[pos + NE].list[MD_2] &= 0xFFF3FF;
  pat[pos + W].list[MD_2] &= 0xFFFCFF;
  pat[pos + E].list[MD_2] &= 0xFFFF3F;
  pat[pos + SW].list[MD_2] &= 0xFFFFCF;
  pat[pos + S].list[MD_2] &= 0xFFFFF3;
  pat[pos + SE].list[MD_2] &= 0xFFFFFC;

  //  md2
  pat[pos + NN].list[MD_2] &= 0xCFFFFF;
  pat[pos + EE].list[MD_2] &= 0x3FFFFF;
  pat[pos + SS].list[MD_2] &= 0xFCFFFF;
  pat[pos + WW].list[MD_2] &= 0xF3FFFF;

  //  md3
  pat[pos + NN + N].list[MD_3] &= 0xFFCFFF;
  pat[pos + NN + E].list[MD_3] &= 0xFF3FFF;
  pat[pos + EE + N].list[MD_3] &= 0xFCFFFF;
  pat[pos + EE + E].list[MD_3] &= 0xF3FFFF;
  pat[pos + EE + S].list[MD_3] &= 0xCFFFFF;
  pat[pos + SS + E].list[MD_3] &= 0x3FFFFF;
  pat[pos + SS + S].list[MD_3] &= 0xFFFFFC;
  pat[pos + SS + W].list[MD_3] &= 0xFFFFF3;
  pat[pos + WW + S].list[MD_3] &= 0xFFFFCF;
  pat[pos + WW + W].list[MD_3] &= 0xFFFF3F;
  pat[pos + WW + N].list[MD_3] &= 0xFFFCFF;
  pat[pos + NN + W].list[MD_3] &= 0xFFF3FF;

  // md4
  pat[pos + NN + NN].list[MD_4] &= 0xFFFCFFFF;
  pat[pos + NN + NE].list[MD_4] &= 0xFFF3FFFF;
  pat[pos + NE + NE].list[MD_4] &= 0xFFCFFFFF;
  pat[pos + EE + NE].list[MD_4] &= 0xFF3FFFFF;
  pat[pos + EE + EE].list[MD_4] &= 0xFCFFFFFF;
  pat[pos + EE + SE].list[MD_4] &= 0xF3FFFFFF;
  pat[pos + SE + SE].list[MD_4] &= 0xCFFFFFFF;
  pat[pos + SS + SE].list[MD_4] &= 0x3FFFFFFF;
  pat[pos + SS + SS].list[MD_4] &= 0xFFFFFFFC;
  pat[pos + SS + SW].list[MD_4] &= 0xFFFFFFF3;
  pat[pos + SW + SW].list[MD_4] &= 0xFFFFFFCF;
  pat[pos + WW + SW].list[MD_4] &= 0xFFFFFF3F;
  pat[pos + WW + WW].list[MD_4] &= 0xFFFFFCFF;
  pat[pos + WW + NW].list[MD_4] &= 0xFFFFF3FF;
  pat[pos + NW + NW].list[MD_4] &= 0xFFFFCFFF;
  pat[pos + NN + NW].list[MD_4] &= 0xFFFF3FFF;

  // md5
  pat[pos + NN + NN + N].large_list[MD_5] &= 0xFFFFCFFFFF;
  pat[pos + NN + NN + E].large_list[MD_5] &= 0xFFFF3FFFFF;
  pat[pos + NN + NE + E].large_list[MD_5] &= 0xFFFCFFFFFF;
  pat[pos + NN + EE + E].large_list[MD_5] &= 0xFFF3FFFFFF;
  pat[pos + NE + EE + E].large_list[MD_5] &= 0xFFCFFFFFFF;
  pat[pos + EE + EE + E].large_list[MD_5] &= 0xFF3FFFFFFF;
  pat[pos + SE + EE + E].large_list[MD_5] &= 0xFCFFFFFFFF;
  pat[pos + SS + EE + E].large_list[MD_5] &= 0xF3FFFFFFFF;
  pat[pos + SS + SE + E].large_list[MD_5] &= 0xCFFFFFFFFF;
  pat[pos + SS + SS + E].large_list[MD_5] &= 0x3FFFFFFFFF;
  pat[pos + SS + SS + S].large_list[MD_5] &= 0xFFFFFFFFFC;
  pat[pos + SS + SS + W].large_list[MD_5] &= 0xFFFFFFFFF3;
  pat[pos + SS + SW + W].large_list[MD_5] &= 0xFFFFFFFFCF;
  pat[pos + SS + WW + W].large_list[MD_5] &= 0xFFFFFFFF3F;
  pat[pos + SW + WW + W].large_list[MD_5] &= 0xFFFFFFFCFF;
  pat[pos + WW + WW + W].large_list[MD_5] &= 0xFFFFFFF3FF;
  pat[pos + NW + WW + W].large_list[MD_5] &= 0xFFFFFFCFFF;
  pat[pos + NN + WW + W].large_list[MD_5] &= 0xFFFFFF3FFF;
  pat[pos + NN + NW + W].large_list[MD_5] &= 0xFFFFFCFFFF;
  pat[pos + NN + NN + W].large_list[MD_5] &= 0xFFFFF3FFFF;
}

void
UpdatePatternStone( struct pattern *pat, int color, int pos )
{
  //  3x3 
  pat[pos + NW].list[MD_2] |= update_mask[0][color];
  pat[pos + N].list[MD_2] |= update_mask[1][color];
  pat[pos + NE].list[MD_2] |= update_mask[2][color];
  pat[pos + W].list[MD_2] |= update_mask[3][color];
  pat[pos + E].list[MD_2] |= update_mask[4][color];
  pat[pos + SW].list[MD_2] |= update_mask[5][color];
  pat[pos + S].list[MD_2] |= update_mask[6][color];
  pat[pos + SE].list[MD_2] |= update_mask[7][color];

  //  md2
  pat[pos + NN].list[MD_2] |= update_mask[8][color];
  pat[pos + EE].list[MD_2] |= update_mask[9][color];
  pat[pos + SS].list[MD_2] |= update_mask[10][color];
  pat[pos + WW].list[MD_2] |= update_mask[11][color];

  //  md3
  pat[pos + NN + N].list[MD_3] |= update_mask[12][color];
  pat[pos + NN + E].list[MD_3] |= update_mask[13][color];
  pat[pos + EE + N].list[MD_3] |= update_mask[14][color];
  pat[pos + EE + E].list[MD_3] |= update_mask[15][color];
  pat[pos + EE + S].list[MD_3] |= update_mask[16][color];
  pat[pos + SS + E].list[MD_3] |= update_mask[17][color];
  pat[pos + SS + S].list[MD_3] |= update_mask[18][color];
  pat[pos + SS + W].list[MD_3] |= update_mask[19][color];
  pat[pos + WW + S].list[MD_3] |= update_mask[20][color];
  pat[pos + WW + W].list[MD_3] |= update_mask[21][color];
  pat[pos + WW + N].list[MD_3] |= update_mask[22][color];
  pat[pos + NN + W].list[MD_3] |= update_mask[23][color];

  // md4 
  pat[pos + NN + NN].list[MD_4] |= update_mask[24][color];
  pat[pos + NN + NE].list[MD_4] |= update_mask[25][color];
  pat[pos + NE + NE].list[MD_4] |= update_mask[26][color];
  pat[pos + EE + NE].list[MD_4] |= update_mask[27][color];
  pat[pos + EE + EE].list[MD_4] |= update_mask[28][color];
  pat[pos + EE + SE].list[MD_4] |= update_mask[29][color];
  pat[pos + SE + SE].list[MD_4] |= update_mask[30][color];
  pat[pos + SS + SE].list[MD_4] |= update_mask[31][color];
  pat[pos + SS + SS].list[MD_4] |= update_mask[32][color];
  pat[pos + SS + SW].list[MD_4] |= update_mask[33][color];
  pat[pos + SW + SW].list[MD_4] |= update_mask[34][color];
  pat[pos + WW + SW].list[MD_4] |= update_mask[35][color];
  pat[pos + WW + WW].list[MD_4] |= update_mask[36][color];
  pat[pos + WW + NW].list[MD_4] |= update_mask[37][color];
  pat[pos + NW + NW].list[MD_4] |= update_mask[38][color];
  pat[pos + NN + NW].list[MD_4] |= update_mask[39][color];

  // md5
  pat[pos + NN + NN + N].large_list[MD_5] |= large_mask[0][color];
  pat[pos + NN + NN + E].large_list[MD_5] |= large_mask[1][color];
  pat[pos + NN + NE + E].large_list[MD_5] |= large_mask[2][color];
  pat[pos + NN + EE + E].large_list[MD_5] |= large_mask[3][color];
  pat[pos + NE + EE + E].large_list[MD_5] |= large_mask[4][color];
  pat[pos + EE + EE + E].large_list[MD_5] |= large_mask[5][color];
  pat[pos + SE + EE + E].large_list[MD_5] |= large_mask[6][color];
  pat[pos + SS + EE + E].large_list[MD_5] |= large_mask[7][color];
  pat[pos + SS + SE + E].large_list[MD_5] |= large_mask[8][color];
  pat[pos + SS + SS + E].large_list[MD_5] |= large_mask[9][color];
  pat[pos + SS + SS + S].large_list[MD_5] |= large_mask[10][color];
  pat[pos + SS + SS + W].large_list[MD_5] |= large_mask[11][color];
  pat[pos + SS + SW + W].large_list[MD_5] |= large_mask[12][color];
  pat[pos + SS + WW + W].large_list[MD_5] |= large_mask[13][color];
  pat[pos + SW + WW + W].large_list[MD_5] |= large_mask[14][color];
  pat[pos + WW + WW + W].large_list[MD_5] |= large_mask[15][color];
  pat[pos + NW + WW + W].large_list[MD_5] |= large_mask[16][color];
  pat[pos + NN + WW + W].large_list[MD_5] |= large_mask[17][color];
  pat[pos + NN + NW + W].large_list[MD_5] |= large_mask[18][color];
  pat[pos + NN + NN + W].large_list[MD_5] |= large_mask[19][color];
}

//////////////////////////
//  同一パターンを返す  //
//////////////////////////

//  3x3
void
Pat3Transpose8( unsigned int pat3, unsigned int *transp )
{
  transp[0] = pat3;
  transp[1] = Pat3VerticalMirror(pat3);
  transp[2] = Pat3HorizontalMirror(pat3);
  transp[3] = Pat3VerticalMirror(transp[2]);
  transp[4] = Pat3Rotate90(pat3);
  transp[5] = Pat3Rotate90(transp[1]);
  transp[6] = Pat3Rotate90(transp[2]);
  transp[7] = Pat3Rotate90(transp[3]);
}

void
Pat3Transpose16( unsigned int pat3, unsigned int *transp )
{
  transp[0] = pat3;
  transp[1] = Pat3VerticalMirror(pat3);
  transp[2] = Pat3HorizontalMirror(pat3);
  transp[3] = Pat3VerticalMirror(transp[2]);
  transp[4] = Pat3Rotate90(pat3);
  transp[5] = Pat3Rotate90(transp[1]);
  transp[6] = Pat3Rotate90(transp[2]);
  transp[7] = Pat3Rotate90(transp[3]);
  transp[8] = Pat3Reverse(transp[0]);
  transp[9] = Pat3Reverse(transp[1]);
  transp[10] = Pat3Reverse(transp[2]);
  transp[11] = Pat3Reverse(transp[3]);
  transp[12] = Pat3Reverse(transp[4]);
  transp[13] = Pat3Reverse(transp[5]);
  transp[14] = Pat3Reverse(transp[6]);
  transp[15] = Pat3Reverse(transp[7]);
}

//  md2
void
MD2Transpose8( unsigned int md2, unsigned int *transp )
{
  transp[0] = md2;
  transp[1] = MD2VerticalMirror(md2);
  transp[2] = MD2HorizontalMirror(md2);
  transp[3] = MD2VerticalMirror(transp[2]);
  transp[4] = MD2Rotate90(md2);
  transp[5] = MD2Rotate90(transp[1]);
  transp[6] = MD2Rotate90(transp[2]);
  transp[7] = MD2Rotate90(transp[3]);
}

void
MD2Transpose16( unsigned int md2, unsigned int *transp )
{
  transp[0] = md2;
  transp[1] = MD2VerticalMirror(md2);
  transp[2] = MD2HorizontalMirror(md2);
  transp[3] = MD2VerticalMirror(transp[2]);
  transp[4] = MD2Rotate90(md2);
  transp[5] = MD2Rotate90(transp[1]);
  transp[6] = MD2Rotate90(transp[2]);
  transp[7] = MD2Rotate90(transp[3]);
  transp[8] = MD2Reverse(transp[0]);
  transp[9] = MD2Reverse(transp[1]);
  transp[10] = MD2Reverse(transp[2]);
  transp[11] = MD2Reverse(transp[3]);
  transp[12] = MD2Reverse(transp[4]);
  transp[13] = MD2Reverse(transp[5]);
  transp[14] = MD2Reverse(transp[6]);
  transp[15] = MD2Reverse(transp[7]);
}

//  md3
void
MD3Transpose8( unsigned int md3, unsigned int *transp )
{
  transp[0] = md3;
  transp[1] = MD3VerticalMirror(md3);
  transp[2] = MD3HorizontalMirror(md3);
  transp[3] = MD3VerticalMirror(transp[2]);
  transp[4] = MD3Rotate90(md3);
  transp[5] = MD3Rotate90(transp[1]);
  transp[6] = MD3Rotate90(transp[2]);
  transp[7] = MD3Rotate90(transp[3]);
}

void
MD3Transpose16( unsigned int md3, unsigned int *transp )
{
  transp[0] = md3;
  transp[1] = MD3VerticalMirror(md3);
  transp[2] = MD3HorizontalMirror(md3);
  transp[3] = MD3VerticalMirror(transp[2]);
  transp[4] = MD3Rotate90(md3);
  transp[5] = MD3Rotate90(transp[1]);
  transp[6] = MD3Rotate90(transp[2]);
  transp[7] = MD3Rotate90(transp[3]);
  transp[8] = MD3Reverse(transp[0]);
  transp[9] = MD3Reverse(transp[1]);
  transp[10] = MD3Reverse(transp[2]);
  transp[11] = MD3Reverse(transp[3]);
  transp[12] = MD3Reverse(transp[4]);
  transp[13] = MD3Reverse(transp[5]);
  transp[14] = MD3Reverse(transp[6]);
  transp[15] = MD3Reverse(transp[7]);
}

//  md4
void
MD4Transpose8( unsigned int md4, unsigned int *transp )
{
  transp[0] = md4;
  transp[1] = MD4VerticalMirror(md4);
  transp[2] = MD4HorizontalMirror(md4);
  transp[3] = MD4VerticalMirror(transp[2]);
  transp[4] = MD4Rotate90(md4);
  transp[5] = MD4Rotate90(transp[1]);
  transp[6] = MD4Rotate90(transp[2]);
  transp[7] = MD4Rotate90(transp[3]);
}

void
MD4Transpose16( unsigned int md4, unsigned int *transp )
{
  transp[0] = md4;
  transp[1] = MD4VerticalMirror(md4);
  transp[2] = MD4HorizontalMirror(md4);
  transp[3] = MD4VerticalMirror(transp[2]);
  transp[4] = MD4Rotate90(md4);
  transp[5] = MD4Rotate90(transp[1]);
  transp[6] = MD4Rotate90(transp[2]);
  transp[7] = MD4Rotate90(transp[3]);
  transp[8] = MD4Reverse(transp[0]);
  transp[9] = MD4Reverse(transp[1]);
  transp[10] = MD4Reverse(transp[2]);
  transp[11] = MD4Reverse(transp[3]);
  transp[12] = MD4Reverse(transp[4]);
  transp[13] = MD4Reverse(transp[5]);
  transp[14] = MD4Reverse(transp[6]);
  transp[15] = MD4Reverse(transp[7]);
}

//  md5
void
MD5Transpose8( unsigned long long md5, unsigned long long *transp )
{
  transp[0] = md5;
  transp[1] = MD5VerticalMirror(md5);
  transp[2] = MD5HorizontalMirror(md5);
  transp[3] = MD5VerticalMirror(transp[2]);
  transp[4] = MD5Rotate90(md5);
  transp[5] = MD5Rotate90(transp[1]);
  transp[6] = MD5Rotate90(transp[2]);
  transp[7] = MD5Rotate90(transp[3]);
}

void
MD5Transpose16( unsigned long long md5, unsigned long long *transp )
{
  transp[0] = md5;
  transp[1] = MD5VerticalMirror(md5);
  transp[2] = MD5HorizontalMirror(md5);
  transp[3] = MD5VerticalMirror(transp[2]);
  transp[4] = MD5Rotate90(md5);
  transp[5] = MD5Rotate90(transp[1]);
  transp[6] = MD5Rotate90(transp[2]);
  transp[7] = MD5Rotate90(transp[3]);
  transp[8] = MD5Reverse(transp[0]);
  transp[9] = MD5Reverse(transp[1]);
  transp[10] = MD5Reverse(transp[2]);
  transp[11] = MD5Reverse(transp[3]);
  transp[12] = MD5Reverse(transp[4]);
  transp[13] = MD5Reverse(transp[5]);
  transp[14] = MD5Reverse(transp[6]);
  transp[15] = MD5Reverse(transp[7]);
}

////////////////
//  色の反転  //
////////////////

//  3x3
unsigned int
Pat3Reverse( unsigned int pat3 )
{
  return ((pat3 >> 1) & 0x5555) | ((pat3 & 0x5555) << 1);
}

//  md2
unsigned int
MD2Reverse( unsigned int md2 )
{
  return ((md2 >> 1) & 0x555555) | ((md2 & 0x555555) << 1);
}

//  md3
unsigned int
MD3Reverse( unsigned int md3 )
{
  return ((md3 >> 1) & 0x555555) | ((md3 & 0x555555) << 1);
}

//  md4
unsigned int
MD4Reverse( unsigned int md4 )
{
  return ((md4 >> 1) & 0x55555555) | ((md4 & 0x55555555) << 1);
}

//  md5
unsigned long long
MD5Reverse( unsigned long long md5 )
{
  return ((md5 >> 1) & 0x5555555555) | ((md5 & 0x5555555555) << 1);
}

////////////////
//  上下対称  //
////////////////

//  3x3
unsigned int
Pat3VerticalMirror( unsigned int pat3 )
{
  return ((pat3 & 0xFC00) >> 10) | (pat3 & 0x03C0) | ((pat3 & 0x003F) << 10);
}

//  md2
unsigned int
MD2VerticalMirror( unsigned int md2 )
{
  return ((md2 & 0x00FC00) >> 10) | (md2 & 0x0003C0) | ((md2 & 0x00003F) << 10)
    | (REV2((md2 & 0x330000) >> 16) << 16)	// 9<->11
    | (md2 & 0xCC0000);
}

//  md3
unsigned int
MD3VerticalMirror( unsigned int md3 )
{
  return (REV6(md3 & 0x003003))				// 13<->19
    | (REV4((md3 & 0x000C0C) >> 2) << 2)	// 14<->18
    | (REV2((md3 & 0x000330) >> 4) << 4)	// 15<->17
    | (REV4((md3 & 0xC0C000) >> 14) << 14)	// 20<->24
    | (REV2((md3 & 0x330000) >> 16) << 16)	// 21<->23
    | (md3 & 0x0C00C0);
}

//  md4
unsigned int
MD4VerticalMirror( unsigned int md4 )
{
  return (REV8(md4 & 0x00030003))					// 25<->33
    | (REV6((md4 & 0x0000C00C) >> 2) << 2)		// 26<->32
    | (REV4((md4 & 0x00003030) >> 4) << 4)		// 27<->31
    | (REV2((md4 & 0x00000CC0) >> 6) << 6)		// 28<->30
    | (REV6((md4 & 0xC00C0000) >> 18) << 18)	// 34<->40
    | (REV4((md4 & 0x30300000) >> 20) << 20)	// 35<->39
    | (REV2((md4 & 0x0CC00000) >> 22) << 22)	// 36<->38
    | (md4 & 0x03000300);
}

//  md5
unsigned long long
MD5VerticalMirror( unsigned long long md5 )
{
  return (REV10(md5 & 0x0000300003))
    | (REV8((md5 & 0x00000C000C) >> 2) << 2)
    | (REV6((md5 & 0x0000030030) >> 4) << 4)
    | (REV4((md5 & 0x000000C0C0) >> 6) << 6)
    | (REV2((md5 & 0x0000003300) >> 8) << 8)
    | (REV8((md5 & 0xC000C00000) >> 22) << 22)
    | (REV6((md5 & 0x3003000000) >> 24) << 24)
    | (REV4((md5 & 0x0C0C000000) >> 26) << 26)
    | (REV2((md5 & 0x0330000000) >> 28) << 28)
    | (md5 & 0x00C0000C00);
}

////////////////
//  左右対称  //
////////////////

//  3x3
unsigned int
Pat3HorizontalMirror( unsigned int pat3 )
{
  return (REV3((pat3 & 0xFC00) >> 10) << 10)
    | (REV((pat3 & 0x03C0) >> 6) << 6)
    | REV3((pat3 & 0x003F));
}

//  md2
unsigned int
MD2HorizontalMirror( unsigned int md2 )
{
  return (REV3((md2 & 0x00FC00) >> 10) << 10)
    | (REV((md2 & 0x0003C0) >> 6) << 6)
    | REV3((md2 & 0x00003F))
    | (REV2((md2 & 0xCC0000) >> 18) << 18)
    | (md2 & 0x330000);
}

//  md3
unsigned int
MD3HorizontalMirror( unsigned int md3 )
{
  return (md3 & 0x003003)
    | (REV10((md3 & 0xC0000C) >> 2) << 2)	// 14<->24
    | (REV8((md3 & 0x300030) >> 4) << 4)	// 15<->23
    | (REV6((md3 & 0x0C00C0) >> 6) << 6)	// 16<->22
    | (REV4((md3 & 0x030300) >> 8) << 8)	// 17<->21
    | (REV2((md3 & 0x00CC00) >> 10) << 10);	// 18<->20
}

//  md4 
unsigned int
MD4HorizontalMirror( unsigned int md4 )
{
  return (md4 & 0x00030003)
    | (REV14((md4 & 0xC000000C) >> 2) << 2)		// 26<->40
    | (REV12((md4 & 0x30000030) >> 4) << 4)		// 27<->39
    | (REV10((md4 & 0x0C0000C0) >> 6) << 6)		// 28<->38
    | (REV8((md4 & 0x03000300) >> 8) << 8)		// 29<->37
    | (REV6((md4 & 0x00C00C00) >> 10) << 10)	// 30<->36
    | (REV4((md4 & 0x00303000) >> 12) << 12)	// 31<->35
    | (REV2((md4 & 0x000CC000) >> 14) << 14);	// 32<->34
}

//  md5 
unsigned long long
MD5HorizontalMirror( unsigned long long md5 )
{
  return (md5 & 0x0000300003)
    | (REV18((md5 & 0xC00000000C) >> 2) << 2)
    | (REV16((md5 & 0x3000000030) >> 4) << 4)
    | (REV14((md5 & 0x0C000000C0) >> 6) << 6)
    | (REV12((md5 & 0x0300000300) >> 8) << 8)
    | (REV10((md5 & 0x00C0000C00) >> 10) << 10)
    | (REV8((md5 & 0x0030003000) >> 12) << 12)
    | (REV6((md5 & 0x000C00C000) >> 14) << 14)
    | (REV4((md5 & 0x0003030000) >> 16) << 16)
    | (REV2((md5 & 0x0000CC0000) >> 18) << 18);
}

////////////////
//  90度回転  //
////////////////

// @3x3
unsigned int
Pat3Rotate90( unsigned int pat3 )
{
  // 1 2 3    3 5 8
  // 4   5 -> 2   7
  // 6 7 8    1 4 6

  return ((pat3 & 0x0003) << 10)
    | ((pat3 & 0x0C0C) << 4)
    | ((pat3 & 0x3030) >> 4)
    | ((pat3 & 0x00C0) << 6)
    | ((pat3 & 0x0300) >> 6)
    | ((pat3 & 0xC000) >> 10);
}

//  md2 
unsigned int
MD2Rotate90( unsigned int md2 )
{
  return ((md2 & 0x000003) << 10)
    | ((md2 & 0x000C0C) << 4)
    | ((md2 & 0x003030) >> 4)
    | ((md2 & 0x0300C0) << 6)
    | ((md2 & 0x000300) >> 6)
    | ((md2 & 0x00C000) >> 10)
    | ((md2 & 0xFC0000) >> 2);
}

//  md3
unsigned int
MD3Rotate90( unsigned int md3 )
{
  return ((md3 & 0x00003F) << 18)
    | ((md3 & 0xFFFFC0) >> 6);
}

//  md4
unsigned int
MD4Rotate90( unsigned int md4 )
{
  return ((md4 & 0x000000FF) << 24)
    | ((md4 & 0xFFFFFF00) >> 8);
}

//  md5
unsigned long long
MD5Rotate90( unsigned long long md5 )
{
  return ((md5 & 0x00000003FF) << 30)
    | ((md5 & 0xFFFFFFFC00) >> 10);
}

//////////////////////
//  パターンを返す  //
//////////////////////

//  3x3 
unsigned int
Pat3( const struct pattern *pat, int pos )
{
  return (pat[pos].list[MD_2] & 0xFFFF);
}

//  md2
unsigned int
MD2( const struct pattern *pat, int pos )
{
  return (pat[pos].list[MD_2]);
}

//  md3
unsigned int
MD3( const struct pattern *pat, int pos )
{
  return (pat[pos].list[MD_3]);
}


//  md4
unsigned int
MD4( const struct pattern *pat, int pos )
{
  return (pat[pos].list[MD_4]);
}

//  md5
unsigned long long
MD5( const struct pattern *pat, int pos )
{
  return (pat[pos].large_list[MD_5]);
}

////////////////
//  表示する  //
////////////////

//  3x3
void
DisplayInputPat3( unsigned int pat3 )
{
  static char stone[] = { '+', '@', 'O', '#' };

  printf("\n");
  printf("%c%c%c\n", stone[pat3 & 0x3], stone[(pat3 >> 2) & 0x3], stone[(pat3 >> 4) & 0x3]);
  printf("%c*%c\n", stone[(pat3 >> 6) & 0x3], stone[(pat3 >> 8) & 0x3]);
  printf("%c%c%c\n", stone[(pat3 >> 10) & 0x3], stone[(pat3 >> 12) & 0x3], stone[(pat3 >> 14) & 0x3]);
}

//  md2
void
DisplayInputMD2( unsigned int md2 )
{
  static char stone[] = { '+', '@', 'O', '#' };

  printf("\n");
  printf("  %c  \n", stone[(md2 >> 16) & 0x3]);
  printf(" %c%c%c \n", stone[md2 & 0x3], stone[(md2 >> 2) & 0x3], stone[(md2 >> 4) & 0x3]);
  printf("%c%c*%c%c\n", stone[(md2 >> 22) & 0x3], stone[(md2 >> 6) & 0x3], stone[(md2 >> 8) & 0x3], stone[(md2 >> 18) & 0x3]);
  printf(" %c%c%c \n", stone[(md2 >> 10) & 0x3], stone[(md2 >> 12) & 0x3], stone[(md2 >> 14) & 0x3]);
  printf("  %c  \n", stone[(md2 >> 20) & 0x3]);
}

//  md3
void
DisplayInputMD3( unsigned int md3 )
{
  static char stone[] = { '+', '@', 'O', '#' };

  printf("\n");
  printf("   %c   \n", stone[md3 & 0x3]);
  printf("  %c %c  \n", stone[(md3 >> 22) & 0x3], stone[(md3 >> 2) & 0x3]);
  printf(" %c   %c \n", stone[(md3 >> 20) & 0x3], stone[(md3 >> 4) & 0x3]);
  printf("%c  *  %c\n", stone[(md3 >> 18) & 0x3], stone[(md3 >> 6) & 0x3]);
  printf(" %c   %c \n", stone[(md3 >> 16) & 0x3], stone[(md3 >> 8) & 0x3]);
  printf("  %c %c  \n", stone[(md3 >> 14) & 0x3], stone[(md3 >> 10) & 0x3]);
  printf("   %c   \n", stone[(md3 >> 12) & 0x3]);
}

//  md4
void
DisplayInputMD4( unsigned int md4 )
{
  static char stone[] = { '+', '@', 'O', '#' };

  printf("\n");
  printf("    %c    \n", stone[md4 & 0x3]);
  printf("   %c %c   \n", stone[(md4 >> 30) & 0x3], stone[(md4 >> 2) & 0x3]);
  printf("  %c   %c  \n", stone[(md4 >> 28) & 0x3], stone[(md4 >> 4) & 0x3]);
  printf(" %c     %c \n", stone[(md4 >> 26) & 0x3], stone[(md4 >> 6) & 0x3]);
  printf("%c   *   %c\n", stone[(md4 >> 24) & 0x3], stone[(md4 >> 8) & 0x3]);
  printf(" %c     %c \n", stone[(md4 >> 22) & 0x3], stone[(md4 >> 10) & 0x3]);
  printf("  %c   %c  \n", stone[(md4 >> 20) & 0x3], stone[(md4 >> 12) & 0x3]);
  printf("   %c %c   \n", stone[(md4 >> 18) & 0x3], stone[(md4 >> 14) & 0x3]);
  printf("    %c    \n", stone[(md4 >> 16) & 0x3]);
}

//  md5
void
DisplayInputMD5( unsigned long long md5 )
{
  static char stone[] = { '+', '@', 'O', '#' };

  printf("\n");
  printf("     %c      \n", stone[md5 & 0x3]);
  printf("    %c %c    \n", stone[(md5 >> 38) & 0x3], stone[(md5 >> 2) & 0x3]);
  printf("   %c   %c   \n", stone[(md5 >> 36) & 0x3], stone[(md5 >> 4) & 0x3]);
  printf("  %c     %c  \n", stone[(md5 >> 34) & 0x3], stone[(md5 >> 6) & 0x3]);
  printf(" %c       %c \n", stone[(md5 >> 32) & 0x3], stone[(md5 >> 8) & 0x3]);
  printf("%c    *    %c\n", stone[(md5 >> 30) & 0x3], stone[(md5 >> 10) & 0x3]);
  printf(" %c       %c \n", stone[(md5 >> 28) & 0x3], stone[(md5 >> 12) & 0x3]);
  printf("  %c     %c  \n", stone[(md5 >> 26) & 0x3], stone[(md5 >> 14) & 0x3]);
  printf("   %c   %c   \n", stone[(md5 >> 24) & 0x3], stone[(md5 >> 16) & 0x3]);
  printf("    %c %c    \n", stone[(md5 >> 22) & 0x3], stone[(md5 >> 18) & 0x3]);
  printf("     %c      \n", stone[(md5 >> 20) & 0x3]);
}

//  Pattern
void
DisplayInputPattern( struct pattern *pattern, int size )
{
  static char stone[] = { '+', '@', 'O', '#' };
  unsigned int md2, md3, md4;

  md2 = pattern->list[MD_2];
  md3 = pattern->list[MD_3];
  md4 = pattern->list[MD_4];

  if (size == 4) {
    printf("\n");
    printf("    %c    \n", stone[md4 & 0x3]);
    printf("   %c%c%c   \n", stone[(md4 >> 30) & 0x3], stone[md3 & 0x3], stone[(md4 >> 2) & 0x3]);
    printf("  %c%c%c%c%c  \n", stone[(md4 >> 28) & 0x3], stone[(md3 >> 22) & 0x3], stone[(md2 >> 16) & 0x3], stone[(md3 >> 2) & 0x3], stone[(md4 >> 4) & 0x3]);
    printf(" %c%c%c%c%c%c%c \n", stone[(md4 >> 26) & 0x3], stone[(md3 >> 20) & 0x3], stone[md2 & 0x3], stone[(md2 >> 2) & 0x3], stone[(md2 >> 4) & 0x3], stone[(md3 >> 4) & 0x3], stone[(md4 >> 6) & 0x3]);
    printf("%c%c%c%c*%c%c%c%c\n", stone[(md4 >> 24) & 0x3], stone[(md3 >> 18) & 0x3], stone[(md2 >> 22) & 0x3], stone[(md2 >> 6) & 0x3], stone[(md2 >> 8) & 0x3], stone[(md2 >> 18) & 0x3], stone[(md3 >> 6) & 0x3], stone[(md4 >> 8) & 0x3]);
    printf(" %c%c%c%c%c%c%c \n", stone[(md4 >> 22) & 0x3], stone[(md3 >> 16) & 0x3], stone[(md2 >> 10) & 0x3], stone[(md2 >> 12) & 0x3], stone[(md2 >> 14) & 0x3], stone[(md3 >> 8) & 0x3], stone[(md4 >> 10) & 0x3]);
    printf("  %c%c%c%c%c  \n", stone[(md4 >> 20) & 0x3], stone[(md3 >> 14) & 0x3], stone[(md2 >> 20) & 0x3], stone[(md3 >> 10) & 0x3], stone[(md4 >> 12) & 0x3]);
    printf("   %c%c%c   \n", stone[(md4 >> 18) & 0x3], stone[(md3 >> 12) & 0x3], stone[(md4 >> 14) & 0x3]);
    printf("    %c    \n", stone[(md4 >> 16) & 0x3]);
  } else if (size == 3) {
    printf("\n");
    printf("   %c   \n", stone[md3 & 0x3]);
    printf("  %c%c%c  \n", stone[(md3 >> 22) & 0x3], stone[(md2 >> 16) & 0x3], stone[(md3 >> 2) & 0x3]);
    printf(" %c%c%c%c%c \n", stone[(md3 >> 20) & 0x3], stone[md2 & 0x3], stone[(md2 >> 2) & 0x3], stone[(md2 >> 4) & 0x3], stone[(md3 >> 4) & 0x3]);
    printf("%c%c%c*%c%c%c\n", stone[(md3 >> 18) & 0x3], stone[(md2 >> 22) & 0x3], stone[(md2 >> 6) & 0x3], stone[(md2 >> 8) & 0x3], stone[(md2 >> 18) & 0x3], stone[(md3 >> 6) & 0x3]);
    printf(" %c%c%c%c%c \n", stone[(md3 >> 16) & 0x3], stone[(md2 >> 10) & 0x3], stone[(md2 >> 12) & 0x3], stone[(md2 >> 14) & 0x3], stone[(md3 >> 8) & 0x3]);
    printf("  %c%c%c  \n", stone[(md3 >> 14) & 0x3], stone[(md2 >> 20) & 0x3], stone[(md3 >> 10) & 0x3]);
    printf("   %c   \n", stone[(md3 >> 12) & 0x3]);
  } else if (size == 2) {
    printf("\n");
    printf("  %c  \n", stone[(md2 >> 16) & 0x3]);
    printf(" %c%c%c \n", stone[md2 & 0x3], stone[(md2 >> 2) & 0x3], stone[(md2 >> 4) & 0x3]);
    printf("%c%c*%c%c\n", stone[(md2 >> 22) & 0x3], stone[(md2 >> 6) & 0x3], stone[(md2 >> 8) & 0x3], stone[(md2 >> 18) & 0x3]);
    printf(" %c%c%c \n", stone[(md2 >> 10) & 0x3], stone[(md2 >> 12) & 0x3], stone[(md2 >> 14) & 0x3]);
    printf("  %c  \n", stone[(md2 >> 20) & 0x3]);
  }
}
