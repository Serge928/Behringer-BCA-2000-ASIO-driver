/*!
#
# Win-Widget. Windows related software for Audio-Widget/SDR-Widget (http://code.google.com/p/sdr-widget/)
# Copyright (C) 2012 Nikolay Kovbasa
#
# Permission to copy, use, modify, sell and distribute this software 
# is granted provided this copyright notice appears in all copies. 
# This software is provided "as is" without express or implied
# warranty, and with no claim as to its suitability for any purpose.
#
#----------------------------------------------------------------------------
# Contact: nikkov@gmail.com
#----------------------------------------------------------------------------
*/

#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#include <conio.h>
#include <math.h>

#include "USBAudioDevice.h"
#include "tlist.h"

#ifdef _ENABLE_TRACE

void debugPrintf(const char *szFormat, ...)
{
    char str[4096];
    va_list argptr;
    va_start(argptr, szFormat);
    vsprintf_s(str, szFormat, argptr);
    va_end(argptr);

    //printf(str);
    OutputDebugString(str);
}
#endif


struct AudioSample4
{
	int left;
	int right;
};



struct ThreeByteSample
{
	UCHAR sample[3];
	ThreeByteSample(int val = 0)
	{
		UCHAR *ptrVal = (UCHAR *)&val;
		sample[0] = *(ptrVal);
		sample[1] = *(ptrVal+1);
		sample[2] = *(ptrVal+2);
	}

	operator int()const
	{
		return (sample[0] << 0) + (sample[1] << 8) + (sample[2] << 16);
	}
};

struct AudioSample3
{
	ThreeByteSample left;
	ThreeByteSample right;
};

AudioSample4 dummybuffer[48];
DWORD globalReadBuffer = 0;
DWORD globalPacketCounter = 0;

void FillBuffer(bool zoom)
{
	memset(dummybuffer, 0, 48*sizeof(AudioSample4));
	for(int i = 0; i < 48; i++)
	{
		//dummybuffer[i].left = (int)(0x1FFFFF*sin(2.0*3.14159265358979323846*(double)i/48.));
		dummybuffer[i].left = (int)(0x1FFFFF*sin(2.0*3.14159265358979323846*(double)i/48.));
		dummybuffer[i].right = dummybuffer[i].left;

		if(zoom)
		{
			dummybuffer[i].left = dummybuffer[i].left << 8;
			dummybuffer[i].right = dummybuffer[i].right << 8;
		}
	}
}

