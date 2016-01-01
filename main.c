#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "linked_list.h"
#include "parse_data.h"
#include <pthread.h>
#include <getopt.h>

#define MAME_ROOT_NODE "mame"
#define MAME_ENTRY_TYPE "machine"

#define PARAM_LISTXML "-listxml"
#define PARAM_GETSOFTLIST "-getsoftlist"
#define SOFTLIST_ENTRY_TYPE "softwarelist"
#define SOFT_ENTRY_TYPE "software"

#define NOT_MERGED 0
#define MERGED 1

#define MISSING_ROMS "/missing_roms"
#define MISSING_DISKS "/missing_disks"
#define MISSING_SOFTLISTS "/missing_softlists"
#define MISSING_SOFTS "/missing_softs"
#define UNNEEDED_FILES "/unneeded_files"
#define UNNEEDED_DIRECTORIES "/unneeded_directories"
#define MISSING_ROMS_MERGED "/missing_roms_merged"
#define MISSING_DISKS_MERGED "/missing_disks_merged"
#define MISSING_SOFTLISTS_MERGED "/missing_softlists_merged"
#define MISSING_SOFTS_MERGED "/missing_softs_merged"
#define UNNEEDED_FILES_MERGED "/unneeded_files_merged"
#define UNNEEDED_DIRECTORIES_MERGED "/unneeded_directories_merged"

#define BUFFER_SIZE (10000)

llist_t * listxml;
llist_t * softlist;
int unneeded_dir = 0;
int unneeded_file = 0;

char * tmp_dir = NULL;
char * working_dir = NULL;
char * binary = NULL;
char * roms_dir = NULL;
char * root_node = NULL;
char * entry_type = NULL;

char * missing_roms_path = NULL;
char * missing_disks_path = NULL;
char * missing_softlists_path = NULL;
char * missing_softs_path = NULL;
char * unneeded_files_path = NULL;
char * unneeded_directories_path = NULL;
char * missing_roms_merged_path = NULL;
char * missing_disks_merged_path = NULL;
char * missing_softlists_merged_path = NULL;
char * missing_softs_merged_path = NULL;
char * unneeded_files_merged_path = NULL;
char * unneeded_directories_merged_path = NULL;

char * list_xml = NULL;
char * list_software = NULL;

FILE * missing_roms = NULL;
FILE * missing_disks = NULL;
FILE * missing_softlists = NULL;
FILE * missing_softs = NULL;
FILE * unneeded_files = NULL;
FILE * unneeded_directories = NULL;
FILE * missing_roms_merged = NULL;
FILE * missing_disks_merged = NULL;
FILE * missing_softlists_merged = NULL;
FILE * missing_softs_merged = NULL;
FILE * unneeded_files_merged = NULL;
FILE * unneeded_directories_merged = NULL;

int update = 0;

const char optstring[] = "?U";
const struct option longopts[] = {
        { "update",no_argument,NULL,'U' },
        {NULL,0,NULL,0}};


static char *filter[]= { "dipswitch", "dipvalue", "chip", "display", "sound", "input", "control", "configuration", "confsetting", "adjuster", "device", "instance", "extension", "slot", "slotoption", "ramoption", NULL };

/* a list of string containing filename of confirmed roms, disks
softwares... to be tested against all files in the directories to find unneeded files */
char ** confirmed_filename = NULL;
int confirmed_number = 0;

void add_confirmed(char * filename)
{
	confirmed_filename = realloc(confirmed_filename,confirmed_number*sizeof(char *));
	confirmed_filename[confirmed_number-1] = strdup(filename);
}

