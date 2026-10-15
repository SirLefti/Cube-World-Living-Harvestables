// Address table for the SERVER: Server.exe
#ifndef LH_TARGET_H
#define LH_TARGET_H

#include <stdint.h>

// hit-resolution happens at two locations in the same function, we patch both
static const uint32_t RVA_HIT_A           = 0x000CF00E;
static const uint32_t RVA_HIT_B           = 0x000CF031;

static const uint32_t RVA_GET_MAX_HP      = 0x0000FDA0; // Creature::GetMaxHP -> st0

// behavior machinery
static const uint32_t RVA_OPERATOR_NEW    = 0x0014A7B6; // operator new(uint)    cdecl
static const uint32_t RVA_SEQ_BEHAVIOR    = 0x0001CFC0; // SequentialBehavior    (12 b)
static const uint32_t RVA_COMBAT_BEHAVIOR = 0x000029E0; // CombatBehavior(float) (20 b)
static const uint32_t RVA_LOOKAT_BEHAVIOR = 0x00014720; // LookAtPlayerBehavior  (4 b)
static const uint32_t RVA_WALK_BEHAVIOR   = 0x0001CB90; // RandomWalkBehavior    (8 b)
static const uint32_t RVA_LIST_PUSH_BACK  = 0x000D6620; // list<Behavior*>::push_back

#endif
