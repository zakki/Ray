#include <algorithm>
#include <cstring>
#include <iostream>

#include "Message.h"
#include "Nakade.h"
#include "Pattern.h"
#include "Point.h"

using namespace std;

// 3目中手のパターンのハッシュ値
unsigned long long nakade3_hash[6];
// 4目中手のパターンのハッシュ値
unsigned long long nakade4_hash[5];
// 5目中手のパターンのハッシュ値
unsigned long long nakade5_hash[9];
// 6目中手のパターンのハッシュ値
unsigned long long nakade6_hash[4];

// 3目中手の急所
int nakade3_pos[6];
// 4目中手の急所
int nakade4_pos[5];
// 5目中手の急所
int nakade5_pos[9];
// 6目中手の急所
int nakade6_pos[4];

static int start = BOARD_MAX / 2;  

// ナカデが現れないパターン
static const unsigned int nakade_none[134] = {
  0x0000, 0x0001, 0x0004, 0x0005, 0x0006, 0x0012, 0x0015, 0x0016, 0x003f, 0x0044,
  0x0045, 0x0046, 0x0048, 0x0049, 0x0054, 0x0055, 0x0056, 0x0060, 0x0061, 0x0064,
  0x0065, 0x0068, 0x0069, 0x006a, 0x007f, 0x0180, 0x0182, 0x0184, 0x0185, 0x0186,
  0x018a, 0x0192, 0x0195, 0x0196, 0x01bf, 0x0420, 0x0428, 0x042a, 0x0464, 0x0465,
  0x0468, 0x0469, 0x0608, 0x060a, 0x0628, 0x062a, 0x0641, 0x0644, 0x0645, 0x0648,
  0x0649, 0x064a, 0x0660, 0x0661, 0x0664, 0x0664, 0x0668, 0x0669, 0x066a, 0x1144,
  0x1145, 0x1146, 0x1148, 0x114a, 0x1155, 0x1156, 0x1166, 0x116a, 0x117f, 0x1188, 
  0x118a, 0x1192, 0x1194, 0x1196, 0x1198, 0x119a, 0x11aa, 0x1284, 0x12aa, 0x1512, 
  0x151a, 0x152a, 0x1554, 0x1555, 0x1566, 0x1564, 0x1565, 0x1566, 0x1568, 0x156a, 
  0x157f, 0x159a, 0x15a8, 0x15aa, 0x162a, 0x1665, 0x1668, 0x1669, 0x1964, 0x1965, 

  0x1966, 0x197f, 0x1dff, 0x4821, 0x48a9, 0x48aa, 0x4992, 0x4995, 0x4996, 0x49bf, 
  0x5555, 0x5556, 0x5566, 0x556a, 0x557f, 0x559a, 0x55aa, 0x5965, 0x5966, 0x597f,  
  0x5dff, 0x6699, 0x66bf, 0x6eff, 
};

