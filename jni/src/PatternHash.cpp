#include <cstdio>
#include <iostream>

#include "PatternHash.h"

using namespace std;

////////////
//  変数  //
////////////

// bit
const unsigned long long random_bitstrings[BIT_MAX][S_MAX] = {
  { 0xc96d191cf6f6aea6LLU, 0x401f7ac78bc80f1cLLU, 0xb5ee8cb6abe457f8LLU, 0xf258d22d4db91392LLU },
  { 0x04eef2b4b5d860ccLLU, 0x67a7aabe10d172d6LLU, 0x40565d50e72b4021LLU, 0x05d07b7d1e8de386LLU },
  { 0x8548dea130821accLLU, 0x583c502c832e0a3aLLU, 0x4631aede2e67ffd1LLU, 0x8f9fccba4388a61fLLU },
  { 0x23d9a035f5e09570LLU, 0x8b3a26b7aa4bcecbLLU, 0x859c449a06e0302cLLU, 0xdb696ab700feb090LLU },
  { 0x7ff1366399d92b12LLU, 0x6b5bd57a3c9113efLLU, 0xbe892b0c53e40d3dLLU, 0x3fc97b87bed94159LLU },
  { 0x3d413b8d11b4cce2LLU, 0x51efc5d2498d7506LLU, 0xe916957641c27421LLU, 0x2a327e8f39fc19a6LLU },
  { 0x3edb3bfe2f0b6337LLU, 0x32c51436b7c00275LLU, 0xb744bed2696ed37eLLU, 0xf7c35c861856282aLLU },
  { 0xc4f978fb19ffb724LLU, 0x14a93ca1d9bcea61LLU, 0x75bda2d6bffcfca4LLU, 0x41dbe94941a43d12LLU },
  { 0xc6ec7495ac0e00fdLLU, 0x957955653083196eLLU, 0xf346de027ca95d44LLU, 0x702751d1bb724213LLU },
  { 0x528184b1277f75feLLU, 0x884bb2027e9ac7b0LLU, 0x41a0bc6dd5c28762LLU, 0x0ba88011cd101288LLU },
  { 0x814621bd927e0dacLLU, 0xb23cb1552b043b6eLLU, 0x175a1fed9bbda880LLU, 0xe838ff59b1c9d964LLU },
  { 0x07ea06b48fca72acLLU, 0x26ebdcf08553011aLLU, 0xfb44ea3c3a45cf1cLLU, 0x9ed34d63df99a685LLU },
  { 0x4c7bf671eaea7207LLU, 0x5c7fc5fc683a1085LLU, 0x7b20c584708499b9LLU, 0x4c3fb0ceb4adb6b9LLU },
  { 0x4902095a15d7f3d2LLU, 0xec97f42c55bc9f40LLU, 0xa0ffc0f9681bb9acLLU, 0xc149bd468ac1ac86LLU },
  { 0xb6c1a68207ba2fc9LLU, 0xb906a73e05a92c74LLU, 0x11e0d6ebd61d941dLLU, 0x7ca12fb5b05b5c4dLLU },
  { 0x16bf95defa2cd170LLU, 0xc27697252e02cb81LLU, 0x6c7f49bf802c66f5LLU, 0x98d3daaa3b2e8562LLU },
  { 0x161f5fc4ba37f6d7LLU, 0x45e0c63e93fc6383LLU, 0x9fb1dbfbc95c83a0LLU, 0x38ddd8a535d2cbbdLLU },
  { 0x39b6f08daf36ca87LLU, 0x6f23d32e2a0fd7faLLU, 0xfcc027348974b455LLU, 0x360369eda9c0e07dLLU },
  { 0xda6c4763c2c466d7LLU, 0x48bbb7a741e6ddd9LLU, 0xd61c0c76deb4818cLLU, 0x5de152345f136375LLU },
  { 0xef65d2fcbb279cfdLLU, 0xdc22b9f9f9d7538dLLU, 0x7dac563216d61e70LLU, 0x05a6f16b79bbd6e9LLU },
  { 0x5cb3b670ae90be6cLLU, 0xbc87a781b47462ceLLU, 0x84f579568a8972c8LLU, 0x6c469ad3cba9b91aLLU },
  { 0x076eb3891fd21cabLLU, 0xe8c41087c07c91fcLLU, 0x1cb7cd1dfbdab648LLU, 0xfaec2f3c1e29110dLLU },
  { 0xb0158aacd4dca9f9LLU, 0x7cc1b5019ea1196dLLU, 0xbc647d48e5e2aeb0LLU, 0x96b30966f70500d8LLU },
  { 0x87489ee810f7daa5LLU, 0x74a51eba09dd373dLLU, 0xd40bb2b0a7ca242dLLU, 0xded20384ba4b0368LLU },
  { 0x7dd248ab68b9df14LLU, 0xf83326963d78833dLLU, 0xe38821faf65bb505LLU, 0x23654ff720304706LLU },
  { 0x6fc1c8b51eec90b2LLU, 0x580a8a7e936a997fLLU, 0x1e7207fe6315d685LLU, 0x8c59c6afcbfab7bfLLU },
  { 0xc24f82b980d1fa2eLLU, 0x084b779ccc9fbe44LLU, 0x1a02f04511f6064eLLU, 0x9640ec87ea1bee8aLLU },
  { 0xb1ee0052dd55d069LLU, 0xcab4f30bb95c5561LLU, 0xd998babcaf69019fLLU, 0xe0126bea2556ccd2LLU },
  { 0x9b016f17c8800310LLU, 0xf41cc5d147950f43LLU, 0xfda9511773320334LLU, 0xddf85a4c56345e4dLLU },
  { 0xa4e47a8efae8deabLLU, 0x9acaa313e6ded943LLU, 0xe9a600be8f5c822bLLU, 0x778d332a7e54ab53LLU },
  { 0x1442a265cefe20caLLU, 0xe78262e6b329807cLLU, 0xd3ccfa96fed4ad17LLU, 0x25b6315bb4e3d4f1LLU },
  { 0xcea2b7e820395a1fLLU, 0xab3b169e3f7ba6baLLU, 0x237e6923d4000b08LLU, 0xac1e02df1e10ef6fLLU },
  { 0xd519dc015ebf61b2LLU, 0xf4f51187fe96b080LLU, 0xa137326e14771e17LLU, 0x5b10d4a4c1fc81eaLLU },
  { 0x52bed44bc6ec0a60LLU, 0x10359cffb84288ceLLU, 0x47d17b92cd7647a9LLU, 0x41c9bafdb9158765LLU },
  { 0x16676aa636f40c88LLU, 0x12d8aefdff93ad5cLLU, 0x19c55cbab761fc6eLLU, 0x2174ee4468bdd89fLLU },
  { 0xa0bd26f5eddaac55LLU, 0x4fdda840f2bea00dLLU, 0xf387cba277ee3737LLU, 0xf90bba5c10dac7b4LLU },
  { 0x33a43afbda5aeebeLLU, 0xb9e3019d9af169bbLLU, 0xad210ac8d15bbd2bLLU, 0x9132a5599c996d32LLU },
  { 0xb7e64eb925c34b07LLU, 0x35cb859f0469f3c8LLU, 0xbf1f44d40cbdfdaeLLU, 0xbfbabeaa1611b567LLU },
  { 0xe4ea67d4c915e61aLLU, 0x1debfa223ca7efe1LLU, 0xa77dfc79c3a3071aLLU, 0x06cc239429a34614LLU },
  { 0x4927012902f7e84cLLU, 0x9ca15a0aff31237fLLU, 0x5d9e9bc902c99ca8LLU, 0x47fa9818255561ffLLU },
  { 0xb613301ca773d9f1LLU, 0xde64d791fb9ac4faLLU, 0x1f5ac2193e8e6749LLU, 0xe312b85c388acffbLLU },
  { 0x986b17a971a64ff9LLU, 0xcb8b41a1609c47bbLLU, 0x9132359c66f27446LLU, 0xfd13d5b1693465e5LLU },
  { 0xf676c5b9c8c31decLLU, 0x819c9d4648bde72eLLU, 0xcb1b9807f2e17075LLU, 0xb833da21219453aeLLU },
  { 0x66f5c5f44fb6895fLLU, 0x1db2622ebc8a5156LLU, 0xd4d55c5a8d8e65c8LLU, 0x57518131d59044b5LLU },
  { 0xcfda297096d43d12LLU, 0x3c92c59d9f4f4fc7LLU, 0xef253867322ed69dLLU, 0x75466261f580f644LLU },
  { 0xda5501f76531dfafLLU, 0xbff23daff1ecf103LLU, 0x5ea264d24cafa620LLU, 0xa4f6e95085e2c1d3LLU },
  { 0x96fd21923d8280b4LLU, 0xd7e000660c4e449dLLU, 0x0175f4ea08c6d68fLLU, 0x2fc41e957fb4d4c4LLU },
  { 0x4c103d0c50171bc7LLU, 0x56b4530e5704ae62LLU, 0xb9d88e9704345821LLU, 0xfe9bba04dff384a1LLU },
  { 0xe6e0124e32eda8e3LLU, 0xc45bfbf985540db8LLU, 0x20f9dbcc42ded8c7LLU, 0x47814256f39a4658LLU },
  { 0x20dcfe42bcb14929LLU, 0xe38adfbdc8aaba12LLU, 0xce488f3a3480ba0dLLU, 0x669aa0a29e8fba7cLLU },
  { 0x87014f5f7986e0f5LLU, 0x4c13ab920adf86f3LLU, 0xeaec363831ef859dLLU, 0xd012ad6ad0766d3eLLU },
  { 0x849098d9f6e9e379LLU, 0x99a456e8a46cf927LLU, 0xd5756ecf52fa0945LLU, 0x7a595501987485daLLU },
  { 0x54440bc1354ae014LLU, 0x979dad1d15e065ddLLU, 0xd37e09f9234fd36fLLU, 0x778f38e1b1ff715cLLU },
  { 0x443d82e64256a243LLU, 0xceb84e9fd0a49a60LLU, 0x20bf8789b57f6a91LLU, 0x5e2332efbdfa86ebLLU },
  { 0x05017bb4eb9c21b1LLU, 0x1fbfa8b6c8cd6444LLU, 0x2969d7638335eb59LLU, 0x6f51c81fe6160790LLU },
  { 0xb111fe1560733b30LLU, 0x16010e086db16febLLU, 0xfcb527b00aaa9de5LLU, 0x9e7078912213f6efLLU },
  { 0x5f0564bea972c16eLLU, 0x3c96a8ea4778734aLLU, 0x28b01e6ae9968fb3LLU, 0x0970867931d700aeLLU },
  { 0x1974ede07597749aLLU, 0xaf16f2f8d8527448LLU, 0xf3be7db0fe807f1dLLU, 0xc97fae4ba2516408LLU },
  { 0x3c5c9fe803f69af3LLU, 0x5d2fbe764a80fa7fLLU, 0x5ced7949a12ab4a1LLU, 0xef23ea8441cf5c53LLU },
  { 0xffb5a3079c5f3418LLU, 0x3373d7f543f1ab0dLLU, 0x8d84012afc9aa746LLU, 0xb287a6f25e5acdf8LLU },
};

