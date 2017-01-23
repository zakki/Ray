#include <iostream>
#include <string>
#include <cstdio>
#include <cstdlib>

#include "Utility.h"

using namespace std;

//////////////////////
//  消費時間の算出  //
//////////////////////
double
GetSpendTime( clock_t start_time )
{
  return (double)(clock() - start_time) * (1.0 / CLOCKS_PER_SEC);
}

////////////////////////////
//  テキスト入力 (float)  //
////////////////////////////
void
InputTxtFLT( const char *filename, float *ap, int array_size )
{
  FILE *fp;
  int i;

#if defined (_WIN32)
  errno_t err;

  err = fopen_s(&fp, filename, "r");
  if (err != 0) {
    cerr << "can not open -" << filename << "-" << endl;
    exit(1);
  }
  for (i = 0; i < array_size; i++) {
    if (fscanf_s(fp, "%f", &ap[i]) == EOF) {
      cerr << "Read Error : " << filename << endl;
    }
  }
#else
  fp = fopen(filename, "r");
  if (fp == NULL) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  for (i = 0; i < array_size; i++) {
    if (fscanf(fp, "%f", &ap[i]) == EOF) {
      cerr << "Read Error : " << filename << endl;
      exit(1);
    }
  }
#endif
}

/////////////////////////////
//  テキスト入力 (double)  //
/////////////////////////////
void
InputTxtDBL( const char *filename, double *ap, int array_size )
{
  FILE *fp;
  int i;

#if defined (_WIN32)
  errno_t err;

  err = fopen_s(&fp, filename, "r");
  if (err != 0) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  for (i = 0; i < array_size; i++) {
    if (fscanf_s(fp, "%lf", &ap[i]) == EOF) {
      cerr << "Read Error : " << filename << endl;
    }
  }
#else
  fp = fopen(filename, "r");
  if (fp == NULL) {
    cerr << "can not open -" << filename << "-" << endl;
  }
  for (i = 0; i < array_size; i++) {
    if (fscanf(fp, "%lf", &ap[i]) == EOF) {
      cerr << "Read Error : " << filename << endl;
    }
  }
#endif
}
