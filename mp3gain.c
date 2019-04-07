#define MP3GAIN_VERSION "1.0.4"

/*
 *  mp3gain.c - analyzes mp3 files, determines the perceived volume, 
 *              and adjusts the volume of the mp3 accordingly
 *
 *  Copyright (C) 2002 Glen Sawyer
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  coding by Glen Sawyer (glensawyer@hotmail.com) 442 N 700 E, Provo, UT 84606 USA
 *    -- go ahead and laugh at me for my lousy coding skillz, I can take it :)
 *       Just do me a favor and let me know how you improve the code.
 *       Thanks.
 */


/*
 *  General warning: I coded this in several stages over the course of several
 *  months. During that time, I changed my mind about my coding style and
 *  naming conventions many, many times. So there's not a lot of consistency
 *  in the code. Sorry about that. I may clean it up some day, but by the time
 *  I would be getting around to it, I'm sure that the more clever programmers
 *  out there will have come up with superior versions already...
 *
 *  So have fun dissecting.
 */

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <string.h>

/* Yes, there are some Windoze-specific bits of code in here,
 * but nothing that should be too hard to replace with *nix
 * code
 */
#include <windows.h>

/* I tweaked the mpglib library just a bit to spit out the raw
 * decoded double values, instead of rounding them to 16-bit integers.
 * Hence the "mpglibDBL" directory
 */
#include "mpglibDBL\interface.h"
#include "gain_analysis.h"


#define HEADERSIZE 4

#define CRC16_POLYNOMIAL        0x8005

#define BUFFERSIZE 3000000
#define BUFFERMARGIN 1250
#define WRITEBUFFERSIZE 100000

typedef struct {
	unsigned long fileposition;
	unsigned char val[2];
} wbuffer;

/* Yes, yes, I know I should do something about these globals */

wbuffer writebuffer[WRITEBUFFERSIZE];

unsigned long writebuffercnt;

unsigned char buffer[BUFFERSIZE];

int writeself = 0;
int QuietMode = 0;
int UsingTemp = 0;
int NowWriting = 0;
double lastfreq = -1.0;

int whichChannel = 0;
int BadLayer = 0;
int LayerSet = 0;

long inbuffer;
unsigned long bitidx;
unsigned char *wrdpntr;
unsigned char *curframe;

char *curfilename;

FILE *inf;

HANDLE outf;

short int saveTime;

unsigned long filepos;

static const double bitrate[4][16] = {
	{ 1,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, 1 },
	{ 1,  1,  1,  1,  1,  1,  1,  1,   1,   1,   1,   1,   1,   1,   1, 1 },
	{ 1,  8, 16, 24, 32, 40, 48, 56,  64,  80,  96, 112, 128, 144, 160, 1 },
	{ 1, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 1 }
};
static const double frequency[4][4] = {
	{ 11.025, 12,  8,  1 },
	{      1,  1,  1,  1 },
	{  22.05, 24, 16,  1 },
	{   44.1, 48, 32,  1 }
};


/* instead of writing each byte change, I buffer them up */
static
void flushWriteBuff() {
	unsigned long i;
	for (i = 0; i < writebuffercnt; i++) {
		fseek(inf,writebuffer[i].fileposition,SEEK_SET);
		fwrite(writebuffer[i].val,1,2,inf);
	}
	writebuffercnt = 0;
};



static
void addWriteBuff(unsigned long pos, unsigned char *vals) {
	if (writebuffercnt >= WRITEBUFFERSIZE) {
		flushWriteBuff();
		fseek(inf,filepos,SEEK_SET);
	}
	writebuffer[writebuffercnt].fileposition = pos;
	writebuffer[writebuffercnt].val[0] = *vals;
	writebuffer[writebuffercnt].val[1] = vals[1];
	writebuffercnt++;
	
};


/* fill the mp3 buffer */
static
unsigned long fillBuffer(long savelastbytes) {
	unsigned long i;
	unsigned long skip;
	unsigned long rbval;

	skip = 0;
	if (savelastbytes < 0) {
		skip = -savelastbytes;
		savelastbytes = 0;
	}

	if (UsingTemp && NowWriting)
		WriteFile(outf,buffer,inbuffer-savelastbytes,&i,NULL);
		/*fwrite(buffer,1,inbuffer-savelastbytes,outf);*/

	if (savelastbytes != 0) /* save some of the bytes at the end of the buffer */
		memmove((void*)buffer,(const void*)(buffer+inbuffer-savelastbytes),savelastbytes);
	
	if (skip != 0) { /* skip some bytes from the input file */
		i = fread(buffer,1,skip,inf);
		if (UsingTemp && NowWriting)
			WriteFile(outf,buffer,skip,&rbval,NULL);
			/*fwrite(buffer,1,skip,outf);*/
		filepos = filepos + i;
	}
	i = fread(buffer+savelastbytes,1,BUFFERSIZE-savelastbytes,inf);

	filepos = filepos + i;
	inbuffer = i + savelastbytes;
	return i;
}


