#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#define RAM_SIZE 512
#define ROW_SIZE 32

#define update() printf("\033[H\033[J")
#define gotoxy(x, y) printf("\033[%d;%dH", x, y)
#define min(a, b) (((a) < (b)) ? (a) : (b))

#define CLK_SPEED 1

struct instr {
	unsigned int co : 3;
	unsigned int cd : 9;
};
typedef struct instr instr_t;


int compose(instr_t inst) {
	int result = (inst.co << 9) | inst.cd;
	return result;
}

int compose_cd(instr_t inst) {
	return (int)inst.cd;
}

instr_t decompose(int val) {
	instr_t tmp;
	tmp.co = ((val & 0xE00) >> 9);
	tmp.cd = (val & 0x1FF);
	return tmp;
}

instr_t ram[RAM_SIZE];
int ram_index = 0;
char filename[256];

int free_ram() {
	return (ram_index >= RAM_SIZE-1);
}

void zeroram() {
	for (int i = 0; i < RAM_SIZE; i++) {
		ram[i].co = 0;
		ram[i].cd = 0;
	}
}

void ram_write() {
	FILE *fp;
	fp = fopen(filename, "w+");
	if (fp) {
		fwrite(ram, sizeof(ram), 1, fp);
		if (fwrite != 0) printf("Write ended okay\n"); else printf("Error writing\n");
		
		fclose(fp);
	}
}

int main(int argc, char** argv) {
	
	if (argc==2) strncpy(filename, argv[1], strlen(argv[1]));
	else strncpy(filename, "default.simplez", strlen("default.simplez"));
	
	zeroram();
	int tmp;
	int result;
	while (free_ram) {
		printf("Enter the command (0/4095) [%d/%d]: ", ram_index, RAM_SIZE);
		result = scanf("%d", &tmp);
		
		if (result == EOF) {
			printf("ERROR INPUT DETECTED\n");
			return -1;
		}
		
		if (result == 0) {
			while (fgetc(stdin) != '\n');
			continue;
		}
		if (tmp == -1) {
			ram_write();
			return 0;
		}
		if (tmp < 0 || tmp > 4095) {
			printf("Command out of range!!\n"); 
			continue;
		}
		
		ram[ram_index] = decompose(tmp);
		ram_index++;
	}
}