/*
 * Copyright 2016 CSIRO
 *
 * This file is part of Mastik.
 *
 * Mastik is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mastik is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mastik.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <util.h>
#include <l3.h>
#include <low.h>


#define BIT_THRESHOLD 50
#define HIT_THRESHOLD 6
#define SET_SLICE_MAP_THRESHOLD 100
#define SYNC_THRESHOLD 99999
#define SAMPLES 100
#define PERIOD 500000U
#define SLICES 4
#define DATA_SIZE 240

int  find_set_slice_map(l3pp_t l3, unsigned set_num);
void channel_sync(l3pp_t l3, int16_t* res);
int wait_for_preamble(l3pp_t l3, int16_t* res);
void get_data(l3pp_t l3, int16_t* res);


int main(int ac, char **av) {
/*
	FILE *fp = fopen("dump.txt", "w+");
	if( fp == NULL)
	{
		printf("file open failed\n");
		exit(0);
	}
  */
	delayloop(3000000000U);
	
	l3pp_t l3 = l3_prepare(NULL);
	
	int nsets = l3_getSets(l3);
	if(nsets < 6144){
		printf("not enough sets");
		exit(0);
	}
	

	uint16_t *res = calloc(SAMPLES, sizeof(uint16_t));
	
	l3_unmonitorall(l3);
	int set_index = find_set_slice_map(l3, 63);
//	printf("set in index : %d\n",set_index);
	
	l3_monitor(l3,set_index);
	
	int runs = 0;
	while(runs<10)
	{
		channel_sync(l3, res);			
		if(wait_for_preamble(l3, res))
			get_data(l3, res);

		runs++;
	}
	
//	fclose(fp);
	free(res);
	l3_release(l3);
}

int find_set_slice_map(l3pp_t l3, unsigned set_num)
{
	int flag = 1;
	int index = 0;
    int16_t *res = calloc(SAMPLES*SLICES, sizeof(uint16_t));

	printf("finding set index...\n");
    
    l3_monitor(l3, set_num);
	l3_monitor(l3, set_num + 2048);
	l3_monitor(l3, set_num + 4096);
	l3_monitor(l3, set_num + 6144);

	while(flag){
	
		int misses_in_slices[SLICES] = {0};
		l3_repeatedprobecount(l3, SAMPLES, res, 2000);

		//Sum samples of each set
		for(int i = 0 ; i < SAMPLES*SLICES ; i++)/* WE NEED TO MAKE SURE "SAMPLES" VALUE CAN BE DIVIDED BY 4*/
				if(res[i] != -1) misses_in_slices[i%4] += (int)res[i];	
	
		//return set index if samples sum bigger then SET_SLICE_MAP_THRESHOLD
		for(int i = 1 ; i < SLICES ; i++)
		{
		//	printf("sum %d, %d\n", i, misses_in_slices[i]);
			if(misses_in_slices[i] > SET_SLICE_MAP_THRESHOLD){
				index  = i;
				flag = 0;
				break;
			}
		}

	}

	printf("done!\n\n");
	
	l3_unmonitorall(l3);	
	free(res);
	return  (index * 2048) + set_num;
	
}

void channel_sync(l3pp_t l3, int16_t* res)
{

	int hit = 0;
	int miss = 0;
	int bit_rec = -1;
	int sync_count = 0;
	int last_bit_rec = -1;

	printf("synchronizing channel...\n");
	
	//synchronize over - 0101010101
	while(sync_count < 5){

  		//sample channel
		l3_repeatedprobecount(l3, SAMPLES, res, 5000);
		for(int i = 0; i < SAMPLES; i++){

			if(res[i] != -1 && res[i] >= HIT_THRESHOLD)
				hit++;
			else
				miss++;
		}

/*
		if(miss < SAMPLES)
			printf("HIT: %d\n MISS: %d\n", hit, miss);
*/		
		// decide between 0 or 1 and and synchronize if too much samples taken from next period
		if(hit > BIT_THRESHOLD){
			bit_rec = 1;
		//	printf("1\n");		
			if(miss > SYNC_THRESHOLD){
				int prev = rdtscp();
				float wait = (1 - (float)miss/SAMPLES) * PERIOD;
				while((rdtscp() - prev) < wait);
			}
			
		}	
		else{
			bit_rec = 0;
		//	printf("0\n");		
			if(hit > SYNC_THRESHOLD){
				int prev = rdtscp();
				float wait = (1 - (float)miss/SAMPLES) * PERIOD;
				while((rdtscp() - prev) < wait);
		 	}

		}
	
		if(last_bit_rec == 0 && bit_rec == 1)
			sync_count++;
		else if (last_bit_rec == bit_rec)
			sync_count = 0;
		
					
		last_bit_rec = bit_rec;
		hit  = 0;
		miss = 0;

	}

	printf("channel synchronized!\n\n");

}

int wait_for_preamble(l3pp_t l3, int16_t* res)
{

	int hit = 0;
	int preamble_count = 0;

	printf("waiting for preamble...\n");
	
	// Preamble - 1111111111
	while(preamble_count < 10)
	{
		//sample channel
		l3_repeatedprobecount(l3, SAMPLES, res, 5000);
		for(int i = 0; i < SAMPLES; i++){
	
			if(res[i] != -1 && res[i] >= HIT_THRESHOLD)
				hit++;
	
		}

		if(hit > BIT_THRESHOLD)	
			preamble_count++;
		else if(preamble_count > 0)
			preamble_count = 0;	

		hit  = 0;

	}

	printf("receiving message...\n");
	return 1;
	
}

void get_data(l3pp_t l3, int16_t* res)
{

	uint8_t data[240]  = {0};
	uint8_t bit_mask = 1;
	int 	byte_count =  0;
	int		bit_location = 0;
	int 	hit = 0;

	printf("\n");
	printf("message received:\n");
	while(byte_count < 240)
	{

		//sample channel
		l3_repeatedprobecount(l3, SAMPLES, res, 5000);
		for(int i = 0; i < SAMPLES; i++){
	
			if(res[i] != -1 && res[i] >= HIT_THRESHOLD)
				hit++;
		}

		if(((bit_location % 8) == 0) && (bit_location > 0)){
			byte_count ++;
			bit_location = 0;
		}
		
		if(hit > BIT_THRESHOLD)
			data[byte_count] = data[byte_count] | (bit_mask << bit_location); // DATA array initialized to zero , hence we just skip the bit if zero received
	
		hit  = 0;
		bit_location++;
			
	}

	printf("----> %s\n\n" , data);

}