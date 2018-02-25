#ifndef PIXEL_EDITOR_H

#include <stdio.h>
#include <stdint.h>

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef float real32;
typedef double real64;
typedef int32 bool32;

#define true 1
#define false 0

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define Assert(Condition) if(!(Condition)) { *(int *)0 = 0; }

#pragma pack(push, 1)
struct bitmap_header
{
	uint16 FileType;
	uint32 FileSize;
	uint16 ReservedOne;
	uint16 ReservedTwo;
	uint32 BitmapOffset;
	struct
	{
		int32 Size;
		int32 Width;
		int32 Height;
		uint16 Planes;
		uint16 BitsPerPixel;
		int32 Compression;
		int32 SizeOfBitmap;
		int32 XPelsPerMeter;
		int32 YPelsPerMeter;
		int32 ColorsUsed;
		int32 ColorsImportant;
	} InfoHeader;
};
#pragma pack(pop)

struct game_screen_buffer
{
	void *BitmapMemory;
	int32 Width;
	int32 Height;
	int32 BytesPerPixel;
	int32 Pitch;
	struct bitmap_header BitmapInfo;
};

#pragma pack(push, 1)
struct bitmap
{
	struct bitmap_header BitmapHeader;
	void *BitmapMemory;
};
#pragma pack(pop)

struct input_button_state
{
	bool32 EndedDown;
	bool32 Tapped;
	int32 HalfTransitionCount;
};

struct app_input
{
	real32 dtForFrame;
	real32 MouseX;
	real32 MouseY;
	real32 LastMouseX;
	real32 LastMouseY;
	int32 MouseWheelScrollDirection;
	union
	{
		struct input_button_state Buttons[3];
		struct
		{
			struct input_button_state ButtonPrimary;
			struct input_button_state ButtonSecondary;

			struct input_button_state ButtonSave;
			struct input_button_state ButtonReset;
			struct input_button_state ButtonEraser;
			struct input_button_state ButtonQuickSwitch;
			struct input_button_state ButtonEyeDropper;

			struct input_button_state ButtonSize1;  // 32
			struct input_button_state ButtonSize2;  // 64
			struct input_button_state ButtonSize3;  // 128
			struct input_button_state ButtonSize4;  // 256
			struct input_button_state ButtonSize5;  // 512
			struct input_button_state ButtonSize6;  // 1024
		};
	};
};

union v4
{
	struct
	{
		real32 x, y, z, w;
	};
	struct
	{
		real32 r, g, b, a;
	};
	real32 E[4];
};

inline union v4
V4(real32 X, real32 Y, real32 Z, real32 W)
{
	union v4 Result = {0};
	Result.x = X;
	Result.y = Y;
	Result.z = Z;
	Result.w = W;
	return(Result);
}

union v2
{
	struct
	{
		real32 x, y;
	};
	struct
	{
		real32 Width, Height;
	};
	real32 E[2];
};

inline union v2
V2(real32 X, real32 Y)
{
	union v2 Result = {0};
	Result.x = X;
	Result.y = Y;
	return(Result);
}

struct custom_color_button
{
	v2 Position;
	v2 Dimensions;
	v4 Color;
};

#define PLATFORM_WRITE_FILE(name) bool32 name(char *Filename, void *Data, uint32 Size)
typedef PLATFORM_WRITE_FILE(platform_write_file);

#define PLATFORM_ALLOCATE_MEMORY(name) void * name(uint32 Size)
typedef PLATFORM_ALLOCATE_MEMORY(platform_allocate_memory);

#define PLATFORM_FREE_MEMORY(name) void name(void *Memory)
typedef PLATFORM_FREE_MEMORY(platform_free_memory);

struct app_state
{
	bool32 Initialized;
	bool32 EyeDropperModeEnabled;

	uint32 PixelMapWidth;
	uint32 PixelMapHeight;
	real32 PixelMapZoom;
	real32 MinPixelMapZoom;
	v4 *PixelMap;

	v2 EditingAreaOffset;
	v2 EditingAreaSize;
	v2 EditingAreaMapOffset;

	struct custom_color_button QuickSwitchColor;
	struct custom_color_button ColorPickerButton;
	bool32 ColorPickerButtonClicked;
	v4 PixelColor;

	struct custom_color_button CustomColorButtons[16];
	v2 CustomColorDims;

	platform_write_file *PlatformWriteFile;
	platform_allocate_memory *PlatformAllocateMemory;
	platform_free_memory *PlatformFreeMemory;
};

#define PIXEL_EDITOR_H
#endif