static const unsigned char maskLeft8bits[8] = {
	0x00, 0x80, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC, 0xFE };

static const unsigned char maskRight8bits[8] = {
	0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01 };

static
void set8Bits(unsigned short val) {

	val <<= (8 - bitidx);
	wrdpntr[0] &= maskLeft8bits[bitidx];
	wrdpntr[0] |= (val  >> 8);
	wrdpntr[1] &= maskRight8bits[bitidx];
	wrdpntr[1] |= (val  & 0xFF);
	
	if (!UsingTemp) 
		addWriteBuff(filepos-(inbuffer-(wrdpntr-buffer)),wrdpntr);
}



static
void skipBits(int nbits) {

	bitidx += nbits;
	wrdpntr += (bitidx >> 3);
	bitidx &= 7;

	return;
}



static
unsigned char peek8Bits() {
	unsigned short rval;

	rval = wrdpntr[0];
	rval <<= 8;
	rval |= wrdpntr[1];
	rval >>= (8 - bitidx);

	return (rval & 0xFF);

}



static
unsigned long skipID3v2() {
/*
 *  An ID3v2 tag can be detected with the following pattern:
 *    $49 44 33 yy yy xx zz zz zz zz
 *  Where yy is less than $FF, xx is the 'flags' byte and zz is less than
 *  $80.
 */
	unsigned long ok;
	unsigned long ID3Size;

	ok = 1;

	if (wrdpntr[0] == 'I' && wrdpntr[1] == 'D' && wrdpntr[2] == '3' 
		&& wrdpntr[3] < 0xFF && wrdpntr[4] < 0xFF) {

		ID3Size = (long)(wrdpntr[9]) | ((long)(wrdpntr[8]) << 7) |
			((long)(wrdpntr[7]) << 14) | ((long)(wrdpntr[6]) << 21);

		ID3Size += 10;

		wrdpntr = wrdpntr + ID3Size;

		if ((wrdpntr+HEADERSIZE-buffer) > inbuffer) {
			ok = fillBuffer(inbuffer-(wrdpntr-buffer));
			wrdpntr = buffer;
		}
	}

	return ok;
}



static
unsigned long frameSearch() {
	unsigned long ok;
	int done;

	done = 0;
	ok = 1;

	if ((wrdpntr+HEADERSIZE-buffer) > inbuffer) {
		ok = fillBuffer(inbuffer-(wrdpntr-buffer));
		wrdpntr = buffer;
		if (!ok) done = 1;
	}

	while (!done) {
		
		done = 1;

		if ((wrdpntr[0] & 0xFF) != 0xFF)
			done = 0;       /* first 8 bits must be '1' */
		else if ((wrdpntr[1] & 0xE0) != 0xE0)
			done = 0;       /* next 3 bits are also '1' */
		else if ((wrdpntr[1] & 0x18) == 0x08)
			done = 0;       /* invalid MPEG version */
		else if ((wrdpntr[1] & 0x06) != 0x02) { /* not Layer III */
			if (!LayerSet) {
				switch (wrdpntr[1] & 0x06) {
					case 0x06:
						BadLayer = !0;
						fprintf(stdout,"%s is an MPEG Layer I file, not a layer III file\n", curfilename);
						return 0;
					case 0x04:
						BadLayer = !0;
						fprintf(stdout,"%s is an MPEG Layer II file, not a layer III file\n", curfilename);
						return 0;
				}
			}
			done = 0; /* probably just corrupt data, keep trying */
		}
		else if ((wrdpntr[2] & 0xF0) == 0xF0)
			done = 0;       /* bad bitrate */
		else if ((wrdpntr[2] & 0x0C) == 0x0C)
			done = 0;       /* bad sample frequency */

		if (!done) wrdpntr++;

		if ((wrdpntr+HEADERSIZE-buffer) > inbuffer) {
			ok = fillBuffer(inbuffer-(wrdpntr-buffer));
			wrdpntr = buffer;
			if (!ok) done = 1;
		}
	}

	if (ok) {
		if (inbuffer - (wrdpntr-buffer) < BUFFERMARGIN) {
			fillBuffer(inbuffer-(wrdpntr-buffer));
			wrdpntr = buffer;
		}
		bitidx = 0;
		curframe = wrdpntr;
	}
	return ok;
}



static
int crcUpdate(int value, int crc)
{
    int i;
    value <<= 8;
    for (i = 0; i < 8; i++) {
		value <<= 1;
		crc <<= 1;

		if (((crc ^ value) & 0x10000))
			crc ^= CRC16_POLYNOMIAL;
	}
    return crc;
}



static
void crcWriteHeader(int headerlength, char *header)
{
    int crc = 0xffff; /* (jo) init crc16 for error_protection */
    int i;

    crc = crcUpdate(((unsigned char*)header)[2], crc);
    crc = crcUpdate(((unsigned char*)header)[3], crc);
    for (i = 6; i < headerlength; i++) {
	crc = crcUpdate(((unsigned char*)header)[i], crc);
    }

    header[4] = crc >> 8;
    header[5] = crc & 255;
}



