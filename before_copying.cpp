#include "rvm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstdlib>
#include <iostream>
#include <fstream>

using namespace std;

struct seg_list
{
	char* seg;
	char* seg_data;
	int size;
};
typedef struct seg_list seg_list;

int rvm_count = 0;

int find_seg_address(void* segbase, rvm_t rvm)
{
	printf("***Number of segments = %d\n", rvm->num_seg);
	int i;
	for(i=0; i<rvm->num_seg; i++)
	{
		printf("Address segbase %p\n", segbase);
		printf("Address segment %p\n", rvm->segment_list[i]->memory);
		if(rvm->segment_list[i]->memory == segbase)
		{
			//printf("Found and unmapping\n");
			//free(rvm->segment_list[i]->memory);
			//rvm->segment_list[i]->mapped = UNMAPPED;
			return i;
		}
	}
	//printf("Not found the segment to unmap\n");
	return -1;
}

int find_seg_address_trans(void* segbase, trans_t tid)
{
	printf("***Number of segments = %d\n", tid->num_seg);
	int i;
	for(i=0; i<tid->num_seg; i++)
	{
		printf("Address segbase %p\n", segbase);
		printf("Address segment %p\n", tid->segment_list[i]->memory);
		if(tid->segment_list[i]->memory == segbase)
		{
			//printf("Found and unmapping\n");
			//free(rvm->segment_list[i]->memory);
			//rvm->segment_list[i]->mapped = UNMAPPED;
			return i;
		}
	}
	//printf("Not found the segment to unmap\n");
	return -1;
}

int find_seg(const char* segname, rvm_t rvm)
{
	printf("Find is called. Count is %d\n", rvm->num_seg);
	int i;
	//std::vector<mem_segment*>::iterator it;
	printf("---Size of list = %d\n", (rvm->segment_list).size());
	for(i=0/*, it = (rvm->segment_list).begin()*/; i< (rvm->segment_list).size()/*it!=(rvm->segment_list).end()*/; i++)
	{
		printf("Inside the for loop\n");
		if(!strcmp(segname, (rvm->segment_list[i])->segname))
		{
			printf("Match found\n");
			return i;
		}
	}
	printf("Find is returning\n");
	return -1;
}


void check_log_file(rvm_t rvm, mem_segment* seg)
{
	struct stat file_stat;
	char* file_name = (char*)malloc(sizeof(char)*(strlen(rvm->dir)+4));
	strcpy(file_name, rvm->dir);
	strcat(file_name, "/");
	strcat(file_name, "log");

	stat(file_name, &file_stat);
	//FILE* fd = fopen(file_name, "rb");
	//stat(file_name, &file_stat);
	printf("Size of log file is %d\n", file_stat.st_size);
	if(file_stat.st_size == 0)
	{
		printf("Log file is empty!!\n");
		return;
	}

	char *str = (char*)malloc(sizeof(char)*file_stat.st_size);

	fstream myfile;
	string line;
	size_t pos;
	myfile.open(file_name, ios::binary | ios::in);
	while ( myfile.good() )
	{
		getline (myfile,line);
		//cout << line << endl;
		pos=line.find("Segbase:"); // search
		if(pos!=string::npos) // string::npos is returned if string is not found
		{
			cout <<"Found!\n";
			printf("Segment = %s\n", (line.substr(pos+8)).c_str());

			while ( myfile.good() )
			{
				size_t pos;
				getline (myfile,line);
				//cout << line << endl;
				pos=line.find("Offset:"); // search
				int pos2 = line.find(" ");
				if((pos!=string::npos) && (pos2!=string::npos)) // string::npos is returned if string is not found
				{
					cout <<" Offset Found!\n";
					cout << line << endl;
					int offset, size;
					printf("Offset = %d\n", offset = atoi((line.substr(pos+7,pos2-pos)).c_str()));
					printf("Size = %d\n", size = atoi((line.substr(pos2+6)).c_str()));

					//char* mem = (char*)malloc(sizeof(char)*size);
					myfile.read(((char*)seg->memory)+offset, size);
					//printf("Read memory = \n%s\n", mem);
					
					break;
				}

			}
		}

	}
	myfile.close();

}

