// Cube World Alpha - Living Harvestables Mod
//
// Bushes, shrubs, cobweb scrub, crafting plants and ore/gem deposits get a
// chance, when you hit them, to come to life and turn hostile - so you have to
// fight them for the resources instead of just mining them.
//
// Builds for two targets from one source: Cube.exe (client) and Server.exe
// (server). The creature code is identical in both, so only the address table
// differs - see target_client.h / target_server.h.

// ---- build requirements -----------------------------------------------------
#include <stdint.h>   // UINTPTR_MAX - pointer width, checked portably
#include <stddef.h>   // offsetof - the struct layout below checks itself

#if !defined(_WIN32)
#  error "Target must be Windows: this is a DLL injected into Cube.exe / Server.exe. Cross-compile, e.g. i686-w64-mingw32-g++."
#endif
#if UINTPTR_MAX != 0xFFFFFFFFu
#  error "Target must be 32-bit: Cube.exe and Server.exe are both PE32."
#endif
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ < 8
#  error "GCC 8 or newer: older GCC silently IGNORES __attribute__((naked)) on x86 and builds a broken DLL."
#endif

#include <windows.h>

// makefile passes -DTARGET_SERVER for the server build
#ifdef TARGET_SERVER
  #include "target_server.h"
#else
  #include "target_client.h"
#endif

// ---- tuning -----------------------------------------------------------------
// chance per eligible hit that the harvestable wakes up, 0.0 - 1.0
static const float WAKE_CHANCE = 0.10f;

// true  = roll only on the first hit at full HP
// false = roll on every hit
static const bool FIRST_HIT_ONLY = true;

// level of the entity, 0 = leave alone
static const int WAKE_LEVEL = 0;

// multiply the entity's HP so the fight lasts more than one swing
static const float WAKE_HP_MULTIPLIER = 10.0f;

// entity types that can wake up (some of those do not spawn naturally)
static const uint32_t WAKEABLE_TYPES[] = {
    120, 121, 122, 123,   // Bush, SnowBush, SnowBerryBush, CottonPlant
    124, 125, 126, 127,   // Scrub, CobwebScrub, FireScrub, Ginseng
    128,                  // Cactus
    130,                  // ThornTree
    131, 132, 133, 134,   // Gold, Iron, Silver, Sandstone deposits
    135, 136, 137, 138,   // Emerald, Sapphire, Ruby, Diamond deposits
    139                   // IceCrystalDeposit
};

// ---- struct layout ----------------------------------------------------------
// sparse recreation of a creature with only the properties we need and the rest
// being padded with placeholders
struct Creature
{
    uint8_t  _pad0[0x60];
    uint8_t  hostile_type; // see HOSTILE_TYPE_HOSTILE
    uint8_t  _pad1[0x3];
    uint32_t entity_type;
    uint8_t  _pad2[0x16];
    uint16_t appearance_flags;
    uint8_t  _pad3[0xEC];
    float    hp;
    uint8_t  _pad4[0x20];
    int32_t  level;
    uint8_t  _pad5[0x1250];
    void*    behavior; // SequentialBehavior* - 0 on harvestables
};

static_assert(offsetof(Creature, hostile_type)     == 0x60,  "hostile_type offset");
static_assert(offsetof(Creature, entity_type)      == 0x64,  "entity_type offset");
static_assert(offsetof(Creature, appearance_flags) == 0x7E,  "appearance_flags offset");
static_assert(offsetof(Creature, hp)               == 0x16C, "hp offset");
static_assert(offsetof(Creature, level)            == 0x190, "level offset");
static_assert(offsetof(Creature, behavior)         == 0x13E4, "behavior offset");

static const uint8_t  HOSTILE_TYPE_HOSTILE = 1; // 0=friendly player, 1=hostile, 2=friendly, 6=target
static const uint16_t APPEARANCE_IMMOVABLE = 0x0100; // bit 8 set -> creature immovable

// the game itself passes this value as well
static const float COMBAT_BEHAVIOR_ARG = 20.0f;

// bytes size each behaviour object takes
static const uint32_t SEQUENTIAL_BEHAVIOR_SIZE = 0x0C;
static const uint32_t COMBAT_BEHAVIOR_SIZE     = 0x14;
static const uint32_t LOOKAT_BEHAVIOR_SIZE     = 0x04;
static const uint32_t WALK_BEHAVIOR_SIZE       = 0x08;

