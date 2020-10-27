// vid_sdl.h -- sdl video driver 

#include "SDL.h"
#include "quakedef.h"
#include "d_local.h"
// for rendering errors
#include "font8x8_basic.h"

viddef_t    vid;                // global video state
unsigned short  d_8to16table[256];

unsigned char og_pal[768];

// The original defaults
#define    BASEWIDTH    320
#define    BASEHEIGHT   240 // 200
// Much better for high resolution displays
//#define    BASEWIDTH    (320*2)
//#define    BASEHEIGHT   (200*2)

int    VGA_width, VGA_height, VGA_rowbytes, VGA_bufferrowbytes = 0;
byte    *VGA_pagebase;

static SDL_Surface *screen = NULL;

static qboolean mouse_avail;
static float   mouse_x, mouse_y;
static int mouse_oldbuttonstate = 0;

static SDL_Joystick *analog_stick = NULL;

static int joy1_x, joy1_y, joy2_x, joy2_y;
float weapwheel_x, weapwheel_y;

// No support for option menus
void (*vid_menudrawfn)(void) = NULL;
void (*vid_menukeyfn)(int key) = NULL;

void VID_SetPalette (unsigned char *palette)
{
    int i;
    SDL_Color colors[256];

    for ( i=0; i<256; ++i )
    {
        colors[i].r = *palette++;
        colors[i].g = *palette++;
        colors[i].b = *palette++;
    }
    SDL_SetColors(screen, colors, 0, 256);
}

void VID_ShiftPalette (unsigned char *palette)
{
    VID_SetPalette(palette);
}

void RG_WriteChar (char ch, int cx, int cy)
{
    int x, y, set;
    char* dest = screen->pixels;
    for (y = cy; y < cy+8; y++)
    {
        for (x = cx; x < cx+8; x++)
        {
            set = font8x8_basic[ch][y-cy] & 1 << (x-cx);
            dest[y*screen->w + x] = set != 0;
        }
    }
}

// x = 0, y = 0 writes to upper left corner, x = 1, y = 2 writes 2 lines down, 1 column right
void RG_WriteString (char* string, int x, int y)
{
    int i;
    int cx = x*8;
    int cy = y*8;
    char ch;
    for (i = 0; i < 1200; i++)
    {
        ch = string[i];
        if (ch == 0) break;
        if (ch == '\n')
        {
            cx = 0;
            cy += 8;
            continue;
        }

        RG_WriteChar(ch, cx, cy);

        cx += 8;
        if (cx >= screen->w)
        {
            cx = 0;
            cy += 8;
        }
        if (cy >= screen->h) return;
    }
}

// Move to better place? just init a display to show errors.
void RG_ErrorScreen (char *error, ...)
{ 
    va_list     argptr;
    char        string[1200];
    SDL_Color colors[256];

    va_start (argptr,error);
    vsprintf (string,error,argptr);
    va_end (argptr);

    printf("ERROR: %s\n", string);

    if (!SDL_WasInit(SDL_INIT_VIDEO))
    {
        SDL_Init (SDL_INIT_VIDEO);
        screen = SDL_SetVideoMode(BASEWIDTH, BASEHEIGHT, 8, SDL_SWSURFACE|SDL_HWPALETTE|SDL_FULLSCREEN);
        SDL_ShowCursor(0);
    }

    int x, y;
    char* dest = screen->pixels;
    for (y = 0; y < screen->h; y++)
    {
        for (x = 0; x < screen->w; x++)
        {
            dest[y*screen->w + x] = 0;
        }
    }
    RG_WriteString("ERROR:", 0, 0);
    RG_WriteString(string, 0, 2);
    RG_WriteString("Quitting automatically...", 0, 28);

    colors[0].r = 0;
    colors[0].g = 0;
    colors[0].b = 0;
    colors[1].r = 200;
    colors[1].g = 200;
    colors[1].b = 200;
    SDL_SetColors(screen, colors, 0, 256);
    SDL_UpdateRect(screen, 0, 0, screen->w, screen->h);

    SDL_Delay(6000);

    Host_Shutdown();
    // fprintf(stderr, "Error: %s\n", string);
    // Sys_AtExit is called by exit to shutdown the system
    exit(0);
}

