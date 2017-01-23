#ifndef _UTILITY_H_
#define _UTILITY_H_

#include <ctime>

// 消費時間の算出
double GetSpendTime( clock_t start_time );

// データ読み込み(float)
void InputTxtFLT( const char *filename, float *ap, int array_size );

// データ読み込み(double)
void InputTxtDBL( const char *filename, double *ap, int array_size );

#endif
