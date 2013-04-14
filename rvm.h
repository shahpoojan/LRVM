#include <list>
#include <vector>
using namespace std;

#define MAPPED 0
#define UNMAPPED 1

#define NOT_COMMITTED 0
#define COMMITTED 1

struct mem_segment
{
        char* segname;
        int size;
        int file_handle;
        void* memory;
	void* undo_log;
	int state;
};
typedef struct mem_segment mem_segment;


struct mem_region
{
	mem_segment* segment;
	int offset;
	int size;	
};
typedef struct mem_region mem_region;


struct rvm_struct
{
	char dir[100];
	/*char* seg;
	int seg_size;
	char* data;*/
	int id;
	int logfile;
	vector<mem_segment*> segment_list;
	int num_seg;
};


struct trans_struct
{
	vector<mem_segment*> segment_list;
	int num_seg;
	int state;
	vector<mem_region*> region_list;
	int num_region;
	struct rvm_struct* rvm;
};

typedef struct rvm_struct* rvm_t;
typedef struct trans_struct* trans_t;

rvm_t rvm_init(const char* dir);
void* rvm_map(rvm_t rvm, const char* segname, int size_to_create);
void rvm_unmap(rvm_t rvm, void *segbase);
void rvm_destroy(rvm_t rvm, const char *segname);
trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases);
void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size);
void rvm_commit_trans(trans_t tid);
void rvm_abort_trans(trans_t tid);
void rvm_truncate_log(rvm_t rvm);

