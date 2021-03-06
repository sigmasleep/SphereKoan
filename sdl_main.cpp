// Todo: address this
#define _CRT_SECURE_NO_WARNINGS

#include <assert.h>
#define ASSERT(...) assert(__VA_ARGS__)

#define internal static
#define global_variable static
#define local_persist static

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "game.h"
#include "renderer.h"

#include <windows.h>
#include <stdio.h>
#include <SDL2/SDL.h>

#define Kilobytes(Value) ((Value) * 1024L)
#define Megabytes(Value) (Kilobytes(Value) * 1024L)
#define Gigabytes(Value) (Megabytes(Value) * 1024L)

typedef void* library;
struct game_functions
{
  time_t Timestamp;
  library Library;

  load_game *LoadGame;
  reload_game *ReloadGame;
  update_game *UpdateGame;
};

struct renderer_functions
{
  time_t Timestamp;
  library Library;

  load_renderer *LoadRenderer;
  reload_renderer *ReloadRenderer;
  render_game *RenderGame;

  add_line_to_renderer *AddLineToRenderer;
  add_semicircle_to_renderer *AddSemicircleToRenderer;
  add_rect_to_renderer *AddRectToRenderer;
};

global_variable bool GlobalRunning = true;

global_variable renderer_functions *GlobalRendererFunctions;

internal int
GetTerminatedStringLength(char* String)
{
  int Length = 0;
  while(*String != '\0')
  {
    Length++;
    String++;
  }

  return Length;
}

DEBUG_PRINT(DebugPrint)
{
  va_list Arguments;
  va_start(Arguments, OutputString);

  char OutputBuffer[255];
  vsnprintf(OutputBuffer, 255, OutputString, Arguments);

  OutputDebugStringA(OutputBuffer);
}

struct color
{
  float R, G, B, A;
};
global_variable color CurrentDebugColor =
{
  0.f, 0.f, 0.f, 1.f
};
// Note: Replace SDL draws with OpenGL equivalent
DEBUG_SET_COLOR(DebugSetColor)
{
  // Todo: Replace int parameters to float on game layer
  CurrentDebugColor.R = (float)R;
  CurrentDebugColor.G = (float)G;
  CurrentDebugColor.B = (float)B;
  CurrentDebugColor.A = (float)A; // Currently ignored
}

DEBUG_DRAW_LINE(DebugDrawLine)
{
  GlobalRendererFunctions->AddLineToRenderer(
    X1, Y1,
    X2, Y2,
    CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
  );
}

DEBUG_DRAW_SEMICIRCLE(DebugDrawSemicircle)
{
  float RadAngle = Angle * DEG2RAD_CONSTANT;

  vector Position1;
  vector Position2;
  Position1.X = X + cosf(RadAngle) * Radius;
  Position1.Y = Y + sinf(RadAngle) * Radius;

  for(int PointNum = 0; PointNum < Segments; PointNum++)
  {
    Position2.X = X + cosf(RadAngle + PointNum / (float)TotalSegments * PI * 2) * Radius;
    Position2.Y = Y + sinf(RadAngle + PointNum / (float)TotalSegments * PI * 2) * Radius;

    GlobalRendererFunctions->AddLineToRenderer(
      Position1.X, Position1.Y,
      Position2.X, Position2.Y,
      CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
    );

    Position1.X = Position2.X;
    Position1.Y = Position2.Y;
  }

  Position2.X = X + cosf(RadAngle) * Radius;
  Position2.Y = Y + sinf(RadAngle) * Radius;

  GlobalRendererFunctions->AddLineToRenderer(
    Position1.X, Position1.Y,
    Position2.X, Position2.Y,
    CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
  );
 }

DEBUG_FILL_SEMICIRCLE(DebugFillSemicircle)
{
  GlobalRendererFunctions->AddSemicircleToRenderer(
    X, Y,
    Radius,
    Segments, TotalSegments,
    Angle,
    CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
  );
}


DEBUG_FILL_CIRCLE(DebugFillCircle)
{
  DebugFillSemicircle(X, Y, Radius, Segments, Segments, 0);
}

DEBUG_DRAW_CIRCLE(DebugDrawCircle)
{
  DebugDrawSemicircle(X, Y, Radius, Segments, Segments, 0);
}

DEBUG_FILL_TRIANGLE(DebugFillTriangle)
{
  DebugFillSemicircle(X, Y, HalfHeight, 3, 3, Angle);
}

