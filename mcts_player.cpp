#include <cstdio>
#include <cstdlib>
#include <string.h>
#include <iostream>
#include <string>
#include "mybt.h"

#include <unordered_map>
#include <utility>
#include <vector>
#include <cmath>
#include <chrono>
#include <thread>
#include <mutex>
#include <random>

bt_t B;
int boardwidth = 0;
int boardheight = 0;
bool white_turn = true;

#ifndef VERBOSE_RAND_PLAYER
#define VERBOSE_RAND_PLAYER
bool verbose = false;
bool showboard_at_each_move = false;
#endif

char playername[128];

// Variables globales
std::unordered_map<std::string, std::pair<int, int>> value_map;
std::unordered_map<std::string, double> td_values;
std::vector<bt_t> state_list;
std::unordered_map<uint64_t, std::pair<int, int>> transposition_table;
uint64_t zobrist_table[MAX_LINES][MAX_COLS][2];
std::mutex mtx;

// Fonctions
bt_move_t random_move(bt_t state);
std::string get_state(bt_t state);
int heuristic_playout(bt_t state, int color);
bt_t selection(bt_t state);
double uct(bt_t state, bt_t current_state);
void backpropagate(double score);
bt_move_t bestnext(bt_t state);
void MCTS_thread(bt_t state, bt_move_t& best_move);
bt_move_t MCTS(bt_t state);
uint64_t zobrist_hash(bt_t state);
void init_zobrist();