rvm_t rvm_init(const char* dir)
{
	rvm_t rvm = (rvm_t)malloc(sizeof(struct rvm_struct));
	struct stat file_stat;

	char* filepath;
	strcpy(rvm->dir, dir);

	if(stat(dir, &file_stat) < 0)
	{
		printf("Directory does not exist\n");
		char temp[200];
		strcpy(temp, "mkdir ");
		strcat(temp, dir);
		system(temp);
		strcpy(temp, dir);
		strcat(temp, "/log");
		rvm->logfile = open(temp, O_RDWR | O_CREAT);
		rvm->num_seg = 0;
		rvm->id = rvm_count++;

	}

	else
	{
		printf("Directory exists\n");
		char temp[200];
		strcpy(temp, dir);
		strcat(temp, "/log");
		rvm->logfile = open(temp, O_RDWR);
		rvm->num_seg = 0;
		rvm->id = rvm_count++;
	}

	return rvm;
}


void *rvm_map(rvm_t rvm, const char *name_seg, int size_to_create)
{
	printf("---Size of list = %d\n", rvm->id);//(rvm->segment_list).size());
	printf("Map called\n");
	char* segment_name = (char*)malloc(sizeof(char)*(strlen(name_seg)+strlen(rvm->dir)+1));
	strcpy(segment_name, rvm->dir);
	strcat(segment_name, "/");
	strcat(segment_name, name_seg);
	int fd;

	mem_segment *seg = (mem_segment*)malloc(sizeof(mem_segment));
	if(find_seg(segment_name, rvm) == -1)
	{
		printf("Segment is not found. Segment name = %s\n",segment_name);
		seg->segname = (char*)malloc(sizeof(char)*strlen(segment_name));
		strcpy(seg->segname, segment_name);
		seg->size = size_to_create;
		int result;
		struct stat file_stat;
		if(stat(segment_name, &file_stat) < 0)
		{
			printf("Segment file not found\n");
			fd = open(segment_name, O_WRONLY | O_CREAT | O_EXCL, (mode_t)0600);
			if (fd == -1) {
				perror("Error opening file for writing");
				return NULL;
			}

			/* Stretch the file size. Note that the bytes are not actually written, and cause no IO activity. Also, the diskspace is not even used on filesystems that support sparse files. */
			result = lseek(fd, size_to_create-1, SEEK_SET);
			if (result == -1) {
				close(fd);
				perror("Error calling lseek() to 'stretch' the file");
				return NULL;
			}

			/* write just one byte at the end */
			result = write(fd, "A", 1);
			if (result < 0) {
				close(fd);
				perror("Error writing a byte at the end of the file");
				return NULL;
			}

			stat(segment_name, &file_stat);
			printf("Size of segment is %d\n", file_stat.st_size);
		}

		else
		{
			fd = open(segment_name, O_RDWR);
			struct stat file_stat;
			stat(segment_name, &file_stat);

			printf("Size of segment = %d\n", file_stat.st_size);
			if(file_stat.st_size < size_to_create)
			{
				printf("Exists but size is less\n");
				int result = lseek(fd, size_to_create-1, SEEK_SET);
				if (result == -1) {
					close(fd);
					perror("Error calling lseek() to 'stretch' the file");
					return NULL;
				}

				/* write just one byte at the end */
				result = write(fd, "", 1);
				if (result < 0) {
					close(fd);
					perror("Error writing a byte at the end of the file");
					return NULL;
				}

				printf("Segment file found\n");
			}
		}

		printf("Segment is ready\n");
		/* do other things here */
		lseek(fd, 0, SEEK_SET);
		printf("After seek\n");
		seg->file_handle = fd;
		seg->state = MAPPED;
		seg->memory = (char*)malloc(sizeof(char)*size_to_create);
		printf("After malloc\n");
		//char *file_data = (char*)malloc(sizeof(char)(size_to_create);
		read(fd, seg->memory, size_to_create);
		printf("Data read\n");

		check_log_file(rvm, seg);

		rvm->segment_list.push_back(seg);
		rvm->num_seg++;
		printf("Added to the list\n");
		close(fd);
		printf("segment mapped\n");
		return seg->memory;
	}

	else
	{
		printf("Segment is found. This is an error!! \n");
	}

}