void check_roms_file(int merged)
{
	llist_t * current;
	llist_t * rom;
	char buf[BUFFER_SIZE];
	char * name;
	char * romof;
	char * merge;
	char * status;
	int missing_roms_count = 0;
	int needed_roms_count = 0;
	int rom_needed;
	struct stat stat_buf;
	FILE * output_file = missing_roms;

	if(merged) {
		output_file = missing_roms_merged;
	}

	current = find_first_node(listxml,entry_type);
	do {
		/* Skip merged file */
		if( merged ) {
			romof = find_attr(current,"romof");
			if( romof != NULL ) {
				continue;
			}
		}

		rom_needed = 1;

		name = find_attr(current,"name");

		/* Check if a rom is needed for this entry */
		rom_needed = 0;
		rom = find_first_node(current,"rom");
		if(rom==NULL) {
			continue;
		}
		do {
			status=find_attr(rom,"status");
			if(!strcmp(status,"nodump")) {
				continue;
			}
			if(merged && (merge=find_attr(rom,"merge"))!=NULL){
				continue;
			}
			rom_needed = 1;
			break;
		}while((rom=find_next_node(rom))!=NULL);

		if( rom_needed == 0 ) {
			continue;
		}

		needed_roms_count++;

		sprintf(buf,"%s/%s.zip",roms_dir,name);
		if( stat(buf,&stat_buf) == 0 ) {
			confirmed_number++;
			add_confirmed(buf);
			continue;
		}
		sprintf(buf,"%s/%s.7z",roms_dir,name);
		if( stat(buf,&stat_buf) == 0 ) {
			confirmed_number++;
			add_confirmed(buf);
			continue;
		}
		fprintf(output_file,"%s\n",name);
		missing_roms_count++;
	} while((current=find_next_node(current))!=NULL);

	printf("%d/%d roms file missing\n",missing_roms_count,needed_roms_count);
}

void check_disk_file(int merged)
{
	llist_t * current;
	llist_t * disk;
	char buf[BUFFER_SIZE];
	char * name;
	char * romof;
	char * merge;
	char * status;
	char * disk_name;
	int missing_disk_count = 0;
	int needed_disk_count = 0;
	struct stat stat_info;
	FILE * output_file = missing_disks;

	if(merged) {
		output_file = missing_disks_merged;
	}

	current = find_first_node(listxml,entry_type);
	do {
		name = find_attr(current,"name");
		romof = find_attr(current,"romof");

		/* Check if a disk is needed for this entry */
		disk = find_first_node(current,"disk");
		if(disk==NULL) {
			continue;
		}
		do {
			status=find_attr(disk,"status");
			if(!strcmp(status,"nodump")) {
				continue;
			}
			if(merged && (merge=find_attr(disk,"merge"))!=NULL){
				continue;
			}
			/* needed disk found */
			needed_disk_count++;

			disk_name=find_attr(disk,"name");

			// Try to find disk in machine directory
			sprintf(buf,"%s/%s/%s.chd",roms_dir,name,disk_name);
			if(stat(buf,&stat_info)==0) {
				confirmed_number++;
				add_confirmed(buf);
				continue;
			}
			else {
			// Try in romof machine in case of merged set
				if(merged && romof) {
					sprintf(buf,"%s/%s/%s.chd",roms_dir,romof,disk_name);
					if(stat(buf,&stat_info)==0) {
						confirmed_number++;
						add_confirmed(buf);
						continue;
					}
				}
			}

			fprintf(output_file,"%s/%s\n",name,disk_name);
			missing_disk_count++;
		}while((disk=find_next_node(disk))!=NULL);

	} while((current=find_next_node(current))!=NULL);

	printf("%d/%d disk files missing\n",missing_disk_count,needed_disk_count);
}

int check_soft_disks(char * softlist_name,char * parent_soft_name,llist_t * list,int * needed, int * missing,int merged)
{
	FILE * output_file = missing_softs;
	llist_t * current_part;
	char * name;
	llist_t * current_diskarea;
	llist_t * current_disk;
	char * name_disk;
	char buf[BUFFER_SIZE];
	struct stat stat_buf;
	int found = 0;

	if( merged ) {
		output_file = missing_softs_merged;
	}

	current_part = find_first_node(list,"part");
	do {
		name = find_attr(current_part,"name");

		current_diskarea = find_first_node(current_part,"diskarea");
		if( current_diskarea == NULL ) {
			continue;
		}
		do {
			current_disk = find_first_node(current_diskarea,"disk");
			do {
				(*needed)++;
				name_disk = find_attr(current_disk,"name");

				sprintf(buf,"%s/%s/%s/%s.chd",roms_dir,softlist_name,parent_soft_name,name_disk);
				if( stat(buf,&stat_buf) == 0 ) {
					confirmed_number++;
					add_confirmed(buf);
					found = 1;
					continue;
				}
				fprintf(output_file,"%s\n",buf);
				(*missing)++;
			} while((current_disk=find_next_node(current_disk))!=NULL);
		} while((current_diskarea=find_next_node(current_diskarea))!=NULL);
	} while((current_part=find_next_node(current_part))!=NULL);

	return found;
}