void rgb2hsl (float r, float g, float b, float* h, float* s, float* l)
{
    float cmax, cmin, delta;

    cmax = fmax(r, fmax(g, b));
    cmin = fmin(r, fmin(g, b));
    delta = cmax - cmin;
    *l = (cmax + cmin) / 2;
    *s = delta / (1 - fabs(2*(*l) - 1));
    if (delta == 0)
    {
        *s = 0;
        *h = 0;
    }
    else if (cmax == r)
        *h = (g - b)/delta + (g < b ? 6 : 0);
    else if (cmax == g)
        *h = (b - r)/delta + 2;
    else
        *h = (r - g)/delta + 4;

    *h *= 60.0;
}

void hsl2rgb (float h, float s, float l, float* r, float* g, float* b)
{
    if (h > 360)
        h -= 360;
    if (h < 0)
        h += 360;

    float c, x, m;
    c = (1 - fabs(2*l - 1)) * s;
    x = c * (1.0 - fabs(fmod(h / 60.0, 2) - 1.0));
    m = l - c*0.5;

    if (0 <= h && h < 60)
    {
        *r = c;
        *g = x;
        *b = 0;
    }
    else if (60 <= h && h < 120)
    {
        *r = x;
        *g = c;
        *b = 0;
    }
    else if (120 <= h && h < 180)
    {
        *r = 0;
        *g = c;
        *b = x;
    }
    else if (180 <= h && h < 240)
    {
        *r = 0;
        *g = x;
        *b = c;
    }
    else if (240 <= h && h < 300)
    {
        *r = x;
        *g = 0;
        *b = c;
    }
    else
    {
        *r = c;
        *g = 0;
        *b = x;
    }

    *r = fmin(*r+m, 1.0);
    *g = fmin(*g+m, 1.0);
    *b = fmin(*b+m, 1.0);
}

void VID_PaletteColormath (unsigned char *palette, float hshift, float sf, float lf)
{
    int i;
    float r, g, b, h, s, l;
    for (i = 0; i < 256; i++)
    {
        r = og_pal[i*3] / 255.0;
        g = og_pal[i*3+1] / 255.0;
        b = og_pal[i*3+2] / 255.0;

        rgb2hsl(r, g, b, &h, &s, &l);

        h += hshift;
        if (h < 0) h += 360;
        if (h > 360) h -= 360;
        s = fmax(0.0, fmin(1.0, s * sf));
        l = fmax(0.0, fmin(1.0, l * lf));

        hsl2rgb(h, s, l, &r, &g, &b);
        palette[i*3] = r * 255;
        palette[i*3+1] = g * 255;
        palette[i*3+2] = b * 255;
    }

    VID_SetPalette (palette);
}

