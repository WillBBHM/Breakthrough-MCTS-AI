CC=g++
CFLAGS=-std=c++11 -Wall -O2

##### BREAKTHROUGH
rand_player: mybt.h rand_player.cpp
	$(CC) $(CFLAGS) rand_player.cpp -o $@

mcts_player: mybt.h mcts_player.cpp
	$(CC) $(CFLAGS) mcts_player.cpp -o $@