void check_softs(char * softlist_name,llist_t * list,int * needed, int * missing, int merged)
{
	FILE * output_file = missing_softs;
	llist_t * current;
	char * name;
	char * cloneof;
	char * parent_name;
	char buf[BUFFER_SIZE];
	struct stat stat_buf;
	int found = 0;

        if( merged ) {
                output_file = missing_softs_merged;
        }

	current = find_first_node(list,SOFT_ENTRY_TYPE);
	do {
		(*needed)++;

		name = find_attr(current,"name");
		cloneof = find_attr(current,"cloneof");

		parent_name = name;
		if( merged ) {
			if( cloneof ) {
				parent_name = cloneof;
			}
		}

		sprintf(buf,"%s/%s/%s.zip",roms_dir,softlist_name,parent_name);
		if( stat(buf,&stat_buf) == 0 ) {
			confirmed_number++;
			add_confirmed(buf);
			found = 1;
		}
		else {
			sprintf(buf,"%s/%s/%s.7z",roms_dir,softlist_name,parent_name);
			if( stat(buf,&stat_buf) == 0 ) {
				confirmed_number++;
				add_confirmed(buf);
				found = 1;
			}
		}

		if(check_soft_disks(softlist_name,parent_name,current,needed,missing,merged)) {
			confirmed_number++;
			add_confirmed(buf);
			found = 1;
		}

		if( found == 1 ) {
			continue;
		}

		fprintf(output_file,"%s\n",buf);
		(*missing)++;
	} while((current=find_next_node(current))!=NULL);
}

void check_softlists(int merged)
{
	llist_t * current;
	char buf[BUFFER_SIZE];
	struct stat stat_buf;
	int missing_softlists_count = 0;
	int needed_softlists_count = 0;
	int missing_softs_count = 0;
	int needed_softs_count = 0;
	FILE * output_file = missing_softlists;
	char * name;

	if( merged ) {
		output_file = missing_softlists_merged;
	}

	if(softlist == NULL) {
		return;
	}

	printf("Checking softlists:\n");

	current = find_first_node(softlist,SOFTLIST_ENTRY_TYPE);
	do {
		name = find_attr(current,"name");

		needed_softlists_count++;

		sprintf(buf,"%s/%s",roms_dir,name);
		if( stat(buf,&stat_buf) == 0 ) {
			confirmed_number++;
			add_confirmed(buf);
			check_softs(name,current,&needed_softs_count,&missing_softs_count,merged);
			continue;
		}
		fprintf(output_file,"%s\n",name);
		missing_softlists_count++;
	} while((current=find_next_node(current))!=NULL);

	printf("%d/%d softlists missing\n",missing_softlists_count,needed_softlists_count);
	printf("%d/%d softs missing\n",missing_softs_count,needed_softs_count);
}

int compare_file_to_list(char * basepath, char * currentpath, int merged)
{
	DIR * d;
	struct dirent * e;
	int no_empty = 0;
	int ret;
	int i;
	char buf[BUFFER_SIZE];
	FILE * output_files = unneeded_files;
	FILE * output_directories = unneeded_directories;

	if(merged) {
		output_files = unneeded_files_merged;
		output_directories = unneeded_directories_merged;
	}

	if(currentpath != NULL) {
		sprintf(buf,"%s/%s",basepath,currentpath);
	}
	else {
		sprintf(buf,"%s",basepath);
	}

	d = opendir(buf);
	while( (e=readdir(d)) != NULL ) {
		if( !strcmp(e->d_name,"." )) continue;
		if( !strcmp(e->d_name,".." )) continue;

		if( e->d_type == DT_DIR ) {
			if(currentpath == NULL) {
				sprintf(buf,"%s",e->d_name);
			}
			else {
				sprintf(buf,"%s/%s",currentpath,e->d_name);
			}
			ret = compare_file_to_list(basepath,buf,merged);
			if(ret) {
				no_empty = 1;
			}
			else {
				fprintf(output_directories,"%s/%s\n",basepath,buf);
				unneeded_dir++;
			}
		}
		else {
			if(currentpath == NULL) {
				sprintf(buf,"%s/%s",basepath,e->d_name);
			}
			else {
				sprintf(buf,"%s/%s/%s",basepath,currentpath,e->d_name);
			}

			for( i=0; i<confirmed_number; i++ ) {
				if(!strcmp(buf,confirmed_filename[i])) {
					no_empty = 1;
					break;
				}
			}

			if( i >= confirmed_number ) {
				fprintf(output_files,"%s\n",buf);
				unneeded_file++;
			}
		}
	}

	closedir(d);

	return no_empty;
}
void check_unneeded_file(char * path,int merged)
{
	unneeded_dir = 0;
	unneeded_file = 0;
	compare_file_to_list(path, NULL,merged);
}
void * launch_load_listxml(void * arg)
{
        parse_data_t data;
        char filename[1024];
        char cmd[1024];
        struct stat stat_info;
        char * type_info = PARAM_LISTXML;

        data.root_node = NULL;
        data.decoding_string = 0;
        data.xml_filter = NULL;
        data.current = NULL;

	sprintf(filename,"%s%s%s",tmp_dir,root_node,type_info);
	sprintf(cmd,"%s %s | tee %s ",binary,type_info,filename);
		
        if(stat(filename,&stat_info)==0 && !update) {
                sprintf(cmd,"/bin/cat %s",filename);
        }

        listxml = LoadXML(cmd,&data,filter);
        return NULL;
}