typedef float (__attribute__((thiscall)) *GetMaxHpFn)(Creature* creature);
typedef void* (__cdecl *OperatorNewFn)(uint32_t size);
typedef void* (__attribute__((thiscall)) *CtorFn)(void* self);
typedef void* (__attribute__((thiscall)) *CtorFloatFn)(void* self, float arg);
typedef void  (__attribute__((thiscall)) *ListPushBackFn)(void* list, void** value);

static uint32_t       base;
static GetMaxHpFn     GetMaxHP;
static OperatorNewFn  GameNew;
static CtorFn         SequentialBehaviorCtor;
static CtorFloatFn    CombatBehaviorCtor;
static CtorFn         LookAtPlayerBehaviorCtor;
static CtorFn         RandomWalkBehaviorCtor;
static ListPushBackFn ListPushBack;

// ---- infrastructure ---------------------------------------------------------
// rebuild the behavior set the spawn function refuses to give a harvestable
static void* BuildBehaviors()
{
    // reserve game managed block for behaviour container
    void* seq = GameNew(SEQUENTIAL_BEHAVIOR_SIZE);
    if (!seq) {
        return 0;
    }
    // construct the behaviour container
    SequentialBehaviorCtor(seq);

    void* list = (uint8_t*)seq + 4; // the std::list<Behavior*> lives at +4

    void* b;
    if ((b = GameNew(COMBAT_BEHAVIOR_SIZE)) != 0) {
        CombatBehaviorCtor(b, COMBAT_BEHAVIOR_ARG);
        ListPushBack(list, &b); // append to behaviour
    }
    if ((b = GameNew(LOOKAT_BEHAVIOR_SIZE)) != 0) {
        LookAtPlayerBehaviorCtor(b);
        ListPushBack(list, &b); // append to behaviour
    }
    if ((b = GameNew(WALK_BEHAVIOR_SIZE)) != 0) {
        RandomWalkBehaviorCtor(b);
        ListPushBack(list, &b); // append to behaviour
    }

    return seq;
}

// xorshift32, George Marsaglia
class XorShift32
{
public:
    void Seed()
    {
        // mix in the game tick count to provide a different start
        state ^= GetTickCount();
        // state being 0 is unrecoverable, bitshifts cannot change this value
        // ever again, resulting in always the same next float value
        if (!state) {
            state = 1;
        }
    }

    // the next value, scaled to [0, 1)
    float NextFloat()
    {
        state ^= state << 13;
        state ^= state >> 17;
        state ^= state << 5;
        return (float)(state >> 8) / (float)0x01000000;
    }

private:
    uint32_t state = 0x1BADB002; // any non-zero constant
};

static XorShift32 rng;

static bool IsWakeable(uint32_t type)
{
    for (size_t i = 0; i < sizeof(WAKEABLE_TYPES) / sizeof(WAKEABLE_TYPES[0]); i++) {
        if (WAKEABLE_TYPES[i] == type) {
            return true;
        }
    }
    return false;
}

// ---- hook body --------------------------------------------------------------
// length of instructions we replace
static const int HOOK_LEN = 8;

extern "C" void OnCreatureHit(Creature* victim)
{
    if (!victim) {
        return;
    }
    if (!IsWakeable(victim->entity_type)) {
        return;
    }

    if (victim->hostile_type == HOSTILE_TYPE_HOSTILE) {
        // already woken up
        return;
    }

    if (FIRST_HIT_ONLY && victim->hp < GetMaxHP(victim)) {
        // if only on first hit enabled and not at full HP any more
        return;
    }

    if (rng.NextFloat() >= WAKE_CHANCE) {
        // wake chance not reached
        return;
    }

    // set type to hostile
    victim->hostile_type = HOSTILE_TYPE_HOSTILE;
    // disable immovable flag by bitwise AND with inverse of immovable flag
    victim->appearance_flags &= (uint16_t)~APPEARANCE_IMMOVABLE;

    if (WAKE_LEVEL > 0) {
        // write wake level and hp, re-derive hp via game logic
        victim->level = WAKE_LEVEL;
        victim->hp = GetMaxHP(victim);
    }

    // apply health multiplier
    victim->hp *= WAKE_HP_MULTIPLIER;

    // patch behaviour, because they spawn without any behaviour
    if (!victim->behavior) {
        void* seq = BuildBehaviors();
        if (seq) {
            victim->behavior = seq;
        }
    }
}

