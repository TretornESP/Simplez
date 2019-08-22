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

pthread_mutex_t wp;
pthread_mutex_t excl;
pthread_mutex_t cont_lectores;
pthread_mutex_t cont_escritores;

struct instr {
	unsigned int co : 3;
	unsigned int cd : 9;
};

typedef struct instr instr_t;

char filename[256];

instr_t ram[RAM_SIZE];
instr_t A;
instr_t IP;
instr_t Z;

int lectores = 0;
int escritores = 0;
int halt = 0;

long clocks;
int override_tick = 0;

void zeroram();
void reset();
void tick();
void dump_ram();

void zeroram() {
	for (int i = 0; i < RAM_SIZE; i++) {
		ram[i].co = 0;
		ram[i].cd = 0;
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

void reset() {
	clocks = 0;
	A = decompose(0);
	Z = decompose(0);
	IP = decompose(0);
	zeroram();
}

void tick() {
	clocks++;
}

void dump_ram() {
	for (int i = 0; i < RAM_SIZE; i++) {
		if ((i%ROW_SIZE) == 0) printf("\n");
		printf("%3x ", compose(ram[i]));
	}
	printf("\n");
}

void st(instr_t instruction) {
	ram[instruction.cd] = A;
}

void ld(instr_t instruction) {
	A = ram[instruction.cd];
}

void add(instr_t instruction) {
	A = decompose(compose(A) + compose(ram[instruction.cd]));
}

void br(instr_t instruction) {
	override_tick = 1;
	IP = decompose(compose_cd(instruction));
}

void bz(instr_t instruction) {
	if (compose(Z) == 1) {
		override_tick = 1;
		IP = decompose(compose_cd(instruction));
	}
}

void clr() {
	A = decompose(0);
}

void dec() {
	A = decompose(compose(A)-1);
}

void ext(instr_t instruction) {

}

void execute(instr_t instruction) {
	switch (instruction.co) {
		case 0: st(instruction);
		break;
		case 1: ld(instruction);
		break;
		case 2: add(instruction);
		break;
		case 3: br(instruction);
		break;
		case 4: bz(instruction);
		break;
		case 5: clr();
		break;
		case 6: dec();
		break;
		case 7: ext(instruction);
		break;
		
	}
}

void delay() {
	usleep(1000*1000/CLK_SPEED);
}

void dump_data() {
	printf("CLK: %ld A: %3x IP: %3x Z: %1x\n", clocks, compose(A), compose(IP), compose(Z));
}

void * run(void* arg) {
	while (!halt) {
		pthread_mutex_lock(&cont_escritores);
		escritores++;
		if (escritores==1) pthread_mutex_lock(&wp);
		pthread_mutex_unlock(&cont_escritores);
		pthread_mutex_lock(&excl);
		
		execute(ram[compose(IP)]);
		
		if (override_tick)
			override_tick = 0;
		else
			IP = decompose(compose(IP)+1);
		
		tick();
		
		if (compose(IP) >= RAM_SIZE-1) halt = 1;
		pthread_mutex_unlock(&excl);
		pthread_mutex_lock(&cont_escritores);
		escritores--;
		if (escritores==0) pthread_mutex_unlock(&wp);
		pthread_mutex_unlock(&cont_escritores);
		delay();
	}
	return NULL;
}

void * monitor_ram(void* arg) {
	while (1) {
		pthread_mutex_lock(&wp);
		pthread_mutex_lock(&cont_lectores);
		lectores++;
		if (lectores==1) pthread_mutex_lock(&excl);
		pthread_mutex_unlock(&cont_lectores);
		pthread_mutex_unlock(&wp);
		
		update();
		dump_ram();
		dump_data();
		gotoxy(0,0);
		
		pthread_mutex_lock(&cont_lectores);
		lectores--;
		if (lectores==0) pthread_mutex_unlock(&excl);
		pthread_mutex_unlock(&cont_lectores);
		delay();

	}
	return NULL;
}

void load_code() {

	int fd, rc, ii;
	instr_t *ptr;
	struct stat st;
	size_t size;
	
	fd = open(filename, O_RDWR);
	
	rc = fstat(fd, &st);
	fprintf(stderr, "stat() = %d\n", rc);
	size = st.st_size;
	fprintf(stderr, "size=%zu\n", size);
	
	ptr = mmap(0, size, PROT_READ|PROT_EXEC, MAP_PRIVATE, fd, 0);
	fprintf(stderr, "addr: %p\n", ptr);
	
	/* for (ii=0; ii< size/sizeof * ptr; ii++) {
		printf("data in raw[%d]: %x\n", ii, compose(ptr[ii]));
	} */
		
	int min = min(RAM_SIZE, size / sizeof *ptr);
	printf("MIN: %d\n", min);
	for (int i = 0; i < min; i++) {
		ram[i] = ptr[i];
	}
	
	rc = munmap(ptr, size);
	fprintf(stderr, "unmap() = %d\n", rc);
	close(fd);
}

int main(int argc, char** argv) {
	if (argc==2) strncpy(filename, argv[1], strlen(argv[1]));
	else strncpy(filename, "a.sim", 5);
	
	reset();
	load_code();
	printf("READY\n");
	getc(stdin);
	pthread_t runner;
	pthread_t monitor;
	
	pthread_create(&monitor, NULL, monitor_ram, NULL);
	pthread_create(&runner, NULL, run, NULL);
	pthread_join(runner, NULL);
	return 0;
}