void FillData4(void* context, UCHAR *buffer, int& len)
{
	static int globalCount = 0;
	UCHAR *sampleBuff = buffer;
	int sampleLength = len / sizeof(int);

	for(int i = 0; i < sampleLength; i += 8)
	{
		for (int j = 0; j < 2; j++)
		{
			*((UINT *)(sampleBuff)) = dummybuffer[globalReadBuffer].left;
			sampleBuff += 4;
			//*(sampleBuff++) = (dummybuffer[globalReadBuffer].left&0xff;
			//*(sampleBuff++) = (dummybuffer[globalReadBuffer].left >> 8)&0xff;
			//*(sampleBuff++) = (dummybuffer[globalReadBuffer].left >> 16)&0xff;
			//*(sampleBuff++) = (dummybuffer[globalReadBuffer].left >> 24)&0xff;
		}
		for (int j = 2; j < 8; j++)
		{
			*(sampleBuff++) = 1;
			*(sampleBuff++) = 2;
			*(sampleBuff++) = 3;
			*(sampleBuff++) = 4;
		}
		globalReadBuffer++;
		if(globalReadBuffer >= 48) 
			globalReadBuffer = 0;

		globalCount++;
	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;
}

void FillData3(void* context, UCHAR *buffer, int& len)
{
	AudioSample3 *sampleBuff = (AudioSample3 *)buffer;
	int sampleLength = len / sizeof(AudioSample3);

	for(int i = 0; i < sampleLength; i++)
	{
		sampleBuff[i].left =  dummybuffer[globalReadBuffer].left;
		sampleBuff[i].right = dummybuffer[globalReadBuffer].right;
		globalReadBuffer++;
		if(globalReadBuffer >= 48) 
			globalReadBuffer = 0;
	}
	globalPacketCounter++;
	if(globalPacketCounter > 0xFF)
		globalPacketCounter = 0;
}


// Parse a wav file header and return file parameters
// See https://ccrma.stanford.edu/courses/422/projects/WaveFormat/
int parsewavheader (FILE* wavfile, int* NumChannels, int* SampleRate, int* BytesPerSample, long* NumSamples) {
#define WAVHEADER_L 44				// Length of standard wav file header
#define IG 0xAB						// A byte which doesn't occur elsewhere in header

	unsigned char wavheader[WAVHEADER_L] = {	// Required header contents
		'R', 'I', 'F', 'F',			// Chunk ID
		IG, IG, IG, IG,				// ChunkSize, 36 + bytes of audio data
		'W', 'A', 'V', 'E',			// Format
		'f', 'm', 't', 0x20,		// Subchunk1ID
		0x10, 0x00, 0x00, 0x00,		// Subchunk1Size, 16 for PCM
		0x01, 0x00,					// AudioFormat, 1 for PCM
		IG, IG,						// NumChannels, extract number, only accept 2
		IG, IG, IG, IG,				// SampleRate
		IG, IG, IG, IG,				// ByteRate = SampleRate * NumChannels * BitsPerSample / 8
		IG, IG,						// BlockAlign = NumChannels * BitsPerSample / 8
		IG, IG,						// BitsPerSample
		'd', 'a', 't', 'a',			// Subchunk2ID
		IG, IG, IG, IG};			// SubChunk2Size

	unsigned char readwavheader[WAVHEADER_L];

	rewind (wavfile);				// Make sure to read header from beginning of file!

	// Extract wav file header and do initial error checking
	int n = fread (readwavheader, 1, WAVHEADER_L, wavfile); // Try to read 44 header bytes

	if (n != WAVHEADER_L) {
		printf ("ERROR: Could not read %d bytes wav file header\n", WAVHEADER_L);
		return 0;
	}

	// Compare wavfile to default header, ignoring all bytes set to IG
	n=0;
	while ( ( (readwavheader[n] == wavheader[n]) || (wavheader[n] == IG) ) && (n < WAVHEADER_L) ) n++;

	if (n != WAVHEADER_L) {
		printf ("ERROR: wav file header error at position %d\n", n);
		return 0;
	}

	// Extract wav file parameters
	int ChunkSize = (readwavheader[4]) + (readwavheader[5]<<8) + (readwavheader[6]<<16) + (readwavheader[7]<<24);
	*NumChannels = (readwavheader[22]) + (readwavheader[23]<<8);
	*SampleRate = (readwavheader[24]) + (readwavheader[25]<<8) + (readwavheader[26]<<16) + (readwavheader[27]<<24);
	int ByteRate = (readwavheader[28]) + (readwavheader[29]<<8) + (readwavheader[30]<<16) + (readwavheader[31]<<24);
	int BlockAlign = (readwavheader[32]) + (readwavheader[33]<<8);
	int BitsPerSample = (readwavheader[34]) + (readwavheader[35]<<8);
	*BytesPerSample = BitsPerSample >> 3;
	int SubChunk2Size = (readwavheader[40]) + (readwavheader[41]<<8) + (readwavheader[42]<<16) + (readwavheader[43]<<24);
	*NumSamples = SubChunk2Size / *BytesPerSample / *NumChannels;
	double Duration = *NumSamples; Duration /= *SampleRate;

	// Print parameters
	printf ("ChunkSize = %d\n", ChunkSize);
	printf ("NumChannels = %d\n", *NumChannels);
	printf ("SampleRate = %d\n", *SampleRate);
	printf ("ByteRate = %d\n", ByteRate);
	printf ("BlockAlign = %d\n", BlockAlign);
	printf ("BytesPerSample = %d\n", *BytesPerSample); // Bytes per MONO sample, *2 for stereo
	printf ("SubChunk2Size = %d\n", SubChunk2Size);
	printf ("NumSamples = %d\n", *NumSamples);
	printf ("Duration = %4.3fs\n", Duration);

	// Full error checking
	n = 1; // Assuming a clean return. But report all found errors before returning

	if (SubChunk2Size + 36 != ChunkSize) {
		printf ("ERROR: SubChunk2Size, ChunkSize mismatch %d+36 != %d\n", SubChunk2Size, ChunkSize);
		n = 0;
	}

	if (*NumChannels != 2) {
		printf ("ERROR: Only 2-channel wav is accepted, not the detected %d-channel.\n", *NumChannels);
		n = 0;
	}

	if ( (*SampleRate != 44100) && (*SampleRate != 48000) && 
		 (*SampleRate != 88200) && (*SampleRate != 96000) &&
		 (*SampleRate != 176400) && (*SampleRate != 192000) ) {
		printf ("ERROR: Only 44.1/48/88.2/96/176.4/192ksps accepted, not the detected %d.\n", *SampleRate);
		n = 0 ;
	}

	if (ByteRate != *SampleRate * *NumChannels * *BytesPerSample) {
		printf ("ERROR: Mismatch between ByteRate, SampleRate, NumChannels, BitsPerSample\n");
		n = 0;
	}

	if (BlockAlign != *NumChannels * *BytesPerSample) {
		printf ("ERROR: Mismatch between BlockAlign, NumChannels, BitsPerSample\n");
		n = 0;
	}

	if ( (*BytesPerSample != 2) && (*BytesPerSample != 3) && (*BytesPerSample != 4) ) {
		printf ("ERROR: Only 2/3/4 bytes per mono sample accepted, not the detected %d.\n", *BytesPerSample);
		n = 0;
	}

	return n;
}


// Fill 3L, 3R bytes from wavfile
long fillwavbuffer3 (AudioSample3* wavbuffer, FILE* wavfile, int* BytesPerSample, long* NumSamples) {
	unsigned char temp[8];	// Temporary variable for reading a stereo sample from the wavfile
	long readsamples = 0;

	if (*BytesPerSample == 2) {
		for (long n=0; n<*NumSamples; n++) {
			if (fread(temp, 1, 4, wavfile) == 4) { 		// 16 bits -> 24 bits stereo
				wavbuffer[n].left  = (temp[0]<<8) + (temp[1]<<16);
				wavbuffer[n].right = (temp[2]<<8) + (temp[3]<<16);
				readsamples++;
			}
			else {
				printf ("a\n");
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	else if (*BytesPerSample == 3) {
		for (long n=0; n<*NumSamples; n++) {
			if (fread(temp, 1, 6, wavfile) == 6) { 		// 24 bits -> 24 bits stereo. Can we do simple copy?
				wavbuffer[n].left  = (temp[0]) + (temp[1]<<8) + (temp[2]<<16);
				wavbuffer[n].right = (temp[3]) + (temp[4]<<8) + (temp[5]<<16);
				readsamples++;
			}
			else {
				printf ("b\n");
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	else if (*BytesPerSample == 4) {
		for (long n=0; n<*NumSamples; n++) {
			if (fread(temp, 1, 8, wavfile) == 8) {		// 32 bits -> 24 bits stereo. FIX: add dither!
				wavbuffer[n].left  = (temp[1]) + (temp[2]<<8) + (temp[3]<<16);
				wavbuffer[n].right = (temp[5]) + (temp[6]<<8) + (temp[7]<<16);
				readsamples++;
			}
			else {
				printf ("c\n");
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	return readsamples;
}


// Fill 3L, 3R bytes from wavfile
long fillwavbuffer4 (AudioSample4* wavbuffer, FILE* wavfile, int* BytesPerSample, long* NumSamples) {
	unsigned char temp[8];	// Temporary variable for reading a stereo sample from the wavfile
	long readsamples = 0;
	int m = 0;

	if (*BytesPerSample == 2) {
		for (long n=0; n<*NumSamples; n++) {
			m = fread(temp, 1, 4, wavfile);
			if (m == 4) {		// 16 bits -> 32 bits stereo
				wavbuffer[n].left  = (temp[0]<<16) + (temp[1]<<24);
				wavbuffer[n].right = (temp[2]<<16) + (temp[3]<<24);
				readsamples++;
			}
			else {
				printf ("d %d, %d, %d", n, readsamples, m);
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	else if (*BytesPerSample == 3) {
		for (long n=0; n<*NumSamples; n++) {
			if (fread(temp, 1, 6, wavfile) == 6) {		// 24 bits -> 32 bits stereo.
				wavbuffer[n].left  = (temp[0]<<8) + (temp[1]<<16) + (temp[2]<<24);
				wavbuffer[n].right = (temp[3]<<8) + (temp[4]<<16) + (temp[5]<<24);
				readsamples++;
			}
			else {
				printf ("e\n");
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	else if (*BytesPerSample == 4) {
		for (long n=0; n<*NumSamples; n++) {
			if (fread(temp, 1, 8, wavfile) == 8) { 		// 32 bits -> 32 bits stereo.  Can we do simple copy?
				wavbuffer[n].left  = (temp[0]) + (temp[1]<<8) + (temp[2]<<16) + (temp[3]<<24);
				wavbuffer[n].right = (temp[4]) + (temp[5]<<8) + (temp[6]<<16) + (temp[7]<<24);
				readsamples++;
			}
			else {
				printf ("f\n");
				return readsamples;						// Cut the process short if read fails
			}
		}
	}

	return readsamples;
}


int main(int argc, char* argv[]) {

	int freq;
	int mode; // BSB 20121007 0:signal generator 1:wav file player

	if (argc == 1) {	// No arguments
		freq = 48000;	// Default sampling frequency
		mode = 0;		// Run as signal generator
	}

	else if (argc == 2) { // One argument, may it be a valid audio frequency?
		freq = atoi(argv[1]);
		if ( (freq == 44100) || (freq == 48000) || (freq == 88200) || (freq == 96000) || (freq == 176400) || (freq == 192000) )
			mode = 0;	// Run as signal generator
		else
			mode = 1;	// Run as wav file player
	}

	else				// >1 argument, run as wav file player
		mode = 1;		// Run as wav file player

	if (mode == 0) {	// signal generator
		printf("Running as signal generator.\n");

		// First check for valid playback device
		USBAudioDevice device(true);
		if (!device.InitDevice()) {
			printf ("ERROR: UAC2 Audio device not found\n");
//	offline		return -1;
		}
		
		if(device.GetDACSubslotSize() == 3) {
			FillBuffer(false);
			device.SetDACCallback(FillData3, NULL);
		}
		else if(device.GetDACSubslotSize() == 4) {
			FillBuffer(true);
			device.SetDACCallback(FillData4, NULL);
		}
		device.SetSampleRate(freq);
		device.Start();
		printf("Press any key to stop...\n");
		_getch();
		device.Stop();
	}

	else if (mode == 1) { // wav file player
		AudioSample3 * wavbuffer3; // Memory pointers for wav file data access, make global!
		AudioSample4 * wavbuffer4;

		int NumChannels = 0;
		int SampleRate = 0;
		int BytesPerSample = 0;
		long NumSamples = 0;
		long ReadSamples = 0;
		FILE * wavfile;
		int SubSlotSize = 0;

		printf("Running as wav file player.\n");

		// First check for valid playback device
		USBAudioDevice device(true);
		if (!device.InitDevice()) {
			printf ("ERROR: UAC2 Audio device not found\n");
//	offline		return -1;
		}

		// Open files
		for (int n=1; n<argc; n++) {
			// Then do various checks on wav file(s) to play
			wavfile = fopen(argv[n],"rb");	// fclose_s is recommended..
			if (wavfile==NULL) {
				printf ("ERROR: File not found: %s\n",argv[n]);
				continue;
			}

			printf ("\nFound file: %s\n",argv[n]);
			if (!parsewavheader (wavfile, &NumChannels, &SampleRate, &BytesPerSample, &NumSamples))
				return -1;	// Error message reported in above function

//	offline		SubSlotSize = device.GetDACSubslotSize();
			SubSlotSize = 4;

			if(SubSlotSize == 3) {
				wavbuffer3 = new AudioSample3[NumSamples];

				ReadSamples = fillwavbuffer3 (wavbuffer3, wavfile, &BytesPerSample, &NumSamples);
				printf ("%d %d\n", ReadSamples, NumSamples);

				FillBuffer(false); // FIX: change
				device.SetDACCallback(FillData3, NULL); // FIX: change
			}
			else if(SubSlotSize == 4) {
				wavbuffer4 = new AudioSample4[NumSamples];

				ReadSamples = fillwavbuffer4 (wavbuffer4, wavfile, &BytesPerSample, &NumSamples);
				printf ("%d %d\n", ReadSamples, NumSamples);

				FillBuffer(true); // FIX: change
				device.SetDACCallback(FillData4, NULL); // FIX: change
			}

			if (ReadSamples == NumSamples) {
				printf ("Wav file read into memory\n");
				ReadSamples = 1;

				// Now play the darn thing :-) 
				device.SetSampleRate(SampleRate);
				device.Start();
				fclose (wavfile);
				printf("Press any key to stop...\n"); // FIX: also continue at end of file
				device.Stop();
				while (!_kbhit()) {}				// FIX: add file finished test, replaces _getch()
			}
			else {
				printf ("ERROR: Wav file not read into memory\n");
				ReadSamples = 0;
			}

			if(SubSlotSize == 3)
				delete [] wavbuffer3;
			else if(SubSlotSize == 3)
				delete [] wavbuffer4;
		}
	}

	_CrtDumpMemoryLeaks();
	return 0;
}

