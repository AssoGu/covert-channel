
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <util.h>
#include <l3.h>
#include <low.h>

#define SAMPLES 1 
#define PERIOD  500000U
#define DATA_SIZE 240
#define SET_NUMBER 63
void sync(l3pp_t l3, int16_t* res);
void send_preamble(l3pp_t l3, int16_t* res);
void get_message(char* data);
void noise(l3pp_t l3, int16_t* res);
int	 send_one(l3pp_t l3, int16_t* res);
int  send_zero(l3pp_t l3, int16_t* res);
int  send_data(l3pp_t l3, int16_t* res, uint8_t* data);


int main(int ac, char **av) {

	char* data = (char*)malloc(DATA_SIZE);
	
	delayloop(3700000000U);
	l3pp_t l3 = l3_prepare(NULL);
	
	int nsets = l3_getSets(l3);
	uint16_t *res = calloc(SAMPLES, sizeof(uint16_t));
	
	//set 0 
	l3_unmonitorall(l3);
	l3_monitor(l3, SET_NUMBER);
	
	
	   
	noise(l3, res); // help receiver indicate in which slice the set is mapped to

	while(1){
		get_message(data);
		sync(l3, res);
		send_preamble(l3, res);
		send_data(l3, res, data);
		
		if(data[0] == '0')
				break;
		
		printf("Sent!!\n");
	}
 
	printf("terminating...\n");
	free(res);
	l3_release(l3);
}

void get_message(char* data)
{
	printf("Enter Message:\n");
	memset(data, 0, DATA_SIZE); 
	fgets(data, 240, stdin);
	fseek(stdin, 0 ,SEEK_END);
}

void noise(l3pp_t l3, int16_t* res)
{

	int noise_count = 10;

	while(noise_count > 0){
		send_one(l3, res);
		noise_count--;
	}
		
	
}
void sync(l3pp_t l3, int16_t* res)
{
	int sync_count = 0;

 	while(sync_count < 10){
		send_one(l3, res);
		send_zero(l3, res);
		sync_count++;
	 }
	
}
void send_preamble(l3pp_t l3, int16_t* res)
{
	int	preamble_count = 0;

	while(preamble_count < 10)
		preamble_count += send_one(l3, res);
	
}

int send_one(l3pp_t l3, int16_t* res)
{

	int prev = rdtscp();
	while(1){
		l3_repeatedprobe(l3, SAMPLES, res,0);	
		if((rdtscp() - prev) >= PERIOD)
			return 1;
	}
	
}

int send_zero(l3pp_t l3, int16_t* res)
{

	int prev = rdtscp();
	while((rdtscp() - prev) <= PERIOD);
	
	return 1;	
}

int send_data(l3pp_t l3, int16_t* res, uint8_t* data)
{
	int byte_count = 0;

	while(byte_count < DATA_SIZE)
	{
		// extract the i-th bit from the current byte and transmite
		for(int i = 0; i < 8; i++)
		{
			int bit = (data[byte_count] & (1 << i)) >> i;
			if(bit){
		//		printf("1");
				send_one(l3, res);
			}
			else{
		//		printf("0");
				send_zero(l3, res);
			}

		}

		byte_count++;
		//printf("\n");
	}

	return 1;
	
}