// ナカデが出現するパターンとその方向
static const unsigned int nakade_mask[446][2] = {
  {0x0011, 0x0004}, {0x0019, 0x0004}, {0x0050, 0x0004}, {0x0051, 0x0004}, {0x0052, 0x0004},
  {0x0058, 0x0004}, {0x0059, 0x0004}, {0x005a, 0x0004}, {0x0062, 0x0008}, {0x0066, 0x0008},
  {0x0140, 0x1004}, {0x0141, 0x1004}, {0x0142, 0x1004}, {0x0144, 0x1000}, {0x0145, 0x1000},
  {0x0146, 0x1000}, {0x0148, 0x1000}, {0x0149, 0x1004}, {0x014a, 0x1000}, {0x0151, 0x1004},
  {0x0152, 0x1004}, {0x0155, 0x1000}, {0x0156, 0x1000}, {0x0159, 0x1004}, {0x015a, 0x1004},
  {0x0162, 0x100c}, {0x0166, 0x1008}, {0x016a, 0x1000}, {0x017f, 0x1000}, {0x0181, 0x0004},
  {0x0189, 0x0004}, {0x0191, 0x0004}, {0x0199, 0x0004}, {0x01a1, 0x000c}, {0x01a5, 0x0008},
  {0x0410, 0x1040}, {0x0411, 0x1044}, {0x0412, 0x1000}, {0x0414, 0x1040}, {0x0415, 0x1040},
  {0x0416, 0x1040}, {0x0418, 0x1040}, {0x0419, 0x1044}, {0x041a, 0x1000}, {0x0421, 0x0040},
  {0x0424, 0x0040}, {0x0425, 0x0040}, {0x0426, 0x0048}, {0x0429, 0x0040}, {0x043f, 0x1040}, 

  {0x0454, 0x1000}, {0x0455, 0x1000}, {0x0456, 0x1000}, {0x0458, 0x1004}, {0x0459, 0x1004},
  {0x045a, 0x1004}, {0x0466, 0x0008}, {0x047f, 0x1000}, {0x0498, 0x1040}, {0x0499, 0x1044},
  {0x049a, 0x1000}, {0x04a4, 0x0048}, {0x04a5, 0x0048}, {0x04bf, 0x1040}, {0x0501, 0x1044},
  {0x0502, 0x1000}, {0x0504, 0x1040}, {0x0505, 0x1040}, {0x0506, 0x1040}, {0x0508, 0x1000},
  {0x0509, 0x1040}, {0x050a, 0x1000}, {0x0511, 0x1044}, {0x0512, 0x1000}, {0x0514, 0x1040},
  {0x0515, 0x1040}, {0x0516, 0x1040}, {0x0518, 0x1040}, {0x0519, 0x1044}, {0x051a, 0x1000},
  {0x0521, 0x1044}, {0x0522, 0x1008}, {0x0524, 0x1040}, {0x0525, 0x1040}, {0x0526, 0x1048},
  {0x0528, 0x1000}, {0x0529, 0x1048}, {0x052a, 0x1000}, {0x053f, 0x1040}, {0x0541, 0x1004},
  {0x0542, 0x1004}, {0x0544, 0x1000}, {0x0545, 0x1000}, {0x0546, 0x1000}, {0x0548, 0x1000},
  {0x0549, 0x1004}, {0x054a, 0x1000}, {0x0550, 0x1004}, {0x0551, 0x1004}, {0x0552, 0x1004},

  {0x0554, 0x1000}, {0x0555, 0x1000}, {0x0556, 0x1000}, {0x0558, 0x1004}, {0x0559, 0x1004},
  {0x055a, 0x1004}, {0x0560, 0x1004}, {0x0561, 0x1004}, {0x0562, 0x100c}, {0x0564, 0x1000},
  {0x0565, 0x1000}, {0x0566, 0x100c}, {0x0568, 0x1000}, {0x0569, 0x1004}, {0x056a, 0x1000},
  {0x057f, 0x1000}, {0x0581, 0x1044}, {0x0582, 0x1000}, {0x0584, 0x1040}, {0x0585, 0x1040},
  {0x0586, 0x1040}, {0x0588, 0x1000}, {0x0589, 0x1044}, {0x058a, 0x1000}, {0x0590, 0x1040},
  {0x0591, 0x1044}, {0x0592, 0x1000}, {0x0594, 0x1040}, {0x0595, 0x1040}, {0x0596, 0x1040},
  {0x0598, 0x1040}, {0x0599, 0x1044}, {0x059a, 0x1000}, {0x05a0, 0x1008}, {0x05a1, 0x104c},
  {0x05a2, 0x1008}, {0x05a4, 0x1048}, {0x05a5, 0x1048}, {0x05a6, 0x1048}, {0x05a8, 0x1000},
  {0x05a9, 0x1044}, {0x05aa, 0x1000}, {0x05bf, 0x1040}, {0x0601, 0x0040}, {0x0605, 0x0040}, 
  {0x0606, 0x0048}, {0x0609, 0x0040}, {0x0611, 0x1044}, {0x0612, 0x1008}, {0x0615, 0x1040},

  {0x0616, 0x1048}, {0x0618, 0x1040}, {0x0619, 0x1044}, {0x061a, 0x1000}, {0x0621, 0x0040}, 
  {0x0622, 0x0008}, {0x0625, 0x0040}, {0x0626, 0x0040}, {0x0629, 0x0040}, {0x063f, 0x1040}, 
  {0x0646, 0x0008}, {0x0651, 0x1004}, {0x0652, 0x100c}, {0x0654, 0x1000}, {0x0655, 0x1000},
  {0x0656, 0x1008}, {0x0658, 0x1004}, {0x0659, 0x1004}, {0x065a, 0x1004}, {0x0662, 0x0008},
  {0x0666, 0x0008}, {0x067f, 0x1000}, {0x0681, 0x2048}, {0x0684, 0x2040}, {0x0685, 0x2040},
  {0x0686, 0x2048}, {0x0688, 0x2000}, {0x0689, 0x2040}, {0x068a, 0x2000}, {0x0690, 0x3048},
  {0x0691, 0x304c}, {0x0692, 0x3008}, {0x0694, 0x3040}, {0x0695, 0x3040}, {0x0696, 0x3048},
  {0x0698, 0x3040}, {0x0699, 0x3044}, {0x069a, 0x3000}, {0x06a1, 0x2048}, {0x06a2, 0x2008},
  {0x06a4, 0x2048}, {0x06a5, 0x2048}, {0x06a6, 0x2048}, {0x06a8, 0x2000}, {0x06a9, 0x2040}, 
  {0x06aa, 0x2000}, {0x06bf, 0x2040}, {0x0cff, 0x3000}, {0x0dc3, 0x1004}, {0x0dc7, 0x1000}, 

  {0x0dcb, 0x1004}, {0x0dd3, 0x1004}, {0x0dd7, 0x1000}, {0x0ddb, 0x1004}, {0x0de3, 0x100c},
  {0x0de7, 0x1008}, {0x0deb, 0x3004}, {0x0dff, 0x3000}, {0x1149, 0x0004}, {0x1151, 0x0004},
  {0x1152, 0x0004}, {0x1159, 0x0004}, {0x115a, 0x0004}, {0x1162, 0x000c}, {0x1189, 0x0044},
  {0x1191, 0x0044}, {0x1195, 0x0040}, {0x1199, 0x0044}, {0x11a1, 0x004c}, {0x11a2, 0x0008},
  {0x11a4, 0x0048}, {0x11a5, 0x0048}, {0x11a6, 0x0048}, {0x11a9, 0x0044}, {0x11bf, 0x0040},
  {0x1285, 0x0040}, {0x1291, 0x014c}, {0x1292, 0x0108}, {0x1295, 0x0140}, {0x1296, 0x0108},
  {0x1299, 0x0144}, {0x129a, 0x0100}, {0x12a2, 0x0008}, {0x12a6, 0x0008}, {0x12bf, 0x0140},
  {0x1511, 0x0044}, {0x1515, 0x0040}, {0x1516, 0x0040}, {0x1519, 0x0044}, {0x1521, 0x0044},
  {0x1522, 0x0008}, {0x1525, 0x0040}, {0x1526, 0x0048}, {0x1529, 0x0044}, {0x153f, 0x0040},
  {0x1558, 0x0004}, {0x1559, 0x0004}, {0x155a, 0x0004}, {0x1561, 0x0004}, {0x1562, 0x000c}, 

  {0x1569, 0x0004}, {0x1598, 0x0040}, {0x1599, 0x0044}, {0x15a1, 0x004c}, {0x15a2, 0x0008},
  {0x15a4, 0x0048}, {0x15a5, 0x0048}, {0x15a6, 0x0048}, {0x15a9, 0x0044}, {0x15bf, 0x0040},
  {0x1611, 0x0144}, {0x1612, 0x0108}, {0x1615, 0x0140}, {0x1616, 0x0148}, {0x1619, 0x0144},
  {0x161a, 0x0100}, {0x1621, 0x0040}, {0x1625, 0x0040}, {0x1626, 0x0048}, {0x1629, 0x0040},
  {0x163f, 0x0140}, {0x1651, 0x0104}, {0x1652, 0x010c}, {0x1655, 0x0100}, {0x1656, 0x0108},
  {0x1658, 0x0104}, {0x1659, 0x0104}, {0x165a, 0x0104}, {0x1666, 0x0008}, {0x167f, 0x0100},
  {0x1691, 0x014c}, {0x1692, 0x0108}, {0x1694, 0x0140}, {0x1695, 0x0140}, {0x1696, 0x0148},
  {0x1698, 0x0140}, {0x1699, 0x0144}, {0x169a, 0x0100}, {0x16a4, 0x0048}, {0x16a5, 0x0048},
  {0x16bf, 0x0140}, {0x1921, 0x2044}, {0x1922, 0x2088}, {0x1925, 0x2040}, {0x1926, 0x20c8},
  {0x1929, 0x20c4}, {0x192a, 0x2080}, {0x193f, 0x20c0}, {0x1969, 0x2084}, {0x196a, 0x2080}, 

  {0x19a9, 0x2044}, {0x19aa, 0x2000}, {0x19bf, 0x2040}, {0x1a11, 0x2144}, {0x1a15, 0x2140},
  {0x1a16, 0x21c8}, {0x1a19, 0x21c4}, {0x1a1a, 0x2180}, {0x1a3f, 0x21c0}, {0x1a55, 0x2100},
  {0x1a56, 0x2188}, {0x1a58, 0x2084}, {0x1a59, 0x2184}, {0x1a7f, 0x2180}, {0x1a95, 0x2140},
  {0x1abf, 0x2140}, {0x1eff, 0x2100}, {0x4411, 0x1144}, {0x4412, 0x1100}, {0x4415, 0x1140},
  {0x4416, 0x1140}, {0x4419, 0x1144}, {0x441a, 0x1100}, {0x4422, 0x1008}, {0x4426, 0x1148},
  {0x44a2, 0x1000}, {0x443f, 0x1140}, {0x4455, 0x1100}, {0x4456, 0x1100}, {0x4459, 0x1108},
  {0x445a, 0x1108}, {0x4461, 0x1000}, {0x4462, 0x1008}, {0x4465, 0x1100}, {0x4466, 0x1108},
  {0x4469, 0x1000}, {0x446a, 0x1000}, {0x447f, 0x1100}, {0x4499, 0x1144}, {0x449a, 0x1100},
  {0x44a1, 0x1048}, {0x44a5, 0x1148}, {0x44a6, 0x1148}, {0x44a9, 0x1040}, {0x44aa, 0x1000},
  {0x44bf, 0x1140}, {0x4551, 0x1008}, {0x4552, 0x1008}, {0x4555, 0x1000}, {0x4556, 0x1000}, 

  {0x4559, 0x1008}, {0x455a, 0x1008}, {0x4562, 0x100c}, {0x4566, 0x1008}, {0x456a, 0x1000},
  {0x457f, 0x1000}, {0x4591, 0x1044}, {0x4592, 0x1000}, {0x4595, 0x1040}, {0x4596, 0x1040},
  {0x4599, 0x1044}, {0x459a, 0x1000}, {0x45a1, 0x104c}, {0x45a2, 0x1008}, {0x45a5, 0x1048},
  {0x45a6, 0x1048}, {0x45a9, 0x1044}, {0x45aa, 0x1000}, {0x45bf, 0x1040}, {0x4691, 0x314c},
  {0x4692, 0x3108}, {0x4695, 0x3140}, {0x4696, 0x3148}, {0x4699, 0x3144}, {0x469a, 0x3100},
  {0x46a6, 0x3148}, {0x46aa, 0x3000}, {0x46bf, 0x3140}, {0x4825, 0x0100}, {0x483f, 0x0180},
  {0x4865, 0x1100}, {0x4866, 0x1188}, {0x4869, 0x1080}, {0x486a, 0x1080}, {0x487f, 0x1180},
  {0x48bf, 0x0100}, {0x4952, 0x1084}, {0x4055, 0x1000}, {0x4956, 0x1080}, {0x4959, 0x1084},
  {0x495a, 0x1084}, {0x4961, 0x1004}, {0x4965, 0x1000}, {0x4966, 0x1088}, {0x4969, 0x1084},
  {0x496a, 0x1080}, {0x497f, 0x1080}, {0x4999, 0x0008}, {0x49a1, 0x000c}, {0x49a5, 0x0008}, 

  {0x4a52, 0x318c}, {0x4a55, 0x3100}, {0x4a56, 0x3188}, {0x4a59, 0x3184}, {0x4a65, 0x3100},
  {0x4a7f, 0x3180}, {0x4cff, 0x1100}, {0x4dd3, 0x1004}, {0x4dd7, 0x1000}, {0x4ddb, 0x1004},
  {0x4de3, 0x100c}, {0x4de7, 0x1008}, {0x4deb, 0x1004}, {0x4dff, 0x1000}, {0x4ed3, 0x310c},
  {0x4ed7, 0x3108}, {0x4edb, 0x3104}, {0x4ee7, 0x3108}, {0x4eeb, 0x3000}, {0x4eff, 0x3100},
  {0x5559, 0x0004}, {0x555a, 0x0004}, {0x5599, 0x0044}, {0x55a5, 0x0048}, {0x55a6, 0x0048},
  {0x55a9, 0x0044}, {0x55bf, 0x0040}, {0x5695, 0x0140}, {0x5696, 0x0148}, {0x5699, 0x0144},
  {0x569a, 0x0100}, {0x56a6, 0x0148}, {0x56bf, 0x0140}, {0x5969, 0x0084}, {0x59a9, 0x0044},
  {0x59bf, 0x0040}, {0x5a56, 0x2188}, {0x5a59, 0x2184}, {0x5a5a, 0x2184}, {0x5a66, 0x2188},
  {0x5a7f, 0x2180}, {0x5aa5, 0x2148}, {0x5abf, 0x2140}, {0x5ed7, 0x2108}, {0x5edb, 0x2104},
  {0x5eff, 0x2100},
};

