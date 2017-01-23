#ifndef _POINT_H_
#define _POINT_H_

#define GOGUI_X(pos) (gogui_x[CORRECT_X(pos)])
#define GOGUI_Y(pos) (pure_board_size + 1 - CORRECT_Y(pos))

extern char gogui_x[26];

//  2次元表記から1次元表記へ変換  
int StringToInteger( char *cpos );

//  1次元表記から2次元表記へ変換  
void IntegerToString( int pos, char *cpos );

#endif
