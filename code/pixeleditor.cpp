#include "pixeleditor.h"

static void
ClearScreenToColor(struct game_screen_buffer *Buffer, union v4 Color)
{
	uint32 PixelColor = ( ((int32)Color.a << 24) |
						  ((int32)Color.r << 16) |
						  ((int32)Color.g << 8) |
						  ((int32)Color.b << 0) );

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

	uint32 PixelColor = ( ((int32)Color.a << 24) |
						  ((int32)Color.r << 16) |
						  ((int32)Color.g << 8) |
						  ((int32)Color.b << 0) );

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

	uint32 PixelColor = ( ((int32)Color.a << 24) |
						  ((int32)Color.r << 16) |
						  ((int32)Color.g << 8) |
						  ((int32)Color.b << 0) );

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

static void
SetPixelMapPixelColor(struct app_state *AppState, real32 X, real32 Y, union v4 Color)
{
	int32 MouseX = (int32)X;
	int32 MouseY = (int32)Y;

	int32 GridX = ((MouseX - AppState->EditingAreaOffset.x) / AppState->PixelMapZoom) - AppState->EditingAreaMapOffset.x;
	int32 GridY = ((MouseY - AppState->EditingAreaOffset.y) / AppState->PixelMapZoom) - AppState->EditingAreaMapOffset.y;

	if(((GridX >= 0) && (GridX < AppState->PixelMapWidth)) &&
	   ((GridY >= 0) && (GridY < AppState->PixelMapHeight)))
	{
		v4 *Pixel = (v4 *)(AppState->PixelMap + (GridY * AppState->PixelMapWidth) + GridX);
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
			Pixel = ( (0xff << 24) |
					  ((int32)SourceV4->r << 16) |
					  ((int32)SourceV4->g << 8) |
					  ((int32)SourceV4->b << 0) );
			*Dest++ = Pixel;
			SourceV4++;
		}
	}

	AppState->PlatformWriteFile(Filename, BitmapData, sizeof(struct bitmap_header) + BitmapDataSize);
	AppState->PlatformFreeMemory(BitmapData);
}

static void
EditorUpdateAndRender(struct app_state *AppState, struct game_screen_buffer *Buffer, struct app_input *Input)
{
	if(!AppState->Initialized)
	{
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
			Button->Position = V2(AppState->ColorPickerButton.x + AppState->ColorPickerButton.z + 10 + ((Button->Dimensions.x + 4) * (CustomColorIndex % ButtonsPerRow)),
								  AppState->ColorPickerButton.y + ((ButtonRows * (Button->Dimensions.y + 4))));
			Button->Color = V4(0x00, 0x00, 0x00, 0xff);

		}
		AppState->Initialized = true;
	}

	if(Input->MouseWheelScrollDirection != 0)
	{
		if(Input->MouseWheelScrollDirection > 0)
		{
			AppState->PixelMapZoom += 5;
		}
		else
		{
			AppState->PixelMapZoom -= 5;
			if(AppState->PixelMapZoom <= AppState->MinPixelMapZoom)
			{
				AppState->PixelMapZoom = AppState->MinPixelMapZoom;
				AppState->EditingAreaMapOffset = V2(0.0f, 0.0f);
			}
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

	AppState->ColorPickerButtonClicked = ActionPerformedWithinRegion(Input->ButtonPrimary.EndedDown,
																	 Input->MouseX, Input->MouseY,
																	 AppState->ColorPickerButton.x,
																	 AppState->ColorPickerButton.y,
																	 AppState->ColorPickerButton.z,
																	 AppState->ColorPickerButton.w);

	if(ActionPerformedWithinRegion(Input->ButtonSecondary.EndedDown, Input->MouseX, Input->MouseY,
								   AppState->EditingAreaOffset.x, AppState->EditingAreaOffset.y,
								   AppState->EditingAreaOffset.x + AppState->EditingAreaSize.x,
								   AppState->EditingAreaOffset.y + AppState->EditingAreaSize.y))
	{
		real32 DrawingAreaGridSizeX = AppState->EditingAreaSize.x / AppState->PixelMapZoom;
		real32 DrawingAreaGridSizeY = AppState->EditingAreaSize.y / AppState->PixelMapZoom;

		AppState->EditingAreaMapOffset.x += (Input->MouseX - Input->LastMouseX) / AppState->PixelMapZoom;
		AppState->EditingAreaMapOffset.y += (Input->MouseY - Input->LastMouseY) / AppState->PixelMapZoom;

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
	else if(ActionPerformedWithinRegion(Input->ButtonPrimary.EndedDown, Input->MouseX, Input->MouseY,
										AppState->EditingAreaOffset.x, AppState->EditingAreaOffset.y,
										AppState->EditingAreaOffset.x + AppState->EditingAreaSize.x,
										AppState->EditingAreaOffset.y + AppState->EditingAreaSize.y))
	{
		SetPixelMapPixelColor(AppState, Input->MouseX, Input->MouseY, AppState->PixelColor);
	}

	for(int32 CustomColorIndex = 0;
		CustomColorIndex < ArrayCount(AppState->CustomColorButtons);
		++CustomColorIndex)
	{
		struct custom_color_button Button = *(AppState->CustomColorButtons + CustomColorIndex);
		if(ActionPerformedWithinRegion(Input->ButtonPrimary.EndedDown, Input->MouseX, Input->MouseY,
									   Button.Position.x, Button.Position.y,
									   Button.Position.x + Button.Dimensions.x,
									   Button.Position.y + Button.Dimensions.y))
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

	DrawRectangle(Buffer, AppState->ColorPickerButton.x, AppState->ColorPickerButton.y,
				  AppState->ColorPickerButton.z, AppState->ColorPickerButton.w,
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
