/*
 *   Support for Foxus printer
 */

#include <cups/cups.h>
#include <cups/raster.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <math.h>
#include <stdio.h>
#include <stdarg.h>
/*
 * Model number constants...
 */

#define LINES_BATCH_SIZE 24
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define DEBUG_MSG_FILENAME "/tmp/cups_debug_foxus"
#define DEBUG_BIN_FILENAME "/tmp/cups_debug_foxus.bin"


FILE* dbg;
FILE* dbgOut;

void writeInfo(char* format, ...){
	va_list args;

	va_start(args, format);
	vfprintf(dbg, format, args);
	vfprintf(stderr, format, args);
	va_end(args);
}

void writeData(int argc, ...)
{
	va_list valist;
	va_start(valist, argc);

	for (int i = 0; i < argc; i++)
	{
		unsigned char c = va_arg(valist, int);
		fputc(c, dbgOut);
		fputc(c, stdout);
	}

	va_end(valist);
}

void writeData2(unsigned char* data, int length)
{
	fwrite(data, sizeof(unsigned char), length, dbgOut);
	fwrite(data, sizeof(unsigned char), length, stdout);
	fflush(stdout);
}

unsigned char lo (int val)
{
	return val & 0xFF;
}

unsigned char hi (int val)
{
	return lo (val >> 8);
}

void initPrinter()
{
	writeData(2, 0x1b, 0x40);
	//writeData(3, 0x1b, 0x33, 24);
}

int isBlankLine(char* line, int lineLength)
{
	if(line)
	{
		for(int i = 0; i < lineLength; i++)
		{
			if (line[i] != 0)
				return 0;
		}
	}

	return 1;
}

void DebugPageInfo(cups_page_header2_t *header)
{
	writeInfo("DEBUG: StartPage...\n");
	writeInfo("DEBUG: MediaClass = \"%s\"\n", header->MediaClass);
	writeInfo("DEBUG: MediaColor = \"%s\"\n", header->MediaColor);
	writeInfo("DEBUG: MediaType = \"%s\"\n", header->MediaType);
	writeInfo("DEBUG: OutputType = \"%s\"\n", header->OutputType);
	writeInfo("DEBUG: AdvanceDistance = %d\n", header->AdvanceDistance);
	writeInfo("DEBUG: AdvanceMedia = %d\n", header->AdvanceMedia);
	writeInfo("DEBUG: Collate = %d\n", header->Collate);
	writeInfo("DEBUG: CutMedia = %d\n", header->CutMedia);
	writeInfo("DEBUG: Duplex = %d\n", header->Duplex);
	writeInfo("DEBUG: HWResolution = [ %d %d ]\n", header->HWResolution[0], header->HWResolution[1]);
	writeInfo("DEBUG: ImagingBoundingBox = [ %d %d %d %d ]\n", header->ImagingBoundingBox[0], header->ImagingBoundingBox[1], header->ImagingBoundingBox[2], header->ImagingBoundingBox[3]);
	writeInfo("DEBUG: InsertSheet = %d\n", header->InsertSheet);
	writeInfo("DEBUG: Jog = %d\n", header->Jog);
	writeInfo("DEBUG: LeadingEdge = %d\n", header->LeadingEdge);
	writeInfo("DEBUG: Margins = [ %d %d ]\n", header->Margins[0], header->Margins[1]);
	writeInfo("DEBUG: ManualFeed = %d\n", header->ManualFeed);
	writeInfo("DEBUG: MediaPosition = %d\n", header->MediaPosition);
	writeInfo("DEBUG: MediaWeight = %d\n", header->MediaWeight);
	writeInfo("DEBUG: MirrorPrint = %d\n", header->MirrorPrint);
	writeInfo("DEBUG: NegativePrint = %d\n", header->NegativePrint);
	writeInfo("DEBUG: NumCopies = %d\n", header->NumCopies);
	writeInfo("DEBUG: Orientation = %d\n", header->Orientation);
	writeInfo("DEBUG: OutputFaceUp = %d\n", header->OutputFaceUp);
	writeInfo("DEBUG: cupsPageSize = [ %f %f ]\n", header->cupsPageSize[0], header->cupsPageSize[1]);
	writeInfo("DEBUG: Separations = %d\n", header->Separations);
	writeInfo("DEBUG: TraySwitch = %d\n", header->TraySwitch);
	writeInfo("DEBUG: Tumble = %d\n", header->Tumble);
	writeInfo("DEBUG: cupsWidth = %d\n", header->cupsWidth);
	writeInfo("DEBUG: cupsHeight = %d\n", header->cupsHeight);
	writeInfo("DEBUG: cupsMediaType = %d\n", header->cupsMediaType);
	writeInfo("DEBUG: cupsBitsPerColor = %d\n", header->cupsBitsPerColor);
	writeInfo("DEBUG: cupsBitsPerPixel = %d\n", header->cupsBitsPerPixel);
	writeInfo("DEBUG: cupsBytesPerLine = %d\n", header->cupsBytesPerLine);
	writeInfo("DEBUG: cupsColorOrder = %d\n", header->cupsColorOrder);
	writeInfo("DEBUG: cupsColorSpace = %d\n", header->cupsColorSpace);
	writeInfo("DEBUG: cupsCompression = %d\n", header->cupsCompression);
}