static int nakade_pat3_mask[PAT3_MAX];

//////////////
//  初期化  //
//////////////
void
InitializeNakadeHash( void )
{
  int nakade3[6][3], nakade4[5][4], nakade5[9][5], nakade6[4][6];
  int i, j;

  start = board_max / 2;

  // 3目のナカデ
  nakade3[0][0] = 0; nakade3[0][1] = 1;              nakade3[0][2] = 2;
  nakade3[1][0] = 0; nakade3[1][1] = board_size;     nakade3[1][2] = 2 * board_size;
  nakade3[2][0] = 0; nakade3[2][1] = 1;              nakade3[2][2] = board_size + 1;
  nakade3[3][0] = 0; nakade3[3][1] = board_size - 1; nakade3[3][2] = board_size;
  nakade3[4][0] = 0; nakade3[4][1] = board_size;     nakade3[4][2] = board_size + 1;
  nakade3[5][0] = 0; nakade3[5][1] = 1;              nakade3[5][2] = board_size;

  // 4目のナカデ
  nakade4[0][0] = 0;              nakade4[0][1] = board_size - 1; 
  nakade4[0][2] = board_size;     nakade4[0][3] = board_size + 1;
  nakade4[1][0] = 0;              nakade4[1][1] = board_size - 1; 
  nakade4[1][2] = board_size;     nakade4[1][3] = 2 * board_size; 
  nakade4[2][0] = 0;              nakade4[2][1] = board_size; 
  nakade4[2][2] = board_size + 1; nakade4[2][3] = 2 * board_size;
  nakade4[3][0] = 0;              nakade4[3][1] = 1; 
  nakade4[3][2] = 2;              nakade4[3][3] = board_size + 1;
  nakade4[4][0] = 0;              nakade4[4][1] = 1; 
  nakade4[4][2] = board_size;     nakade4[4][3] = board_size + 1;

  // 5目のナカデ
  nakade5[0][0] = 0;                  nakade5[0][1] = board_size - 1; nakade5[0][2] = board_size;
  nakade5[0][3] = board_size + 1;     nakade5[0][4] = 2 * board_size;
  nakade5[1][0] = 0;                  nakade5[1][1] = board_size - 1; nakade5[1][2] = board_size;
  nakade5[1][3] = 2 * board_size - 1; nakade5[1][4] = 2 * board_size;
  nakade5[2][0] = 0;                  nakade5[2][1] = 1;              nakade5[2][2] = board_size;
  nakade5[2][3] = board_size + 1;     nakade5[2][4] = board_size + 2;
  nakade5[3][0] = 0;                  nakade5[3][1] = 1;              nakade5[3][2] = board_size;
  nakade5[3][3] = board_size + 1;     nakade5[3][4] = 2 * board_size;
  nakade5[4][0] = 0;                  nakade5[4][1] = 1;              nakade5[4][2] = 2;
  nakade5[4][3] = board_size + 1;     nakade5[4][4] = board_size + 2;
  nakade5[5][0] = 0;                  nakade5[5][1] = board_size;     nakade5[5][2] = board_size + 1;
  nakade5[5][3] = 2 * board_size;     nakade5[5][4] = 2 * board_size + 1;
  nakade5[6][0] = 0;                  nakade5[6][1] = 1;              nakade5[6][2] = 2;
  nakade5[6][3] = board_size;         nakade5[6][4] = board_size + 1;
  nakade5[7][0] = 0;                  nakade5[7][1] = 1;              nakade5[7][2] = board_size;
  nakade5[7][3] = board_size + 1;     nakade5[7][4] = 2 * board_size + 1;
  nakade5[8][0] = 0;                  nakade5[8][1] = 1;              nakade5[8][2] = board_size - 1;
  nakade5[8][3] = board_size;         nakade5[8][4] = board_size + 1;

  nakade6[0][0] = 0;                  nakade6[0][1] = board_size - 1;     
  nakade6[0][2] = board_size;         nakade6[0][3] = board_size + 1; 
  nakade6[0][4] = 2 * board_size - 1; nakade6[0][5] = 2 * board_size;
  nakade6[1][0] = 0;                  nakade6[1][1] = 1; 
  nakade6[1][2] = board_size;         nakade6[1][3] = board_size + 1; 
  nakade6[1][4] = board_size + 2;     nakade6[1][5] = 2 * board_size;
  nakade6[2][0] = 0;                  nakade6[2][1] = 1;
  nakade6[2][2] = board_size - 1;     nakade6[2][3] = board_size; 
  nakade6[2][4] = board_size + 1;     nakade6[2][5] = 2 * board_size;
  nakade6[3][0] = 0;                  nakade6[3][1] = board_size - 1; 
  nakade6[3][2] = board_size;         nakade6[3][3] = board_size + 1; 
  nakade6[3][4] = 2 * board_size;     nakade6[3][5] = 2 * board_size + 1;

  nakade3_pos[0] = 1;
  nakade3_pos[1] = board_size;
  nakade3_pos[2] = 1;
  nakade3_pos[3] = board_size;
  nakade3_pos[4] = board_size;
  nakade3_pos[5] = 0;

  nakade4_pos[0] = board_size;
  nakade4_pos[1] = board_size;
  nakade4_pos[2] = board_size;
  nakade4_pos[3] = 1;
  nakade4_pos[4] = 0;

  nakade5_pos[0] = board_size;
  nakade5_pos[1] = board_size;
  nakade5_pos[2] = board_size + 1;
  nakade5_pos[3] = board_size;
  nakade5_pos[4] = 1;
  nakade5_pos[5] = board_size;
  nakade5_pos[6] = 1;
  nakade5_pos[7] = board_size + 1;
  nakade5_pos[8] = board_size;

  nakade6_pos[0] = board_size;
  nakade6_pos[1] = board_size + 1;
  nakade6_pos[2] = board_size;
  nakade6_pos[3] = board_size;

  for (i = 0; i < 6; i++) {
    nakade3_hash[i] = 0;
    for (j = 0; j < 3; j++) {
      nakade3_hash[i] ^= shape_bit[start + nakade3[i][j]];
    }
  }

  for (i = 0; i < 5; i++) {
    nakade4_hash[i] = 0;
    for (j = 0; j < 4; j++) {
      nakade4_hash[i] ^= shape_bit[start + nakade4[i][j]];
    }
  }

  for (i = 0; i < 9; i++) {
    nakade5_hash[i] = 0;
    for (j = 0; j < 5; j++) {
      nakade5_hash[i] ^= shape_bit[start + nakade5[i][j]];
    }
  }

  for (i = 0; i < 4; i++) {
    nakade6_hash[i] = 0;
    for (j = 0; j < 6; j++) {
      nakade6_hash[i] ^= shape_bit[start + nakade6[i][j]];
    }
  }

  // 初期化
  for (i = 0; i < PAT3_MAX; i++) {
    nakade_pat3_mask[i] = 0xffff;
  }

  // ナカデが出現しないパターンの初期化
  for (i = 0; i < 134; i++) {
    unsigned int tmp_pat3[16];
    Pat3Transpose16(nakade_none[i], tmp_pat3);
    for (j = 0; j < 16; j++) {
      nakade_pat3_mask[tmp_pat3[j]] = 0;
    }
  }

  // ナカデが出現しうるパターンの初期化
  for (i = 0; i < 446; i++) {
    unsigned int tmp_pat3[16], tmp_mask[16];
    Pat3Transpose16(nakade_mask[i][0], tmp_pat3);
    Pat3Transpose16(nakade_mask[i][1], tmp_mask);
    for (j = 0; j < 16; j++) {
      nakade_pat3_mask[tmp_pat3[j]] = tmp_mask[j];
    }
  }
}

