/* Stringed Instrument Heroine (TM)
   Author: Larry Pyeatt
   Date: 11-1-2012
  (C) All rights reserved

   This program uses to Karplus-Strong algorithm to simulate a
   stringed instrument.  It sounds similar to a harp.  The sample
   rate for the audio samples can be adjusted using the SAMPLE_RATE
   constant.  The audio is first written to a file, then the file is
   converted to Microsoft (TM) Wave (TM) format.

   The input file is made of triplets of values.  The first value is
   an integer specifying the time at which a note is to be played, in
   milliseconds since the start of the song.  The second value is the
   MIDI number of the note to be played, and the third number is the
   relative volume of the note.  The volume is given as a doubleing
   point number between 0.0 and 1.0 inclusive.
*/

#include "wave_header.h"
#include "time_it.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

#define SAMPLE_RATE 44100 //Can't change this line
#define NUM_NOTES (120) //might be able to change this line
#define BASE_SIZE (44100/16) //Can't change this line
#define SMPLS_PER_MS (441) //Can't change this line
#define CUTOFF_THRESHOLD .005

#define INT16_T_MAX (0x7FFF)

/* if note is a midi number, then the frequency is:
   hz = pow(2,(note-69.0)/12.0) * 440;
   and the array size needed is SAMPLE_RATE/Hz        
*/
static unsigned int array_size(int note)
{
  static unsigned int N;
  double hz;
  hz = pow(2,(note-69.0)/12.0) * 440;
  N = (int)(SAMPLE_RATE/hz + 0.5);
//  fprintf(stderr, "Fundamental Frequency for %d: %d\n",note,N);
  return N;
}

/* Plucking a string is simulated by filling the array with
   random numbers between -0.5 and +0.5 
*/
void pluck(double buffer[], int size, double volume)
{
  int i;
  for(i=size; i--; )
    buffer[i] = volume * ((double)rand()/RAND_MAX - 0.5);
}

extern void udiv32(int quotient, int remainder);

/* The average function treats the array as a queue.  The
   first item is taken from the queue, then averaged with
   the next item.  The result of the average is added to
   the end of the queue and returned by the function as the
*/

static double average(double buffer[], int size, int *position)
{
  //int nextpos = size;
  //int quotient = (*position+1);
  static unsigned int nextpos;
  //fprintf(stderr,"Before Divide Quotient: %d Nextpos: %d ", quotient, nextpos);
  //udiv32(quotient, nextpos); 
  //fprintf(stderr,"After Divide Quotient: %d Nextpos: %d", quotient, nextpos);
  //nextpos = quotient%size; 
  //fprintf(stderr,"After Modulus Quotient: %d Nextpos: %d", quotient, nextpos);
  nextpos = (*position+1)%size;
  buffer[*position] = 0.498*(buffer[*position]+buffer[nextpos]);
//  *position = nextpos;
  return buffer[*position];
}

void scream_and_die(char progname[])
{
  fprintf(stderr,"Usage: %s input_file <tempo>\n",progname);;
  fprintf(stderr,"The input file is in Dr. Pyeatt's .notes format.\n");
  fprintf(stderr,"Tempo can be any number between 0.25 and 4.0\n");
  exit(1);
}

struct filedat{
  int32_t time;
  int16_t note;
  int16_t vol;
};