void VID_Init (unsigned char *palette)
{
    int pnum, chunk;
    byte *cache;
    int cachesize;
    Uint8 video_bpp;
    Uint16 video_w, video_h;
    Uint32 flags;

    // Load the SDL library
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
        Sys_Error("VID: Couldn't load SDL: %s", SDL_GetError());

    // Set up display mode (width and height)
    vid.width = BASEWIDTH;
    vid.height = BASEHEIGHT;
    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;

    // check for command-line window size

    /* Ignore these params, we're working with fixed size display, dont ignore because testing on desktop */
    if ((pnum=COM_CheckParm("-winsize")))
    {
        if (pnum >= com_argc-2)
            Sys_Error("VID: -winsize <width> <height>\n");
        vid.width = Q_atoi(com_argv[pnum+1]);
        vid.height = Q_atoi(com_argv[pnum+2]);
        if (!vid.width || !vid.height)
            Sys_Error("VID: Bad window width/height\n");
    }
    if ((pnum=COM_CheckParm("-width"))) {
        if (pnum >= com_argc-1)
            Sys_Error("VID: -width <width>\n");
        vid.width = Q_atoi(com_argv[pnum+1]);
        if (!vid.width)
            Sys_Error("VID: Bad window width\n");
    }
    if ((pnum=COM_CheckParm("-height"))) {
        if (pnum >= com_argc-1)
            Sys_Error("VID: -height <height>\n");
        vid.height = Q_atoi(com_argv[pnum+1]);
        if (!vid.height)
            Sys_Error("VID: Bad window height\n");
    }

    // Set video width, height and flags
    flags = (SDL_SWSURFACE|SDL_HWPALETTE|SDL_FULLSCREEN);

    if ( COM_CheckParm ("-fullscreen") )
        flags |= SDL_FULLSCREEN;

    if ( COM_CheckParm ("-window") ) {
        flags &= ~SDL_FULLSCREEN;
    }

    // vid.width = 320;
    // vid.height = 240;

    // Initialize display 
    screen = SDL_SetVideoMode(vid.width, vid.height, 8, flags);

    if (screen == NULL) // ((screen = SDL_SetVideoMode(vid.width, vid.height, 8, flags)) == NULL)
    {
        Sys_Error("VID: Couldn't set video mode: %s\n", SDL_GetError());
    }

    memcpy(og_pal, palette, 256*3);

    VID_PaletteColormath(palette, 0, 1, 1);

    VID_SetPalette(palette);

    // SDL_WM_SetCaption("sdlquake","sdlquake");
    // now know everything we need to know about the buffer
    VGA_width = vid.conwidth = vid.width;
    VGA_height = vid.conheight = vid.height;
    vid.aspect = ((float)vid.height / (float)vid.width) * (320.0 / 240.0);
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
    VGA_pagebase = vid.buffer = screen->pixels;
    VGA_rowbytes = vid.rowbytes = screen->pitch;
    vid.conbuffer = vid.buffer;
    vid.conrowbytes = vid.rowbytes;
    vid.direct = 0;
    
    // allocate z buffer and surface cache
    chunk = vid.width * vid.height * sizeof (*d_pzbuffer);
    cachesize = D_SurfaceCacheForRes (vid.width, vid.height);
    chunk += cachesize;
    d_pzbuffer = Hunk_HighAllocName(chunk, "video");
    if (d_pzbuffer == NULL)
        Sys_Error ("Not enough memory for video mode\n");

    // initialize the cache memory 
        cache = (byte *) d_pzbuffer
                + vid.width * vid.height * sizeof (*d_pzbuffer);
    D_InitCaches (cache, cachesize);

    // initialize the mouse
    SDL_ShowCursor(0);
}

void    VID_Shutdown (void)
{
    SDL_Quit();
}

void    VID_Update (vrect_t *rects)
{
    SDL_Rect *sdlrects;
    int n, i;
    vrect_t *rect;

    // Two-pass system, since Quake doesn't do it the SDL way...

    // First, count the number of rectangles
    n = 0;
    for (rect = rects; rect; rect = rect->pnext)
        ++n;

    // Second, copy them to SDL rectangles and update
    if (!(sdlrects = (SDL_Rect *)alloca(n*sizeof(*sdlrects))))
        Sys_Error("Out of memory");
    i = 0;
    for (rect = rects; rect; rect = rect->pnext)
    {
        sdlrects[i].x = rect->x;
        sdlrects[i].y = rect->y;
        sdlrects[i].w = rect->width;
        sdlrects[i].h = rect->height;
        ++i;
    }
    SDL_UpdateRects(screen, n, sdlrects);
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
    Uint8 *offset;


    if (!screen) return;
    if ( x < 0 ) x = screen->w+x-1;
    offset = (Uint8 *)screen->pixels + y*screen->pitch + x;
    while ( height-- )
    {
        memcpy(offset, pbitmap, width);
        offset += screen->pitch;
        pbitmap += width;
    }
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
    if (!screen) return;
    if (x < 0) x = screen->w+x-1;
    SDL_UpdateRect(screen, x, y, width, height);
}


