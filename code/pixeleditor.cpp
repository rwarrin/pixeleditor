#include "pixeleditor.h"

static inline union v4
U32ToV4Pixel(uint32 Color)
{
	union v4 Result = {0};
	Result.a = (Color & (0xff << 24));
	Result.r = (Color & (0xff << 16));
	Result.g = (Color & (0xff << 8));
	Result.b = (Color & (0xff << 0));

	return(Result);
}

static inline uint32
V4ToU32Pixel(union v4 Color)
{
	uint32 Result = 0;
	Result = ( ((int32)Color.a << 24) |
			   ((int32)Color.r << 16) |
			   ((int32)Color.g << 8) |
			   ((int32)Color.b << 0) );

	return(Result);
}

static void
ClearScreenToColor(struct game_screen_buffer *Buffer, union v4 Color)
{
	uint32 PixelColor = V4ToU32Pixel(Color);
	uint8 *Base = (uint8 *)Buffer->BitmapMemory;
	for(uint32 Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)(Base + (Y * Buffer->Pitch));
		for(uint32 X = 0; X < Buffer->Width; ++X)
		{
			*Pixel++ = PixelColor;
		}
	}
}

static void
DrawRectangle(struct game_screen_buffer *Buffer, uint32 XPos, uint32 YPos,
			  uint32 Width, uint32 Height, union v4 Color)
{
	int32 MinX = (int32)XPos;
	int32 MinY = (int32)YPos;
	int32 MaxX = (int32)XPos + Width;
	int32 MaxY = (int32)YPos + Height;

	if(MinX < 0) { MinX = 0; }
	if(MinY < 0) { MinY = 0; }
	if(MaxX > Buffer->Width) { MaxX = Buffer->Width; }
	if(MaxY > Buffer->Height) { MaxY = Buffer->Height; }

	if(MaxX < MinX) { MaxX = MinX; }
	if(MaxY < MinY) { MaxY = MinY; }

	uint32 PixelColor = V4ToU32Pixel(Color);
	uint8 *Row = ((uint8 *)Buffer->BitmapMemory + (MinY * Buffer->Pitch) + (MinX * Buffer->BytesPerPixel));
	for(uint32 Y = MinY; Y < MaxY; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for(uint32 X = MinX; X < MaxX; ++X)
		{
			*Pixel++ = PixelColor;
		}
		Row += Buffer->Pitch;
	}
}

static void
DrawRectangleWithBounds(struct game_screen_buffer *Buffer,
						uint32 XMinBound, uint32 XMaxBound,
						uint32 YMinBound, uint32 YMaxBound,
						uint32 XPos, uint32 YPos,
						uint32 Width, uint32 Height, union v4 Color)
{
	int32 MinX = (int32)XPos;
	int32 MinY = (int32)YPos;
	int32 MaxX = (int32)XPos + Width;
	int32 MaxY = (int32)YPos + Height;

	if(XMinBound < 0) { XMinBound = 0; }
	if(YMinBound < 0) { YMinBound = 0; }
	if(XMaxBound > Buffer->Width) { XMaxBound = Buffer->Width; }
	if(YMaxBound > Buffer->Height) { YMaxBound = Buffer->Height; }

	if(MinX < XMinBound) { MinX = XMinBound; }
	if(MinY < YMinBound) { MinY = YMinBound; }
	if(MaxX > XMaxBound) { MaxX = XMaxBound; }
	if(MaxY > YMaxBound) { MaxY = YMaxBound; }

	if(MaxX < MinX) { MaxX = MinX; }
	if(MaxY < MinY) { MaxY = MinY; }

	uint32 PixelColor = V4ToU32Pixel(Color);
	uint8 *Row = ((uint8 *)Buffer->BitmapMemory + (MinY * Buffer->Pitch) + (MinX * Buffer->BytesPerPixel));
	for(uint32 Y = MinY; Y < MaxY; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for(uint32 X = MinX; X < MaxX; ++X)
		{
			if((Y == MaxY-1) || (X == MaxX-1))
			{
				*Pixel++ = 0xff666666;
			}
			else
			{
				*Pixel++ = PixelColor;
			}
		}
		Row += Buffer->Pitch;
	}
}