/////////////////////////////
//  パターンのハッシュ関数  //
/////////////////////////////
void
PatternHash( struct pattern *pat, pattern_hash_t *hash_pat )
{
  int i;
  unsigned int md2_transp[16];
  unsigned int md3_transp[16];
  unsigned int md4_transp[16];
  unsigned long long md5_transp[16];
  unsigned int tmp2, min2;
  unsigned int tmp3, min3;
  unsigned long long tmp4, min4;
  unsigned long long tmp5, min5;
  int index2, index3, index4, index5;

  MD2Transpose16(pat->list[MD_2], md2_transp);
  MD3Transpose16(pat->list[MD_3], md3_transp);
  MD4Transpose16(pat->list[MD_4], md4_transp);
  MD5Transpose16(pat->large_list[MD_5], md5_transp);

  index2 = index3 = index4 = index5 = 0;

  min2 = md2_transp[0];
  min3 = md3_transp[0] + md2_transp[0];
  min4 = (unsigned long long)md4_transp[0] + md3_transp[0] + md2_transp[0];
  min5 = md5_transp[0] + md4_transp[0] + md3_transp[0] + md2_transp[0];

  for (i = 1; i < 16; i++) {
    tmp2 = md2_transp[i];
    if (min2 > tmp2){
      index2 = i;
      min2 = tmp2;
    }
    tmp3 = md3_transp[i] + md2_transp[i];
    if (min3 > tmp3){
      index3 = i;
      min3 = tmp3;
    }
    tmp4 = (unsigned long long)md4_transp[i] + md3_transp[i] + md2_transp[i];
    if (min4 > tmp4) {
      index4 = i;
      min4 = tmp4;
    }
    tmp5 = md5_transp[i] + md4_transp[i] + md3_transp[i] + md2_transp[i];
    if (min5 > tmp5){
      index5 = i;
      min5 = tmp5;
    }
  }

  hash_pat->list[MD_2] = MD2Hash(md2_transp[index2]);
  hash_pat->list[MD_3] = MD3Hash(md3_transp[index3]) ^ MD2Hash(md2_transp[index3]);
  hash_pat->list[MD_4] = MD4Hash(md4_transp[index4]) ^ MD3Hash(md3_transp[index4]) ^ MD2Hash(md2_transp[index4]);
  hash_pat->list[MD_5 + MD_MAX] = MD5Hash(md5_transp[index5]) ^ MD4Hash(md4_transp[index5]) ^ MD3Hash(md3_transp[index5]) ^ MD2Hash(md2_transp[index5]);
}