void * launch_load_getsoftlist(void * arg)
{
        parse_data_t data;
        char filename[1024];
        char cmd[1024];
        struct stat stat_info;
        char * type_info = PARAM_GETSOFTLIST;

        data.root_node = NULL;
        data.decoding_string = 0;
        data.xml_filter = NULL;
        data.current = NULL;

	sprintf(filename,"%s%s%s",tmp_dir,root_node,type_info);
	sprintf(cmd,"%s %s | tee %s ",binary,type_info,filename);

        if(stat(filename,&stat_info)==0 && !update ) {
                sprintf(cmd,"/bin/cat %s",filename);
        }

        softlist = LoadXML(cmd,&data,filter);
        return NULL;
}

void init()
{
	char buf[BUFFER_SIZE];

	roms_dir = getenv("MAME_ROMS_DIR");
	if(roms_dir == NULL) {
		printf("MAME_ROMS_DIR not set in environnement variable\n");
		exit(-1);
	}
	binary = getenv("MAME_BINARY");
	if(binary == NULL) {
		printf("MAME_BINARY not set in environnement variable\n");
		exit(-1);
	}
	root_node = MAME_ROOT_NODE;
	entry_type = MAME_ENTRY_TYPE;

	printf("Roms   : %s\n",roms_dir);
	printf("Binary : %s\n",binary);

	tmp_dir = getenv("TMP_DIR");
	if( tmp_dir == NULL ) {
		printf("TMP_DIR not set in environnement variable\n");
		exit(-1);
	}

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,MISSING_ROMS);
	missing_roms_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,MISSING_DISKS);
	missing_disks_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,MISSING_SOFTLISTS);
	missing_softlists_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,MISSING_SOFTS);
	missing_softs_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,UNNEEDED_FILES);
	unneeded_files_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,UNNEEDED_DIRECTORIES);
	unneeded_directories_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,MISSING_ROMS_MERGED);
	missing_roms_merged_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,MISSING_DISKS_MERGED);
	missing_disks_merged_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,MISSING_SOFTLISTS_MERGED);
	missing_softlists_merged_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,MISSING_SOFTS_MERGED);
	missing_softs_merged_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,UNNEEDED_FILES_MERGED);
	unneeded_files_merged_path = strdup(buf);

	strncpy(buf,tmp_dir,BUFFER_SIZE);
	strcat(buf,UNNEEDED_DIRECTORIES_MERGED);
	unneeded_directories_merged_path = strdup(buf);
}