void rvm_destroy(rvm_t rvm, const char *name_seg)
{
	char* segment_name = (char*)malloc(sizeof(char)*(strlen(name_seg)+strlen(rvm->dir)+1));
	strcpy(segment_name, rvm->dir);
	strcat(segment_name, "/");
	strcat(segment_name, name_seg);

	int index =find_seg(segment_name, rvm);
	if(index != -1)
	{
		printf("DD segment found\n");
		if((rvm->segment_list[index])->state == UNMAPPED)
		{			
			remove(segment_name);
			return;
		}

		else
		{
			printf("DD Segment is mapped. Cant destaroy it!!@@\n");
			return;
		}
	}

	else
	{
		printf("DD Segment not found in the list\n");
		remove(segment_name);
		return;
	}
}

void rvm_unmap(rvm_t rvm, void *segbase)
{	
	int i;
	for(i=0; i<rvm->num_seg; i++)
	{
		if(rvm->segment_list[i]->memory == segbase)
		{
			printf("Found and unmapping\n");
			free(rvm->segment_list[i]->memory);
			rvm->segment_list[i]->state = UNMAPPED;
			return;
		}
	}
	printf("Not found the segment to unmap\n");
	return;
}


trans_t rvm_begin_trans(rvm_t rvm, int numsegs, void **segbases)
{
	int i;
	for(i = 0; i<numsegs; i++)
	{
		int index = find_seg_address(segbases[i], rvm);
		if(index == -1)
		{
			printf("Transaction can't be initialized because segment is not loaded in the memory\n");
			return (trans_t)-1;
		}
		else
		{
			if(rvm->segment_list[i]->state != MAPPED)
			{
				printf("One of the segments is already being modified. Cant initialize\n");
				return (trans_t)-1;
			}
		}
	}

	trans_t transaction = (trans_t)malloc(sizeof(trans_struct));
	transaction->rvm = rvm;
	transaction->state = NOT_COMMITTED;
	transaction->num_seg = numsegs;
	transaction->num_region = 0;
	for(i=0; i<numsegs; i++)
	{
		int index = find_seg_address(segbases[i], rvm);
		transaction->segment_list.push_back(rvm->segment_list[index]);
	}
	printf("Transaction initialized!!\n");
	return transaction;
}

void rvm_about_to_modify(trans_t tid, void *segbase, int offset, int size)
{
	mem_region* region = (mem_region*)malloc(sizeof(mem_region));
	int index = find_seg_address_trans(segbase, tid);
	if(index == -1)
	{
		printf("Error: segment not initialized for transaction\n");
		return;
	}

	region->segment = tid->segment_list[index];
	region->offset = offset;
	region->size = size;
	tid->region_list.push_back(region);
	tid->num_region++;
	printf("Region added to the transaction\n");
	return;
}

void rvm_commit_trans(trans_t tid)
{
	//int fd = open("rvm_segments/log", O_WRONLY | O_APPEND);
	int i;
	for(i=0; i<tid->num_region; i++)
	{
		write(tid->rvm->logfile, "Segbase:", 8);
		write(tid->rvm->logfile, tid->region_list[i]->segment->segname, strlen(tid->region_list[i]->segment->segname));
		write(tid->rvm->logfile, "\n", 1);

		char buffer[100];
		sprintf(buffer, "%d", tid->region_list[i]->offset);
		//itoa(tid->region_list[i]->offset, buffer, 10);
		write(tid->rvm->logfile, "Offset:", 7);
		write(tid->rvm->logfile, buffer, strlen(buffer));
		write(tid->rvm->logfile, " ", 1);
		sprintf(buffer, "%d", tid->region_list[i]->size);
                //itoa(tid->region_list[i]->offset, buffer, 10);
                write(tid->rvm->logfile, "Size:", 5);
                write(tid->rvm->logfile, buffer, strlen(buffer));

		write(tid->rvm->logfile, "\n", 1);

		write(tid->rvm->logfile, tid->region_list[i]->segment->memory + tid->region_list[i]->offset, tid->region_list[i]->size);
		write(tid->rvm->logfile, "\n\n", 2);
	}
	system("chmod -R 777 *");
}