/*  
  main opens the input and temporary files, generates the audio data
  as a stream of numbers in ASCII format, then calls convert_to_wave()
  to convert the temporary file into the output file in .wav format.
*/    
int main(int argc, char **argv)
{
  static double notes[NUM_NOTES][BASE_SIZE] = {0};
//  static double **notes;
//  notes = (double **) malloc(sizeof(double *) * 120);
  static unsigned int position[NUM_NOTES] = {0};
  static int16_t sam_buffer[SAMPLE_RATE];
  static unsigned int sam_buffer_pos = 0;
  static unsigned int frequency[NUM_NOTES] = {2756, 2756, 2756, 2756, 2756, 2756, 2756, 2756, 2756, 2756, 2756, 2756, 2697, 2546, 2403, 2268, 2141, 2020, 1907, 1800, 1699, 1604, 1514, 1429, 1348, 1273, 1201, 1134, 1070, 1010, 954, 900, 849, 802, 757, 714, 674, 636, 601, 567, 535, 505, 477, 450, 425401, 378, 357, 337, 318, 300, 283, 268, 253, 238, 225, 212, 200, 189, 179, 169, 159, 150, 142, 134, 126, 119, 113, 106, 100, 95, 89, 84, 80, 75, 71, 67, 63, 60, 56, 53, 50, 47, 45, 42, 40, 38, 35, 33, 32, 30, 28, 27, 25, 24, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 13, 12, 11, 11, 10, 9, 9, 8, 8, 7, 7, 7, 6, 6, 6};
  static unsigned int active[NUM_NOTES] = {0};
  static double max[NUM_NOTES] = {0};
  double temp;
  register unsigned int i,j;
  FILE *input;
  FILE *output = stdout;
  register unsigned int current_time = 0;
  double tempo = 1.0;
  double sum;
  char *endptr;
  register unsigned int num_samples;
  int16_t sample;
  struct filedat next_note;
  time_it run_time; 
 
  if(argc < 2)
    scream_and_die(argv[0]);
  
  if(argc == 3)
    {
      // try to interpret argv[2] as a double
      tempo = strtof(argv[2],&endptr);
      if(endptr==argv[2] || tempo<0.25 || tempo > 4.0)
	{
	  fprintf(stderr,"Illegal tempo value.\n");
	  scream_and_die(argv[0]);
	}
      tempo = 1.0/tempo;
    }

  if((input = fopen(argv[1],"r"))==NULL)
    {
      perror("Unable to open input file");
      scream_and_die(argv[0]);
    }
/*   
  for(i = 120; i--; )
  {
   frequency[i] = array_size(i);	
   fprintf(stderr,"%d\n",frequency[i]);
  }
*/
/*
  for(i = 0; i < 120; i++ )
  {
	notes[i] = (double *) malloc(sizeof(double) * frequency[i]);
  }
*/
  time_it_start(&run_time);

  //find time of for end of song
  //TODO Seek to end and read last note for time
  while((fread(&next_note,sizeof(next_note),1,input)==1)&&
	(next_note.note != 1));
  fseek(input,0,SEEK_SET);
  
  num_samples = tempo * next_note.time * SMPLS_PER_MS;
    
  write_wave_header(STDOUT_FILENO,num_samples);

  srand(time(NULL));

  do{
    // read the next note
    if(fread(&next_note,sizeof(next_note),1,input)==1)
      {
	next_note.time = (int)(next_note.time * tempo);
	// generate sound, one ms at a time, until we need to start
	// the next note	
	while(current_time < next_note.time)
	  {
	    // generate another millsecond of sound 
	    for(i = 441; i--;)
	      {
		sum = 0.0;
		// average each active string and add its output to the sum
		for(j = 120; j--;)
		{
		  if(active[j] == 1)
	          { 
		    if(position[j] == 0 && max[j] < CUTOFF_THRESHOLD)
		    	active[j] = 0;
		    else
		    {
		        temp = average(notes[j],frequency[j],&position[j]);
		        sum += temp;
			if(position[j] == 0)
			  max[j] = temp;
			else if(max[j] < temp)
			  max[j] = temp;
			position[j] = (position[j]+1)%frequency[j];
		    }
		  }
		}
		sample = (int16_t)(sum * (INT16_T_MAX-1));
		if(sam_buffer_pos >= SAMPLE_RATE)
		{
		  fwrite(sam_buffer,sizeof(int16_t),sam_buffer_pos,output);
		  sam_buffer_pos = 0;
		}
		sam_buffer[sam_buffer_pos++] = sample;
	      }
	      i = 0;
	      current_time++;
	  }
	//pluck the next note
	if(next_note.note >= 0)
	{
	  pluck(notes[next_note.note],frequency[next_note.note],
		next_note.vol/32767.0);
          active[next_note.note] = 1;
	  max[next_note.note] = 1;
	}
      }
  }while(!feof(input));   
  
  //Empty sam_buffer
  if(sam_buffer_pos > 0)
  {
	fwrite(sam_buffer,sizeof(int16_t),sam_buffer_pos,output);
	sam_buffer_pos = 0;
  }
 
  time_it_stop(&run_time);

  time_it_report(&run_time, "Total Time: ");

  fclose(input);
  fclose(output);
  return 0;
}
