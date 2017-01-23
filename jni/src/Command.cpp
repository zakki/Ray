#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Command.h"
#include "DynamicKomi.h"
#include "GoBoard.h"
#include "Message.h"
#include "UctSearch.h"
#include "ZobristHash.h"
#include "Rating.h"
#include "UctRating.h"

using namespace std;

const string command[COMMAND_MAX] = {
  "--playout",
  "--time",
  "--size",
  "--const-time",
  "--thread",
  "--komi",
  "--handicap",
  "--reuse-subtree",
  "--pondering",
  "--tree-size",
  "--no-debug",
  "--no-early-pass",
  "--params-dir",
};

const string errmessage[COMMAND_MAX] = {
  "Set playouts",
  "Set all thinking time",
  "Set board size",
  "Set mode const time, and set thinking time per move",
  "Set threads",
  "Set komi",
  "Set the number of handicap stones (for testing)",
  "Reuse subtree",
  "Set pondering mode",
  "Set tree size (tree size must be 2 ^ n)",
  "Prohibit any debug message",
  "No early pass",
  "set the parameter directory",
};

//////////////////////
//  ƒRƒ}ƒ“ƒh‚Ìˆ—  //
//////////////////////
void
AnalyzeCommand( int argc, char **argv )
{
  int i, j, n, size;
  
  for (i = 1; i < argc; i++){
    n = COMMAND_MAX + 1;
    for (j = 0; j < COMMAND_MAX; j++){
      if (!strcmp(argv[i], command[j].c_str())){
	      n = j;
      }
    }

    switch (n) {
      case COMMAND_PLAYOUT:
      	SetPlayout(atoi(argv[++i]));
      	SetMode(CONST_PLAYOUT_MODE);
      	break;
      case COMMAND_TIME:
      	SetTime(atof(argv[++i]));
      	SetMode(TIME_SETTING_MODE);
      	break;
      case COMMAND_SIZE:
      	i++;
      	size = atoi(argv[i]);
      	if (pure_board_size != size &&
      	    size > 0 && size <= PURE_BOARD_SIZE) {
      	  SetBoardSize(size);
      	  SetParameter();
      	}
      	break;
      case COMMAND_CONST_TIME:
      	SetMode(CONST_TIME_MODE);
      	SetConstTime(atof(argv[++i]));
      	break;
      case COMMAND_THREAD:
      	SetThread(atoi(argv[++i]));
      	break;
      case COMMAND_KOMI:
      	SetKomi(atof(argv[++i]));
      	break;
      case COMMAND_HANDICAP:
      	SetConstHandicapNum(atoi(argv[++i]));
      	SetHandicapNum(0);
      	break;
      case COMMAND_REUSE_SUBTREE:
        SetReuseSubtree(true);
        break;
      case COMMAND_PONDERING :
      	SetReuseSubtree(true);
      	SetPonderingMode(true);
      	break;
      case COMMAND_TREE_SIZE:
      	SetHashSize((unsigned int)atoi(argv[++i]));
      	break;
      case COMMAND_NO_DEBUG:
        SetDebugMessageMode(false);
        break;
      case COMMAND_NO_EARLY_PASS:
      	SetEarlyPass(false);
      	break;
      case COMMAND_PARAMS_DIR:
        char program_path[1024];
        int last;

        // ŽÀsƒtƒ@ƒCƒ‹‚Ì‚ ‚éƒfƒBƒŒƒNƒgƒŠ‚ÌƒpƒX‚ð’Šo
        strcpy(program_path, argv[++i]);
        last = (int)strlen(program_path);
        while (last--){
      #if defined (_WIN32)
          if (program_path[last] == '\\' || program_path[last] == '/') {
            program_path[last] = '\0';
            break;
          }
      #else
          if (program_path[last] == '/') {
            program_path[last] = '\0';
            break;
          }
      #endif
        }

        // ŠeŽíƒpƒX‚ÌÝ’è
      #if defined (_WIN32)
        sprintf_s(uct_params_path, 1024, "%s\\uct_params", program_path);
        sprintf_s(po_params_path, 1024, "%s\\sim_params", program_path);
      #else
        sprintf(uct_params_path, "%s/uct_params", program_path);
        sprintf(po_params_path, "%s/sim_params", program_path);
      #endif
        break;
      default:
      	for (j = 0; j < COMMAND_MAX; j++){
      	  fprintf(stderr, "%-22s : %s\n", command[j].c_str(), errmessage[j].c_str());
      	}
      	exit(1);
    }
  }
}