/*
================
Sys_SendKeyEvents
================
*/

void Sys_SendKeyEvents(void)
{
    SDL_Event event;
    int sym, state;
     int modstate;

    while (SDL_PollEvent(&event))
    {
        switch (event.type) {

            case SDL_KEYDOWN:
            case SDL_KEYUP:
                sym = event.key.keysym.sym;
                state = event.key.state;
                modstate = SDL_GetModState();
                switch(sym)
                {
                   case SDLK_DELETE: sym = K_DEL; break;
                   case SDLK_BACKSPACE: sym = K_BACKSPACE; break;
                   case SDLK_F1: sym = K_F1; break;
                   case SDLK_F2: sym = K_F2; break;
                   case SDLK_F3: sym = K_F3; break;
                   case SDLK_F4: sym = K_F4; break;
                   case SDLK_F5: sym = K_F5; break;
                   case SDLK_F6: sym = K_F6; break;
                   case SDLK_F7: sym = K_F7; break;
                   case SDLK_F8: sym = K_F8; break;
                   case SDLK_F9: sym = K_F9; break;
                   case SDLK_F10: sym = K_F10; break;
                   case SDLK_F11: sym = K_F11; break;
                   case SDLK_F12: sym = K_F12; break;
                   case SDLK_BREAK:
                   case SDLK_PAUSE: sym = K_PAUSE; break;
                   case SDLK_UP: sym = K_UPARROW; break;
                   case SDLK_DOWN: sym = K_DOWNARROW; break;
                   case SDLK_RIGHT: sym = K_RIGHTARROW; break;
                   case SDLK_LEFT: sym = K_LEFTARROW; break;
                   case SDLK_INSERT: sym = K_INS; break;
                   case SDLK_HOME: sym = K_HOME; break;
                   case SDLK_END: sym = K_END; break;
                   case SDLK_PAGEUP: sym = K_PGUP; break;
                   case SDLK_PAGEDOWN: sym = K_PGDN; break;
                   case SDLK_RSHIFT:
                   case SDLK_LSHIFT: sym = K_SHIFT; break;
                   case SDLK_RCTRL:
                   case SDLK_LCTRL: sym = K_CTRL; break;
                   case SDLK_RALT:
                   case SDLK_LALT: sym = K_ALT; break;
                   case SDLK_KP0: 
                       if(modstate & KMOD_NUM) sym = K_INS; 
                       else sym = SDLK_0;
                       break;
                   case SDLK_KP1:
                       if(modstate & KMOD_NUM) sym = K_END;
                       else sym = SDLK_1;
                       break;
                   case SDLK_KP2:
                       if(modstate & KMOD_NUM) sym = K_DOWNARROW;
                       else sym = SDLK_2;
                       break;
                   case SDLK_KP3:
                       if(modstate & KMOD_NUM) sym = K_PGDN;
                       else sym = SDLK_3;
                       break;
                   case SDLK_KP4:
                       if(modstate & KMOD_NUM) sym = K_LEFTARROW;
                       else sym = SDLK_4;
                       break;
                   case SDLK_KP5: sym = SDLK_5; break;
                   case SDLK_KP6:
                       if(modstate & KMOD_NUM) sym = K_RIGHTARROW;
                       else sym = SDLK_6;
                       break;
                   case SDLK_KP7:
                       if(modstate & KMOD_NUM) sym = K_HOME;
                       else sym = SDLK_7;
                       break;
                   case SDLK_KP8:
                       if(modstate & KMOD_NUM) sym = K_UPARROW;
                       else sym = SDLK_8;
                       break;
                   case SDLK_KP9:
                       if(modstate & KMOD_NUM) sym = K_PGUP;
                       else sym = SDLK_9;
                       break;
                   case SDLK_KP_PERIOD:
                       if(modstate & KMOD_NUM) sym = K_DEL;
                       else sym = SDLK_PERIOD;
                       break;
                   case SDLK_KP_DIVIDE: sym = SDLK_SLASH; break;
                   case SDLK_KP_MULTIPLY: sym = SDLK_ASTERISK; break;
                   case SDLK_KP_MINUS: sym = SDLK_MINUS; break;
                   case SDLK_KP_PLUS: sym = SDLK_PLUS; break;
                   case SDLK_KP_ENTER: sym = SDLK_RETURN; break;
                   case SDLK_KP_EQUALS: sym = SDLK_EQUALS; break;
                }
                // If we're not directly handled and still above 255
                // just force it to 0
                if(sym > 255) sym = 0;
                Key_Event(sym, state);
                break;

            case SDL_MOUSEMOTION:
                if ( (event.motion.x != (vid.width/2)) ||
                     (event.motion.y != (vid.height/2)) ) {
                    mouse_x = event.motion.xrel*10;
                    mouse_y = event.motion.yrel*10;
                    if ( (event.motion.x < ((vid.width/2)-(vid.width/4))) ||
                         (event.motion.x > ((vid.width/2)+(vid.width/4))) ||
                         (event.motion.y < ((vid.height/2)-(vid.height/4))) ||
                         (event.motion.y > ((vid.height/2)+(vid.height/4))) ) {
                        SDL_WarpMouse(vid.width/2, vid.height/2);
                    }
                }
                break;

            case SDL_JOYAXISMOTION:
                // Con_Printf("joy motion %i %i %i\n", event.jaxis.which, event.jaxis.axis, event.jaxis.value);
                if (event.jaxis.which == 0)
                {
                    if (event.jaxis.axis == 0)
                    {
                        joy1_x = event.jaxis.value;
                    }
                    else if (event.jaxis.axis == 1)
                    {
                        joy1_y = event.jaxis.value;
                    }
                    else if (event.jaxis.axis == 2)
                    {
                        joy2_x = event.jaxis.value;
                    }
                    else if (event.jaxis.axis == 3)
                    {
                        joy2_y = event.jaxis.value;
                    }
                }
                break;

            case SDL_QUIT:
                CL_Disconnect ();
                Host_ShutdownServer(false);        
                Sys_Quit ();
                break;
            default:
                break;
        }
    }
}

