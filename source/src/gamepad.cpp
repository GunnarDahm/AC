// gamepad.cpp: game controller support (USB/Bluetooth) via the SDL2 GameController API
//
// Controller buttons show up as bindable keys named PAD_* (see config/keymap.cfg, the default
// binds are in config/resetbinds.cfg). The left stick moves, the right stick looks around.
// While a menu or the console is open, the D-pad and left stick navigate, A selects and B goes back.

#include "cube.h"

VARP(gamepad, 0, 1, 1);
VARP(paddeadzone, 0, 20, 90);          // percent of stick travel ignored around the center
VARP(padtriggerthreshold, 1, 30, 100); // percent of trigger travel needed to count as pressed
VARP(padyawspeed, 1, 200, 1000);       // degrees per second at full right-stick deflection
VARP(padpitchspeed, 1, 140, 1000);
FVARP(padlookcurve, 1, 2, 4);          // response curve exponent, higher = finer aim near the center
VARP(padinvert, 0, 0, 1);
VARP(padmenubuttons, 0, 1, 1);         // buttons drive menus and the console while those are open
VARP(padrepeatdelay, 50, 400, 2000);   // menu navigation auto-repeat
VARP(padrepeatrate, 20, 90, 1000);

enum
{
    PADKEY_BUTTONS = -100,                  // PAD_A = -100 ... one code per SDL_GameControllerButton
    PADKEY_LT = PADKEY_BUTTONS - 30,
    PADKEY_RT = PADKEY_BUTTONS - 31
};

struct gamepadstate
{
    SDL_GameController *controller;
    SDL_JoystickID id;
    bool lt, rt;
    int sentcode[SDL_CONTROLLER_BUTTON_MAX]; // key code sent for each held button, so its release matches
};

static vector<gamepadstate> pads;
static bool padmoving = false;
static int navkey = 0, navnext = 0;       // menu navigation key currently held, and when it repeats

static gamepadstate *findpad(SDL_JoystickID id)
{
    loopv(pads) if(pads[i].id == id) return &pads[i];
    return NULL;
}

static void padkey(int code, bool down)
{
    keypress(code, 0, down, KMOD_NONE);
}

static void openpad(int index)
{
    if(!SDL_IsGameController(index))
    {
        conoutf("\f3joystick \"%s\" has no controller mapping, add one to config/gamecontrollerdb.txt", SDL_JoystickNameForIndex(index));
        return;
    }
    SDL_GameController *c = SDL_GameControllerOpen(index);
    if(!c) { conoutf("\f3could not open controller: %s", SDL_GetError()); return; }
    SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(c));
    if(findpad(id)) { SDL_GameControllerClose(c); return; }
    gamepadstate &p = pads.add();
    p.controller = c;
    p.id = id;
    p.lt = p.rt = false;
    loopi(SDL_CONTROLLER_BUTTON_MAX) p.sentcode[i] = 0;
    conoutf("controller connected: %s", SDL_GameControllerName(c));
}

static void releasepad(gamepadstate &p)
{
    // release anything still held so binds don't get stuck
    loopj(SDL_CONTROLLER_BUTTON_MAX) if(p.sentcode[j]) { int code = p.sentcode[j]; p.sentcode[j] = 0; padkey(code, false); }
    if(p.lt) { p.lt = false; padkey(PADKEY_LT, false); }
    if(p.rt) { p.rt = false; padkey(PADKEY_RT, false); }
}

static void closepad(SDL_JoystickID id)
{
    loopv(pads) if(pads[i].id == id)
    {
        gamepadstate &p = pads[i];
        releasepad(p);
        conoutf("controller disconnected: %s", SDL_GameControllerName(p.controller));
        SDL_GameControllerClose(p.controller);
        pads.remove(i);
        break;
    }
}

void initgamepads()
{
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
    {
        conoutf("\f3could not initialize game controller support: %s", SDL_GetError());
        return;
    }
    const char *db = findfile("config/gamecontrollerdb.txt", "rb");
    if(fileexists(db, "r")) SDL_GameControllerAddMappingsFromFile(db);
    SDL_GameControllerEventState(SDL_ENABLE);
    // devices already plugged in are also reported through SDL_CONTROLLERDEVICEADDED
}

void cleanupgamepads()
{
    loopv(pads) SDL_GameControllerClose(pads[i].controller);
    pads.setsize(0);
}

// menus and the console are driven with the D-pad/left stick and A/B instead of the game binds
static bool uiactive()
{
    return padmenubuttons && (menuallowsinput() || getcurcommand(NULL));
}

static void padbutton(gamepadstate &p, int button, bool down)
{
    if(button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) return;
    if(down)
    {
        if(p.sentcode[button]) return;
        int code = PADKEY_BUTTONS - button;
        if(uiactive()) switch(button)
        {
            case SDL_CONTROLLER_BUTTON_A: code = SDLK_RETURN; break;
            case SDL_CONTROLLER_BUTTON_B:
            case SDL_CONTROLLER_BUTTON_START: code = SDLK_ESCAPE; break;
            case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: code = SDLK_PAGEUP; break;
            case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: code = SDLK_PAGEDOWN; break;
            case SDL_CONTROLLER_BUTTON_DPAD_UP:
            case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
            case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
            case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return; // polled with auto-repeat in updategamepads()
            default: if(getcurcommand(NULL)) return; // other buttons would type into the console
        }
        p.sentcode[button] = code;
        padkey(code, true);
    }
    else
    {
        int code = p.sentcode[button];
        if(!code) return;
        p.sentcode[button] = 0;
        padkey(code, false);
    }
}

