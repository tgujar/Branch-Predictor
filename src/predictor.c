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
const int perceptron_threshold = 32768;

int ghistoryBits; // Number of bits used for Global History
int lhistoryBits; // Number of bits used for Local History
int pcIndexBits;  // Number of bits used for PC index
int bpType;       // Branch Prediction Type
int verbose;

//////////////////////////////// utils //////////////////////////////////////////////
uint32_t getLowerNBits(uint32_t val, int n)
{
  if (n == 32)
    return val;
  if (n == 0)
    return 0;
  uint32_t mask = 1 << n;
  mask = mask - 1;
  return val & mask;
}

int getTableSize(int bits)
{
  return (1 << bits);
}

int8_t getSign(int32_t v)
{
  if (v < 0)
    return -1;
  return 1;
}

void checkMem(void *v)
{
  if (v == NULL)
  {
    printf("error allocating memory");
    exit(EXIT_FAILURE);
  }
}

struct Counter
{
  int *counts;
  int table_size;
  int max_count;
};
typedef struct Counter Counter;

Counter *counter_init(int table_size, int max_count)
{
  Counter *c = (Counter *)malloc(sizeof(Counter));
  checkMem(c);
  int *counters_arr = (int *)calloc(table_size, sizeof(int));
  checkMem(counters_arr);
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
  BimodalCounter *bc = (BimodalCounter *)malloc(sizeof(BimodalCounter));
  checkMem(bc);
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

//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

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
  int *hist_table;
  int hist_bits;
  int pc_bits;
  BimodalCounter *bc;
};
typedef struct Lhist Lhist;
Lhist *lhist;

// Choice
struct Choice
{
  uint32_t ghistory;
  uint32_t ghistoryMask;
  Lhist *lhist;
  BimodalCounter *choice_bc; // we choose global if outcome is true
  BimodalCounter *global_bc;
};
typedef struct Choice Choice;
Choice *choice;

// Custom
struct Perceptron
{
  uint32_t width;
  int16_t *weights;
  int16_t bias;
};
typedef struct Perceptron Perceptron;

struct PerceptronTable
{
  Perceptron **pt;
  int table_size;
  uint64_t ghistory;
  uint64_t ghistoryMask;
  uint32_t pcMask;
};
typedef struct PerceptronTable PerceptronTable;
PerceptronTable *ptable;

struct PShare // Hybid Gshare and PerceptronTable, weakly favor gshare at start
{
  PerceptronTable *ptable;
  Gshare *gshare;
  BimodalCounter *bc;
  uint32_t ghistory;
  uint32_t ghistoryMask;
};
typedef struct PShare PShare;
PShare *pshare;

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

