#include "fat.h"
#include "ds.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define SUPER 0
#define TABLE 2
#define DIR 1

#define SIZE 1024

// o superbloco
#define MAGIC_N           0xAC0010DE
typedef struct{
	int magic;
	int number_blocks;
	int n_fat_blocks;
	char empty[BLOCK_SIZE-3*sizeof(int)];
} super;

super sb;

//item
#define MAX_LETTERS 6
#define OK 1
#define NON_OK 0
typedef struct{
	unsigned char used;
	char name[MAX_LETTERS+1];
	unsigned int length;
	unsigned int first;
} dir_item;

#define N_ITEMS (BLOCK_SIZE / sizeof(dir_item))
dir_item dir[N_ITEMS];

// tabela
#define FREE 0
#define EOFF 1
#define BUSY 2
unsigned int *fat;

int mountState = 0;

int fat_format(){
  	return 0;
}

void fat_debug(){
	printf("\nSuperblock:\n");
	printf("    magic is ");
	if(sb.magic == MAGIC_N) {
		printf("ok\n");
	} else {
		printf("not ok\n");
	}
	printf("    blocks:    %d\n",sb.number_blocks);
	printf("    block fat: %d\n",sb.n_fat_blocks);

	for(int i = 0; i < N_ITEMS; i++){
		if(dir[i].used == OK) {
			printf("\nFile \"%s\":\n", dir[i].name);
			printf("    Size %u bytes\n", dir[i].length);
			printf("	Blocks:");
			unsigned int bloco = dir[i].first;
			while(bloco != EOFF && bloco < sb.number_blocks) {
				printf(" %u", bloco);
				if(fat[bloco] == EOFF || fat[bloco] >= sb.number_blocks) {
					break;
				}
				bloco = fat[bloco];
			}
			printf("\n");
		}
	}
}

int fat_mount(){

	if(mountState) {
		return -1;
	}
	ds_read(SUPER, (char*) &sb);
	if(sb.magic == MAGIC_N) {

		ds_read(DIR, (char*) dir);
		fat = malloc(sb.n_fat_blocks * BLOCK_SIZE);
		if(!fat){return - 1;}
		for(int i = 0; i < sb.n_fat_blocks; i++){
			ds_read(TABLE, ((char*) fat) + i * BLOCK_SIZE);
		}
		mountState = 1;
  		return 0;
	}
	return -1;
}

int fat_create(char *name){
  	return 0;
}

int fat_delete( char *name){
  	return 0;
}

int fat_getsize( char *name){ 
	return 0;
}

//Retorna a quantidade de caracteres lidos
int fat_read( char *name, char *buff, int length, int offset){
	return 0;
}

//Retorna a quantidade de caracteres escritos
int fat_write( char *name, const char *buff, int length, int offset){
	return 0;
}
