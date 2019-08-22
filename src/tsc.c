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
#define CMD_SIZE 32

#define update() printf("\033[H\033[J")
#define gotoxy(x, y) printf("\033[%d;%dH", x, y)
#define min(a, b) (((a) < (b)) ? (a) : (b))

#define CLK_SPEED 1

struct instr {
	unsigned int co : 3;
	unsigned int cd : 9;
};
typedef struct instr instr_t;

struct equs {
	char name[CMD_SIZE];
	int val;
	struct equs * next;
};

struct etis {
	char name[CMD_SIZE];
	int val;
	int addr;
	struct etis * next;
};

struct equs * equ_head;
struct etis * eti_head;

void load_equs(struct equs* head) {
	head = (struct equs*)malloc(sizeof(struct equs));
	head -> next = NULL;
}

void load_etis(struct etis* head) {
	head = (struct etis*)malloc(sizeof(struct etis));
	head -> next = NULL;
}

void add_equ(struct equs* head, char name[CMD_SIZE], int val) {
		struct equs * node = head;
		
		while ((node -> next) != NULL) {
			if (!strcmp(node->name, name)) {
				node -> val = val;
				return;
			}
			node = node->next;
		}
		
		struct equs * item = (struct equs*)malloc(sizeof(struct equs));
		item -> next = NULL;
		
		node -> next = item;
		strcpy(node -> name, name);
		node -> val  = val;
}

void add_eti(struct etis* head, char name[CMD_SIZE], int val, int addr) {
		struct etis * node = head;
		
		while ((node -> next) != NULL) {
			if (!strcmp(node->name, name)) {
				node -> val = val;
				node -> addr = addr;
				return;
			}
			node = node->next;
		}
		
		struct etis * item = (struct etis*)malloc(sizeof(struct etis));
		item -> next = NULL;
		
		node -> next = item;
		strcpy(node -> name, name);
		node -> val  = val;
		node -> addr = addr;
}

void print_equ() {
		struct equs * node = equ_head;
		
		while ((node -> next) != NULL) {
			printf("N: %s V: %d\n", node -> name, node -> val);
			node = node->next;
		}
}

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
char infile[256];
char outfile[256];

int free_ram() {
	return (ram_index >= RAM_SIZE-1);
}

void zeroram() {
	for (int i = 0; i < RAM_SIZE; i++) {
		ram[i].co = 0;
		ram[i].cd = 0;
	}
}

void dump_ram() {
	for (int i = 0; i < RAM_SIZE; i++) {
		if ((i%ROW_SIZE) == 0) printf("\n");
		printf("%3x ", compose(ram[i]));
	}
	printf("\n");
}

int parsedigit(char digit[CMD_SIZE]) {
	if (strlen(digit)>2) {
		if (digit[1] == '\'') {
			switch (digit[0]) {
				case 'H':
					memmove(digit, digit+2, strlen(digit));
					return (int)strtol(digit, NULL, 16);
					break;
				case 'O': 
					memmove(digit, digit+2, strlen(digit));
					return (int)strtol(digit, NULL, 8);
					break;
				case 'B': 
					memmove(digit, digit+2, strlen(digit));
					return (int)strtol(digit, NULL, 2);
					break;
				case 'D': 
				    memmove(digit, digit+2, strlen(digit));
					return atoi(digit);
				break;
				default:
					printf("Unknown digit base %c assuming DECIMAL\n", digit[0]);
					memmove(digit, digit+2, strlen(digit));
					return atoi(digit);
			}
		} else {
			return atoi(digit);
		}
	} else {
		return atoi(digit);
	}
}

void org(int * ram_index, char addr[CMD_SIZE]) {
	*ram_index = parsedigit(addr);
}
void data(int * ram_index, char eti[CMD_SIZE], char addr[CMD_SIZE]) {
	printf("RAM_INDEX: %d DIGIT: %d\n", *ram_index, parsedigit(addr));
	add_eti(eti_head, eti, parsedigit(addr), *ram_index);
	ram[*ram_index] = decompose(parsedigit(addr));
}
void res(int * ram_index, char size[CMD_SIZE]) {
	*ram_index += parsedigit(size);
}
void equ(int * ram_index, char eti[CMD_SIZE], char addr[CMD_SIZE]) {
	add_equ(equ_head, eti, parsedigit(addr));
}