////////////////////////////////////////////////
//  自己アタリの形がナカデになっているか確認  //
//  3目, 4目, 5目ナカデのみ確認する           //
////////////////////////////////////////////////
bool
IsNakadeSelfAtari( game_info_t *game, int pos, int color )
{
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int *string_next = game->string_next;
  int stones[10];
  int my_stone;
  int i, n = 0, reviser;
  unsigned long long hash = 0;
  int checked[4] = { 0 };
  int check = 0;
  int id;

  // 上が自分の連だったら, それぞれの石の座標を記録
  if (board[NORTH(pos)] == color) {
    id = string_id[NORTH(pos)];
    my_stone = string[id].origin;
    while (my_stone != STRING_END) {
      stones[n++] = my_stone;
      my_stone = string_next[my_stone];
    }
    checked[check++] = id;
  }

  // 左が自分の連だったら, それぞれの石の座標を記録
  if (board[WEST(pos)] == color) {
    id = string_id[WEST(pos)];
    if (checked[0] != id) {
      my_stone = string[id].origin;
      while (my_stone != STRING_END) {
	stones[n++] = my_stone;
	my_stone = string_next[my_stone];
      }
      checked[check++] = id;
    }
  }

  // 右が自分の連だったら, それぞれの石の座標を記録
  if (board[EAST(pos)] == color) {
    id = string_id[EAST(pos)];
    if (checked[0] != id && checked[1] != id) {
      my_stone = string[id].origin;
      while (my_stone != STRING_END) {
	stones[n++] = my_stone;
	my_stone = string_next[my_stone];
      }
      checked[check++] = id;
    }
  }

  // 下が自分の連だったら, それぞれの石の座標を記録
  if (board[SOUTH(pos)] == color) {
    id = string_id[SOUTH(pos)];
    if (checked[0] != id && checked[1] != id && checked[2] != id) {
      my_stone = string[id].origin;
      while (my_stone != STRING_END) {
	stones[n++] = my_stone;
	my_stone = string_next[my_stone];
      }
      checked[check++] = id;
    }
  }

  // 確認している座標も加える
  stones[n++] = pos;
  
  // 連の大きさが6以上なら打たないように
  // falseを返す
  if (n > 5) {
    return false;
  }

  // 座標をソート
  std::sort(stones, stones + n);

  // 石の座標の補正項
  reviser = start - stones[0];

  // ハッシュ値を計算
  for (i = 0; i < n; i++) {
    hash ^= shape_bit[stones[i] + reviser];
  }

  // ハッシュ値が一致しているか確認
  // ナカデのパターンならばtrueを返す
  // そうでなければfalseを返す
  switch (n) {
  case 3:
    for (i = 0; i < 6; i++) {
      if (nakade3_hash[i] == hash) {
	      return true;
      }
    }
    break;
  case 4:
    for (i = 0; i < 5; i++) {
      if (nakade4_hash[i] == hash) {
	      return true;
      }
    }
    break;
  case 5:
    for (i = 0; i < 9; i++) {
      if (nakade5_hash[i] == hash) {
	      return true;
      }
    }
    break;
  }

  return false;
}

