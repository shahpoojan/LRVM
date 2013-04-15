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

// Finds the segment by the starting address of its memory. Returns -1 if the segment is  not found
int find_seg_address(void* segbase, rvm_t rvm)
{
	int i;
	for(i=0; i<rvm->num_seg; i++)
	{
		//printf("Address segbase %p\n", segbase);
		//printf("Address segment %p\n", rvm->segment_list[i]->memory);
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

// Finds the segments in the transaction by the staring address of its memory. Returns -1 if it is not found
int find_seg_address_trans(void* segbase, trans_t tid)
{
	int i;
	for(i=0; i<tid->num_seg; i++)
	{
		//printf("Address segbase %p\n", segbase);
		//printf("Address segment %p\n", tid->segment_list[i]->memory);
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

// Finds the segment by its name
int find_seg(const char* segname, rvm_t rvm)
{
	int i;
	for(i=0; i< (rvm->segment_list).size(); i++)
	{
		if(!strcmp(segname, (rvm->segment_list[i])->segname))
		{
			return i;
		}
	}
	//printf("Find is returning\n");
	return -1;
}

// Checks the log while the mapping operation to check if there are some changes that are not written back to the original file
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
	//printf("Size of log file is %d\n", file_stat.st_size);
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

		// Read the log file to find the segment name and the offset within the memory
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
					offset = atoi((line.substr(pos+7,pos2-pos)).c_str());
					size = atoi((line.substr(pos2+6)).c_str());

					// Write the corresponding changes to the memory. Thus these changes will be reflected in the future transactions when the segments are mapped.
					myfile.read(((char*)seg->memory)+offset, size);
					//printf("Read memory = \n%s\n", mem);
					
					break;
				}

			}
		}

	}
	myfile.close();
	free(file_name);
	free(str);

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
		close(rvm->logfile);

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
		close(rvm->logfile);
	}

	system("chmod -R 777 *");
	return rvm;
}


void *rvm_map(rvm_t rvm, const char *name_seg, int size_to_create)
{
	char* segment_name = (char*)malloc(sizeof(char)*(strlen(name_seg)+strlen(rvm->dir)+1));
	strcpy(segment_name, rvm->dir);
	strcat(segment_name, "/");
	strcat(segment_name, name_seg);
	int fd;

	mem_segment *seg = (mem_segment*)malloc(sizeof(mem_segment));
	int seg_index = find_seg(segment_name, rvm);

	if((seg_index == -1) || (rvm->segment_list[seg_index]->state == UNMAPPED))
	{
		printf("Segment is not yet mapped. Segment name = %s\n",segment_name);
		seg->segname = (char*)malloc(sizeof(char)*strlen(segment_name));
		strcpy(seg->segname, segment_name);
		seg->size = size_to_create;
		int result;
		struct stat file_stat;
		if(stat(segment_name, &file_stat) < 0)
		{
			//printf("Segment file not found\n");
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

			}
		}

		/* do other things here */
		lseek(fd, 0, SEEK_SET);
		seg->file_handle = fd;
		seg->state = MAPPED;

		// Copy the file data to the memory
		seg->memory = malloc(sizeof(char)*size_to_create);
		seg->undo_log = malloc(sizeof(char)*size_to_create);
		read(fd, seg->memory, size_to_create);
		memcpy(seg->undo_log, seg->memory, size_to_create);

		// Check the log for the changes that are not written to the file
		check_log_file(rvm, seg);

		seg->modified = NOT_MODIFIED;
		rvm->segment_list.push_back(seg);
		rvm->num_seg++;
		close(fd);
		//printf("segment mapped\n");

		free(segment_name);
		return seg->memory;
	}

	else
	{
		printf("ERROR:: Segment is already mapped\n");
		free(segment_name);
		exit(0);
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
		//printf("DD segment found\n");
		if((rvm->segment_list[index])->state == UNMAPPED)
		{			
			remove(segment_name);
			free(segment_name);
			return;
		}

		else
		{
			printf("ERROR: Segment is mapped. Cant destaroy it!!@@\n");
			free(segment_name);
			return;
		}
	}

	else
	{
		//printf("DD Segment not found in the list\n");
		remove(segment_name);
		free(segment_name);
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
			free(rvm->segment_list[i]->undo_log);
			rvm->segment_list[i]->state = UNMAPPED;
			return;
		}
	}
	//printf("Not found the segment to unmap\n");
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
			printf("ERROR: Transaction can't be initialized because segment is not loaded in the memory\n");
			return (trans_t)-1;
		}
		else
		{
			if(rvm->segment_list[i]->modified == MODIFIED)
			{
				printf("ERROR: One of the segments in segbase is already being modified. Cant initialize\n");
				return (trans_t)-1;
			}
			rvm->segment_list[i]->modified = MODIFIED;
		}
	}

	trans_t transaction = (trans_t)malloc(sizeof(trans_struct));
	transaction->rvm = rvm;
	//transaction->state = NOT_COMMITTED;
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