static void padtrigger(gamepadstate &p, int axis, int value)
{
    bool down = value >= padtriggerthreshold*32767/100;
    bool &state = axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? p.lt : p.rt;
    if(down == state) return;
    if(down && uiactive()) return;
    state = down;
    padkey(axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT ? PADKEY_LT : PADKEY_RT, down);
}

bool gamepadevent(const SDL_Event &e)
{
    switch(e.type)
    {
        case SDL_CONTROLLERDEVICEADDED:
            openpad(e.cdevice.which);
            return true;
        case SDL_CONTROLLERDEVICEREMOVED:
            closepad(e.cdevice.which);
            return true;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        {
            gamepadstate *p = findpad(e.cbutton.which);
            if(p && gamepad) padbutton(*p, e.cbutton.button, e.cbutton.state == SDL_PRESSED);
            return true;
        }
        case SDL_CONTROLLERAXISMOTION:
        {
            gamepadstate *p = findpad(e.caxis.which);
            if(p && gamepad && (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
                padtrigger(*p, e.caxis.axis, e.caxis.value);
            return true;
        }
    }
    return false;
}

// returns the stick position in [-1, 1] with the dead zone removed
static void readstick(SDL_GameController *c, SDL_GameControllerAxis ax, SDL_GameControllerAxis ay, float &x, float &y)
{
    x = SDL_GameControllerGetAxis(c, ax)/32767.0f;
    y = SDL_GameControllerGetAxis(c, ay)/32767.0f;
    float mag = sqrtf(x*x + y*y), dead = paddeadzone/100.0f;
    if(mag <= dead) { x = y = 0; return; }
    float scale = min((mag - dead)/(1 - dead), 1.0f)/mag;
    x *= scale;
    y *= scale;
}

static void stopmoving()
{
    if(!padmoving) return;
    player1->move = player1->k_up ? 1 : (player1->k_down ? -1 : 0);
    player1->strafe = player1->k_left ? 1 : (player1->k_right ? -1 : 0);
    padmoving = false;
}

// D-pad or left stick in a menu: arrow keys with auto-repeat
static void menunavigate(float lx, float ly)
{
    int key = 0;
    loopv(pads)
    {
        SDL_GameController *c = pads[i].controller;
        if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP)) key = SDLK_UP;
        else if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN)) key = SDLK_DOWN;
        else if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT)) key = SDLK_LEFT;
        else if(SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) key = SDLK_RIGHT;
        if(key) break;
    }
    const float navthreshold = 0.5f;
    if(!key && max(fabs(lx), fabs(ly)) > navthreshold)
    {
        if(fabs(ly) >= fabs(lx)) key = ly < 0 ? SDLK_UP : SDLK_DOWN;
        else key = lx < 0 ? SDLK_LEFT : SDLK_RIGHT;
    }
    if(!key) { navkey = 0; return; }
    if(key == navkey && totalmillis - navnext < 0) return;
    navnext = totalmillis + (key == navkey ? padrepeatrate : padrepeatdelay);
    navkey = key;
    padkey(key, true);
    padkey(key, false);
}

void updategamepads()
{
    if(!gamepad || pads.empty())
    {
        stopmoving();
        navkey = 0;
        return;
    }
    float lx = 0, ly = 0, rx = 0, ry = 0;
    loopv(pads)
    {
        float x, y;
        readstick(pads[i].controller, SDL_CONTROLLER_AXIS_LEFTX, SDL_CONTROLLER_AXIS_LEFTY, x, y);
        if(fabs(x) > fabs(lx)) lx = x;
        if(fabs(y) > fabs(ly)) ly = y;
        readstick(pads[i].controller, SDL_CONTROLLER_AXIS_RIGHTX, SDL_CONTROLLER_AXIS_RIGHTY, x, y);
        if(fabs(x) > fabs(rx)) rx = x;
        if(fabs(y) > fabs(ry)) ry = y;
    }

    if(uiactive())
    {
        stopmoving();
        menunavigate(lx, ly);
        return;
    }
    navkey = 0;

    // left stick: movement (the engine only knows full speed, so it's digital past half deflection)
    const float movethreshold = 0.5f;
    int move = ly < -movethreshold ? 1 : (ly > movethreshold ? -1 : 0),
        strafe = lx < -movethreshold ? 1 : (lx > movethreshold ? -1 : 0);
    if(move || strafe)
    {
        if(move != player1->move || strafe != player1->strafe) player1->lastmove = lastmillis;
        player1->move = move;
        player1->strafe = strafe;
        padmoving = true;
    }
    else stopmoving();

    // right stick: looking around
    if((rx || ry) && curtime > 0)
    {
        float secs = curtime/1000.0f,
              cx = (rx < 0 ? -1 : 1)*powf(fabs(rx), padlookcurve),
              cy = (ry < 0 ? -1 : 1)*powf(fabs(ry), padlookcurve);
        lookdelta(cx*padyawspeed*secs, -cy*padpitchspeed*secs*(padinvert ? -1 : 1));
    }
}

COMMANDF(numgamepads, "", () { intret(pads.length()); });
COMMANDF(gamepadname, "i", (int *i) { result(pads.inrange(*i) ? SDL_GameControllerName(pads[*i].controller) : ""); });