//////////////////////////////////////////////////
//  自己アタリがナカデの形になっているかを確認  //
//////////////////////////////////////////////////
bool
IsUctNakadeSelfAtari( game_info_t *game, int pos, int color )
{
  char *board = game->board;
  string_t *string = game->string;
  int *string_id = game->string_id;
  int *string_next = game->string_next;
  int stones[10];
  int my_stone;
  int i, n = 0, reviser;
  unsigned long long hash = 0;
  int checked[4] = { 0 };
  int check = 0;
  int id;

  // 上が自分の連だったら, それぞれの石の座標を記録
  if (board[NORTH(pos)] == color) {
    id = string_id[NORTH(pos)];
    my_stone = string[id].origin;
    while (my_stone != STRING_END) {
      stones[n++] = my_stone;
      my_stone = string_next[my_stone];
    }
    checked[check++] = id;
  }

  // 左が自分の連だったら, それぞれの石の座標を記録
  if (board[WEST(pos)] == color) {
    id = string_id[WEST(pos)];
    if (checked[0] != id) {
      my_stone = string[id].origin;
      while (my_stone != STRING_END) {
	      stones[n++] = my_stone;
	      my_stone = string_next[my_stone];
      }
      checked[check++] = id;
    }
  }

  // 右が自分の連だったら, それぞれの石の座標を記録
  if (board[EAST(pos)] == color) {
    id = string_id[EAST(pos)];
    if (checked[0] != id && checked[1] != id) {
      my_stone = string[id].origin;
      while (my_stone != STRING_END) {
	      stones[n++] = my_stone;
	      my_stone = string_next[my_stone];
      }
      checked[check++] = id;
    }
  }

  // 下が自分の連だったら, それぞれの石の座標を記録
  if (board[SOUTH(pos)] == color) {
    id = string_id[SOUTH(pos)];
    if (checked[0] != id && checked[1] != id && checked[2] != id) {
      my_stone = string[id].origin;
      while (my_stone != STRING_END) {
        stones[n++] = my_stone;
        my_stone = string_next[my_stone];
      }
      checked[check++] = id;
    }
  }

  // 確認している座標も加える
  stones[n++] = pos;

  // 座標をソート
  std::sort(stones, stones + n);

  // 石の座標の補正項
  reviser = start - stones[0];

  // ハッシュ値の計算
  for (i = 0; i < n; i++) {
    hash ^= shape_bit[stones[i] + reviser];
  }

  // ハッシュ値が一致しているか確認
  // ナカデのパターンならばtrueを返す
  // そうでなければfalseを返す
  switch (n) {
  case 3:
    for (i = 0; i < 6; i++) {
      if (nakade3_hash[i] == hash) {
	return true;
      }
    }
    break;
  case 4:
    for (i = 0; i < 5; i++) {
      if (nakade4_hash[i] == hash) {
	return true;
      }
    }
    break;
  case 5:
    for (i = 0; i < 9; i++) {
      if (nakade5_hash[i] == hash) {
	return true;
      }
    }
    break;
  case 6:
    for (i = 0; i < 4; i++) {
      if (nakade6_hash[i] == hash) {
	return true;
      }
    }
    break;
  }

  return false;
}