DEBUG_DRAW_TRIANGLE(DebugDrawTriangle)
{
  DebugDrawSemicircle(X, Y, HalfHeight, 3, 3, Angle);
}

DEBUG_DRAW_BOX(DebugDrawBox)
{
  GlobalRendererFunctions->AddLineToRenderer(
    X, Y,
    X + Width, Y,
    CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
  );
  GlobalRendererFunctions->AddLineToRenderer(
    X + Width, Y,
    X + Width, Y + Height,
    CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
  );
  GlobalRendererFunctions->AddLineToRenderer(
    X + Width, Y + Height,
    X, Y + Height,
    CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
  );
  GlobalRendererFunctions->AddLineToRenderer(
    X, Y + Height,
    X, Y,
    CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
  );
}

DEBUG_FILL_BOX(DebugFillBox)
{
  GlobalRendererFunctions->AddRectToRenderer(
    X + Width / 2, Y + Height / 2,
    Width, Height,
    0.f,
    CurrentDebugColor.R, CurrentDebugColor.G, CurrentDebugColor.B
  );
}

internal void
GenerateFilepath(
  char* Filename,
  char* OutputFilepath, int OutputFilepathBufferSize
)
{
  int FilenameLength = GetTerminatedStringLength(Filename);
  char* BaseFilepath;
  BaseFilepath = SDL_GetBasePath();
  {
    int BaseFilepathLength = GetTerminatedStringLength(BaseFilepath);
    ASSERT(BaseFilepathLength + FilenameLength < OutputFilepathBufferSize);

    sprintf_s(OutputFilepath, OutputFilepathBufferSize, "%s%s",
              BaseFilepath, Filename);
  } SDL_free(BaseFilepath);
}

internal void
LoadLibraryAs(char *LibraryFilename, char *LibraryCopyFilename,
              library *OutputLibrary,
              time_t *Timestamp)
{
  char LibraryFilepath[200];
  GenerateFilepath(LibraryFilename, LibraryFilepath, 200);

  char LibraryCopyFilepath[200];
  GenerateFilepath(LibraryCopyFilename, LibraryCopyFilepath, 200);

  // Note: Does this ensure lock?
  struct stat ThrowAway;
  while(stat("lock.tmp", &ThrowAway) == 0)
    SDL_Delay(1);

  // Todo: Find a x-platform method?
  CopyFile(LibraryFilepath, LibraryCopyFilepath, FALSE);

  *OutputLibrary = SDL_LoadObject(LibraryCopyFilepath);
  ASSERT(*OutputLibrary);
  // Note: Handle a soft-fail for release build.
  if(*OutputLibrary)
  {
    struct stat DLLInfo;
    stat(LibraryCopyFilepath, &DLLInfo);
    *Timestamp = DLLInfo.st_mtime;
  }
  else
  {
    // Todo
  }
}

internal void
LoadGameFunctions(game_functions *GameFunctions)
{
  LoadLibraryAs("game.dll", "game_runtime.dll",
                &GameFunctions->Library,
                &GameFunctions->Timestamp);

  GameFunctions->LoadGame =
    (load_game*)SDL_LoadFunction(GameFunctions->Library, "LoadGame");
  GameFunctions->ReloadGame =
    (reload_game*)SDL_LoadFunction(GameFunctions->Library, "ReloadGame");
  GameFunctions->UpdateGame =
    (update_game*)SDL_LoadFunction(GameFunctions->Library, "UpdateGame");
}

internal void
UnloadGameFunctions(game_functions *GameFunctions)
{
  SDL_UnloadObject(GameFunctions->Library);
  GameFunctions->Library = NULL;
  GameFunctions->LoadGame = NULL;
  GameFunctions->UpdateGame = NULL;
}

internal void
LoadRendererFunctions(renderer_functions *RendererFunctions)
{
  LoadLibraryAs("renderer.dll", "renderer_runtime.dll",
                &RendererFunctions->Library,
                &RendererFunctions->Timestamp);

  RendererFunctions->LoadRenderer =
    (load_renderer*)SDL_LoadFunction(
      RendererFunctions->Library, "LoadRenderer");
  RendererFunctions->ReloadRenderer =
    (reload_renderer*)SDL_LoadFunction(
      RendererFunctions->Library, "ReloadRenderer");
  RendererFunctions->RenderGame =
    (render_game*)SDL_LoadFunction(
      RendererFunctions->Library, "RenderGame");

  RendererFunctions->AddLineToRenderer =
    (add_line_to_renderer*)SDL_LoadFunction(
      RendererFunctions->Library, "AddLineToRenderer"); 
  RendererFunctions->AddSemicircleToRenderer =
    (add_semicircle_to_renderer*)SDL_LoadFunction(
      RendererFunctions->Library, "AddSemicircleToRenderer"); 
  RendererFunctions->AddRectToRenderer =
    (add_rect_to_renderer*)SDL_LoadFunction(
      RendererFunctions->Library, "AddRectToRenderer"); 
}

