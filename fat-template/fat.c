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

	if (sb.magic != 0){
		//Limpa FAT
		unsigned int *empty_fat = calloc(sb.n_fat_blocks * BLOCK_SIZE,sizeof(char));
		for(int i = 0; i < sb.n_fat_blocks; i++){
			ds_write(TABLE+i, ((char*) empty_fat) + i * BLOCK_SIZE);
		}
		free(empty_fat);
	}

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
			unsigned int block = dir[i].first;
			while(block != EOFF && block < sb.number_blocks) {
				printf(" %u", block);
				if(fat[block] == EOFF || fat[block] >= sb.number_blocks) {
					break;
				}
				block = fat[block];
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

//Criação de um arquivo
int fat_create(char *name){
    if (mountState == 0 || strlen(name) > MAX_LETTERS) {
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

//Deletar um arquivo
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
			unsigned int block = dir[i].first;
			dir[i].first = EOFF;
			while(block != EOFF && block < sb.number_blocks) {
				unsigned int aux = block;
				block = fat[block];
				fat[aux] = FREE;
			}
			fat[block] = FREE;

			//Escreve a FAT na RAM
			for(int i = 0; i < sb.n_fat_blocks; i++){
				ds_write(TABLE+i, ((char*) fat) + i * BLOCK_SIZE);
			}
			return 0;
		}
	}

  	return -1;
}

//Obtém o tamanho do arquivo em bytes
int fat_getsize( char *name){
    if (mountState == 0 || strlen(name) > MAX_LETTERS) {
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

//Leitura de dados de um arquivo
int fat_read(char *name, char *buff, int length, int offset) {
    if (mountState == 0 || strlen(name) > MAX_LETTERS) {
        return -1;
    }

    for (int i = 0; i < N_ITEMS; i++) {
		// Verifica se o arquivo existe
        if (dir[i].used == OK && strcasecmp(dir[i].name, name) == 0) {
			// Verifica se o offset é válido
            if (offset > dir[i].length) {
                return -1;
            }

			// Verifica se o tamanho do buffer é válido
            length = (length > dir[i].length - offset) ? dir[i].length - offset : length;

            unsigned int block = dir[i].first;
            int bytes_read = 0;
            int block_offset = offset % BLOCK_SIZE;

            // Pula os blocos iniciais por causa  do offset
            for (int j = 0; j < offset / BLOCK_SIZE && block != EOFF && block < sb.number_blocks; j++) {
                block = fat[block];
            }

            char temp[BLOCK_SIZE];
            while (block != EOFF && block < sb.number_blocks && bytes_read < length) {
                ds_read(block, temp);

                int copy_bytes = BLOCK_SIZE - block_offset;
                copy_bytes = (copy_bytes > length - bytes_read) ? length - bytes_read : copy_bytes;

                memcpy(buff + bytes_read, temp + block_offset, copy_bytes);
                bytes_read += copy_bytes;
                block_offset = 0;
                block = fat[block];
            }
            return bytes_read;
        }
    }
    return -1;
}

//Escrita de dados no arquivo do sistema
int fat_write(char *name, const char *buff, int length, int offset) {
    if (mountState == 0 || strlen(name) > MAX_LETTERS || !buff || length <= 0 || offset < 0) {
        return -1;
    }

    for (int i = 0; i < N_ITEMS; i++) {
        if (dir[i].used == OK && strcasecmp(dir[i].name, name) == 0) {
            if (offset > dir[i].length) {
                return -1;
            }

            // Calcula o tamanho máximo possível para escrita
            int bytes_available = (sb.number_blocks - 2 - sb.n_fat_blocks) * BLOCK_SIZE; // espaço útil
            if (offset + length > bytes_available) {
                length = bytes_available - offset;
            }
            if (length == 0) return 0;

			//Se chegou aqui, há espaço para escrever algo
            unsigned int block = dir[i].first;
            unsigned int prev = EOFF;
            int bytes_written = 0;
            int block_offset = offset % BLOCK_SIZE;
            int skip_blocks = offset / BLOCK_SIZE;

            // Pula blocos já existentes até o offset
            for (int j = 0; j < skip_blocks; j++) {
                if (block == EOFF || block >= sb.number_blocks) break;
                prev = block;
                block = fat[block];
            }

            // Se offset aponta para o fim do arquivo, precisamos alocar o primeiro bloco
            if (block == EOFF) {
                // Aloca novo bloco
                for (unsigned int k = 2 + sb.n_fat_blocks; k < sb.number_blocks; k++) {
                    if (fat[k] == FREE) {
                        fat[k] = EOFF;
                        if (prev != EOFF) {fat[prev] = k;}
                        else {dir[i].first = k;}
                        block = k;
                        break;
                    }
                }
                if (block == EOFF) return -32000; // disco cheio
            }

            char temp[BLOCK_SIZE];
            while (bytes_written < length && block != EOFF && block < sb.number_blocks) {
                ds_read(block, temp);

                int bytes_to_copy = BLOCK_SIZE - block_offset;

				//Se tem mais bytes disponíveis no bloco do que tem para se copiar, define novo valor para a qtd de bytes a copiar 
                if (bytes_to_copy > length - bytes_written)
                    bytes_to_copy = length - bytes_written;

                memcpy(temp + block_offset, buff + bytes_written, bytes_to_copy);
                ds_write(block, temp);
                bytes_written += bytes_to_copy;
                block_offset = 0;

                // Se ainda falta escrever, avança ou aloca novo bloco
                if (bytes_written < length) {
                    if (fat[block] == EOFF) {
                        // Aloca novo bloco
                        unsigned int novo = EOFF;
                        for (unsigned int k = 2 + sb.n_fat_blocks; k < sb.number_blocks; k++) {
                            if (fat[k] == FREE) {
                                novo = k;
                                break;
                            }
                        }
                        if (novo == EOFF) break; // disco cheio
                        fat[block] = novo;
                        fat[novo] = EOFF;
                    }
                    block = fat[block];
                }
            }

            // Atualiza o tamanho do arquivo se necessário
            if (offset + bytes_written > dir[i].length) {
                dir[i].length = offset + bytes_written;
            }

            // Atualiza diretório e FAT no disco
            ds_write(DIR, (char*)dir);
            for (int j = 0; j < sb.n_fat_blocks; j++) {
                ds_write(TABLE + j, ((char*)fat) + j * BLOCK_SIZE);
            }

            return bytes_written;
        }
    }
    return -1;
}

//Desmontagem da FAT, do Diretório e do Superbloco da RAM, salvando no disco.
int fat_dismount() {

	if (!mountState) {
		return -1;
	}

	ds_write(SUPER, (char*) &sb);
	ds_write(DIR, (char*)dir);
	for (int j = 0; j < sb.n_fat_blocks; j++) {
		ds_write(TABLE + j, ((char*)fat) + j * BLOCK_SIZE);
	}
	free(fat);

	mountState=0;
	return 0;
}