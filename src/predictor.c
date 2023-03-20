//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <stdbool.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Tanmay Gujar";
const char *studentID = "A59002351";
const char *email = "tgujar@ucsd.edu";

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = {"Static", "Gshare",
                         "Tournament", "Custom"};

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

// utils
uint32_t getLowerNBits(uint32_t val, int n)
{
  if (n == 32)
    return val;
  uint32_t mask = 1 << n;
  mask = mask - 1;
  return val & mask;
}

int getTableSize(int bits)
{
  return (1 << bits);
}

struct Counter
{
  int *counts;
  int table_size;
  int max_count;
};
typedef struct Counter Counter;
Counter gshare_c, lhist_c;

Counter *counter_init(int table_size, int max_count)
{
  Counter *c = malloc(sizeof(Counter));
  int counter_table_size = getTableSize(table_size);
  int *counters_arr = calloc(counter_table_size, sizeof(int));
  c->counts = counters_arr;
  c->table_size = table_size;
  c->max_count = max_count;
  return c;
}

void counter_destroy(Counter *c)
{
  free(c->counts);
  free(c);
}

void increment(Counter *c, int index)
{
  int *val = &(c->counts[index]);
  *val += 1;
  if (*val > c->max_count)
    *val = c->max_count;
}

void decrement(Counter *c, int index)
{
  int *val = &(c->counts[index]);
  *val -= 1;
  if (*val == -1)
    *val = 0;
}

struct BimodalCounter
{
  Counter *counter;
};
typedef struct BimodalCounter BimodalCounter;

BimodalCounter *bimodalCounter_init(int table_size)
{
  BimodalCounter *bc = malloc(sizeof(BimodalCounter));
  Counter *c = counter_init(table_size, 3);
  bc->counter = c;
  return bc;
}

void bimodalCounter_destroy(BimodalCounter *bc)
{
  counter_destroy(bc->counter);
  free(bc);
}

uint8_t getOutcome(BimodalCounter *bc, int index)
{
  return bc->counter->counts[index] >= 2;
}

// GShare
struct Gshare
{
  uint32_t ghistory;
  uint32_t ghistoryMask;
  BimodalCounter *bc;
};
typedef struct Gshare Gshare;
Gshare *gshare;

// LocalHistory
struct Lhist
{
  int *hist;
  int hist_table_size;
  BimodalCounter *bc;
};
typedef struct Lhist Lhist;

Gshare *gshare_init(int ghistoryBits)
{
  Gshare *g = malloc(sizeof(Gshare));
  int table_size = getTableSize(ghistoryBits);
  BimodalCounter *bc = bimodalCounter_init(table_size);
  g->bc = bc;
  g->ghistory = 0;
  g->ghistoryMask = getLowerNBits(~0, ghistoryBits);
  return g;
}

void gshare_destroy(Gshare *g)
{
  bimodalCounter_destroy(g->bc);
  free(g);
}

void gshare_add_history(Gshare *g, bool taken)
{
  g->ghistory = g->ghistory << 1;
  if (taken)
    g->ghistory |= 1;
  g->ghistory &= g->ghistoryMask;
}

uint32_t gshare_getIndex(Gshare *g, uint32_t pc)
{
  return getLowerNBits(pc, ghistoryBits) ^ g->ghistory;
}

uint8_t gshare_predict(Gshare *g, uint32_t pc)
{
  uint32_t idx = gshare_getIndex(g, pc);
  return getOutcome(g->bc, idx);
}

void gshare_update(Gshare *g, uint32_t pc, uint8_t outcome)
{
  uint32_t idx = gshare_getIndex(g, pc);
  if (outcome == 0)
    decrement(g->bc->counter, idx);
  else
    increment(g->bc->counter, idx);
  gshare_add_history(g, outcome == 1);
}

// Local History functions
Lhist *lhist_init(int pcIndexBits, int lhistoryBits)
{
  Lhist *lh = malloc(sizeof(Lhist));
  int history_table_size = getTableSize(pcIndexBits);
  int counter_table_size = getTableSize(lhistoryBits);

  lh->hist = calloc(history_table_size, sizeof(int));
  lh->hist_table_size = history_table_size;
  lh->bc = bimodalCounter_init(counter_table_size);
  return lh;
}

void lhist_destroy(Lhist *lh)
{
  free(lh->hist);
  bimodalCounter_destroy(lh->bc);
  free(lh);
}

//
// TODO: Add your own Branch Predictor data structures here
//

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Initialize the predictor
//
void init_predictor()
{
  // Gshare
  gshare = gshare_init(ghistoryBits);
  // Local History
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{

  // Make a prediction based on the bpType
  switch (bpType)
  {
  case STATIC:
    return TAKEN;
  case GSHARE:
    return gshare_predict(gshare, pc);
  case TOURNAMENT:
  case CUSTOM:
  default:
    break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//
void train_predictor(uint32_t pc, uint8_t outcome)
{
  // Gshare
  gshare_update(gshare, pc, outcome);
}
