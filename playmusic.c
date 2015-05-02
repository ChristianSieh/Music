/* Stringed Instrument Heroine (TM)
   Author: Larry Pyeatt
   Optimized: Christian Sieh
   Date: 5/1/2015
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

#define SAMPLE_RATE 44100
#define NUM_NOTES (120)
#define BASE_SIZE (44100/16)
#define SMPLS_PER_MS (441)

#define INT16_T_MAX (0x7FFF)

/* if note is a midi number, then the frequency is:
   hz = pow(2,(note-69.0)/12.0) * 440;
   and the array size needed is SAMPLE_RATE/Hz        
*/
static unsigned int array_size(int note)
{
  register unsigned int N;
  double hz;
  hz = pow(2,(note-69.0)/12.0) * 440;
  N = (int)(SAMPLE_RATE/hz + 0.5);
  return N;
}

/* Plucking a string is simulated by filling the array with
   random numbers between -0.5 and +0.5 
*/
void pluck(double buffer[], int size, double volume)
{
  register unsigned int i;
  for(i = 0; i < size; i++)
    buffer[i] = volume * ((double)rand()/RAND_MAX - 0.5);
}

//Assembly function that replaces modulus in the average function
long udiv32(int quotient, int remainder);

/* The average function treats the array as a queue.  The
   first item is taken from the queue, then averaged with
   the next item.  The result of the average is added to
   the end of the queue and returned by the function as the
*/

static double average(double buffer[], int size, int *position)
{
  register unsigned int nextpos;
  register unsigned int quotient = (*position+1);
  double value;
  nextpos = udiv32(quotient, size);
  buffer[*position] = value =  0.498*(buffer[*position]+buffer[nextpos]);
  *position = nextpos;
  return value;
}

//Prints Usage statement if incorrect arguements are given
void scream_and_die(char progname[])
{
  fprintf(stderr,"Usage: %s input_file <tempo>\n",progname);;
  fprintf(stderr,"The input file is in Dr. Pyeatt's .notes format.\n");
  fprintf(stderr,"Tempo can be any number between 0.25 and 4.0\n");
  exit(1);
}

//Structure for the notes
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
  static unsigned int position[NUM_NOTES] = {0};
  static uint16_t sam_buffer[SAMPLE_RATE];
  static unsigned int sam_buffer_pos = 0;
  static unsigned int frequency[NUM_NOTES];
  static unsigned int active[NUM_NOTES] = {0};
  static unsigned int max[NUM_NOTES] = {0};
  register unsigned int stackpointer = 0;
  register unsigned int currentSample = 0;
  register unsigned int i,j;
  FILE *input;
  FILE *output = stdout;
  register unsigned int current_time = 0;
  double tempo = 1.0;
  double sum;
  char *endptr;
  register unsigned int num_samples;
  register uint16_t sample;
  struct filedat next_note;

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

  //Check if input file opened
  if((input = fopen(argv[1],"r"))==NULL)
    {
      perror("Unable to open input file");
      scream_and_die(argv[0]);
    }
  
  //Get all the natural frequencies for the notes and store in array 
  for(i = 0; i < 120; i++)
  {
   frequency[i] = array_size(i);	
  }

  //find time of for end of song
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
	    for(i = 441; i--; )
	      {
		sum = 0.0;
		// average each active string and add its output to the sum
		for(j = 0; j < stackpointer; j++)
		{
	          sum += average(notes[active[j]],frequency[active[j]],
		  &position[active[j]]);
		  //Kill note and active notes so we only play notes that are
		  //currently producing sound
		  if(max[j] < currentSample + 1)
		  {
		    active[j] = active[stackpointer - 1];
		    active[stackpointer - 1] = 0;
		    max[j] = max[stackpointer - 1];
		    max[stackpointer - 1] = 0;
		    stackpointer--;
		    j--;  
		  }
		}
		//Store the sample so we only write once a ms instead of 441
		//times a ms
		sample = (int16_t)(sum * 32766);

		if(sam_buffer_pos >= SAMPLE_RATE)
		{
		  fwrite(sam_buffer,sizeof(int16_t),sam_buffer_pos,output);
		  sam_buffer_pos = 0;
		}
		sam_buffer[sam_buffer_pos++] = sample;
	      }
	      currentSample += 441;
	      current_time++;
	  }

	//pluck the next note
	if(next_note.note >= 0)
	{
	  pluck(notes[next_note.note],frequency[next_note.note],
		next_note.vol/32767.0);
	  //Used for killing nodes and defining if they are active
	  for(i = 0; i < stackpointer; i++)
            if(next_note.note == active[i])
	     break;
	  active[i] = next_note.note;
	  max[i] = currentSample + frequency[next_note.note] * 350;
	  if( i == stackpointer)
		stackpointer++;
	}
      }
  }while(!feof(input));   
  
  //Empty sam_buffer
  if(sam_buffer_pos > 0)
  {
	fwrite(sam_buffer,sizeof(int16_t),sam_buffer_pos,output);
	sam_buffer_pos = 0;
  }

  fclose(input);
  fclose(output);
  return 0;
}