static inline union v4 *
GetPixelMapPixelColor(struct app_state *AppState, real32 X, real32 Y)
{
	union v4 *Result = 0;

	int32 MouseX = (int32)X;
	int32 MouseY = (int32)Y;
	int32 GridX = ((MouseX - AppState->EditingAreaOffset.x) / AppState->PixelMapZoom) - AppState->EditingAreaMapOffset.x;
	int32 GridY = ((MouseY - AppState->EditingAreaOffset.y) / AppState->PixelMapZoom) - AppState->EditingAreaMapOffset.y;

	if(((GridX >= 0) && (GridX < AppState->PixelMapWidth)) &&
	   ((GridY >= 0) && (GridY < AppState->PixelMapHeight)))
	{
		Result = (v4 *)(AppState->PixelMap + (GridY * AppState->PixelMapWidth) + GridX);
	}

	return(Result);
}

static void
SetPixelMapPixelColor(struct app_state *AppState, real32 X, real32 Y, union v4 Color)
{
	v4 *Pixel = GetPixelMapPixelColor(AppState, X, Y);
	if(Pixel)
	{
		*Pixel = Color;
	}
}

inline static bool32
ActionPerformedWithinRegion(bool32 InputState, real32 MouseX, real32 MouseY,
							real32 X, real32 Y, real32 Width, real32 Height)
{
	bool32 Result = false;

	if(InputState)
	{
		if(((MouseX >= X) && (MouseX <= X + Width)) &&
		   ((MouseY >= Y) && (MouseY <= Y + Height)))
		{
			Result = true;
		}
	}

	return(Result);
}

static void
ExportBitmap(char *Filename, v4 *PixelMap, uint32 Width, uint32 Height, struct app_state *AppState)
{
	struct bitmap_header BitmapHeader = {0};
	BitmapHeader.FileType = 0x4D42;
	BitmapHeader.BitmapOffset = sizeof(struct bitmap_header);
	BitmapHeader.InfoHeader.Size = sizeof(BitmapHeader.InfoHeader);
	BitmapHeader.InfoHeader.Width = Width;
	BitmapHeader.InfoHeader.Height = -Height;
	BitmapHeader.InfoHeader.Planes = 1;
	BitmapHeader.InfoHeader.BitsPerPixel = 32;
	BitmapHeader.InfoHeader.Compression = BI_RGB;

	uint32 BitmapDataSize = sizeof(struct bitmap_header) + ((Width * Height) * (BitmapHeader.InfoHeader.BitsPerPixel / 8));
	uint8 *BitmapData = (uint8 *)AppState->PlatformAllocateMemory(BitmapDataSize);
	Assert(BitmapData != 0);

	*((struct bitmap_header *)BitmapData) = BitmapHeader;
	uint32 *Dest = (uint32 *)(BitmapData + sizeof(struct bitmap_header));
	v4 *SourceV4 = PixelMap;
	for(uint32 Y = 0; Y < Height; ++Y)
	{
		for(uint32 X = 0; X < Width; ++X)
		{
			uint32 Pixel = 0;
			Pixel = V4ToU32Pixel(*SourceV4);
			*Dest++ = Pixel;
			SourceV4++;
		}
	}

	AppState->PlatformWriteFile(Filename, BitmapData, sizeof(struct bitmap_header) + BitmapDataSize);
	AppState->PlatformFreeMemory(BitmapData);
}