static
void changeGain(char *filename, int leftgainchange, int rightgainchange) {
  unsigned long ok;
  int mode;
  int crcflag;
  unsigned char *Xingcheck;
  unsigned long frame;
  int nchan;
  int ch;
  int gr;
  unsigned char gain;
  int bitridx;
  int freqidx;
  int pad;
  long bitsinframe;
  long bytesinframe;
  int sideinfo_len;
  int mpegver;
  long gFilesize = 0;
  char *outfilename;
  HANDLE statf;
  short int timeOK;
  unsigned char gainchange[2];
  int singlechannel;

  /* Windows stuff to preserve file datetime stamp */
  FILETIME create_time, access_time, write_time;

  BadLayer = 0;
  LayerSet = 0;

  NowWriting = !0;
  timeOK = 0;

  if ((leftgainchange == 0) && (rightgainchange == 0))
	  return;

  gainchange[0] = leftgainchange;
  gainchange[1] = rightgainchange;
  singlechannel = !(leftgainchange == rightgainchange);
	  
	/* Get File size and datetime stamp */
	statf = CreateFile((LPCTSTR)filename,
						GENERIC_READ,
						FILE_SHARE_READ,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL);
	if (statf != INVALID_HANDLE_VALUE) {
		if (saveTime) {
			if (GetFileTime(statf,&create_time,&access_time,&write_time))
				timeOK = !0;
		}
		gFilesize = GetFileSize(statf,NULL);

		CloseHandle(statf);
	}

  if (UsingTemp) {
	  fflush(stderr);
	  fflush(stdout);
	  outfilename = malloc(strlen(filename)+4);
	  strcpy(outfilename,filename);
	  strcat(outfilename,".tmp");

      inf = fopen(filename,"rb");

	  if (inf != NULL) {
		outf = CreateFile((LPCTSTR)outfilename,
							GENERIC_WRITE,
							0,
							NULL,
							CREATE_ALWAYS,
							FILE_ATTRIBUTE_NORMAL,
							NULL);

		
		if (outf == INVALID_HANDLE_VALUE) {
			fclose(inf);
			fprintf(stdout, "\nCan't open %s for temp writing\n",outfilename);
			return;
		} 
 
	  }
  }
  else {
      inf = fopen(filename,"r+b");
  }

  if (inf == NULL) {
	  if (UsingTemp)
		  CloseHandle(outf);
		  /*fclose(outf);*/
	  fprintf(stdout, "\nCan't open %s for reading\n",filename);
	  return;
  }
  else {
	writebuffercnt = 0;
	inbuffer = 0;
	filepos = 0;
	bitidx = 0;
	ok = fillBuffer(0);
	if (ok) {

		wrdpntr = buffer;

		ok = skipID3v2();

		ok = frameSearch();
		if (!ok) {
			if (!BadLayer)
				fprintf(stdout,"Can't find any valid MP3 frames in file %s\n\n",filename);
		}
		else {
			LayerSet = 1; /* We've found at least one valid layer 3 frame.
						   * Assume any later layer 1 or 2 frames are just
						   * bitstream corruption
						   */
			mode = (curframe[3] >> 6) & 3;

			if (curframe[1] & 0x08 == 0x08) /* MPEG 1 */
				sideinfo_len = (mode == 3) ? 4 + 17 : 4 + 32;
			else                /* MPEG 2 */
				sideinfo_len = (mode == 3) ? 4 + 9 : 4 + 17;

			if (!(curframe[1] & 0x01))
				sideinfo_len += 2;

			Xingcheck = curframe + sideinfo_len;

			if (Xingcheck[0] == 'X' && Xingcheck[1] == 'i' && Xingcheck[2] == 'n' && Xingcheck[3] == 'g') {
				bitridx = (curframe[2] >> 4) & 0x0F;
				if (bitridx == 0) {
					fprintf(stdout, "%s is free format (not currently supported)\n",filename);
					ok = 0;
				}
				else {
					mpegver = (curframe[1] >> 3) & 0x03;
					freqidx = (curframe[2] >> 2) & 0x03;
					pad = (curframe[2] >> 1) & 0x01;
					if (mpegver == 3)
						bitsinframe = (int)(floor((1152.0*bitrate[mpegver][bitridx])/frequency[mpegver][freqidx])) + pad*8;
					else
						bitsinframe = (int)(floor((576.0*bitrate[mpegver][bitridx])/frequency[mpegver][freqidx])) + pad*8;

					bytesinframe = (int)(floor(((double)(bitsinframe)) / 8.0));

					wrdpntr = curframe + bytesinframe;

					ok = frameSearch();
				}
			}
			
			frame = 1;
		} /* if (!ok) else */
		
		while (ok) {
			bitridx = (curframe[2] >> 4) & 0x0F;
			if (singlechannel) {
				if ((curframe[3] >> 6) & 0x01) { /* if mode is NOT stereo or dual channel */
					fprintf(stdout,"%s: Can't adjust single channel for mono or joint stereo",filename);
					ok = 0;
				}
			}
			else if (bitridx == 0) {
				fprintf(stdout,"%s is free format (not currently supported)\n",filename);
				ok = 0;
			}
			else {
				mpegver = (curframe[1] >> 3) & 0x03;
				crcflag = curframe[1] & 0x01;
				freqidx = (curframe[2] >> 2) & 0x03;
				pad = (curframe[2] >> 1) & 0x01;
				if (mpegver == 3) 
					bitsinframe = (int)(floor((1152.0*bitrate[mpegver][bitridx])/frequency[mpegver][freqidx])) + pad*8;
				else
					bitsinframe = (int)(floor((576.0*bitrate[mpegver][bitridx])/frequency[mpegver][freqidx])) + pad*8;

				bytesinframe = (int)(floor(((double)(bitsinframe)) / 8.0));
				mode = (curframe[3] >> 6) & 0x03;
				nchan = (mode == 3) ? 1 : 2;

				if (!crcflag) /* we DO have a crc field */
					wrdpntr = curframe + 6; /* 4-byte header, 2-byte CRC */
				else
					wrdpntr = curframe + 4; /* 4-byte header */

				bitidx = 0;

				if (mpegver == 3) { /* 9 bit main_data_begin */
					wrdpntr++;
					bitidx = 1;

					if (mode == 3)
						skipBits(5); /* private bits */
					else
						skipBits(3); /* private bits */

					skipBits(nchan*4); /* scfsi[ch][band] */
					for (gr = 0; gr < 2; gr++)
						for (ch = 0; ch < nchan; ch++) {
							skipBits(21);
							gain = peek8Bits();
							gain += gainchange[ch];
							set8Bits(gain);
							skipBits(38);
						}
						if (!crcflag) {
							if (nchan == 1)
								crcWriteHeader(23,(char*)curframe);
							else
								crcWriteHeader(38,(char*)curframe);
							/* WRITETOFILE */
							if (!UsingTemp) 
								addWriteBuff(filepos-(inbuffer-(curframe+4-buffer)),curframe+4);
						}
				}
				else { /* mpegver != 3 */
					wrdpntr++; /* 8 bit main_data_begin */

					if (mode == 3)
						skipBits(1);
					else
						skipBits(2);

					/* only one granule, so no loop */
					for (ch = 0; ch < nchan; ch++) {
						skipBits(21);
						gain = peek8Bits();
						gain += gainchange[ch];
						set8Bits(gain);
						skipBits(42);
					}
					if (!crcflag) {
						if (nchan == 1)
							crcWriteHeader(15,(char*)curframe);
						else
							crcWriteHeader(23,(char*)curframe);
						/* WRITETOFILE */
						if (!UsingTemp) 
							addWriteBuff(filepos-(inbuffer-(curframe+4-buffer)),curframe+4);
					}

				}
				if (!QuietMode) {
					frame++;
					if (frame%200 == 0) {
						/* fprintf(stderr,"frame %d\r",frame); */
							fprintf(stderr,"                                       \r%d of %d bytes analyzed\r",filepos-(inbuffer-(curframe+bytesinframe-buffer)),gFilesize);
							fflush(stderr);
					}
				}
				wrdpntr = curframe+bytesinframe;
				ok = frameSearch();
			}
		}
	}
	if (!QuietMode) {
		fprintf(stderr,"                                       \r");
	}
	fflush(stderr);
	fflush(stdout);
	if (UsingTemp) {
		while (fillBuffer(0));
		if (saveTime) {
			if (timeOK) {
				SetFileTime(outf,&create_time,&access_time,&write_time);
			}
		}
		CloseHandle(outf);
		fclose(inf);
		DeleteFile((LPCTSTR)filename);
	    MoveFile((LPCTSTR)outfilename,(LPCTSTR)filename);
		free(outfilename);
	}
	else {
		flushWriteBuff();
		fclose(inf);
		if (saveTime) {
			if (timeOK) {
				outf = CreateFile((LPCTSTR)filename,
							GENERIC_WRITE,
							0,
							NULL,
							OPEN_EXISTING,
							FILE_ATTRIBUTE_NORMAL,
							NULL);
				if (outf != INVALID_HANDLE_VALUE) {
					SetFileTime(outf,&create_time,&access_time,&write_time);
					CloseHandle(outf);
				}
			}
		}
	}
  }

  NowWriting = 0;
}



