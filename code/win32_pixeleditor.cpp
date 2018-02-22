#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <commdlg.h>
#include <timeapi.h>
#include "pixeleditor.cpp"
#include "win32_pixeleditor.h"

static bool32 GlobalRunning;
static struct game_screen_buffer *GlobalScreenBuffer;

static void
Win32ResizeDIBSection(struct game_screen_buffer *Buffer, uint32 Width, uint32 Height)
{
	if(Buffer->BitmapMemory)
	{
		VirtualFree(Buffer->BitmapMemory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;
	Buffer->BytesPerPixel = 4;
	Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;

	Buffer->BitmapInfo.BitmapOffset = sizeof(struct bitmap_header);
	Buffer->BitmapInfo.InfoHeader.Size = 40;
	Buffer->BitmapInfo.InfoHeader.Width = Width;
	Buffer->BitmapInfo.InfoHeader.Height = -Height; // NOTE(rick): Negative height for top down bitmaps
	Buffer->BitmapInfo.InfoHeader.Planes = 1;
	Buffer->BitmapInfo.InfoHeader.BitsPerPixel = 32;
	Buffer->BitmapInfo.InfoHeader.Compression = BI_RGB;

	uint32 BitmapMemorySize = (Buffer->Width * Buffer->Height) * Buffer->BytesPerPixel;
	Buffer->BitmapMemory = (void *)VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

static void
Win32DrawScreenBufferToWindow(HDC DeviceContext, struct game_screen_buffer *Buffer,
							  uint32 X, uint32 Y, uint32 Width, uint32 Height)
{
	StretchDIBits(DeviceContext,
				  X, Y, Width, Height,
				  X, Y, Width, Height,
				  Buffer->BitmapMemory,
				  (BITMAPINFO *)&Buffer->BitmapInfo.InfoHeader,
				  DIB_RGB_COLORS, SRCCOPY);
}

inline static LARGE_INTEGER
Win32GetWallClock()
{
	LARGE_INTEGER Result = {0};
	QueryPerformanceCounter(&Result);
	return(Result);
}

inline static real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
	LARGE_INTEGER CPUFrequency = {0};
	QueryPerformanceFrequency(&CPUFrequency);

	real32 Result = ((real32)(End.QuadPart - Start.QuadPart) / (real32)(CPUFrequency.QuadPart));
	return(Result);
}

static inline struct win32_window_dimensions
Win32GetWindowDimensions(HWND Window)
{
	struct win32_window_dimensions Result = {0};

	RECT ClientRect = {0};
	GetClientRect(Window, &ClientRect);
	Result.Width = ClientRect.right - ClientRect.left;
	Result.Height = ClientRect.bottom - ClientRect.top;

	return(Result);
}

static void
Win32ProcessInputMessage(struct input_button_state *Button, bool32 IsDown)
{
	if(Button->EndedDown != IsDown)
	{
		Button->EndedDown = IsDown;
		++Button->HalfTransitionCount;
	}

	if(Button->EndedDown)
	{
		Button->Tapped = true;
	}
}

LRESULT CALLBACK
Win32WindowsCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
	LRESULT Result = 0;

	switch(Message)
	{
		case WM_QUIT:
		case WM_DESTROY:
		{
			GlobalRunning = false;
			PostQuitMessage(0);
		} break;
		case WM_SIZE:
		{
			if(GlobalScreenBuffer)
			{
				struct win32_window_dimensions WindowDims = Win32GetWindowDimensions(Window);
				Win32ResizeDIBSection(GlobalScreenBuffer, WindowDims.Width, WindowDims.Height);
			}
		} break;
		default:
		{
			Result = DefWindowProc(Window, Message, WParam, LParam);
		} break;
	}

	return(Result);
}

static void
Win32ProcessPendingMessages(struct app_input *Input)
{
	MSG Message = {0};
	while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
	{
		switch(Message.message)
		{
			case WM_SYSKEYDOWN:
			case WM_SYSKEYUP:
			case WM_KEYUP:
			case WM_KEYDOWN:
			{
				uint32 VKCode = (uint32)Message.wParam;
				uint32 WasDown = ((Message.lParam & (1 << 30)) != 0);
				uint32 IsDown = ((Message.lParam & (1 << 31)) == 0);
				if(IsDown != WasDown)
				{
					if(VKCode == 'S')
					{
						Win32ProcessInputMessage(&Input->ButtonSave, IsDown);
					}
					if(VKCode == 'R')
					{
						Win32ProcessInputMessage(&Input->ButtonReset, IsDown);
					}
					if(VKCode == 'E')
					{
						Win32ProcessInputMessage(&Input->ButtonEraser, IsDown);
					}
					if(VKCode == 'Q')
					{
						Win32ProcessInputMessage(&Input->ButtonQuickSwitch, IsDown);
					}
					if(VKCode == VK_SPACE)
					{
						Win32ProcessInputMessage(&Input->ButtonEyeDropper, IsDown);
					}
				}
			} break;
			case WM_MOUSEWHEEL:
			{
				int MouseWheelDirection = (int16)HIWORD(Message.wParam);
				Input->MouseWheelScrollDirection = MouseWheelDirection;
			} break;
			default:
			{
				TranslateMessage(&Message);
				DispatchMessage(&Message);
			} break;
		}
	}
}

PLATFORM_WRITE_FILE(Win32WriteFile)
{
	bool32 Result = false;

	HANDLE File = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if(File != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten = 0;
		if(WriteFile(File, Data, Size, &BytesWritten, 0))
		{
			if(BytesWritten == Size)
			{
				Result = true;
			}
		}

		CloseHandle(File);
	}

	return(Result);
}

PLATFORM_ALLOCATE_MEMORY(Win32AllocateMemory)
{
	void *Result = VirtualAlloc(0, Size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
	return(Result);
}

PLATFORM_FREE_MEMORY(Win32FreeMemory)
{
	if(Memory)
	{
		VirtualFree(Memory, 0, MEM_RELEASE | MEM_DECOMMIT);
	}
}

int WINAPI
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CmdLine, int CmdShow)
{
	WNDCLASSEXA WindowClass = {0};
	WindowClass.cbSize = sizeof(WindowClass);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = Win32WindowsCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "PixelEditorClass";
	WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);

	if(!RegisterClassEx(&WindowClass))
	{
		MessageBox(NULL, "Error", "Failed to register window class.", MB_OK);
		return 1;
	}

	int WindowWidth = 860;
	int WindowHeight = 860;
	HWND Window = CreateWindowEx(0,
								 WindowClass.lpszClassName,
								 "Pixel Editor",
								 WS_OVERLAPPEDWINDOW | WS_VISIBLE,
								 CW_USEDEFAULT, CW_USEDEFAULT,
								 WindowWidth, WindowHeight,
								 0, 0, Instance, 0);

	if(!Window)
	{
		MessageBox(NULL, "Error", "Failed to create window.", MB_OK);
		return 2;
	}

	timeBeginPeriod(1);
	struct game_screen_buffer ScreenBuffer = {0};
	GlobalScreenBuffer = &ScreenBuffer;
	struct win32_window_dimensions WindowDims = Win32GetWindowDimensions(Window);
	Win32ResizeDIBSection(&ScreenBuffer, WindowDims.Width, WindowDims.Height);

	struct app_input Input[2] = {0};
	struct app_input *NewInput = &Input[0];
	struct app_input *OldInput = &Input[1];

	struct app_state AppState = {0};
	AppState.PlatformWriteFile = Win32WriteFile;
	AppState.PlatformAllocateMemory = Win32AllocateMemory;
	AppState.PlatformFreeMemory = Win32FreeMemory;

	COLORREF CustomColors[16] = {0};
	GlobalRunning = true;
	real32 TargetFPS = 60.0f;
	real32 TargetSecondsPerFrame = 1.0f / TargetFPS;
	real32 TargetMSPerFrame = TargetSecondsPerFrame * 1000.0f;
	while(GlobalRunning)
	{
		LARGE_INTEGER StartTime = Win32GetWallClock();

		*NewInput = {0};
		NewInput->dtForFrame = TargetSecondsPerFrame;
		NewInput->LastMouseX = OldInput->MouseX;
		NewInput->LastMouseY = OldInput->MouseY;
		for(uint32 ButtonIndex = 0;
			ButtonIndex < ArrayCount(NewInput->Buttons);
			++ButtonIndex)
		{
			NewInput->Buttons[ButtonIndex].EndedDown = OldInput->Buttons[ButtonIndex].EndedDown;
		}

		Win32ProcessPendingMessages(NewInput);

		POINT CursorPos = {0};
		GetCursorPos(&CursorPos);
		ScreenToClient(Window, &CursorPos);
		NewInput->MouseX = CursorPos.x;
		NewInput->MouseY = CursorPos.y;
		Win32ProcessInputMessage(&NewInput->ButtonPrimary, (GetKeyState(VK_LBUTTON) & (1 << 15)));
		Win32ProcessInputMessage(&NewInput->ButtonSecondary, (GetKeyState(VK_RBUTTON) & (1 << 15)));

		EditorUpdateAndRender(&AppState, &ScreenBuffer, NewInput);

		if(AppState.ColorPickerButtonClicked)
		{
			CHOOSECOLOR ChosenColor = {0};
			ChosenColor.lStructSize = sizeof(ChosenColor);
			ChosenColor.lpCustColors = CustomColors;
			ChosenColor.rgbResult = RGB(AppState.PixelColor.r, AppState.PixelColor.g, AppState.PixelColor.b);
			ChosenColor.Flags = CC_FULLOPEN | CC_RGBINIT;
			bool32 ColorPicked = ChooseColor(&ChosenColor);
			if(ColorPicked)
			{
				AppState.PixelColor = V4(GetRValue(ChosenColor.rgbResult),
										 GetGValue(ChosenColor.rgbResult),
										 GetBValue(ChosenColor.rgbResult),
										 0xff);
				if(AppState.CustomColorButtons)
				{
					for(int32 CustomColorIndex = 0;
						CustomColorIndex < ArrayCount(CustomColors);
						++CustomColorIndex)
					{
						COLORREF CustomColor = CustomColors[CustomColorIndex];
						AppState.CustomColorButtons[CustomColorIndex].Color = V4(GetRValue(CustomColor),
																				 GetGValue(CustomColor),
																				 GetBValue(CustomColor),
																				 0xff);
					}
				}
			}
			AppState.ColorPickerButtonClicked = false;
		}

		HDC DeviceContext = GetDC(Window);
		struct win32_window_dimensions WindowDims = Win32GetWindowDimensions(Window);
		Win32DrawScreenBufferToWindow(DeviceContext, &ScreenBuffer, 0, 0, WindowDims.Width, WindowDims.Height);
		ReleaseDC(Window, DeviceContext);

		struct app_input *TempInput = NewInput;
		NewInput = OldInput;
		OldInput = TempInput;

		LARGE_INTEGER EndTime = Win32GetWallClock();
		real32 SecondsElapsedForFrame = Win32GetSecondsElapsed(StartTime, EndTime);
		real32 MSElapsedForFrame = SecondsElapsedForFrame * 1000.0f;
		if(MSElapsedForFrame < TargetMSPerFrame)
		{
			real32 MSToSleep = (real32)(TargetMSPerFrame - MSElapsedForFrame);
			if(MSToSleep > 0.0f)
			{
				Sleep(MSToSleep);
			}

#if 0
			real32 ActualFPS = 1.0f / Win32GetSecondsElapsed(StartTime, Win32GetWallClock());
			char FPSBuffer[64] = {0};
			_snprintf(FPSBuffer, 64, "%.02ff/s\n", ActualFPS);
			OutputDebugStringA(FPSBuffer);
#endif
		}
	}

	return 0;
}