static void
UpdatePixelEditorPosition(struct app_state *AppState, struct app_input *Input)
{
	real32 DrawingAreaGridSizeX = AppState->EditingAreaSize.x / AppState->PixelMapZoom;
	real32 DrawingAreaGridSizeY = AppState->EditingAreaSize.y / AppState->PixelMapZoom;

	if(Input != 0)
	{
		AppState->EditingAreaMapOffset.x += (Input->MouseX - Input->LastMouseX) / AppState->PixelMapZoom;
		AppState->EditingAreaMapOffset.y += (Input->MouseY - Input->LastMouseY) / AppState->PixelMapZoom;
	}

	if(AppState->EditingAreaMapOffset.x > 0)
	{
		AppState->EditingAreaMapOffset.x = 0;
	}
	if(AppState->EditingAreaMapOffset.x < -(AppState->PixelMapWidth - DrawingAreaGridSizeX))
	{
		AppState->EditingAreaMapOffset.x = -(AppState->PixelMapWidth - DrawingAreaGridSizeX);
	}

	if(AppState->EditingAreaMapOffset.y > 0)
	{
		AppState->EditingAreaMapOffset.y = 0;
	}
	if(AppState->EditingAreaMapOffset.y < -(AppState->PixelMapHeight - DrawingAreaGridSizeY))
	{
		AppState->EditingAreaMapOffset.y = -(AppState->PixelMapHeight - DrawingAreaGridSizeY);
	}
}

static void
ResizeCanvas(struct app_state *AppState, int32 CanvasWidth, int32 CanvasHeight)
{
	if(AppState->PixelMap)
	{
		AppState->PlatformFreeMemory(AppState->PixelMap);
	}

	AppState->EditingAreaSize = V2(700.0f, 700.0f);
	AppState->EditingAreaOffset = V2(80.0f, 10.0f);
	AppState->PixelMapWidth = CanvasWidth;
	AppState->PixelMapHeight = CanvasHeight;
	AppState->PixelMapZoom = AppState->EditingAreaSize.x / (real32)AppState->PixelMapWidth;
	AppState->MinPixelMapZoom = AppState->PixelMapZoom;
	if(AppState->PixelMapZoom < 5.0f)
	{
		AppState->PixelMapZoom = 5.0f;
		AppState->MinPixelMapZoom = 5.0f;
	}

	uint32 PixelMapSize = AppState->PixelMapWidth * AppState->PixelMapHeight;
	AppState->PixelMap = (v4 *)AppState->PlatformAllocateMemory(PixelMapSize * sizeof(v4));
	Assert(AppState->PixelMap);

	UpdatePixelEditorPosition(AppState, NULL);
}