int main(int argc, char * argv[])
{
	llist_t * current;
	long entry_count = 0;
	long softlist_count = 0;

        pthread_t thread_xml;
        pthread_t thread_softlist;
	void * ret;

	int opt_ret;

	        while((opt_ret = getopt_long(argc, argv, optstring, longopts, NULL))!=-1) {
                switch(opt_ret) {
                        case 'U':
                                update = 1;
                                break;
                        default:
                                printf("HELP:\n\n");
                                printf("-U : update cache from binaries\n");
                                exit(0);
                }
        }

	init();

	missing_roms = fopen(missing_roms_path,"w");
	if( missing_roms == NULL ) {
		printf("Error opening %s\n",missing_roms_path);
		exit(1);
	}
	missing_disks = fopen(missing_disks_path,"w");
	if( missing_disks == NULL ) {
		printf("Error opening %s\n",missing_disks_path);
		exit(1);
	}
	missing_softlists = fopen(missing_softlists_path,"w");
	if( missing_softlists == NULL ) {
		printf("Error opening %s\n",missing_softlists_path);
		exit(1);
	}
	missing_softs = fopen(missing_softs_path,"w");
	if( missing_softs == NULL ) {
		printf("Error opening %s\n",missing_softs_path);
		exit(1);
	}
	unneeded_files = fopen(unneeded_files_path,"w");
	if( unneeded_files == NULL ) {
		printf("Error opening %s\n",unneeded_files_path);
		exit(1);
	}
	unneeded_directories = fopen(unneeded_directories_path,"w");
	if( unneeded_directories == NULL ) {
		printf("Error opening %s\n",unneeded_directories_path);
		exit(1);
	}

	missing_roms_merged = fopen(missing_roms_merged_path,"w");
	if( missing_roms_merged == NULL ) {
		printf("Error opening %s\n",missing_roms_merged_path);
		exit(1);
	}
	missing_disks_merged = fopen(missing_disks_merged_path,"w");
	if( missing_disks_merged == NULL ) {
		printf("Error opening %s\n",missing_disks_merged_path);
		exit(1);
	}
	missing_softlists_merged = fopen(missing_softlists_merged_path,"w");
	if( missing_softlists_merged == NULL ) {
		printf("Error opening %s\n",missing_softlists_merged_path);
		exit(1);
	}
	missing_softs_merged = fopen(missing_softs_merged_path,"w");
	if( missing_softs_merged == NULL ) {
		printf("Error opening %s\n",missing_softs_merged_path);
		exit(1);
	}
	unneeded_files_merged = fopen(unneeded_files_merged_path,"w");
	if( unneeded_files_merged == NULL ) {
		printf("Error opening %s\n",unneeded_files_merged_path);
		exit(1);
	}
	unneeded_directories_merged = fopen(unneeded_directories_merged_path,"w");
	if( unneeded_directories_merged == NULL ) {
		printf("Error opening %s\n",unneeded_directories_merged_path);
		exit(1);
	}
        printf("Loading lists\n");
        pthread_create(&thread_xml,NULL,launch_load_listxml,NULL);
        pthread_create(&thread_softlist,NULL,launch_load_getsoftlist,NULL);
        pthread_join(thread_xml,&ret);
        pthread_join(thread_softlist,&ret);

	current = find_first_node(listxml,entry_type);
	entry_count = 1;
	while((current=find_next_node(current))!=NULL) {
		entry_count++;
	}
	printf("%ld entries\n",entry_count);

	if( softlist != NULL ) {
		current = find_first_node(softlist,SOFTLIST_ENTRY_TYPE);
		softlist_count = 1;
		while((current=find_next_node(current))!=NULL) {
			softlist_count++;
		}
	}

	printf("%ld software lists\n",softlist_count);
	printf("\n");

	printf("*************************\n");
	printf("*Assuming non-merged set*\n");
	printf("Checking existing roms:\n");
	check_roms_file(NOT_MERGED);
	printf("\n");
	printf("Checking existing disks:\n");
	check_disk_file(NOT_MERGED);
	printf("\n");
	check_softlists(NOT_MERGED);
	printf("\n");
	printf("Checking un-needed files\n");
	check_unneeded_file(roms_dir,0);
	printf("%d un-needed directories\n",unneeded_dir);
	printf("%d un-needed files\n",unneeded_file);

	if(confirmed_filename) {
		free(confirmed_filename);
		confirmed_filename = NULL;
	}
	confirmed_number = 0;

	printf("\n\n");
	printf("*********************\n");
	printf("*Assuming merged set*\n");
	printf("Checking existing roms:\n");
	check_roms_file(MERGED);
	printf("\n");
	printf("Checking existing disks:\n");
	check_disk_file(MERGED);
	printf("\n");
	check_softlists(MERGED);
	printf("\n");
	printf("Checking un-needed files\n");
	check_unneeded_file(roms_dir,1);
	printf("%d un-needed directories\n",unneeded_dir);
	printf("%d un-needed files\n",unneeded_file);
	return 0;
}