void PrintLines(unsigned char* data, int width, int height)
{
	writeData(8, 0x1d, 0x76, 0x30, 0x00, lo(width), hi(width), lo(height), hi(height)); //raster start
	writeData2(data, width * height); //raster data
	//writeData(3, 0x1b, 0x4a, height);
}

void SkipLines(int count)
{
	count = count * 2;
	for(;count > 0; count -= 0xff)
	{
		writeData(3, 0x1b, 0x4a, count > 0xff ? 0xff : count);
	}
}

int isLineEmpty(unsigned char* data, int width)
{
	if(data){
		while(--width >= 0){
			if(data[width])
				return 0;
		}
	}

	return 1;
}

int emptyLinesCount(unsigned char* data, int width, int height)
{
	int count;
	for(count = 0; count < height; count++){
		if(!isLineEmpty(data + width * count, width))
			return count;
	}

	return count;
}

int fullLinesCount(unsigned char * data, int width, int height)
{
	int count;
	for(count = 0; count < height; count++){
		if(isLineEmpty(data + width * count, width))
			return count;
	}

	return count;
}

void cutPaper()
{
	SkipLines(24 * 6); //cutter is above printer. We have to scroll to cut at the right place!
	writeData(3, 0x1d, 0x56, 1);
}

int main(int argc, char *argv[])
{
	int                  fd;          /* File descriptor */
	cups_raster_t       *ras;         /* Raster stream for printing */
	cups_page_header2_t  header;	  /* Page header from file */

	dbg = fopen(DEBUG_MSG_FILENAME, "w");
	dbgOut = fopen(DEBUG_BIN_FILENAME, "w");

	if(!dbg)
	{
		fprintf(stderr, "Failed to open file '%s' for writing.\n", DEBUG_MSG_FILENAME);
		return 2;
	}

	if(!dbgOut)
	{
		fprintf(stderr, "Failed to open file '%s' for writing.\n", DEBUG_BIN_FILENAME);
		return 2;
	}

	/*
	 * Make sure status messages are not buffered...
	 */
	setbuf(stderr, NULL);
	setbuf(dbg, NULL);
	setbuf(dbgOut, NULL);

	/*
	 * Check command-line...
	 */
	if (argc < 6 || argc > 7)
	{
		writeInfo("ERROR: rastertofoxus job-id user title copies options [file]\n");
		return 1;
	}
	/*
	 * Open the page stream...
	 */
	if (argc == 7)
	{
		if ((fd = open(argv[6], O_RDONLY)) == -1)
		{
			perror("ERROR: Unable to open raster file - ");
			sleep(1);
			return 1;
		}
	}
	else
	{
		fd = 0;
	}

	ras = cupsRasterOpen(fd, CUPS_RASTER_READ);
	if(!ras)
	{
		writeInfo("Failed to open raster!\n");
		return 1;
	}

	int doACut = 0;
	unsigned char *buffer = 0;
	while (cupsRasterReadHeader2(ras, &header))
	{
		DebugPageInfo(&header);

		if(header.cupsBytesPerLine != 72){
			writeInfo("ERROR: rastertofoxus supports only 72 bits/line, but cups is sending %d bits/line.\n", header.cupsBytesPerLine);
			continue;
		}

		if(buffer == 0)
			buffer = malloc(header.cupsBytesPerLine * LINES_BATCH_SIZE);

		int linesRead = 0;
		int emptyLinesTotal = 0;
		do
		{
			memset(buffer, 0, header.cupsBytesPerLine * LINES_BATCH_SIZE);
			linesRead = cupsRasterReadPixels(ras, buffer, header.cupsBytesPerLine * LINES_BATCH_SIZE) / header.cupsBytesPerLine;

			if(linesRead > 0)
			{
				int emptyLines = emptyLinesCount(buffer, header.cupsBytesPerLine, linesRead);
				int fullLines = fullLinesCount(buffer + emptyLines * header.cupsBytesPerLine, header.cupsBytesPerLine, linesRead - emptyLines);

				emptyLinesTotal += emptyLines;

				if(fullLines){
					SkipLines(emptyLinesTotal);
					emptyLinesTotal = 0;

					PrintLines(buffer + emptyLines * header.cupsBytesPerLine, header.cupsBytesPerLine, linesRead - emptyLines);
				}

//				PrintLines(buffer, header.cupsBytesPerLine, linesRead);
			}

		}
		while(linesRead > 0);

		SkipLines(MIN(24, emptyLinesTotal));

		switch(header.CutMedia){
			case CUPS_CUT_FILE:
			case CUPS_CUT_JOB:
			case CUPS_CUT_SET:
				doACut = 1;
				break;
			case CUPS_CUT_PAGE:
				cutPaper();
				break;
		}
	}

	cupsRasterClose(ras);
	if (fd != 0)
		close(fd);

	fclose(dbg);
	fclose(dbgOut);

	if (doACut)
		cutPaper();

	return 0;
}