void tokenize(char* string, char* array[CMD_SIZE], int* arrsize, const char * delim) {
	char* token = strtok(string, delim);
	*(arrsize) = 0;
	while (token != NULL) {
		array[*(arrsize)] = token;
		token = strtok(NULL, delim);
		(*arrsize)++;
	}	
}

int checkspecial(char cmd[CMD_SIZE], int * ram_index) {
	if (!strcmp(cmd, "")) {
		printf("void command detected\n");
		return 1;
	}
	return 0;
}

int checkmacro(char cmd[CMD_SIZE], int * ram_index) {
	int size = 0;
	char* sub[CMD_SIZE];
	tokenize(cmd, sub, &size, " ");
	
	for (int i = 0; i < size; i++) {
		if (!strcmp(sub[i], "org")) {
			if (size==2 && i == 0)
				org(ram_index, sub[1]);
			else
				return -1;
		} else if (!strcmp(sub[i], "data")) {
			if (size==3 && i == 1)
				data(ram_index, sub[0], sub[2]);
			else
				return -1;
		} else if (!strcmp(sub[i], "res")) {
			if (size==2 && i == 0)
				res(ram_index, sub[1]);
			else
				return -1;
		} else if (!strcmp(sub[i], "equ")) {
			if (size==3 && i == 1)
				equ(ram_index, sub[0], sub[2]);
			else
				return -1;
		}
	}
}

int checkop(char cmd[CMD_SIZE], int * ram_index) {
	return 0;
}

void parse(char * string) {
	char* array[CMD_SIZE];
	char* sub;
	int size;
	
	int ram_index = 0;
	
	tokenize(string, array, &size, ";");
	for (int i = 0; i < size; i++) {
		if (checkspecial(array[i], &ram_index) == -1) {
			printf("Syntax error: %s\n", array[i]);
			return;
		}
		if (checkmacro(array[i], &ram_index) == -1) {
			printf("Syntax error: %s\n", array[i]);
			return;
		}
		if (checkop(array[i], &ram_index) == -1) {
			printf("Syntax error: %s\n", array[i]);
			return;
		}
	}
}

void load_code() {

	int fd, rc, ii;
	char *ptr;
	char *string;
	struct stat st;
	size_t size;
	
	fd = open(infile, O_RDWR);
	
	rc = fstat(fd, &st);
	fprintf(stderr, "stat() = %d\n", rc);
	size = st.st_size;
	fprintf(stderr, "size=%zu\n", size);
	ptr = mmap(0, size, PROT_READ|PROT_EXEC, MAP_PRIVATE, fd, 0);
	fprintf(stderr, "addr: %p\n", ptr);
	
	/* for (ii=0; ii< size/sizeof * ptr; ii++) {
		printf("data in raw[%d]: %x\n", ii, compose(ptr[ii]));
	} */
		
	string = malloc(size / sizeof * ptr);
	for (int i = 0; i < size / sizeof * ptr; i++) {
		string[i] = ptr[i];
	}
	
	rc = munmap(ptr, size);
	fprintf(stderr, "unmap() = %d\n", rc);
	close(fd);
	
	parse(string);
}

void ram_write() {
	FILE *fp;
	fp = fopen(outfile, "w+");
	if (fp) {
		fwrite(ram, sizeof(ram), 1, fp);
		if (fwrite != 0) printf("Write ended okay\n"); else printf("Error writing\n");
		
		fclose(fp);
	}
}

int main(int argc, char** argv) {

	if (argc==3) {
		strncpy(infile, argv[1], strlen(argv[1]));
		strncpy(outfile, argv[2], strlen(argv[2]));
	} else if (argc==2) {
		strncpy(infile, argv[1], strlen(argv[1]));
		strncpy(outfile, "a.sim", 5);
	} else {
		printf("Usage: ./comp infile [outfile]\n");
		return 1;
	}
	
	load_equs(equ_head);
	load_etis(eti_head);
	load_code();
	return 0;
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
	printf("COMPILING FINISHED\n");
	dump_ram();
}