void IN_Init (void)
{
    if ( COM_CheckParm ("-nomouse") )
        return;
    mouse_x = mouse_y = 0.0;
    mouse_avail = 1;
}

void IN_Init_Post (void)
{
    SDL_JoystickEventState(SDL_ENABLE);

    int joysticksNum = SDL_NumJoysticks();
    if (joysticksNum > 0)
    {
        analog_stick = SDL_JoystickOpen(0);
        printf( "Joysticks connected: %i\n", joysticksNum);
    }
}

void IN_Shutdown (void)
{
    mouse_avail = 0;
    SDL_JoystickClose(analog_stick);
    analog_stick = NULL;
}

void IN_Commands (void)
{
    int i;
    int mouse_buttonstate;
   
    if (!mouse_avail) return;
   
    i = SDL_GetMouseState(NULL, NULL);
    /* Quake swaps the second and third buttons */
    mouse_buttonstate = (i & ~0x06) | ((i & 0x02)<<1) | ((i & 0x04)>>1);
    for (i=0 ; i<3 ; i++) {
        if ( (mouse_buttonstate & (1<<i)) && !(mouse_oldbuttonstate & (1<<i)) )
            Key_Event (K_MOUSE1 + i, true);

        if ( !(mouse_buttonstate & (1<<i)) && (mouse_oldbuttonstate & (1<<i)) )
            Key_Event (K_MOUSE1 + i, false);
    }
    mouse_oldbuttonstate = mouse_buttonstate;
}

float moveagain_time;