static
void showVersion(char *progname) {
	fprintf(stderr,"%s version %s\n",progname,MP3GAIN_VERSION);
}



static
void errUsage(char *progname) {
		showVersion(progname);
		fprintf(stderr,"Usage: %s [options] <infile> [<infile 2> ...]\n\n",progname);
		fprintf(stderr,"  --use \"%s /?\" for a full list of options\n",progname);
		exit(0);
}



static
void fullUsage(char *progname) {
		showVersion(progname);
		fprintf(stderr,"Usage: %s [options] <infile> [<infile 2> ...]\n\n",progname);
		fprintf(stderr,"options:\n");
		fprintf(stderr,"\t/v - show version number\n");
		fprintf(stderr,"\t/g <i>  - apply gain i to mp3 without doing any analysis\n");
		fprintf(stderr,"\t/l0 <i> - apply gain i to channel 0 (left channel) of mp3\n");
		fprintf(stderr,"\t          without doing any analysis (ONLY works for STEREO mp3s,\n");
		fprintf(stderr,"\t          not Joint Stereo mp3s)\n");
		fprintf(stderr,"\t/l1 <i> - apply gain i to channel 1 (right channel) of mp3\n\n");
		fprintf(stderr,"\t/r - apply Radio gain automatically (all files set to equal loudness)\n");
		fprintf(stderr,"\t/a - apply Audiophile gain automatically (files are all from the same\n");
		fprintf(stderr,"\t              album: a single gain change is applied to all files, so\n");
		fprintf(stderr,"\t              their loudness relative to each other remains unchanged,\n");
		fprintf(stderr,"\t              but the average album loudness is normalized)\n");
		fprintf(stderr,"\t/m <i> - modify suggested MP3 gain by integer i\n");
		fprintf(stderr,"\t/d <n> - modify suggested dB gain by double n\n");
		fprintf(stderr,"\t/c - ignore clipping warning when applying gain\n");
		fprintf(stderr,"\t/o - output is a database-friendly tab-delimited list\n");
		fprintf(stderr,"\t/t - mp3gain writes modified mp3 to temp file, then deletes original\n");
		fprintf(stderr,"\t     instead of modifying bytes in original file\n");
		fprintf(stderr,"\t/q - Quiet mode: no status messages\n");
		fprintf(stderr,"\t/p - Preserve original file timestamp\n");
		fprintf(stderr,"\t/x - Only find max. amplitude of mp3\n");
		fprintf(stderr,"\t/? - show this message\n\n");
		fprintf(stderr,"If you specify /r and /a, only the second one will work\n");
		fprintf(stderr,"If you do not specify /c, the program will stop and ask before\n     applying gain change to a file that might clip\n");
		exit(0);
}