internal void
UnloadRendererFunctions(renderer_functions *RendererFunctions)
{
  SDL_UnloadObject(RendererFunctions->Library);
  RendererFunctions->Library = NULL;
  RendererFunctions->LoadRenderer = NULL;
  RendererFunctions->RenderGame = NULL;
}

internal void
UpdateButton(button_state *Button, bool IsDown)
{
  Button->IsDown = IsDown;
  if(!IsDown)
  {
    Button->WasReleasedSinceLastAction = true;
  }
}

internal void
AllocateMemory(memory *Memory, int size)
{
  Memory->AllocatedSpace =
    VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if(Memory->AllocatedSpace)
    Memory->Size = 0;
  else
    Memory->Size = -1;
}

// Note: This is copy pasted from SDL source in order to use the inner juices.s
struct _SDL_GameController
{
  SDL_Joystick *joystick; // underlying joystick device
 // int ref_count;
 // Uint8 hatState[4]; the current hat state for this controller
 // struct _SDL_ControllerMapping mapping; the mapping object for this controller
 // struct _SDL_GameController *next; pointer to next game controller we have allocated
};

struct _SDL_Joystick
{
  SDL_JoystickID instance_id; // Device instance, monotonically increasing from 0
};

int
main(int argc, char* argv[])
{
  memory Memory;
  AllocateMemory(&Memory, Megabytes(500));

  {
    int SDL_InitStatus = SDL_Init(SDL_INIT_VIDEO);
    ASSERT(SDL_InitStatus == 0);
  }

  SDL_Window *Window =
    SDL_CreateWindow("SphereKoan",
                     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                     SCREEN_WIDTH, SCREEN_HEIGHT,
                     SDL_WINDOW_OPENGL);
  ASSERT(Window != NULL);

  renderer_functions RendererFunctions;
  LoadRendererFunctions(&RendererFunctions);

  void *MemoryAllocatedForRenderer =
    (void *)((size_t)Memory.AllocatedSpace + Megabytes(250));
  RendererFunctions.LoadRenderer(MemoryAllocatedForRenderer, Window);
  GlobalRendererFunctions = &RendererFunctions;

  /*RendererFunctions.AddLineToRenderer(
    500, 500,
    200, 250,
    200.4f, 320.6f, 100.f
  );
  RendererFunctions.AddLineToRenderer(
    800.f, 800.f,
    1000.f, 1000.f,
    255, 0, 0
  );
  RendererFunctions.AddSemicircleToRenderer(
    GlobalScreenWidth / 4.f, GlobalScreenHeight / 4.f,
    100,
    40, 40,
    0.f,
    255, 255, 255
  );
  RendererFunctions.AddRectToRenderer(
    GlobalScreenWidth / 2.f, GlobalScreenHeight / 2.f,
    50, 50,
    25.f,
    255, 255, 255
  );*/

  {
    int SDL_InitStatus = SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    ASSERT(SDL_InitStatus == 0);
  }

  SDL_GameController *Controllers[4] = {};

  int ControllerCount = SDL_NumJoysticks();
  ControllerCount =
    ControllerCount > CONTROLLER_MAX ? CONTROLLER_MAX : ControllerCount;

  for(int ControllerIndex = 0;
      ControllerIndex < CONTROLLER_MAX;
      ++ControllerIndex)
  {
    if(SDL_IsGameController(ControllerIndex))
      Controllers[ControllerIndex] = SDL_GameControllerOpen(ControllerIndex);
  }

  debug_tools DebugTools;
  DebugTools.Print = DebugPrint;
  DebugTools.FillSemicircle = DebugFillSemicircle;
  DebugTools.DrawSemicircle = DebugDrawSemicircle;
  DebugTools.FillCircle = DebugFillCircle;
  DebugTools.DrawCircle = DebugDrawCircle;
  DebugTools.FillTriangle = DebugFillTriangle;
  DebugTools.DrawTriangle = DebugDrawTriangle;
  DebugTools.FillBox = DebugFillBox;
  DebugTools.DrawBox = DebugDrawBox;
  DebugTools.DrawLine = DebugDrawLine;
  DebugTools.SetColor = DebugSetColor;

  game_functions GameFunctions;
  LoadGameFunctions(&GameFunctions);

  float Dt;
  input_state Input = {};

  for(int ControllerIndex = 0;
      ControllerIndex < CONTROLLER_MAX;
      ++ControllerIndex)
  {
    controller_state *Controller = &Input.Controllers[ControllerIndex];
    for(int ButtonIndex = 0; ButtonIndex < BUTTONCOUNT; ++ButtonIndex)
    {
      Controller->Buttons[ButtonIndex].WasReleasedSinceLastAction = true;
    }
  }

  scene Scene = {};
  game_memory *GameMemory = (game_memory*)Memory.AllocatedSpace;
  GameMemory->Input = &Input;
  GameMemory->Scene = &Scene;

  GameFunctions.LoadGame(&Memory, &DebugTools);

  uint32_t LastTime = SDL_GetTicks();

  char *BasePath = SDL_GetBasePath();
  int BasePathLength = GetTerminatedStringLength(BasePath);

  char GameFilePath[200];
  strncpy(
    GameFilePath,
    BasePath, BasePathLength);
  strncpy(GameFilePath + BasePathLength,
          "game.dll", sizeof("game.dll"));

  char RendererFilePath[200];
  strncpy(
    RendererFilePath,
    BasePath, BasePathLength);
  strncpy(RendererFilePath + BasePathLength,
          "renderer.dll", sizeof("renderer.dll"));

  while(GlobalRunning)
  {
    Dt = (SDL_GetTicks() - LastTime) / 1000.f;
    LastTime = SDL_GetTicks();

    struct stat DLLInfo;

    stat(GameFilePath, &DLLInfo);
    if(DLLInfo.st_mtime != GameFunctions.Timestamp)
    {
      UnloadGameFunctions(&GameFunctions);
      LoadGameFunctions(&GameFunctions);
      GameFunctions.ReloadGame(&Memory, &DebugTools);
    } 

    stat(RendererFilePath, &DLLInfo);
    if(DLLInfo.st_mtime != RendererFunctions.Timestamp)
    {
      UnloadRendererFunctions(&RendererFunctions);
      LoadRendererFunctions(&RendererFunctions);
      RendererFunctions.ReloadRenderer(MemoryAllocatedForRenderer, Window);
    }

    {
      for(int ControllerIndex = 0;
          ControllerIndex < CONTROLLER_MAX;
          ++ControllerIndex)
      {
        Input.Controllers[ControllerIndex].WasMovedThisFrame = false;
      }
    }

    SDL_Event e;
    while(SDL_PollEvent(&e) != 0)
    {
      switch(e.type)
      {
        case SDL_QUIT:
        {
          GlobalRunning = false;
        } break;
        case SDL_CONTROLLERDEVICEADDED:
        {
          SDL_ControllerDeviceEvent Event = e.cdevice;
          DebugPrint("Added: %u", Event.which);
          if(ControllerCount < CONTROLLER_MAX)
          {
            if(Controllers[Event.which] == NULL)
            {
              Controllers[Event.which] = SDL_GameControllerOpen(Event.which);
              ++ControllerCount;
            }
          }

        } break;
        case SDL_CONTROLLERDEVICEREMOVED:
        {
          SDL_ControllerDeviceEvent Event = e.cdevice;
          DebugPrint("Removed: %u", Event.which);
          for(int ControllerIndex = 0;
              ControllerIndex < ControllerCount;
              ++ControllerIndex)
          {
            if(Controllers[ControllerIndex]->joystick->instance_id
               == Event.which)
            {
              SDL_GameControllerClose(Controllers[ControllerIndex]);
              Controllers[ControllerIndex] = NULL;
              --ControllerCount;
            }
          }
        } break;
        case SDL_CONTROLLERAXISMOTION:
        {
          SDL_ControllerAxisEvent Event = e.caxis;

          // Note: Deadzones moved to game layer
          // XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  7849
          // XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 8689
          // XINPUT_GAMEPAD_TRIGGER_THRESHOLD    30

          int ActualControllerIndex = -1;
          for(int ControllerIndex = 0;
              ControllerIndex < ControllerCount;
              ++ControllerIndex)
          {
            if(Controllers[ControllerIndex] != NULL
               && Controllers[ControllerIndex]->joystick->instance_id
               == Event.which)
            {
              ActualControllerIndex = ControllerIndex;
              break;
            }
          }

          controller_state* Controller =
            &Input.Controllers[ActualControllerIndex];
          if(ActualControllerIndex != -1)
          {
            // Todo: Add timing

            float NormalizedValue = Event.value < 0 ?
              Event.value / 32768.f :
              Event.value / 32767.f;

            switch(Event.axis)
            {
              case SDL_CONTROLLER_AXIS_LEFTX:
              {
                Controller->WasMovedThisFrame =
                  Controller->WasMovedThisFrame
                  || Controller->RawX != Event.value;
                Controller->RawX = Event.value;

                if(Controller->X != NormalizedValue)
                {
                  Controller->XLastState = Controller->X;

                  Controller->X = NormalizedValue;
                  char output[255];
                  snprintf(output, 255, "LeftX: %f\n",
                           NormalizedValue);
                  OutputDebugStringA(output);
                }
              } break;
              case SDL_CONTROLLER_AXIS_LEFTY:
              {
                Controller->WasMovedThisFrame =
                  Controller->WasMovedThisFrame
                  || Controller->RawY != Event.value;
                Controller->RawY = Event.value;

                if(Controller->Y != NormalizedValue)
                {
                  Controller->YLastState = Controller->Y;

                  Controller->Y = NormalizedValue;
                  char output[255];
                  snprintf(output, 255, "LeftY: %f\n",
                           NormalizedValue);
                  OutputDebugStringA(output);
                }
              } break;
            }

          }
        } break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        {
          SDL_ControllerButtonEvent Event = e.cbutton;

          bool IsDown = Event.state == SDL_PRESSED;

          int ActualControllerIndex = -1;
          for(int ControllerIndex = 0;
              ControllerIndex < ControllerCount;
              ++ControllerIndex)
          {
            if(Controllers[ControllerIndex] != NULL
               && Controllers[ControllerIndex]->joystick->instance_id
               == Event.which)
            {
              ActualControllerIndex = ControllerIndex;
              break;
            }
          }

          if(ActualControllerIndex != -1)
          {
            // Todo: Add timing
            controller_state* Controller =
              &Input.Controllers[ActualControllerIndex];

            switch(Event.button)
            {
              case SDL_CONTROLLER_BUTTON_DPAD_UP:
              {
                UpdateButton(&Controller->Up, IsDown);
              } break;
              case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
              {
                UpdateButton(&Controller->Down, IsDown);
              } break;
              case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
              {
                UpdateButton(&Controller->Left, IsDown);
              } break;
              case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
              {
                UpdateButton(&Controller->Right, IsDown);
              } break;
              case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
              {
                UpdateButton(&Controller->LeftBumper, IsDown);
              } break;
              case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
              {
                UpdateButton(&Controller->RightBumper, IsDown);
              } break;
            }
          }
        } break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
        {
          SDL_KeyboardEvent Event = e.key;

          bool IsDown = Event.state == SDL_PRESSED;

          // Todo: Add a way to change which player the keyboard controls
          // Todo: Add timing
          controller_state* Controller = &Input.Controllers[0];
          switch(Event.keysym.scancode)
          {
            case SDL_SCANCODE_UP:
            {
              UpdateButton(&Controller->Up, IsDown);
            } break;
            case SDL_SCANCODE_DOWN:
            {
              UpdateButton(&Controller->Down, IsDown);
            } break;
            case SDL_SCANCODE_LEFT:
            {
              UpdateButton(&Controller->Left, IsDown);
            } break;
            case SDL_SCANCODE_RIGHT:
            {
              UpdateButton(&Controller->Right, IsDown);
            } break;

            case SDL_SCANCODE_ESCAPE:
            {
              if(IsDown)
                GlobalRunning = false;
            }
          }
        } break;
      }
    }

    // Todo: Re-attach gamefunctions
    GameFunctions.UpdateGame(&Memory, Dt);
    RendererFunctions.RenderGame(MemoryAllocatedForRenderer);

    SDL_Delay(1);
  }

  UnloadGameFunctions(&GameFunctions);
  UnloadRendererFunctions(&RendererFunctions);

  SDL_Quit();

  return(0);
}