void IN_Move (usercmd_t *cmd)
{
    float analog_move_x, analog_move_y, analog_look_x, analog_look_y;
    if (swap_analogs.value)
    {
        analog_move_x = joy2_x / 32767.0;
        analog_move_y = joy2_y / 32767.0;

        analog_look_x = joy1_x / 32767.0;
        analog_look_y = joy1_y / 32767.0;
    }
    else
    {
        analog_move_x = joy1_x / 32767.0;
        analog_move_y = joy1_y / 32767.0;

        analog_look_x = joy2_x / 32767.0;
        analog_look_y = joy2_y / 32767.0;
    }

    if (in_weapwheel.state & 1)
    {
        if (weapwheel_use_move.value)
        {
            weapwheel_x = analog_move_x;
            weapwheel_y = analog_move_y;
        }
        else
        {
            weapwheel_x = analog_look_x;
            weapwheel_y = analog_look_y;
        }
        float len = sqrt(weapwheel_x*weapwheel_x + weapwheel_y*weapwheel_y);
        if (len > 0.2)
        {
            if (len > 1)
            {
                weapwheel_x /= len;
                weapwheel_y /= len;
            }
            float angle = 180 * atan2(weapwheel_y, weapwheel_x) / M_PI + 90;
            if (angle < 0) angle += 360;
            

            int selection = (int)((angle / 45) + 0.5) + 1;
            if (selection >= 9) selection = 1;
            weapwheel_selection = selection;
        }
        // else
        //    weapwheel_selection = 0;

        moveagain_time = cl.time + 0.18;
    }

    if (cl.time < moveagain_time)
    {
        if ((moveagain_time - cl.time) > 0.5)
            moveagain_time = 0;
        if (weapwheel_use_move.value)
            analog_move_x = analog_move_y = 0;
        else
            analog_look_x = analog_look_y = 0;
    }

    if (analog_move_x > 0.15 || analog_move_x < -0.15)
        cmd->sidemove += 350 * analog_move_x;
    if (analog_move_y > 0.15 || analog_move_y < -0.15)
        cmd->forwardmove -= 350 * analog_move_y;


    float edgeAccel = 2.0;
    float edgeThreshold = 0.9;

    float midAccel = 1.5;
    float midThreshold = 0.65;

    float deadzone = 0.05;

    float lookLen = sqrt(analog_look_x*analog_look_x + analog_look_y*analog_look_y);
    // Con_Printf("look len %f\n", lookLen);

    if (lookLen > 1)
    {
        analog_look_x /= lookLen;
        analog_look_y /= lookLen;
    }
    if (lookLen > edgeThreshold)
    {
        analog_look_x *= edgeAccel;
        analog_look_y *= edgeAccel;
    }
    else if (lookLen > midThreshold)
    {
        analog_look_x *= midAccel;
        analog_look_y *= midAccel;
    }
    else if (lookLen < deadzone)
    {
        analog_look_x = 0;
        analog_look_y = 0;
    }

    cl.viewangles[YAW] -= 5 * m_yaw.value * analog_look_x;
    cl.viewangles[PITCH] += 5 * m_pitch.value * analog_look_y;

    if (cl.viewangles[PITCH] > 70)
        cl.viewangles[PITCH] = 70;
    if (cl.viewangles[PITCH] < -70)
        cl.viewangles[PITCH] = -70;

    V_StopPitchDrift ();


    /* Mouse stuff deprecated because analog sticks
    if (!mouse_avail)
        return;
   
    mouse_x *= sensitivity.value;
    mouse_y *= sensitivity.value;

   
    if ( (in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1) ))
        cmd->sidemove += m_side.value * mouse_x;
    else
        cl.viewangles[YAW] -= m_yaw.value * mouse_x;
    if (in_mlook.state & 1)
        V_StopPitchDrift ();
   
    if ( (in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * mouse_y;
        if (cl.viewangles[PITCH] > 80)
            cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70)
            cl.viewangles[PITCH] = -70;
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * mouse_y;
        else
            cmd->forwardmove -= m_forward.value * mouse_y;
    }
    */
    mouse_x = mouse_y = 0.0;
}

/*
================
Sys_ConsoleInput
================
*/
char *Sys_ConsoleInput (void)
{
    return 0;
}