// ---- hook stub --------------------------------------------------------------
extern "C" {
    uint32_t jmp_back = 0;
}

extern "C" __attribute__((naked)) void HookStub()
{
    asm("pushfd");                              // flags
    asm("pushad");                              // esp -= 32
    asm("sub esp, 0x80");
    asm("movups xmmword ptr [esp+0x00], xmm0"); // all of xmm0-7 must
    asm("movups xmmword ptr [esp+0x10], xmm1"); // survive the call
    asm("movups xmmword ptr [esp+0x20], xmm2");
    asm("movups xmmword ptr [esp+0x30], xmm3");
    asm("movups xmmword ptr [esp+0x40], xmm4");
    asm("movups xmmword ptr [esp+0x50], xmm5");
    asm("movups xmmword ptr [esp+0x60], xmm6");
    asm("movups xmmword ptr [esp+0x70], xmm7");

    asm("push esi");                            // esi = victim Creature*
    asm("call _OnCreatureHit");
    asm("add esp, 4");                          // cdecl, caller cleans

    asm("movups xmm0, xmmword ptr [esp+0x00]");
    asm("movups xmm1, xmmword ptr [esp+0x10]");
    asm("movups xmm2, xmmword ptr [esp+0x20]");
    asm("movups xmm3, xmmword ptr [esp+0x30]");
    asm("movups xmm4, xmmword ptr [esp+0x40]");
    asm("movups xmm5, xmmword ptr [esp+0x50]");
    asm("movups xmm6, xmmword ptr [esp+0x60]");
    asm("movups xmm7, xmmword ptr [esp+0x70]");
    asm("add esp, 0x80");
    asm("popad");
    asm("popfd");

    asm("movss xmm0, dword ptr [esi + 0x16c]"); // original code
    asm("jmp dword ptr [_jmp_back]");           // -> subss xmm0, [ebx+0x10]
}

// ---- patching ---------------------------------------------------------------
static const BYTE EXPECTED_BYTES[HOOK_LEN] = {
    0xF3, 0x0F, 0x10, 0x86, 0x6C, 0x01, 0x00, 0x00 // movss xmm0, [esi+0x16c]
};

static bool WriteHook(BYTE* location, void* target)
{
    DWORD oldProtection;
    if (!VirtualProtect(location, HOOK_LEN, PAGE_EXECUTE_READWRITE, &oldProtection)) {
        return false;
    }
    location[0] = 0xE9; // jmp rel32
    *(uint32_t*)(location + 1) = (uint32_t)target - (uint32_t)location - 5;
    // no-op the rest of the movss
    for (int i = 5; i < HOOK_LEN; i++) {
        location[i] = 0x90;
    }
    VirtualProtect(location, HOOK_LEN, oldProtection, &oldProtection);
    FlushInstructionCache(GetCurrentProcess(), location, HOOK_LEN);
    return true;
}

static void PatchSite(uint32_t rva, void* stub)
{
    BYTE* site = (BYTE*)(base + rva);
    // Refuses to patch anything that is not the expected build.
    if (memcmp(site, EXPECTED_BYTES, HOOK_LEN) != 0) {
        return;
    }

    WriteHook(site, stub);
}

extern "C" __declspec(dllexport) BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason != DLL_PROCESS_ATTACH) {
        return TRUE;
    }

    base = (uint32_t)GetModuleHandle(NULL);
    GetMaxHP                 = (GetMaxHpFn)    (base + RVA_GET_MAX_HP);
    GameNew                  = (OperatorNewFn) (base + RVA_OPERATOR_NEW);
    SequentialBehaviorCtor   = (CtorFn)        (base + RVA_SEQ_BEHAVIOR);
    CombatBehaviorCtor       = (CtorFloatFn)   (base + RVA_COMBAT_BEHAVIOR);
    LookAtPlayerBehaviorCtor = (CtorFn)        (base + RVA_LOOKAT_BEHAVIOR);
    RandomWalkBehaviorCtor   = (CtorFn)        (base + RVA_WALK_BEHAVIOR);
    ListPushBack             = (ListPushBackFn)(base + RVA_LIST_PUSH_BACK);

    rng.Seed();

    jmp_back = base + RVA_HIT + HOOK_LEN;

    PatchSite(RVA_HIT, (void*)&HookStub);
    return TRUE;
}