////////////////////////////////////////////////
//  直前の着手でナカデの形が現れているか確認  //
////////////////////////////////////////////////
int
FindNakadePos( game_info_t *game, int pos, int color )
{
  nakade_queue_t nakade_queue;
  char *board = game->board;
  int size = 0;
  bool flag[BOARD_MAX] = { false };  
  int current_pos;
  int nakade[10];
  int nakade_num = 0;
  unsigned long long hash = 0;
  int i, reviser;

  // キューの初期化
  InitializeNakadeQueue(&nakade_queue);

  // 開始点をキューに入れる
  Enqueue(&nakade_queue, pos);

  // 確認済みのフラグ
  flag[pos] = true;

  // キューが空になるまでループ
  while (!IsQueueEmpty(&nakade_queue)) {
    current_pos = Dequeue(&nakade_queue);
    nakade[nakade_num++] = current_pos;

    // 領域のサイズが6以上になったら
    // ナカデではないと判定
    if (size > 5) return NOT_NAKADE;

    // 上が未確認で, 自分の石か空点なら追加
    if (!flag[NORTH(current_pos)] &&
  (board[NORTH(current_pos)] & color) == 0) {
      Enqueue(&nakade_queue, NORTH(current_pos));
      flag[NORTH(current_pos)] = true;
      size++;
    }

    // 左が未確認で, 自分の石か空点なら追加
    if (!flag[WEST(current_pos)] &&
	(board[WEST(current_pos)] & color) == 0) {
      Enqueue(&nakade_queue, WEST(current_pos));
      flag[WEST(current_pos)] = true;
      size++;
    }

    // 右が未確認で, 自分の石か空点なら追加
    if (!flag[EAST(current_pos)] &&
  (board[EAST(current_pos)] & color) == 0) {
      Enqueue(&nakade_queue, EAST(current_pos));
      flag[SOUTH(current_pos)] = true;
      size++;
    }

    // 下が未確認で, 自分の石か空点なら追加
    if (!flag[SOUTH(current_pos)] &&
	(board[SOUTH(current_pos)] & color) == 0) {
      Enqueue(&nakade_queue, SOUTH(current_pos));
      flag[SOUTH(current_pos)] = true;
      size++;
    }
  }

  // 領域が6以上ならナカデではない
  if (nakade_num > 5) return NOT_NAKADE;

  // 座標をソート
  std::sort(nakade, nakade + nakade_num);

  // 座標の補正項を算出
  reviser = start - nakade[0];

  // ハッシュ値の計算
  for (i = 0; i < nakade_num; i++) {
    hash ^= shape_bit[nakade[i] + reviser];
  }

  // ナカデの形になっていれば, その座標を返す
  switch (nakade_num) {
  case 3:
    for (i = 0; i < 6; i++) {
      if (nakade3_hash[i] == hash) {
	return nakade[0] + nakade3_pos[i];
      }
    }
    break;
  case 4:
    for (i = 0; i < 4; i++) {
      if (nakade4_hash[i] == hash) {
	return nakade[0] + nakade4_pos[i];
      }
    }
    break;
  case 5:
    for (i = 0; i < 9; i++) {
      if (nakade5_hash[i] == hash) {
	return nakade[0] + nakade5_pos[i];
      }
    }
    break;
  }


  return NOT_NAKADE;
}

