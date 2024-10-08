#include <cstdlib>
#include <algorithm>

#include "pokemon.h"
#include "db_parse.h"
#include "poke327.h"

static bool operator<(const levelup_move &f, const levelup_move &s)
{
  return ((f.level < s.level) || ((f.level == s.level) && f.move < s.move));
}

static int pkmn_lvl()
{
    int md = (abs(world.cur_idx[dim_x] - (WORLD_SIZE / 2)) +
            abs(world.cur_idx[dim_y] - (WORLD_SIZE / 2)));
  int minl, maxl;

  if (md <= 200) {
    minl = 1;
    maxl = md / 2;
  } else {
    minl = (md - 200) / 2;
    maxl = 100;
  }
  if (minl < 1) {
    minl = 1;
  }
  if (minl > 100) {
    minl = 100;
  }
  if (maxl < 1) {
    maxl = 1;
  }
  if (maxl > 100) {
    maxl = 100;
  }

  return (rand() % (maxl - minl + 1)) + minl;
}

pokemon::pokemon() : pokemon(pkmn_lvl()) {}

pokemon::pokemon(int level) : level(level)
{
  pokemon_species_db *s;
  unsigned i, j;
  bool found;

  pokemon_species_index = rand() % ((sizeof (species) /
                                     sizeof (species[0])) - 1);
  s = species + pokemon_species_index;
  
  if (!s->levelup_moves.size()) {

    for (i = 1; i < (sizeof (pokemon_moves) /
                     sizeof (pokemon_moves[0])); i++) {
      if (s->id == pokemon_moves[i].pokemon_id &&
          pokemon_moves[i].pokemon_move_method_id == 1) {
        for (found = false, j = 0; !found && j < s->levelup_moves.size(); j++) {
          if (s->levelup_moves[j].move == pokemon_moves[i].move_id) {
            found = true;
          }
        }
        if (!found) {
          s->levelup_moves.push_back({ pokemon_moves[i].level,
                                       pokemon_moves[i].move_id });
        }
      }
    }

    
    sort(s->levelup_moves.begin(), s->levelup_moves.end());

    s->base_stat[0] = pokemon_stats[pokemon_species_index * 6 - 5].base_stat;
    s->base_stat[1] = pokemon_stats[pokemon_species_index * 6 - 4].base_stat;
    s->base_stat[2] = pokemon_stats[pokemon_species_index * 6 - 3].base_stat;
    s->base_stat[3] = pokemon_stats[pokemon_species_index * 6 - 2].base_stat;
    s->base_stat[4] = pokemon_stats[pokemon_species_index * 6 - 1].base_stat;
    s->base_stat[5] = pokemon_stats[pokemon_species_index * 6 - 0].base_stat;
  }

  for (i = 0;
       i < s->levelup_moves.size() && s->levelup_moves[i].level <= level;
       i++)
    ;
  move_index[0] = move_index[1] = move_index[2] = move_index[3] = 0;
  if (i) {
    move_index[0] = s->levelup_moves[rand() % i].move;
    if (i != 1) {
      do {
        j = rand() % i;
      } while (s->levelup_moves[j].move == move_index[0]);
      move_index[1] = s->levelup_moves[j].move;
    }
  }
  for (i = 0; i < 6; i++) {
    IV[i] = rand() & 0xf;
    effective_stat[i] = 5 + ((s->base_stat[i] + IV[i]) * 2 * level) / 100;
    if (i == 0) { // HP
      effective_stat[i] += 5 + level;
    }
  }

  shiny = (((rand() & 0x1fff) == 0x1fff) ? true : false);
  gender = ((rand() & 0x1) ? gender_female : gender_male);
}

const char *pokemon::get_species() const
{
  return species[pokemon_species_index].identifier;
}

int pokemon::get_hp() const
{
  return effective_stat[stat_hp];
}

int pokemon::get_atk() const
{
  return effective_stat[stat_atk];
}

int pokemon::get_def() const
{
  return effective_stat[stat_def];
}

int pokemon::get_spatk() const
{
  return effective_stat[stat_spatk];
}

int pokemon::get_spdef() const
{
  return effective_stat[stat_spdef];
}

int pokemon::get_speed() const
{
  return effective_stat[stat_speed];
}

const char *pokemon::get_gender_string() const
{
  return gender == gender_female ? "female" : "male";
}

bool pokemon::is_shiny() const
{
  return shiny;
}

const char *pokemon::get_move(int i) const
{
  if (i < 4 && move_index[i]) {
    return moves[move_index[i]].identifier;
  } else {
    return "";
  }
}
