TARGET=ray
CC = g++
#CC = x86_64-w64-mingw32-g++
OPTIMIZE = -O3
CPP11 = -std=c++11
WARNING = -Wall
DEBUG = -g
CFLAGS = ${OPTIMIZE} ${WARNING} ${CPP11} ${DEBUG}
LIBS = -lm -pthread #-static-libstdc++ -static-libgcc
RM = rm

SRCS=${shell ls src/*.cpp}
HEDS=${shell ls src/*.h}
OBJS=${SRCS:.cpp=.o}

.SUFFIXES:.cpp .o .h

.PHONY: all
all : ${TARGET}

${TARGET} : ${OBJS}
	${CC} ${CFLAGS} -o $@ ${OBJS} ${LIBS}

.cpp.o:
	${CC} ${CFLAGS} -c $< -o $@

.PHONY: clean

clean:
	${RM} -f ${TARGET} src/*~ src/*.o *~


src/Command.o: src/Command.cpp src/Command.h src/DynamicKomi.h src/GoBoard.h \
 src/Pattern.h src/UctSearch.h src/ZobristHash.h
src/Command.o: src/Command.h
src/DynamicKomi.o: src/DynamicKomi.cpp src/DynamicKomi.h src/GoBoard.h \
 src/Pattern.h src/UctSearch.h src/ZobristHash.h
src/DynamicKomi.o: src/DynamicKomi.h src/GoBoard.h src/Pattern.h \
 src/UctSearch.h src/ZobristHash.h
src/GoBoard.o: src/GoBoard.cpp src/GoBoard.h src/Pattern.h src/UctRating.h \
 src/PatternHash.h src/ZobristHash.h
src/GoBoard.o: src/GoBoard.h src/Pattern.h
src/Gtp.o: src/Gtp.cpp src/DynamicKomi.h src/GoBoard.h src/Pattern.h \
 src/UctSearch.h src/ZobristHash.h src/Gtp.h src/Nakade.h src/UctRating.h \
 src/PatternHash.h src/Message.h src/Point.h src/Rating.h \
 src/Simulation.h
src/Gtp.o: src/Gtp.h
src/Ladder.o: src/Ladder.cpp src/Message.h src/GoBoard.h src/Pattern.h \
 src/UctSearch.h src/ZobristHash.h src/Ladder.h src/Point.h
src/Ladder.o: src/Ladder.h src/GoBoard.h src/Pattern.h
src/Message.o: src/Message.cpp src/Message.h src/GoBoard.h src/Pattern.h \
 src/UctSearch.h src/ZobristHash.h src/Point.h
src/Message.o: src/Message.h src/GoBoard.h src/Pattern.h src/UctSearch.h \
 src/ZobristHash.h
src/Nakade.o: src/Nakade.cpp src/Message.h src/GoBoard.h src/Pattern.h \
 src/UctSearch.h src/ZobristHash.h src/Nakade.h src/Point.h
src/Nakade.o: src/Nakade.h src/ZobristHash.h src/GoBoard.h src/Pattern.h
src/Pattern.o: src/Pattern.cpp src/GoBoard.h src/Pattern.h
src/Pattern.o: src/Pattern.h
src/PatternHash.o: src/PatternHash.cpp src/PatternHash.h src/GoBoard.h \
 src/Pattern.h
src/PatternHash.o: src/PatternHash.h src/GoBoard.h src/Pattern.h
src/Point.o: src/Point.cpp src/GoBoard.h src/Pattern.h src/Point.h
src/Point.o: src/Point.h
src/Rating.o: src/Rating.cpp src/Message.h src/GoBoard.h src/Pattern.h \
 src/UctSearch.h src/ZobristHash.h src/Nakade.h src/Point.h src/Rating.h \
 src/UctRating.h src/PatternHash.h src/Semeai.h src/Utility.h
src/Rating.o: src/Rating.h src/GoBoard.h src/Pattern.h src/UctRating.h \
 src/PatternHash.h
src/RayMain.o: src/RayMain.cpp src/Command.h src/GoBoard.h src/Pattern.h \
 src/Gtp.h src/PatternHash.h src/Rating.h src/UctRating.h src/Semeai.h \
 src/UctSearch.h src/ZobristHash.h
src/Semeai.o: src/Semeai.cpp src/GoBoard.h src/Pattern.h src/Message.h \
 src/UctSearch.h src/ZobristHash.h src/Point.h src/Semeai.h \
 src/UctRating.h src/PatternHash.h
src/Semeai.o: src/Semeai.h src/GoBoard.h src/Pattern.h
src/Simulation.o: src/Simulation.cpp src/GoBoard.h src/Pattern.h \
 src/Message.h src/UctSearch.h src/ZobristHash.h src/Point.h src/Rating.h \
 src/UctRating.h src/PatternHash.h src/Simulation.h
src/Simulation.o: src/Simulation.h src/GoBoard.h src/Pattern.h \
 src/UctSearch.h src/ZobristHash.h
src/UctRating.o: src/UctRating.cpp src/Ladder.h src/GoBoard.h src/Pattern.h \
 src/Message.h src/UctSearch.h src/ZobristHash.h src/Nakade.h \
 src/PatternHash.h src/Point.h src/Semeai.h src/Utility.h src/UctRating.h
src/UctRating.o: src/UctRating.h src/GoBoard.h src/Pattern.h \
 src/PatternHash.h
src/UctSearch.o: src/UctSearch.cpp src/DynamicKomi.h src/GoBoard.h \
 src/Pattern.h src/UctSearch.h src/ZobristHash.h src/Ladder.h \
 src/Message.h src/PatternHash.h src/Simulation.h src/UctRating.h \
 src/Utility.h
src/UctSearch.o: src/UctSearch.h src/GoBoard.h src/Pattern.h \
 src/ZobristHash.h
src/Utility.o: src/Utility.cpp src/Utility.h
src/Utility.o: src/Utility.h
src/ZobristHash.o: src/ZobristHash.cpp src/Nakade.h src/ZobristHash.h \
 src/GoBoard.h src/Pattern.h
src/ZobristHash.o: src/ZobristHash.h src/GoBoard.h src/Pattern.h