//////////////////////////////////////
//  直前の着手でナカデがあるか確認  //
//////////////////////////////////////
void
SearchNakade( game_info_t *game, int *nakade_num, int *nakade_pos )
{
  int last_color = game->record[game->moves - 1].color;
  int pos = game->record[game->moves - 1].pos;
  const unsigned int mask[2][4] = {
    {0x0004, 0x0040, 0x0100, 0x1000}, {0x0008, 0x0080, 0x0200, 0x2000},
  };
  const unsigned int all_mask[2] = {
    0x1144, 0x2288
  };
  unsigned int pat3 = Pat3(game->pat, pos);

  // ナカデが出現し得ないパターンなら戻る
  if ((nakade_pat3_mask[pat3] & all_mask[last_color - 1]) == 0) return;

  // 上の確認
  if ((nakade_pat3_mask[pat3] & mask[last_color - 1][0]) != 0) {
    nakade_pos[(*nakade_num)++] = FindNakadePos(game, NORTH(pos), last_color);
  }
  
  // 左の確認
  if ((nakade_pat3_mask[pat3] & mask[last_color - 1][1]) != 0) {
    nakade_pos[(*nakade_num)++] = FindNakadePos(game, WEST(pos), last_color);
  }
  
  // 右の確認
  if ((nakade_pat3_mask[pat3] & mask[last_color - 1][2]) != 0) {
    nakade_pos[(*nakade_num)++] = FindNakadePos(game, EAST(pos), last_color);
  }

  // 下の確認
  if ((nakade_pat3_mask[pat3] & mask[last_color - 1][3]) != 0) {
    nakade_pos[(*nakade_num)++] = FindNakadePos(game, SOUTH(pos), last_color);
  }
}