static void
EditorUpdateAndRender(struct app_state *AppState, struct game_screen_buffer *Buffer, struct app_input *Input)
{
	if(!AppState->Initialized)
	{
		ResizeCanvas(AppState, 64, 64);
		AppState->ColorPickerButton.Position = V2(AppState->EditingAreaOffset.x, AppState->EditingAreaOffset.y + AppState->EditingAreaSize.y + 10);
		AppState->ColorPickerButton.Dimensions = V2(64, 64);
		AppState->ColorPickerButton.Color = V4(0xff, 0x00, 0x00, 0xff);
		AppState->QuickSwitchColor = AppState->ColorPickerButton;
		// NOTE(rick): We really need some vector math functions :)
		AppState->QuickSwitchColor.Position = V2(AppState->QuickSwitchColor.Position.x - 4, AppState->QuickSwitchColor.Position.y - 4);
		AppState->QuickSwitchColor.Color = V4(0xff, 0xff, 0xff, 0xff);

		AppState->PixelColor = AppState->ColorPickerButton.Color;
		AppState->CustomColorDims = V2(30.0f, 30.0f);

		int32 ButtonsPerRow = 8;
		int32 ButtonRows = 0;
		for(int32 CustomColorIndex = 0;
			CustomColorIndex < ArrayCount(AppState->CustomColorButtons);
			++CustomColorIndex)
		{
			if((CustomColorIndex != 0) &&
			   (CustomColorIndex % ButtonsPerRow == 0))
			{
				++ButtonRows;
			}

			struct custom_color_button *Button = AppState->CustomColorButtons + CustomColorIndex;
			Button->Dimensions = AppState->CustomColorDims;
			Button->Position = V2(AppState->ColorPickerButton.Position.x + AppState->ColorPickerButton.Dimensions.x + 10 + ((Button->Dimensions.x + 4) * (CustomColorIndex % ButtonsPerRow)),
								  AppState->ColorPickerButton.Position.y + ((ButtonRows * (Button->Dimensions.y + 4))));
			Button->Color = V4(0x00, 0x00, 0x00, 0xff);

		}

		AppState->Initialized = true;
	}

	if(Input->ButtonSize1.Tapped)
	{
		ResizeCanvas(AppState, 32, 32);
	}
	if(Input->ButtonSize2.Tapped)
	{
		ResizeCanvas(AppState, 64, 64);
	}
	if(Input->ButtonSize3.Tapped)
	{
		ResizeCanvas(AppState, 128, 128);
	}
	if(Input->ButtonSize4.Tapped)
	{
		ResizeCanvas(AppState, 256, 256);
	}
	if(Input->ButtonSize5.Tapped)
	{
		ResizeCanvas(AppState, 512, 512);
	}
	if(Input->ButtonSize6.Tapped)
	{
		ResizeCanvas(AppState, 1024, 1024);
	}

	if(Input->MouseWheelScrollDirection != 0)
	{
		if(Input->MouseWheelScrollDirection > 0)
		{
			AppState->PixelMapZoom = (int32)(AppState->PixelMapZoom + 5.0f);
		}
		else
		{
			AppState->PixelMapZoom = (int32)(AppState->PixelMapZoom - 5.0f);
			if(AppState->PixelMapZoom <= AppState->MinPixelMapZoom)
			{
				AppState->PixelMapZoom = AppState->MinPixelMapZoom;
				AppState->EditingAreaMapOffset = V2(0.0f, 0.0f);
			}
			UpdatePixelEditorPosition(AppState, Input);
		}
	}

	if(Input->ButtonSave.Tapped)
	{
		ExportBitmap("Bitmap.bmp", AppState->PixelMap, AppState->PixelMapWidth, AppState->PixelMapHeight, AppState);
	}
	if(Input->ButtonReset.Tapped)
	{
		v4 *PixelData = AppState->PixelMap;
		for(int32 Y = 0; Y < AppState->PixelMapHeight; ++Y)
		{
			for(int32 X = 0; X < AppState->PixelMapWidth; ++X)
			{
				*PixelData++ = V4(0.0f, 0.0f, 0.0f, 0xff);
			}
		}
	}
	if(Input->ButtonEraser.Tapped)
	{
		AppState->PixelColor = V4(0.0f, 0.0f, 0.0f, 0xff);
	}
	if(Input->ButtonQuickSwitch.Tapped)
	{
		v4 TempColor = AppState->PixelColor;
		AppState->PixelColor = AppState->QuickSwitchColor.Color;
		AppState->QuickSwitchColor.Color = TempColor;
	}
	if(Input->ButtonEyeDropper.Tapped)
	{
		AppState->EyeDropperModeEnabled = !AppState->EyeDropperModeEnabled;
	}

	AppState->ColorPickerButtonClicked = ActionPerformedWithinRegion(Input->ButtonPrimary.EndedDown,
																	 Input->MouseX, Input->MouseY,
																	 AppState->ColorPickerButton.Position.x,
																	 AppState->ColorPickerButton.Position.y,
																	 AppState->ColorPickerButton.Dimensions.x,
																	 AppState->ColorPickerButton.Dimensions.y);

	if(ActionPerformedWithinRegion(Input->ButtonSecondary.EndedDown, Input->MouseX, Input->MouseY,
								   AppState->EditingAreaOffset.x, AppState->EditingAreaOffset.y,
								   AppState->EditingAreaSize.x, AppState->EditingAreaSize.y))
	{
		UpdatePixelEditorPosition(AppState, Input);
	}
	else if(ActionPerformedWithinRegion(Input->ButtonPrimary.EndedDown, Input->MouseX, Input->MouseY,
										AppState->EditingAreaOffset.x, AppState->EditingAreaOffset.y,
										AppState->EditingAreaSize.x, AppState->EditingAreaSize.y))
	{
		if(AppState->EyeDropperModeEnabled)
		{
			// TODO(rick): Add some sort of visual queue that we're in eye
			// dropper mode
			v4 *Pixel = GetPixelMapPixelColor(AppState, Input->MouseX, Input->MouseY);
			if(Pixel != NULL)
			{
				AppState->PixelColor = *Pixel;
			}
		}
		else
		{
			SetPixelMapPixelColor(AppState, Input->MouseX, Input->MouseY, AppState->PixelColor);
		}
	}

	for(int32 CustomColorIndex = 0;
		CustomColorIndex < ArrayCount(AppState->CustomColorButtons);
		++CustomColorIndex)
	{
		struct custom_color_button Button = *(AppState->CustomColorButtons + CustomColorIndex);
		if(ActionPerformedWithinRegion(Input->ButtonPrimary.EndedDown, Input->MouseX, Input->MouseY,
									   Button.Position.x, Button.Position.y,
									   Button.Dimensions.x, Button.Dimensions.y))
		{
			AppState->PixelColor = Button.Color;
		}
	}

	ClearScreenToColor(Buffer, V4(17.0f, 17.0f, 17.0f, 255.0f));
	DrawRectangle(Buffer, AppState->EditingAreaOffset.x - 1, AppState->EditingAreaOffset.y - 1,
				  AppState->EditingAreaSize.x + 2, AppState->EditingAreaSize.y + 2,
				  V4(0xdd, 0xdd, 0xdd, 0xdd));
	DrawRectangle(Buffer, AppState->EditingAreaOffset.x, AppState->EditingAreaOffset.y,
				  AppState->EditingAreaSize.x, AppState->EditingAreaSize.y,
				  V4(0.0f, 0.0f, 0.0f, 255.0f));

	for(uint32 MapY = 0;
		MapY < AppState->PixelMapHeight;
		++MapY)
	{
		for(uint32 MapX = 0;
			MapX < AppState->PixelMapWidth;
			++MapX)
		{
			v4 *Pixel = AppState->PixelMap + (MapY * AppState->PixelMapWidth) + MapX;
			DrawRectangleWithBounds(Buffer,
									AppState->EditingAreaOffset.x, AppState->EditingAreaOffset.x + AppState->EditingAreaSize.x,
									AppState->EditingAreaOffset.y, AppState->EditingAreaOffset.y + AppState->EditingAreaSize.y,
									AppState->EditingAreaOffset.x + ((MapX + AppState->EditingAreaMapOffset.x) * AppState->PixelMapZoom),
									AppState->EditingAreaOffset.y + ((MapY + AppState->EditingAreaMapOffset.y) * AppState->PixelMapZoom),
									AppState->PixelMapZoom, AppState->PixelMapZoom, *Pixel);
		}
	}

	DrawRectangle(Buffer, AppState->QuickSwitchColor.Position.x, AppState->QuickSwitchColor.Position.y,
				  AppState->QuickSwitchColor.Dimensions.x, AppState->QuickSwitchColor.Dimensions.y,
				  AppState->QuickSwitchColor.Color);
	DrawRectangle(Buffer, AppState->ColorPickerButton.Position.x, AppState->ColorPickerButton.Position.y,
				  AppState->ColorPickerButton.Dimensions.x, AppState->ColorPickerButton.Dimensions.y,
				  AppState->PixelColor);

	for(int32 CustomColorIndex = 0;
		CustomColorIndex < ArrayCount(AppState->CustomColorButtons);
		++CustomColorIndex)
	{
		struct custom_color_button Button = *(AppState->CustomColorButtons + CustomColorIndex);
		DrawRectangle(Buffer, Button.Position.x, Button.Position.y, Button.Dimensions.x,
					  Button.Dimensions.y, Button.Color);
	}

}
