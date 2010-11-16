#define TOLERANCE 0 //sizeof(struct mem_desc) + 20

struct mem_desc{
  int size;
  struct mem_desc *next;
};

void *malloc(unsigned int data_size, unsigned int length);
void free(int *);
void init_mem(void *buffer, unsigned int size);