void help() {
  fprintf(stderr, "  mode cla | mis (classic OR misere)\n"); 
  fprintf(stderr, "  mode alt | sim (alternate OR simultaneous)\n");
  fprintf(stderr, "  mode fui | drk | bld (fullinfo OR dark OR blind)\n");
  fprintf(stderr, "  showmodes\n");  
  fprintf(stderr, "  quit\n");
  fprintf(stderr, "  echo ON | OFF\n");
  fprintf(stderr, "  help\n");
  fprintf(stderr, "  name\n");
  fprintf(stderr, "  setname <NEW_PLAYER_NAME>\n");
  fprintf(stderr, "  newgame <NBCOL> <NBLINE>\n");
  fprintf(stderr, "  showboard (print board on stderr)\n");
  fprintf(stderr, "  strboard\n");
  fprintf(stderr, "  seed SEED\n");
  fprintf(stderr, "-- alt MODE\n");
  fprintf(stderr, "  genmove\n");
  fprintf(stderr, "  play <L0C0L1C1>\n");
  fprintf(stderr, "-- sim MODE\n");
  fprintf(stderr, "  genmove <TURN>\n");
  fprintf(stderr, "  play <L0C0L1C1> <L0C0L1C1>\n");
  fprintf(stderr, "-- dark and blind MODES\n");
  fprintf(stderr, "  setboard <GAME_TURN> <BOARD_SEEN>\n"); // player's side cmd
  fprintf(stderr, "  strboard <TURN>\n"); // referee's side cmd on oracle
}
void name() {
  printf("= %s\n\n", playername);
}
void setname(char* _name) {
  strncpy(playername, _name, 127);
  printf("= \n\n");
}
void newgame() {
  if((boardheight < 1 || boardheight > 10) && (boardwidth < 1 || boardwidth > 10)) {
    fprintf(stderr, "boardsize is %d %d ???\n", boardheight, boardwidth);
    printf("= \n\n");
    return;
  }
  B.init(boardheight, boardwidth);
  white_turn = true;
  if(verbose) fprintf(stderr, "ready to play on %dx%d board\n", boardheight, boardwidth);
  printf("= \n\n");
}
void mode(std::string _strmode) {
  if(_strmode.compare("cla") == 0) B.set_cla_or_mis(0);
  if(_strmode.compare("mis") == 0) B.set_cla_or_mis(1);
  if(_strmode.compare("alt") == 0) B.set_alt_or_sim(0);
  if(_strmode.compare("sim") == 0) B.set_alt_or_sim(1);
  if(_strmode.compare("fui") == 0) B.set_fullinfo_or_dark_or_blind(0);
  if(_strmode.compare("drk") == 0) B.set_fullinfo_or_dark_or_blind(1);
  if(_strmode.compare("bld") == 0) B.set_fullinfo_or_dark_or_blind(2);
  if(verbose) {
    if(_strmode.compare("cla") == 0) fprintf(stderr, " set CLASSIC\n");
    if(_strmode.compare("mis") == 0) fprintf(stderr, " set MISERE\n");
    if(_strmode.compare("alt") == 0) fprintf(stderr, " set ALTERNATE\n");
    if(_strmode.compare("sim") == 0) fprintf(stderr, " set SIMULTANEOUS\n");
    if(_strmode.compare("fui") == 0) fprintf(stderr, " set FULLINFO\n");
    if(_strmode.compare("drk") == 0) fprintf(stderr, " set DARK\n");
    if(_strmode.compare("bld") == 0) fprintf(stderr, " set BLIND\n");
  }
  printf("= \n\n");
}
void showmodes() {
  if(B.classic_or_misere == 0) fprintf(stderr, " classic\n");
  if(B.classic_or_misere == 1) fprintf(stderr, " misere\n");
  if(B.alternate_or_simultaneous == 0) fprintf(stderr, " alternate\n");
  if(B.alternate_or_simultaneous == 1) fprintf(stderr, " simultaneous\n");
  if(B.fullinfo_or_dark_or_blind == 0) fprintf(stderr, " fullinfo\n");
  if(B.fullinfo_or_dark_or_blind == 1) fprintf(stderr, " dark\n");
  if(B.fullinfo_or_dark_or_blind == 2) fprintf(stderr, " blind\n");
  printf("= \n\n");
}
void showboard() {
  B.print_board(stderr);
  printf("= \n\n");
}
void getstrboard() {
  char strb[128];
  B.get_board(strb);
  printf("= %s\n\n", strb);  
}
void getstrboard(char _turn) {
  if(_turn == '@') {
    char ret[128];
    B.get_board(BLACK, ret);    
    printf("= %s\n\n", ret);
  } else {
    char ret[128];
    B.get_board(WHITE, ret);    
    printf("= %s\n\n", ret);
  }
}
// to define the board with variants DARK and BLIND
// at each turn, before asking genmove, it is needed to set the board
// board 0 ??????...oooooo
// board 1 @@@@@@.o.??????
// board 2 ???.@@@o..ooooo
void setboard(int _game_turn, char _str_board[MAX_COLS*MAX_LINES]) {  
  white_turn = ((_game_turn%2)==0);
  B.init_board(_game_turn, _str_board);
  printf("= \n\n");
}
void genmove() {
  int ret = B.endgame();  
  if(ret != EMPTY) {
    fprintf(stderr, "game finished\n");
    if(ret == WHITE) fprintf(stderr, "white player wins\n");
    else fprintf(stderr, "black player wins\n");
    printf("= \n\n");
    return;
  }
  state_list.clear();
  value_map.clear();
  bt_move_t m = MCTS(B);
  printf("= %s\n\n", m.tostr(B.nbl).c_str());
}
void genmove(char _turn) {  
  if(_turn == '@') {
    bt_move_t m = B.get_rand_move(BLACK);
    printf("= %s\n\n", m.tostr(B.nbl).c_str());
  } else {
    bt_move_t m = B.get_rand_move(WHITE);
    printf("= %s\n\n", m.tostr(B.nbl).c_str());
  }
}
void play(char a0, char a1, char a2, char a3) {
  bt_move_t m;
  m.line_i = boardheight-(a0-'0');
  m.col_i = a1-'a';
  m.line_f = boardheight-(a2-'0');
  m.col_f = a3-'a';
  if(B.can_play(m)) {
    B.play(m);
    if(verbose) {
      m.print(stderr, white_turn, B.nbl);
      fprintf(stderr, "\n");
    }
    white_turn = !white_turn;
  } else {
    fprintf(stderr, "CANT play %d %d %d %d ?\n", m.line_i, m.col_i, m.line_f, m.col_f);
  }
  if(showboard_at_each_move) showboard();
  printf("= \n\n");
}
void play(char a0, char a1, char a2, char a3, char b0, char b1, char b2, char b3) {
  bt_move_t m0;
  m0.line_i = boardheight-(a0-'0');
  m0.col_i = a1-'a';
  m0.line_f = boardheight-(a2-'0');
  m0.col_f = a3-'a';
  bt_move_t m1;
  m1.line_i = boardheight-(b0-'0');
  m1.col_i = b1-'a';
  m1.line_f = boardheight-(b2-'0');
  m1.col_f = b3-'a';
  if(B.can_simultaneous_play(m0,m1)) {
    B.play(m0,m1);
    //fprintf(stderr, "after play : %d white / %d black\n", B.nb_white_pieces, B.nb_black_pieces);
  } else {
    fprintf(stderr, "CANT play simultaneously %d %d %d %d with %d %d %d %d\n", m0.line_i, m0.col_i, m0.line_f, m0.col_f, m1.line_i, m1.col_i, m1.line_f, m1.col_f);
  }
  if(showboard_at_each_move) showboard();
  printf("= \n\n");
}
int main(int _ac, char** _av) {
  bool echo_on = false;
  setbuf(stdout, 0);
  setbuf(stderr, 0);
  if(verbose) fprintf(stderr, "player started\n");
  char a0,a1,a2,a3; // for play cmd0
  char b0,b1,b2,b3; // for play cmd0 cmd1
  int game_turn;
  char str_board[MAX_COLS*MAX_LINES]; 
  char newname[128];
  char newmode[128];
  int newseed = 0;
  for (std::string line; std::getline(std::cin, line);) {
    if(verbose) fprintf(stderr, "%s receive %s\n", playername, line.c_str());
    if(echo_on) if(verbose) fprintf(stderr, "%s\n", line.c_str());
    bool cmd_ok = false;
    if(sscanf(line.c_str(), "mode %s\n", newmode) == 1) { cmd_ok=true; mode(newmode);}
    else if(line.compare("showmodes") == 0) { cmd_ok=true; showmodes();}
    else if(line.compare("quit") == 0) { cmd_ok=true; printf("= \n\n"); break;}
    else if(line.compare("echo ON") == 0) { cmd_ok=true; echo_on = true;}
    else if(line.compare("echo OFF") == 0) { cmd_ok=true; echo_on = false;}
    else if(line.compare("help") == 0) { cmd_ok=true; help();}
    else if(line.compare("name") == 0) { cmd_ok=true; name();}
    else if(sscanf(line.c_str(), "setname %s\n", newname) == 1) { cmd_ok=true; setname(newname);}
    else if(sscanf(line.c_str(), "newgame %d %d\n", &boardheight, &boardwidth) == 2) 
      { cmd_ok=true; newgame();}
    else if(line.compare("showboard") == 0) { cmd_ok=true; showboard();}
    else if(line.compare("strboard") == 0) { cmd_ok=true; getstrboard();}
    else if(sscanf(line.c_str(), "seed %d\n", &newseed) == 1) 
      { cmd_ok=true; srand(newseed); printf("= \n\n"); }
    
    if(B.alternate_or_simultaneous == 0) {
      if(line.compare("genmove") == 0) { cmd_ok=true; genmove();}
      else if(sscanf(line.c_str(), "play %c%c%c%c\n", &a0,&a1,&a2,&a3) == 4) 
        { cmd_ok=true; play(a0,a1,a2,a3);}
    }

    if(B.alternate_or_simultaneous == 1) {
      if(sscanf(line.c_str(), "genmove %c\n", &a0) == 1) { cmd_ok=true; genmove(a0);}
      else if(sscanf(line.c_str(), "play %c%c%c%c %c%c%c%c\n", &a0,&a1,&a2,&a3,&b0,&b1,&b2,&b3) == 8) 
        { cmd_ok=true; play(a0,a1,a2,a3,b0,b1,b2,b3);}
    }

    if(B.fullinfo_or_dark_or_blind == 1 || B.fullinfo_or_dark_or_blind == 2) {
      if(sscanf(line.c_str(), "setboard %d %s\n", &game_turn, str_board) == 2) 
        { cmd_ok=true; setboard(game_turn, str_board);}
      else if(sscanf(line.c_str(), "strboard %c\n", &a0) == 1) { cmd_ok=true; getstrboard(a0);}
    }        

    if(line.compare(0,2,"//") == 0) { cmd_ok=true; } // just comments
    if(cmd_ok == false) fprintf(stderr, "??? [%s] \n", line.c_str());
    if(echo_on) printf(">");
  }
  if(verbose) fprintf(stderr, "bye.\n");
  return 0;
}

