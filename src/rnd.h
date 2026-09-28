/* Random numbers for everything that is not part of the replayable engine
 * (effects, sound jitter). rand_custom is the generator the original uses
 * for its eye candy, as recovered by the icytower-ng project. */
#pragma once

void rnd_seed_msvc(unsigned int seed);
int rnd_msvc(void);
void rnd_seed_custom(int seed);
int rnd_custom(void);