int main(int argc, char **argv) {
	MPSTR mp;
	unsigned long ok;
	int mode;
	int crcflag;
	unsigned char *Xingcheck;
	unsigned long frame;
	int nchan;
	int bitridx;
	int freqidx;
	int pad;
	long bitsinframe;
	long bytesinframe;
	double dBchange;
	double dblGainChange;
	int intGainChange;
	int nprocsamp;
	int first = 1;
	int mainloop;
	Float_t *maxsample;
	Float_t lsamples[1152];
	Float_t rsamples[1152];
	int ignoreClipWarning = 0;
	int applyRadio = 0;
	int applyAlbum = 0;
	char analysisError = 0;
	int fileStart;
	int numFiles;
	int databaseFormat = 0;
	int i;
	int *fileok;
	int goAhead;
	int directGain = 0;
	int directSingleChannelGain = 0;
	int directGainVal = 0;
	int mp3GainMod = 0;
	double dBGainMod = 0;
	char ch;
	int mpegver;
	int sideinfo_len;
	long gFilesize = 0;
	char tempsamples[8192];
	const int outsize = 8192;

   struct _stat statbuf;
   int stat_fileh, fresult;


   maxAmpOnly = 0;

   saveTime = 0;

	if (argc < 2) {
		errUsage(argv[0]);
	}

	fileStart = 1;
	numFiles = 0;

	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '/') {
			fileStart++;
			switch(argv[i][1]) {
				case 'a':
				case 'A':
					applyRadio = 0;
					applyAlbum = !0;
					break;

				case 'c':
				case 'C':
					ignoreClipWarning = !0;
					break;

				case 'd':
				case 'D':
					if (argv[i][2] != '\0') {
						dBGainMod = atof(argv[i]+2);
					}
					else {
						if (i+1 < argc) {
							dBGainMod = atof(argv[i+1]);
							i++;
							fileStart++;
						}
						else {
							errUsage(argv[0]);
						}
					}
					/* fprintf(stderr,"dBGainMod = %f\n",dBGainMod); */
					break;

				case 'g':
				case 'G':
					directGain = !0;
					directSingleChannelGain = 0;
					if (argv[i][2] != '\0') {
						directGainVal = atoi(argv[i]+2);
					}
					else {
						if (i+1 < argc) {
							directGainVal = atoi(argv[i+1]);
							i++;
							fileStart++;
						}
						else {
							errUsage(argv[0]);
						}
					}
					/* fprintf(stderr,"directGainVal = %d\n",directGainVal); */
					break;

				case 'l':
				case 'L':
					directSingleChannelGain = !0;
					directGain = 0;
					whichChannel = atoi(argv[i]+2);
					if (i+1 < argc) {
						directGainVal = atoi(argv[i+1]);
						i++;
						fileStart++;
					}
					else {
						errUsage(argv[0]);
					}
					/* fprintf(stderr,"directGainVal = %d\n",directGainVal); */
					break;

				case 'm':
				case 'M':
					if (argv[i][2] != '\0') {
						mp3GainMod = atoi(argv[i]+2);
					}
					else {
						if (i+1 < argc) {
							mp3GainMod = atoi(argv[i+1]);
							i++;
							fileStart++;
						}
						else {
							errUsage(argv[0]);
						}
					}
					/* fprintf(stderr,"mp3GainMod = %d\n",mp3GainMod); */
					break;

				case 'o':
				case 'O':
					databaseFormat = !0;
					break;

				case 'p':
				case 'P':
					saveTime = !0;
					break;

				case 'q':
				case 'Q':
					QuietMode = !0;
					break;

				case 'r':
				case 'R':
					applyRadio = !0;
					applyAlbum = 0;
					break;

				case 't':
				case 'T':
					UsingTemp = !0;
					break;

				case 'x':
				case 'X':
					maxAmpOnly = !0;
					break;

				case 'h':
				case 'H':
				case '?':
					fullUsage(argv[0]);
					break;

				case 'v':
				case 'V':
					showVersion(argv[0]);
					exit(0);
					
				default:
					fprintf(stderr,"I don't recognize option %s\n\n",argv[i]);
			}
		}
	}
	maxsample = malloc(sizeof(Float_t) * argc); /* new double[argc]; */
	fileok = malloc(sizeof(int) * argc); /* new int[argc]; */

	if (databaseFormat)
		fprintf(stdout,"File\tMP3 gain\tdB gain\tMax Amplitude\n");

    for (mainloop = fileStart; mainloop < argc; mainloop++) {
	  fileok[mainloop] = 0;
	  curfilename = argv[mainloop];

	  if (directSingleChannelGain) {
		  if (!QuietMode)
			  fprintf(stderr,"Applying gain change of %d to CHANNEL %d of %s...\n\n",directGainVal,whichChannel,argv[mainloop]);
		  if (whichChannel) { /* do right channel */
			  changeGain(argv[mainloop],0,directGainVal);
		  }
		  else { /* do left channel */
			  changeGain(argv[mainloop],directGainVal,0);
		  }
		  if (!QuietMode)
			  fprintf(stderr,"\n\ndone\n");
	  }
	  else if (directGain) {
		  /* fprintf(stderr,"Direct Gain "); */
		  if (!QuietMode)
			  fprintf(stderr,"Applying gain change of %d to %s...\n\n",directGainVal,argv[mainloop]);
		  changeGain(argv[mainloop],directGainVal,directGainVal);
		  if (!QuietMode)
			  fprintf(stderr,"\n\ndone\n");
	  }
	  else {
		  if (!databaseFormat)
			  fprintf(stdout,"%s\n",argv[mainloop]);

			stat_fileh = _open(argv[mainloop],_O_RDONLY | _O_BINARY);
			if (stat_fileh != -1) {
				fresult = _fstat(stat_fileh,&statbuf);
				_close(stat_fileh);

				gFilesize = statbuf.st_size;
			}

		  inf = fopen(argv[mainloop],"rb");

		  if (inf == NULL) {
			  fprintf(stdout, "Can't open %s for reading\n\n",argv[mainloop]);
		  }
		  else {
			
			InitMP3(&mp);
			BadLayer = 0;
			LayerSet = 0;
			maxsample[mainloop] = 0;
			inbuffer = 0;
			filepos = 0;
			bitidx = 0;
			ok = fillBuffer(0);
			if (ok) {

				wrdpntr = buffer;

				ok = skipID3v2();

				ok = frameSearch();
				
				if (!ok) {
					if (!BadLayer)
						fprintf(stdout,"Can't find any valid MP3 frames in file %s\n\n",argv[mainloop]);
				}
				else {
					LayerSet = 1; /* We've found at least one valid layer 3 frame.
								   * Assume any later layer 1 or 2 frames are just
								   * bitstream corruption
								   */
					fileok[mainloop] = !0;
					numFiles++;
					mode = (curframe[3] >> 6) & 3;

					if (curframe[1] & 0x08 == 0x08) /* MPEG 1 */
						sideinfo_len = (curframe[3] & 0xC0 == 0xC0) ? 4 + 17 : 4 + 32;
					else                /* MPEG 2 */
						sideinfo_len = (curframe[3] & 0xC0 == 0xC0) ? 4 + 9 : 4 + 17;

					if (!(curframe[1] & 0x01))
						sideinfo_len += 2;

					Xingcheck = curframe + sideinfo_len;

					if (Xingcheck[0] == 'X' && Xingcheck[1] == 'i' && Xingcheck[2] == 'n' && Xingcheck[3] == 'g') {
						bitridx = (curframe[2] >> 4) & 0x0F;
						if (bitridx == 0) {
							fprintf(stdout, "%s is free format (not currently supported)\n",curfilename);
							ok = 0;
						}
						else {
							mpegver = (curframe[1] >> 3) & 0x03;
							freqidx = (curframe[2] >> 2) & 0x03;
							pad = (curframe[2] >> 1) & 0x01;
							if (mpegver == 3) 
								bitsinframe = (int)(floor((1152.0*bitrate[mpegver][bitridx])/frequency[mpegver][freqidx])) + pad*8;
							else
								bitsinframe = (int)(floor((576.0*bitrate[mpegver][bitridx])/frequency[mpegver][freqidx])) + pad*8;

							bytesinframe = (int)(floor(((double)(bitsinframe)) / 8.0));

							wrdpntr = curframe + bytesinframe;

							ok = frameSearch();
						}
					}
					
					frame = 1;
					
					if (!maxAmpOnly) {
						if (ok) {
							mpegver = (curframe[1] >> 3) & 0x03;
							freqidx = (curframe[2] >> 2) & 0x03;
							
							if (first) {
								lastfreq = frequency[mpegver][freqidx];
								InitGainAnalysis((long)(lastfreq * 1000.0));
								analysisError = 0;
								first = 0;
							}
							else {
								if (frequency[mpegver][freqidx] != lastfreq) {
									lastfreq = frequency[mpegver][freqidx];
									ResetSampleFrequency ((long)(lastfreq * 1000.0));
								}
							}
						}
					}
					else {
						analysisError = 0;
					}

					while (ok) {
						bitridx = (curframe[2] >> 4) & 0x0F;
						if (bitridx == 0) {
							fprintf(stdout,"%s is free format (not currently supported)\n",curfilename);
							ok = 0;
						}
						else {
							mpegver = (curframe[1] >> 3) & 0x03;
							crcflag = curframe[1] & 0x01;
							freqidx = (curframe[2] >> 2) & 0x03;
							pad = (curframe[2] >> 1) & 0x01;
							if (mpegver == 3) 
								bitsinframe = (int)(floor((1152.0*bitrate[mpegver][bitridx])/frequency[mpegver][freqidx])) + pad*8;
							else
								bitsinframe = (int)(floor((576.0*bitrate[mpegver][bitridx])/frequency[mpegver][freqidx])) + pad*8;

							bytesinframe = (int)(floor(((double)(bitsinframe)) / 8.0));
							mode = (curframe[3] >> 6) & 0x03;
							nchan = (mode == 3) ? 1 : 2;

							if (inbuffer >= bytesinframe) {
								lSamp = lsamples;
								rSamp = rsamples;
								maxSamp = maxsample+mainloop;
								procSamp = 0;
								if (decodeMP3(&mp,curframe,bytesinframe,tempsamples,outsize,&nprocsamp) == MP3_OK) {
									if (!maxAmpOnly) {
										if (AnalyzeSamples(lsamples,rsamples,procSamp/nchan,nchan) == GAIN_ANALYSIS_ERROR) {
											fprintf(stderr,"Error analyzing further samples (max time reached)          \n");
											analysisError = !0;
											ok = 0;
										}
									}
								}
							}


							if (!analysisError) {
								wrdpntr = curframe+bytesinframe;
								ok = frameSearch();
							}

							if (!QuietMode) {
								frame++;
								if (frame%200 == 0) {
									/* fprintf(stderr,"frame %d\r",frame); */
										fprintf(stderr,"                                       \r%d of %d bytes analyzed\r",filepos-(inbuffer-(curframe+bytesinframe-buffer)),gFilesize);
										fflush(stderr);
								}
							}
						}
					}
					if (!QuietMode)
						fprintf(stderr,"                                       \r");

					if (maxAmpOnly)
						dBchange = 0;
					else
						dBchange = GetTitleGain();

					if (dBchange == GAIN_NOT_ENOUGH_SAMPLES) {
						fprintf(stdout,"Not enough samples in %s to do analysis\n\n",argv[mainloop]);
						numFiles--;
					}
					else {
						dBchange += dBGainMod;

						dblGainChange = dBchange / (5.0 * log10(2.0));
						if (fabs(dblGainChange) - (double)((int)(fabs(dblGainChange))) < 0.5)
							intGainChange = (int)(dblGainChange);
						else
							intGainChange = (int)(dblGainChange) + (dblGainChange < 0 ? -1 : 1);
						intGainChange += mp3GainMod;

						if ((!applyRadio)&&(!applyAlbum)) {
							if (databaseFormat) {
								fprintf(stdout,"%s\t%d\t%f\t%f\n",argv[mainloop],intGainChange,dBchange,maxsample[mainloop]);
								fflush(stdout);
							}
							else {
								fprintf(stdout,"Recommended \"Radio\" dB change: %f\n",dBchange);
								fprintf(stdout,"Recommended \"Radio\" mp3 gain change: %d\n",intGainChange);
								fprintf(stdout,"Max PCM sample at current gain: %f\n",maxsample[mainloop]);
								if (maxsample[mainloop] * (Float_t)(pow(2.0,(double)(intGainChange)/4.0)) > 32767.0) {
									fprintf(stdout,"WARNING: some clipping may occur with this gain change!\n");
								}
								fprintf(stdout,"\n");
							}
						}
						else if (applyRadio) {
							first = !0; /* don't keep track of Audiophile gain */
							fclose(inf);
							goAhead = !0;
							
							if (intGainChange == 0) {
								fprintf(stdout,"\nNo changes to %s are necessary\n",argv[mainloop]);
							}
							else {
								if (!ignoreClipWarning) {
									if (maxsample[mainloop] * (Float_t)(pow(2.0,(double)(intGainChange)/4.0)) > 32767.0) {
										fprintf(stdout,"\nWARNING: some clipping may occur with mp3 gain change %d\n",intGainChange);
										fprintf(stdout,"Continue(y/n)?:");
										ch = 0;
										fflush(stdout);
										fflush(stderr);
										while ((ch != 'Y') && (ch != 'N')) {
											ch = getchar();
											ch = toupper(ch);
										}
										if (ch == 'N')
											goAhead = 0;
									}
								}
								if (goAhead) {
									fprintf(stdout,"Applying mp3 gain change of %d to %s...",intGainChange,argv[mainloop]);
									changeGain(argv[mainloop],intGainChange,intGainChange);
									fprintf(stdout,"\n\n");
								}
							}
						}
					}
				}
			}
			
			ExitMP3(&mp);
			fflush(stderr);
			fflush(stdout);
			fclose(inf);
			
		  } 
	  }
	}
	if ((numFiles > 0)&&(!applyRadio)) {
		if (maxAmpOnly)
			dBchange = 0;
		else
			dBchange = GetAlbumGain();

		dBchange += dBGainMod;

		if (dBchange == GAIN_NOT_ENOUGH_SAMPLES) {
			fprintf(stdout,"Not enough samples in mp3 files to do analysis\n\n");
		}
		else {
			dblGainChange = dBchange / (5.0 * log10(2.0));
			if (fabs(dblGainChange) - (double)((int)(fabs(dblGainChange))) < 0.5)
				intGainChange = (int)(dblGainChange);
			else
				intGainChange = (int)(dblGainChange) + (dblGainChange < 0 ? -1 : 1);
			intGainChange += mp3GainMod;

			if (!applyAlbum) {
				if (databaseFormat) {
					Float_t maxmaxsample;
					maxmaxsample = 0;
					for (mainloop = fileStart; mainloop < argc; mainloop++)
						if (fileok[mainloop])
							if (maxsample[mainloop] > maxmaxsample)
								maxmaxsample = maxsample[mainloop];

					fprintf(stdout,"\"Album\"\t%d\t%f\t%f\n",intGainChange,dBchange,maxmaxsample);
					fflush(stdout);
				}
				else {
					fprintf(stdout,"\nRecommended \"Audiophile\" dB change for all files: %f\n",dBchange);
					fprintf(stdout,"Recommended \"Audiophile\" mp3 gain change for all files: %d\n",intGainChange);
					for (mainloop = fileStart; mainloop < argc; mainloop++) {
						if (fileok[mainloop])
							if (maxsample[mainloop] * (Float_t)(pow(2.0,(double)(intGainChange)/4.0)) > 32767.0) {
								fprintf(stdout,"\nWARNING: with this global gain change, some clipping may occur in file %s\n",argv[mainloop]);
							}
					}
				}
			}
			else {
				for (mainloop = fileStart; mainloop < argc; mainloop++) {
					if (fileok[mainloop]) {
						goAhead = !0;
						if (intGainChange == 0) {
							fprintf(stdout,"\nNo changes to %s are necessary\n",argv[mainloop]);
						}
						else {
							if (!ignoreClipWarning) {
								if (maxsample[mainloop] * (Float_t)(pow(2.0,(double)(intGainChange)/4.0)) > 32767.0) {
									fprintf(stdout,"\n\nWARNING: %s may clip with mp3 gain change %d\n",argv[mainloop],intGainChange);
									fprintf(stdout,"Continue(y/n)?:");
									ch = 0;
									fflush(stdout);
									fflush(stderr);
									while ((ch != 'Y') && (ch != 'N')) {
										ch = getchar();
										ch = toupper(ch);
									}
									if (ch == 'N')
										goAhead = 0;
								}
							}
							if (goAhead) {
								fprintf(stdout,"\nApplying mp3 gain change of %d to %s...",intGainChange,argv[mainloop]);
								changeGain(argv[mainloop],intGainChange,intGainChange);
								fprintf(stdout,"\n");
							}
						}
					}
				}
			}
		}
	}
	
	free(maxsample);
	free(fileok);

	return 1;
}