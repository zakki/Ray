#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <iostream>

#include "GoBoard.h"
#include "Point.h"

using namespace std;

char gogui_x[] = { 
  'I', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'J', 
  'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 
  'U', 'V', 'W', 'X', 'Y', 'Z' 
};

char pass[] = "PASS";
char resign[] = "resign";

////////////////////////////////////
//  2次元表記から1次元表記へ変換  //
////////////////////////////////////
int
StringToInteger( char *cpos )
{
  char alphabet;
  int i, x, y, pos;

  if (strcmp(cpos, "pass") == 0 || 
      strcmp(cpos, "PASS") == 0){
    pos = PASS;
  } else {
    alphabet = (char)toupper(cpos[0]);
    x = 0;
    for (i = 1; i <= pure_board_size; i++) {
      if (gogui_x[i] == alphabet) {
	x = i;
      }
    }
    y = pure_board_size - atoi(&cpos[1]) + 1;
    pos = POS(x + (OB_SIZE - 1), y + (OB_SIZE - 1));
  }

  return pos;
}

////////////////////////////////////
//  1次元表記から2次元表記へ変換  //
////////////////////////////////////
void
IntegerToString( int pos, char *cpos )
{
  int x, y;

  if (pos == PASS) {
#if defined (_WIN32)
    sprintf_s(cpos, 5, "%s", pass);
#else
    sprintf(cpos, "%s", pass);
#endif	
  } else if (pos == RESIGN) {
#if defined (_WIN32)
    sprintf_s(cpos, 7, "%s", resign);
#else
    sprintf(cpos, "%s", resign);
#endif	
  } else {
    x = X(pos) - (OB_SIZE - 1);
    y = pure_board_size - (Y(pos) - OB_SIZE);
    cpos[0] = gogui_x[x];
    if (y / 10 == 0) {
      cpos[1] = (char)('0' + y % 10);
      cpos[2] = '\0';
    } else {
      cpos[1] = (char)('0' + y / 10);
      cpos[2] = (char)('0' + y % 10);
      cpos[3] = '\0';
    }
  }
}
