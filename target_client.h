// Address table for the CLIENT: Cube.exe
#ifndef LH_TARGET_H
#define LH_TARGET_H

#include <stdint.h>

static const uint32_t RVA_HIT             = 0x00195FEE;

static const uint32_t RVA_GET_MAX_HP      = 0x00044DB0; // Creature::GetMaxHP -> st0

// behavior machinery
static const uint32_t RVA_OPERATOR_NEW    = 0x0028D668; // operator new(uint)    cdecl
static const uint32_t RVA_SEQ_BEHAVIOR    = 0x000DC2D0; // SequentialBehavior    (12 b)
static const uint32_t RVA_COMBAT_BEHAVIOR = 0x0002C550; // CombatBehavior(float) (20 b)
static const uint32_t RVA_LOOKAT_BEHAVIOR = 0x000C8130; // LookAtPlayerBehavior  (4 b)
static const uint32_t RVA_WALK_BEHAVIOR   = 0x000DBF10; // RandomWalkBehavior    (8 b)
static const uint32_t RVA_LIST_PUSH_BACK  = 0x000860B0; // list<Behavior*>::push_back

#endif
