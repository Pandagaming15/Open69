
#include <mod/amlmod.h>
#include <mod/logger.h>
#include <mod/config.h>

#include <aml-psdk/game_sa/Events.h>
#include <aml-psdk/game_sa/entity/Object.h>
#include <aml-psdk/game_sa/entity/PlayerPed.h>

#include <cmath>

#include "isautils.h"

MYMODCFG(Open69, GTASA, 1.1, Panda)
NEEDGAME(com.rockstargames.gtasa)

// SAUtils may not be ready immediately, so retry when needed.
static ISAUtils* g_saUtils = nullptr;

static constexpr int LEFT_DOOR_MODEL = 2927;
static constexpr int RIGHT_DOOR_MODEL = 2929;

// Door behavior
static constexpr float OPEN_DISTANCE = 10.0f;
static constexpr float OPEN_AMOUNT = 5.0f;

// Smaller values produce slower animation.
// Progress is updated only from matching door render callbacks.
static constexpr float ANIMATION_SPEED = 0.005f;

static CObject* g_leftDoor = nullptr;
static CObject* g_rightDoor = nullptr;

static CVector g_leftClosedPos{};
static CVector g_rightClosedPos{};

static bool g_leftReady = false;
static bool g_rightReady = false;

static float g_openProgress = 0.0f;
static bool g_shouldOpen = false;

static void EnsureSAUtils()
{
    if (!g_saUtils)
        g_saUtils = static_cast<ISAUtils*>(GetInterface("SAUtils"));
}

static float DistanceSquared(
    const CVector& a,
    const CVector& b)
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;

    return dx * dx + dy * dy + dz * dz;
}

static CVector GetDoorCenter()
{
    CVector center{};

    center.x = (g_leftClosedPos.x + g_rightClosedPos.x) * 0.5f;
    center.y = (g_leftClosedPos.y + g_rightClosedPos.y) * 0.5f;
    center.z = (g_leftClosedPos.z + g_rightClosedPos.z) * 0.5f;

    return center;
}

static void ProcessOpen69Garage()
{
    if (!g_leftReady || !g_rightReady)
        return;

    if (!g_leftDoor || !g_rightDoor)
        return;

    EnsureSAUtils();

    if (!g_saUtils)
        return;

    CPlayerPed* player = FindPlayerPed(-1);
    if (!player)
        return;

    const CVector playerPos = player->GetPosition();
    const CVector center = GetDoorCenter();

    // Open when the player approaches the garage.
    const float distanceSq = DistanceSquared(playerPos, center);

    g_shouldOpen = distanceSq <=
        OPEN_DISTANCE * OPEN_DISTANCE;

    if (g_shouldOpen)
    {
        g_openProgress = std::fmin(
            1.0f,
            g_openProgress + ANIMATION_SPEED);
    }
    else
    {
        g_openProgress = std::fmax(
            0.0f,
            g_openProgress - ANIMATION_SPEED);
    }

    // Calculate the direction between the two closed door positions.
    const float dx =
        g_rightClosedPos.x - g_leftClosedPos.x;
    const float dy =
        g_rightClosedPos.y - g_leftClosedPos.y;

    const float length = std::sqrt(dx * dx + dy * dy);

    if (length < 0.001f)
        return;

    const float dirX = dx / length;
    const float dirY = dy / length;

    const float movement = OPEN_AMOUNT * g_openProgress;

    // Left door moves away from the center.
    CVector leftTarget = g_leftClosedPos;
    leftTarget.x -= dirX * movement;
    leftTarget.y -= dirY * movement;

    // Right door moves in the opposite direction.
    CVector rightTarget = g_rightClosedPos;
    rightTarget.x += dirX * movement;
    rightTarget.y += dirY * movement;

    g_saUtils->SetPosition(
        g_leftDoor,
        leftTarget.x,
        leftTarget.y,
        leftTarget.z,
        false);

    g_saUtils->SetPosition(
        g_rightDoor,
        rightTarget.x,
        rightTarget.y,
        rightTarget.z,
        false);
}

// Runs before CObject::Render.
// Capture each door's original position only once.
static void OnObjectRender(CObject* object)
{
    if (!object)
        return;

    bool isGarageDoor = false;

    if (object->m_nModelIndex == LEFT_DOOR_MODEL)
    {
        if (object != g_leftDoor)
        {
            g_leftDoor = object;
            g_leftClosedPos = object->GetPosition();
            g_leftReady = true;
            g_openProgress = 0.0f;
        }

        isGarageDoor = true;
    }
    else if (object->m_nModelIndex == RIGHT_DOOR_MODEL)
    {
        if (object != g_rightDoor)
        {
            g_rightDoor = object;
            g_rightClosedPos = object->GetPosition();
            g_rightReady = true;
            g_openProgress = 0.0f;
        }

        isGarageDoor = true;
    }

    // Avoid advancing animation for unrelated objects.
    if (!isGarageDoor)
        return;

    ProcessOpen69Garage();
}

static void OnObjectDestroy(CObject* object)
{
    if (!object)
        return;

    if (object == g_leftDoor)
    {
        g_leftDoor = nullptr;
        g_leftReady = false;
    }

    if (object == g_rightDoor)
    {
        g_rightDoor = nullptr;
        g_rightReady = false;
    }

    if (!g_leftReady || !g_rightReady)
    {
        g_openProgress = 0.0f;
        g_shouldOpen = false;
    }
}

extern "C" void OnModLoad()
{
    EnsureSAUtils();

    // Use the same render-event approach as the working SFFIREDPT code.
    // Do not register gameProcessEvent here.
    Events::objectRenderEvent += &OnObjectRender;
    Events::objectDtorEvent += &OnObjectDestroy;

    logger->Info("Open69 loaded (AML PSDK)");
}