void init_zobrist() {
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;
  
  for (int i = 0; i < MAX_LINES; i++) {
    for (int j = 0; j < MAX_COLS; j++) {
      for (int k = 0; k < 2; k++) {
        zobrist_table[i][j][k] = dis(gen);
      }
    }
  }
}

uint64_t zobrist_hash(bt_t state) {
  uint64_t hash = 0;
  for (int i = 0; i < state.nbl; i++) {
    for (int j = 0; j < state.nbc; j++) {
      if (state.board[i][j] != EMPTY) {
        hash ^= zobrist_table[i][j][state.board[i][j]];
      }
    }
  }
  return hash;
}

std::string get_state(bt_t state)
{
  char boardString[128];
  state.get_board(boardString);
  return boardString;
}

int heuristic_playout(bt_t state, int color) {
  bt_move_t m;
  bt_t s = state;
  
  while (s.endgame() == EMPTY) {
    s.update_moves(color);
    if (s.nb_moves == 0) break;
    
    int best_score = -1;
    for (int i = 0; i < s.nb_moves; i++) {
      bt_move_t move = s.moves[i];
      int score = 0;
      
      // Favoriser les coups qui font progresser les pions vers la ligne de fond adverse
      if (color == WHITE) {
        score += move.line_i - move.line_f;
      } else {
        score += move.line_f - move.line_i;
      }
      
      // Favoriser les coups qui capturent des pions ennemis
      if (s.board[move.line_f][move.col_f] != EMPTY && s.board[move.line_f][move.col_f] != color) {
        score += 2;
      }
      
      // Favoriser les coups qui protègent nos propres pions menacés
      if (s.board[move.line_i][move.col_i] == color) {
        int threats = 0;
        if (color == WHITE) {
          if (move.line_i > 0 && move.col_i > 0 && s.board[move.line_i-1][move.col_i-1] == BLACK) threats++;
          if (move.line_i > 0 && move.col_i < s.nbc-1 && s.board[move.line_i-1][move.col_i+1] == BLACK) threats++;
        } else {
          if (move.line_i < s.nbl-1 && move.col_i > 0 && s.board[move.line_i+1][move.col_i-1] == WHITE) threats++;
          if (move.line_i < s.nbl-1 && move.col_i < s.nbc-1 && s.board[move.line_i+1][move.col_i+1] == WHITE) threats++;
        }
        score += threats;
      }
      
      if (score > best_score) {
        best_score = score;
        m = move;
      }
    }
    
    s.play(m);
    color = !color;
  }
  
  return s.endgame();
}

