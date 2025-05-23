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

//Formatação da FAT
int fat_format(){
	if(mountState == 1) {
		return -1;
	}

	//Limpa Diretório
	memset(dir, 0, sizeof(dir));
	ds_write(DIR, (char*) dir);

	ds_read(SUPER, (char*) &sb);
	
	//Limpa FAT
	unsigned int *empty_fat = calloc(sb.n_fat_blocks * BLOCK_SIZE,sizeof(char));
	for(int i = 0; i < sb.n_fat_blocks; i++){
		ds_write(TABLE+i, ((char*) empty_fat) + i * BLOCK_SIZE);
	}
	free(empty_fat);

	//Inicializa Superbloco
	sb.magic = MAGIC_N;
	sb.number_blocks = ds_size();

	//Quantidade de blocos para a FAT com base na quantidade de blocos existentes no disco
	sb.n_fat_blocks = (sb.number_blocks*sizeof(unsigned int)-1)/BLOCK_SIZE + 1;

	ds_write(SUPER,(char*) &sb);

  	return 0;
}

//Depuração e checagem dos itens no Sistema de Arquivos
void fat_debug(){

	//Montagem da FAT caso não tenha sido feita antes
	int temp = fat_mount();

	//Listagem do Superbloco
	printf("\nSuperblock:\n");
	printf("    magic is ");
	if(sb.magic == MAGIC_N) {
		printf("ok\n");
	} else {
		printf("not ok\n");
	}
	printf("    blocks:    %d\n",sb.number_blocks);
	printf("    block fat: %d\n",sb.n_fat_blocks);

	//Listagem dos arquivos do diretório
	for(int i = 0; i < N_ITEMS; i++){
		if(dir[i].used == OK) {
			printf("\nFile \"%s\":\n", dir[i].name);
			printf("	Size %u bytes\n", dir[i].length);
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
	
	//Desmontagem da FAT caso tenha sido criada dentro desta função
	if (!temp) {
		free(fat);
		mountState = 0;
	}
}

//Montagem da FAT, do Diretório e do Superbloco na RAM
int fat_mount(){

	if(mountState) {
		return -1;
	}

	//Leitura do Superbloco
	ds_read(SUPER, (char*) &sb);
	if(sb.magic == MAGIC_N) {

		//Leitura do Diretório
		ds_read(DIR, (char*) dir);

		//Leitura da FAT
		fat = malloc(sb.n_fat_blocks * BLOCK_SIZE);
		if(!fat){return - 1;}
		for(int i = 0; i < sb.n_fat_blocks; i++){
			ds_read(TABLE+i, ((char*) fat) + i * BLOCK_SIZE);
		}
		mountState = 1;
  		return 0;
	}
	return -1;
}

int fat_create(char *name){
	if(mountState == 0) {
		return -1;
	}

	//Verifica se o nome do arquivo é válido
	if(strlen(name) > MAX_LETTERS) {
		return -1;
	}

	//Verifica se o arquivo já existe
	for(int i = 0; i < N_ITEMS; i++){
		if(dir[i].used == OK && strcasecmp(dir[i].name, name) == 0) {
			return -1;
		}
	}

	for(int i = 0; i < N_ITEMS; i++) {
		if(dir[i].used == NON_OK) {
			dir[i].used = OK;
			strncpy(dir[i].name, name, MAX_LETTERS);
			dir[i].length = 0;
			dir[i].first = EOFF;

			//Escreve o diretório na RAM
			ds_write(DIR, (char*) dir);
			break;
		}
	}
	return 0;
}

int fat_delete( char *name){
	if(mountState == 0) {
		return -1;
	}

	//Verifica se o nome do arquivo é válido
	if(strlen(name) > MAX_LETTERS) {
		return -1;
	}

	//Verifica se o arquivo já existe
	for(int i = 0; i < N_ITEMS; i++){
		if(dir[i].used == OK && strcasecmp(dir[i].name, name) == 0) {

			//deleta o arquivo
			dir[i].used = NON_OK;
			dir[i].length = 0;
			strncpy(dir[i].name, "", MAX_LETTERS);

			//Escreve o diretório na RAM
			ds_write(DIR, (char*) dir);

			//Libera os blocos ocupados pelo arquivo
			unsigned int bloco = dir[i].first;
			dir[i].first = EOFF;
			while(bloco != EOFF && bloco < sb.number_blocks) {
				unsigned int aux = bloco;
				bloco = fat[bloco];
				fat[aux] = FREE;
			}
			fat[bloco] = FREE;

			//Escreve a FAT na RAM
			for(int i = 0; i < sb.n_fat_blocks; i++){
				ds_write(TABLE+i, ((char*) fat) + i * BLOCK_SIZE);
			}
			return 0;
		}
	}

  	return -1;
}

int fat_getsize( char *name){
	if(mountState == 0) {
		return -1;
	}

	//Verifica se o nome do arquivo é válido
	if(strlen(name) > MAX_LETTERS) {
		return -1;
	}

	//Verifica se o arquivo existe
	for(int i = 0; i < N_ITEMS; i++){
		if(dir[i].used == OK && strcasecmp(dir[i].name, name) == 0) {

			//Retorna o tamanho do arquivo
			return dir[i].length;
		}
	}

	return -1;
}

//Retorna a quantidade de caracteres lidos
int fat_read( char *name, char *buff, int length, int offset){
	if(mountState == 0) {
		return -1;
	}

	//Verifica se o nome do arquivo é válido
	if(strlen(name) > MAX_LETTERS) {
		return -1;
	}

	return 0;
}

//Retorna a quantidade de caracteres escritos
int fat_write( char *name, const char *buff, int length, int offset){
	if(mountState == 0) {
		return -1;
	}

	//Verifica se o nome do arquivo é válido
	if(strlen(name) > MAX_LETTERS) {
		return -1;
	}

	return 0;
}