/////////////////////////////
//  パターンのハッシュ関数  //
/////////////////////////////
unsigned long long
MD2Hash( unsigned int md2 )
{
  unsigned long long hash = 0;

  for (int i = 0; i < 12; i++) {
    hash ^= random_bitstrings[i][(md2 >> (i * 2)) & 0x3];
  }

  return hash;
}

unsigned long long
MD3Hash( unsigned int md3 )
{
  unsigned long long hash = 0;

  for (int i = 0; i < 12; i++) {
    hash ^= random_bitstrings[i + 12][(md3 >> (i * 2)) & 0x3];
  }

  return hash;
}


unsigned long long
MD4Hash( unsigned int md4 )
{
  unsigned long long hash = 0;

  for (int i = 0; i < 16; i++) {
    hash ^= random_bitstrings[i + 24][(md4 >> (i * 2)) & 0x3];
  }
  
  return hash;
}

unsigned long long
MD5Hash( unsigned long long md5 )
{
  unsigned long long hash = 0;

  for (int i = 0; i < 20; i++) {
    hash ^= random_bitstrings[i + 40][(md5 >> (i * 2)) & 0x3];
  }

  return hash;
}

////////////////////
//  データを探索  //
////////////////////
int
SearchIndex( index_hash_t *index, unsigned long long hash )
{
  int key = TRANS20(hash);
  int i;

  if (key >= HASH_MAX) cerr << "Over Run" << endl;

  i = key;
  do{
    if (index[i].hash == 0){
      return index[i].index;
    } else if (index[i].hash == hash) {
      return index[i].index;
    }
    i++;
    if (i >= HASH_MAX) i = 0;
  } while (i != key);

  return -1;
}