double uct(bt_t state, bt_t current_state) {
  double td_value = 0.0;
  if (td_values.find(get_state(current_state)) != td_values.end()) {
    td_value = td_values[get_state(current_state)];
  }

  if (value_map[get_state(current_state)].second == 0) 
    return td_value;

  double q = value_map[get_state(current_state)].first / value_map[get_state(current_state)].second;
  double u = 0.4 * sqrt(log(value_map[get_state(state)].second) / value_map[get_state(current_state)].second);

  return q + 0.5 * td_value + u;
}

bt_t selection(bt_t state)
{
  if (state.endgame() != EMPTY) return state;

  double max = -1.0;
  bt_t best = state;
  state_list.push_back(state);

  for (int i = 0; i < state.nb_moves; ++i)
  {
    bt_move_t m = state.moves[i];
    bt_t new_state = state;
    new_state.play(m);
    new_state.update_moves();

    if (value_map.find(get_state(new_state)) == value_map.end()) {
      state_list.push_back(new_state);
      return new_state;
    }

    double new_eval = uct(state, new_state);

    if (new_eval > max)
    {
      max = new_eval;
      best = new_state;
    }
  }
  return selection(best);
}

void backpropagate(double score) {
  double td_error = score;
  for(auto it = state_list.rbegin(); it != state_list.rend(); ++it) {
    bt_t currentState = *it;
    if (value_map.find(get_state(currentState)) != value_map.end()) {
      value_map[get_state(currentState)].first += score; 
      value_map[get_state(currentState)].second += 1;

      if (td_values.find(get_state(currentState)) != td_values.end()) {
        td_error = score + 0.9 * td_values[get_state(currentState)] - td_values[get_state(*std::prev(it))];
      }
      td_values[get_state(currentState)] += 0.1 * td_error;
    }
  }
}