// This will only write the changes to the log file
void rvm_commit_trans(trans_t tid)
{
	int i;
	char log[200];
	strcpy(log, tid->rvm->dir);
	strcat(log, "/log");

	int logfile = open(log, O_WRONLY | O_APPEND);
	if(logfile < 0)
	{
		printf("ERROR: Cannot open log file\n");
		return;
	}

	for(i=0; i<tid->num_region; i++)
	{
		write(logfile, "Segbase:", 8);
		write(logfile, tid->region_list[i]->segment->segname, strlen(tid->region_list[i]->segment->segname));
		write(logfile, "\n", 1);

		char buffer[100];
		sprintf(buffer, "%d", tid->region_list[i]->offset);
		//itoa(tid->region_list[i]->offset, buffer, 10);
		write(logfile, "Offset:", 7);
		write(logfile, buffer, strlen(buffer));
		write(logfile, " ", 1);
		sprintf(buffer, "%d", tid->region_list[i]->size);
                //itoa(tid->region_list[i]->offset, buffer, 10);
                write(logfile, "Size:", 5);
                write(logfile, buffer, strlen(buffer));

		write(logfile, "\n", 1);

		write(logfile, tid->region_list[i]->segment->memory + tid->region_list[i]->offset, tid->region_list[i]->size);
		write(logfile, "\n\n", 2);
		memcpy(tid->region_list[i]->segment->undo_log + tid->region_list[i]->offset, tid->region_list[i]->segment->memory + tid->region_list[i]->offset, tid->region_list[i]->size);

		tid->region_list[i]->segment->modified = NOT_MODIFIED;
	}

	close(logfile);
	system("chmod -R 777 *");
}


void rvm_abort_trans(trans_t tid)
{
	int i;
	for(i=0; i<tid->num_region; i++)
	{
		memcpy(tid->region_list[i]->segment->memory+tid->region_list[i]->offset, tid->region_list[i]->segment->undo_log+tid->region_list[i]->offset, tid->region_list[i]->size);
	}
}


void rvm_truncate_log(rvm_t rvm)
{
        struct stat file_stat;
        char* file_name = (char*)malloc(sizeof(char)*(strlen(rvm->dir)+4));
        strcpy(file_name, rvm->dir);
        strcat(file_name, "/");
        strcat(file_name, "log");

        stat(file_name, &file_stat);
        //printf("Size of log file is %d\n", file_stat.st_size);
        if(file_stat.st_size == 0)
        {
                //printf("Log file is empty!!\n");
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

                // Read the log file to find the segment name and the offset within the memory
                pos=line.find("Segbase:"); // search
                if(pos!=string::npos) // string::npos is returned if string is not found
                {
                        //printf("Segment = %s\n", (line.substr(pos+8)).c_str());
			char *segname = (char*)malloc(sizeof(char)*strlen((line.substr(pos+8)).c_str()));
			strcpy(segname, (line.substr(pos+8)).c_str());
                        while ( myfile.good() )
                        {
                                size_t pos;
                                getline (myfile,line);
                                //cout << line << endl;
                                pos=line.find("Offset:"); // search
                                int pos2 = line.find(" ");
                                if((pos!=string::npos) && (pos2!=string::npos)) // string::npos is returned if string is not found
                                {
                                        //cout <<" Offset Found!\n";
                                        //cout << line << endl;
                                        int offset, size;
                                        offset = atoi((line.substr(pos+7,pos2-pos)).c_str());
                                        size = atoi((line.substr(pos2+6)).c_str());

                                        // Write the corresponding changes to the memory. Thus these changes will be reflected in the future transactions when the segments are mapped.
					//printf("====File name = %s====\n",segname);

					char *buffer = (char*)malloc(sizeof(char)*size);
					myfile.read(buffer, size);

					fstream back_store;
					back_store.open(segname, ios::binary | ios::in | ios::out);
					back_store.seekg(offset);
					back_store.write(buffer, size);
					back_store.close();

                                        break;
                                }

                        }
                }

        }
        myfile.close();
	myfile.open(file_name, ios::out);
	myfile.close();
	
}