////////////////////////////////////////////////////
//  石を取り除いた箇所がナカデになっているか確認  //
////////////////////////////////////////////////////
int
CheckRemovedStoneNakade( game_info_t *game, int color )
{
  int capture_num = game->capture_num[FLIP_COLOR(color)];
  int *capture_pos = game->capture_pos[FLIP_COLOR(color)];
  int reviser, i;
  unsigned long long hash = 0;

  // 捕獲した石の数が3個以上6個以下なら確認し
  // それ以外なら何もしないで終了
  if (capture_num > 6 ||
      capture_num < 3) {
    return NOT_NAKADE;
  }

  // 座標の補正項の算出
  reviser = start - capture_pos[0];

  // ハッシュ値の計算
  for (i = 0; i < capture_num; i++) {
    hash ^= shape_bit[capture_pos[i] + reviser];
  }

  // ナカデになっていれば, その座標を返す
  switch (capture_num) {
  case 3:
    for (i = 0; i < 6; i++) {
      if (nakade3_hash[i] == hash) {
	return capture_pos[0] + nakade3_pos[i];
      }
    }
    break;
  case 4:
    for (i = 0; i < 4; i++) {
      if (nakade4_hash[i] == hash) {
	return capture_pos[0] + nakade4_pos[i];
      }
    }
    break;
  case 5:
    for (i = 0; i < 9; i++) {
      if (nakade5_hash[i] == hash) {
	return capture_pos[0] + nakade5_pos[i];
      }
    }
    break;
  }

  return NOT_NAKADE;
}

void
Enqueue( nakade_queue_t *nq, int pos )
{
  nq->pos[nq->tail++] = pos;
  if (nq->tail >= NAKADE_QUEUE_SIZE) {
    nq->tail = 0;
  }
  if (nq->tail == nq->head) {
    cerr << "queue  overflow" << endl;
  }
}

int
Dequeue( nakade_queue_t *nq )
{
  int pos;

  if (nq->head == nq->tail) {
    cerr << "queue underflow" << endl;
    exit(1);
  }
  else {
    pos = nq->pos[nq->head++];
    if (nq->head >= NAKADE_QUEUE_SIZE) {
      nq->head = 0;
    }
    return pos;
  }
}

void
InitializeNakadeQueue( nakade_queue_t *nq )
{
  nq->head = nq->tail = 0;
}

bool
IsQueueEmpty( nakade_queue_t *nq )
{
  return nq->head == nq->tail;
}