//////////////////////////////////////// GSHARE ////////////////////////////////////////////
Gshare *gshare_init(int ghistoryBits)
{
  Gshare *g = (Gshare *)malloc(sizeof(Gshare));
  checkMem(g);
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
  return (pc & g->ghistoryMask) ^ g->ghistory;
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

//////////////////////////////////////// Local History ////////////////////////////////////////////

// Local History functions
Lhist *lhist_init(int pcIndexBits, int lhistoryBits)
{
  Lhist *lh = (Lhist *)malloc(sizeof(Lhist));
  checkMem(lh);
  lh->hist_bits = lhistoryBits;
  lh->pc_bits = pcIndexBits;
  int history_table_size = getTableSize(pcIndexBits);
  int counter_table_size = getTableSize(lhistoryBits);

  lh->hist_table = calloc(history_table_size, sizeof(int));
  checkMem(lh->hist_table);
  lh->bc = bimodalCounter_init(counter_table_size);
  return lh;
}

void lhist_destroy(Lhist *lh)
{
  free(lh->hist_table);
  bimodalCounter_destroy(lh->bc);
  free(lh);
}

uint32_t lhist_get_hist_index(Lhist *lh, uint32_t pc)
{
  return getLowerNBits(pc, lh->pc_bits);
}

uint8_t lhist_predict(Lhist *lh, uint32_t pc)
{
  uint32_t tidx = lhist_get_hist_index(lh, pc);
  uint32_t cidx = lh->hist_table[tidx];
  return getOutcome(lh->bc, cidx);
}

void lhist_add_history(Lhist *lh, uint32_t pc, bool taken)
{
  uint32_t tidx = lhist_get_hist_index(lh, pc);
  int *curr_hist = &(lh->hist_table[tidx]);
  *curr_hist = *curr_hist << 1;
  if (taken)
    *curr_hist |= 1;
  *curr_hist = getLowerNBits(*curr_hist, lh->hist_bits);
}

void lhist_update(Lhist *lh, uint32_t pc, uint8_t outcome)
{
  uint32_t tidx = lhist_get_hist_index(lh, pc); // index into history table
  uint32_t cidx = lh->hist_table[tidx];         // index for counter = value of history at index tidx
  if (outcome == 1)
  {
    increment(lh->bc->counter, cidx);
  }
  else
  {
    decrement(lh->bc->counter, cidx);
  }
  lhist_add_history(lh, pc, outcome == 1);
}

//////////////////////////////////////// Choice/ Tournament ////////////////////////////////////////////

Choice *choice_init(int ghistoryBits, int pcIndexBits, int lhistoryBits)
{
  Choice *cp = (Choice *)malloc(sizeof(Choice));
  cp->ghistory = 0;
  cp->ghistoryMask = getLowerNBits(~0, ghistoryBits);
  cp->lhist = lhist_init(pcIndexBits, lhistoryBits);
  int table_size = getTableSize(ghistoryBits);
  cp->global_bc = bimodalCounter_init(table_size);

  BimodalCounter *bc = bimodalCounter_init(table_size);
  int *arr = bc->counter->counts;
  for (int i = 0; i < table_size; i++)
  {
    arr[i] = 2; // set to weakly select global
  }
  cp->choice_bc = bc;

  return cp;
}

void choice_destroy(Choice *cp)
{
  lhist_destroy(cp->lhist);
  bimodalCounter_destroy(cp->choice_bc);
  bimodalCounter_destroy(cp->global_bc);
  free(cp);
}

void choice_add_history(Choice *cp, uint32_t pc, bool taken)
{
  cp->ghistory = cp->ghistory << 1;
  if (taken)
    cp->ghistory |= 1;
  cp->ghistory &= cp->ghistoryMask;
}

uint8_t choice_predict(Choice *cp, uint32_t pc)
{
  bool chooseGlobal = getOutcome(cp->choice_bc, cp->ghistory); // history decides index in table
  if (chooseGlobal)
  {
    return getOutcome(cp->global_bc, cp->ghistory); // global depends only on history
  }
  return lhist_predict(cp->lhist, pc);
}

void choice_update(Choice *cp, uint32_t pc, uint8_t outcome)
{
  uint32_t idx = cp->ghistory;
  ////// Update choice predictor
  uint8_t global_pred = getOutcome(cp->global_bc, idx);
  uint8_t lhist_pred = lhist_predict(cp->lhist, pc);
  // if both predict wrong or both predict correct, stay in the current state
  if (!((global_pred != outcome && lhist_pred != outcome) || (global_pred == outcome && lhist_pred == outcome)))
  {
    if (global_pred == outcome) // if correct pred, enforce
    {
      increment(cp->choice_bc->counter, idx);
    }
    else
    {
      decrement(cp->choice_bc->counter, idx);
    }
  }

  // Update local history predictor
  lhist_update(cp->lhist, pc, outcome == 1);

  // Update global predictor
  if (outcome == 0)
    decrement(cp->global_bc->counter, idx);
  else
    increment(cp->global_bc->counter, idx);

  choice_add_history(cp, pc, outcome == 1);
}

//////////////////////////////////////// CUSTOM ////////////////////////////////////////////

Perceptron *perceptron_init(uint32_t width)
{
  Perceptron *p = (Perceptron *)malloc(sizeof(Perceptron));
  p->weights = calloc(width, sizeof(int16_t));
  p->width = width;
  return p;
}

int32_t perceptron_compute(Perceptron *p, uint64_t history)
{
  int32_t out = p->bias;
  for (int i = 0; i < p->width; i++)
  {
    if (history & (1 << i)) // if bit is 1 then taken
    {
      out += p->weights[i];
    }
    else // not taken
    {
      out -= p->weights[i];
    }
  }
  return out;
}

void perceptron_train(Perceptron *p, uint8_t outcome, uint64_t history) // -1 is NT, 1 is T
{
  int32_t y_out = perceptron_compute(p, history);
  int8_t br_outcome = outcome == 1 ? 1 : -1;
  if (getSign(y_out) != br_outcome || abs(y_out) <= perceptron_threshold)
  {
    p->bias += getSign(y_out);
    for (int i = 0; i < p->width; i++)
    {
      if (history & (1 << i))
      {
        p->weights[i] += getSign(y_out);
      }
      else
      {
        p->weights[i] -= getSign(y_out);
      }
    }
  }
}

PerceptronTable *perceptronTable_init(int pcIndexBits, int ghistoryBits)
{
  PerceptronTable *ptable = (PerceptronTable *)malloc(sizeof(PerceptronTable));
  int table_size = getTableSize(pcIndexBits);
  int perceptron_width = ghistoryBits;
  ptable->table_size = table_size;
  ptable->ghistory = 0;
  ptable->ghistoryMask = (1 << ghistoryBits) - 1; // need 64bit val
  ptable->pcMask = getLowerNBits(~0, pcIndexBits);
  ptable->pt = calloc(table_size, sizeof(Perceptron *));
  for (int i = 0; i < table_size; i++)
  {
    ptable->pt[i] = perceptron_init(perceptron_width);
  }
  return ptable;
}

void perceptronTable_addHistory(PerceptronTable *ptable, bool taken)
{
  ptable->ghistory = ptable->ghistory << 1;
  if (taken)
    ptable->ghistory |= 1;
  ptable->ghistory &= ptable->ghistoryMask;
}

Perceptron *perceptronTable_getPerceptron(PerceptronTable *ptable, uint32_t pc)
{
  uint32_t hash = ((uint64_t)pc) * (pc + 3) % ptable->pcMask;
  return ptable->pt[hash];
  // return ptable->pt[(pc ^ ptable->ghistory) & ptable->pcMask];
}

bool perceptronTable_predict(PerceptronTable *ptable, uint32_t pc)
{
  Perceptron *p = perceptronTable_getPerceptron(ptable, pc);
  int32_t y = perceptron_compute(p, ptable->ghistory);
  return y >= 0;
}

void perceptronTable_update(PerceptronTable *ptable, uint32_t pc, uint8_t outcome)
{
  Perceptron *p = perceptronTable_getPerceptron(ptable, pc);
  perceptron_train(p, outcome, ptable->ghistory);
  perceptronTable_addHistory(ptable, outcome == 1);
}

PShare *pshare_init(int pcIndexBits, int ghistoryBits, int phistoryBits)
{
  PShare *pshare = (PShare *)malloc(sizeof(PShare));
  int table_size = getTableSize(ghistoryBits);
  pshare->bc = bimodalCounter_init(table_size);
  pshare->ghistory = 0;
  pshare->ghistoryMask = getLowerNBits(~0, ghistoryBits);
  int *arr = pshare->bc->counter->counts;
  for (int i = 0; i < table_size; i++)
  {
    arr[i] = 2; // set to weakly select gshare
  }
  pshare->ptable = perceptronTable_init(pcIndexBits, phistoryBits);
  pshare->gshare = gshare_init(ghistoryBits);
  return pshare;
}

uint8_t pshare_predict(PShare *pshare, uint32_t pc)
{
  bool chooseGshare = getOutcome(pshare->bc, pshare->ghistory); // history decides index in table
  if (chooseGshare)
  {
    return gshare_predict(pshare->gshare, pc);
  }
  return perceptronTable_predict(pshare->ptable, pc);
}

void pshare_add_history(PShare *pshare, uint32_t pc, bool taken)
{
  pshare->ghistory = pshare->ghistory << 1;
  if (taken)
    pshare->ghistory |= 1;
  pshare->ghistory &= pshare->ghistoryMask;
}

void pshare_update(PShare *pshare, uint32_t pc, uint8_t outcome)
{
  uint32_t idx = pshare->ghistory;
  ////// Update choice predictor
  uint8_t gshare_pred = gshare_predict(pshare->gshare, pc);
  uint8_t ptable_pred = perceptronTable_predict(pshare->ptable, pc);
  // if both predict wrong or both predict correct, stay in the current state
  if (!((gshare_pred != outcome && ptable_pred != outcome) || (gshare_pred == outcome && ptable_pred == outcome)))
  {
    if (gshare_pred == outcome) // if correct pred, enforce
    {
      increment(pshare->bc->counter, idx);
    }
    else
    {
      decrement(pshare->bc->counter, idx);
    }
  }

  // Update ptable predictor
  perceptronTable_update(pshare->ptable, pc, outcome);

  // Update gshare predictor
  gshare_update(pshare->gshare, pc, outcome);

  pshare_add_history(pshare, pc, outcome == 1);
}

// Initialize the predictor
//
void init_predictor()
{
  switch (bpType)
  {
  case STATIC:
    return;
  case GSHARE:
    gshare = gshare_init(ghistoryBits);
    break;
  case TOURNAMENT:
    choice = choice_init(ghistoryBits, pcIndexBits, lhistoryBits);
    break;
  case CUSTOM:
    pshare = pshare_init(5, 13, 32); //(pcIndexBits,ghistoryBits, phistoryBits)
    break;
  default:
    break;
  }
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
    return choice_predict(choice, pc);
  case CUSTOM:
    return pshare_predict(pshare, pc);
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
  switch (bpType)
  {
  case STATIC:
    return;
  case GSHARE:
    gshare_update(gshare, pc, outcome);
    break;
  case TOURNAMENT:
    choice_update(choice, pc, outcome);
    break;
  case CUSTOM:
    pshare_update(pshare, pc, outcome);
    break;
  default:
    break;
  }
}