bt_move_t bestnext(bt_t state)
{
  double max = -1.0;
  bt_move_t best;
  
  for (int i = 0; i < state.nb_moves; i++)
  {
    bt_move_t m = state.moves[i];
    bt_t new_state = state;
    new_state.play(m);
    double new_eval = uct(state, new_state);
    if (new_eval > max)
    {
      max = new_eval;
      best = m;
    }
  }
  return best;
}

double elapsed_time(std::chrono::time_point<std::chrono::steady_clock> start_time) {
    auto end_time = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    return elapsed_seconds.count();
}

void MCTS_thread(bt_t state, bt_move_t& best_move) {
  bt_t current_state;
  state.update_moves();
  
  std::unordered_map<std::string, std::pair<int, int>> local_value_map;
  local_value_map[get_state(state)] = {0, 0};
  
  auto start_time = std::chrono::steady_clock::now();
  
  while (elapsed_time(start_time) < 0.090) {
    std::vector<bt_t> local_state_list;
    current_state = selection(state);
    local_value_map[get_state(current_state)] = {0, 0};
    int r = heuristic_playout(current_state, state.turn % 2);
    local_state_list.push_back(current_state);
    if (state.turn % 2 == 0) {
      backpropagate(r);
    } else {
      backpropagate(!r);
    }
  }
  
  mtx.lock();
  for (auto it = local_value_map.begin(); it != local_value_map.end(); ++it) {
    value_map[it->first].first += it->second.first;
    value_map[it->first].second += it->second.second;
  }
  mtx.unlock();
  
  best_move = bestnext(state);
}

bt_move_t MCTS(bt_t state) {
    state.update_moves();
    value_map[get_state(state)] = {0, 0};

    auto start_time = std::chrono::steady_clock::now();

    while (elapsed_time(start_time) < 0.100) {
        state_list.clear();
        bt_t current_state = selection(state);
        value_map[get_state(current_state)] = {0, 0};
        int r = heuristic_playout(current_state, current_state.turn%2);
        state_list.push_back(current_state);
        if (state.turn % 2 == 0) {
            backpropagate(r);
        } else {
            backpropagate(!r);
        }
    }
    return bestnext(state);
}
