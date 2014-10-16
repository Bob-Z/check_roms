#if 0
#define WITH_SOFTLIST 1
#define WORKING_DIR "/home/fred/mess/mess"
#define BINARY "~/mess/mess/mess"
#define BINARY_LISTXML "~/mess/mess/mess -listxml"
#define BINARY_GETSOFTLIST "~/mess/mess/mess -getsoftlist"
#define LIST_XML "/tmp/check_roms_listxml"
#define LIST_SOFTWARE "/tmp/check_roms_listsoftware"
#define ROMS_DIR "/media/4To/Mess/roms"
#define ROOT_NODE "mess"
#define ENTRY_TYPE "machine"
#define SOFT_ROOT_NODE "softwarelist"
#endif

#define MESS_MODE 0
#define MAME_MODE 1
#define UME_MODE 2

#define TMP_DIR "/media/4To/tmp/"

#define MAME_WORKING_DIR "/media/4To/emu/mame/trunk"
#define MAME_BINARY "/media/4To/emu/mame/trunk/mame"
#define MAME_ROMS_DIR "/media/4To/Mame/roms"
#define MAME_ROOT_NODE "mame"
#define MAME_ENTRY_TYPE "game"

#define UME_WORKING_DIR "/media/4To/emu//mame/trunk"
#define UME_BINARY "/media/4To/emu/mame/trunk/ume"
#define UME_ROMS_DIR "/media/4To/Mess/roms"
#define UME_ROOT_NODE "ume"
#define UME_ENTRY_TYPE "game"

#define MESS_WORKING_DIR "/media/4To/emu/mame/trunk"
#define MESS_BINARY "/media/4To/emu/mame/trunk/mess"
#define MESS_ROMS_DIR "/media/4To/Mess/roms"
#define MESS_ROOT_NODE "mess"
#define MESS_ENTRY_TYPE "machine"

#define PARAM_LISTXML "-listxml"
#define PARAM_GETSOFTLIST "-getsoftlist"
#define LIST_XML "/media/4To/tmp/check_roms_listxml"
#define LIST_SOFTWARE "/media/4To/tmp/check_roms_listsoftware"
#define SOFTLIST_ROOT_NODE "softwarelists"
#define SOFTLIST_ENTRY_TYPE "softwarelist"
#define SOFT_ENTRY_TYPE "software"

#define NOT_MERGED 0
#define MERGED 1

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

#define MISSING_ROMS "/media/4To/tmp/missing_roms"
#define MISSING_DISKS "/media/4To/tmp/missing_disks"
#define MISSING_SOFTLISTS "/media/4To/tmp/missing_softlists"
#define MISSING_SOFTS "/media/4To/tmp/missing_softs"
#define UNNEEDED_FILES "/media/4To/tmp/unneeded_files"
#define UNNEEDED_DIRECTORIES "/media/4To/tmp/unneeded_directories"
#define MISSING_ROMS_MERGED "/media/4To/tmp/missing_roms_merged"
#define MISSING_DISKS_MERGED "/media/4To/tmp/missing_disks_merged"
#define MISSING_SOFTLISTS_MERGED "/media/4To/tmp/missing_softlists_merged"
#define MISSING_SOFTS_MERGED "/media/4To/tmp/missing_softs_merged"
#define UNNEEDED_FILES_MERGED "/media/4To/tmp/unneeded_files_merged"
#define UNNEEDED_DIRECTORIES_MERGED "/media/4To/tmp/unneeded_directories_merged"

llist_t * listxml;
llist_t * softlist;
int unneeded_dir = 0;
int unneeded_file = 0;

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

int emumode = MESS_MODE;
int update = 0;

char * binary = NULL;
char * roms_dir = NULL;
char * entry_type = NULL;

const char optstring[] = "?muU";
const struct option longopts[] =
        {{ "mame",no_argument,NULL,'m' },
        { "ume",no_argument,NULL,'u' },
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
	char buf[10000];
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
	char buf[10000];
	char * name;
	char * cloneof;
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
		cloneof = find_attr(current,"cloneof");

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
			if(merged && cloneof) {
				sprintf(buf,"%s/%s/%s.chd",roms_dir,cloneof,disk_name);
				if(stat(buf,&stat_info)==0) {
					confirmed_number++;
					add_confirmed(buf);
					continue;
				}
			}
			else {
				sprintf(buf,"%s/%s/%s.chd",roms_dir,name,disk_name);
				if(stat(buf,&stat_info)==0) {
					confirmed_number++;
					add_confirmed(buf);
					continue;
				}
			}
			fprintf(output_file,"%s/%s\n",name,disk_name);
			missing_disk_count++;
		}while((disk=find_next_node(disk))!=NULL);

	} while((current=find_next_node(current))!=NULL);

	printf("%d/%d disk files missing\n",missing_disk_count,needed_disk_count);
}

int check_soft_disks(char * softlist_name,char * software_name,llist_t * list,int * needed, int * missing,int merged)
{
	FILE * output_file = missing_softs;
	llist_t * current;
	char * name;
	char * cloneof = NULL;
	llist_t * current_diskarea;
	llist_t * current_disk;
	char * name_disk;
	char buf[10000];
	struct stat stat_buf;
	int found = 0;

	if( merged ) {
		output_file = missing_softs_merged;
	}

	cloneof=find_attr(list,"cloneof");

	current = find_first_node(list,"part");
	do {
		name = find_attr(current,"name");

		if(strncmp(name,"cdrom",5) && 
			strncmp(name,"hdd",3) ) {
			continue;
		}

		current_diskarea = find_first_node(current,"diskarea");
		do {
			current_disk = find_first_node(current_diskarea,"disk");
			do {
				(*needed)++;
				name_disk = find_attr(current_disk,"name");

				if(cloneof) {
					sprintf(buf,"%s/%s/%s/%s.chd",roms_dir,softlist_name,cloneof,name_disk);
					if( stat(buf,&stat_buf) == 0 ) {
						confirmed_number++;
						add_confirmed(buf);
						found = 1;
						continue;
					}
				}

				sprintf(buf,"%s/%s/%s/%s.chd",roms_dir,softlist_name,software_name,name_disk);
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
	} while((current=find_next_node(current))!=NULL);

	return found;
}

void check_softs(char * softlist_name,llist_t * list,int * needed, int * missing, int merged)
{
	FILE * output_file = missing_softs;
	llist_t * current;
	char * name;
	char buf[10000];
	struct stat stat_buf;

	current = find_first_node(list,SOFT_ENTRY_TYPE);
	do {
		name = find_attr(current,"name");

		(*needed)++;

		sprintf(buf,"%s/%s/%s.zip",roms_dir,softlist_name,name);
		if( stat(buf,&stat_buf) == 0 ) {
			confirmed_number++;
			add_confirmed(buf);
			continue;
		}
		sprintf(buf,"%s/%s/%s.7z",roms_dir,softlist_name,name);
		if( stat(buf,&stat_buf) == 0 ) {
			confirmed_number++;
			add_confirmed(buf);
			continue;
		}
//		sprintf(buf,"%s/%s/%s",roms_dir,softlist_name,name);
//		if( stat(buf,&stat_buf) == 0 ) {
			if(check_soft_disks(softlist_name,name,current,needed,missing,merged)) {
				confirmed_number++;
				add_confirmed(buf);
				continue;
			}
//		}
		fprintf(output_file,"%s\n",buf);
		(*missing)++;
	} while((current=find_next_node(current))!=NULL);
}

void check_softlists(int merged)
{
	llist_t * current;
	char buf[10000];
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
	char buf[10000];
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
//      char * type_info = (char *)arg;
        char * type_info = PARAM_LISTXML;

        data.root_node = NULL;
        data.decoding_string = 0;
        data.xml_filter = NULL;
        data.current = NULL;

	switch(emumode) {
		case MAME_MODE:
			sprintf(filename,"%s%s%s",TMP_DIR,MAME_ROOT_NODE,type_info);
			sprintf(cmd,"%s %s | tee %s ",MAME_BINARY,type_info,filename);
			break;
		case UME_MODE:
			sprintf(filename,"%s%s%s",TMP_DIR,UME_ROOT_NODE,type_info);
			sprintf(cmd,"%s %s | tee %s",UME_BINARY,type_info,filename);
			break;
		case MESS_MODE:
			sprintf(filename,"%s%s%s",TMP_DIR,MESS_ROOT_NODE,type_info);
			sprintf(cmd,"%s %s | tee %s",MESS_BINARY,type_info,filename);
			break;
		default:
			printf("Unknow emu type\n");
			exit(-1);
	}
		
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
//      char * type_info = (char *)arg;
        char * type_info = PARAM_GETSOFTLIST;

        data.root_node = NULL;
        data.decoding_string = 0;
        data.xml_filter = NULL;
        data.current = NULL;

	switch(emumode) {
		case MAME_MODE:
			sprintf(filename,"%s%s%s",TMP_DIR,MAME_ROOT_NODE,type_info);
			sprintf(cmd,"%s %s | tee %s ",MAME_BINARY,type_info,filename);
			break;
		case UME_MODE:
			sprintf(filename,"%s%s%s",TMP_DIR,UME_ROOT_NODE,type_info);
			sprintf(cmd,"%s %s | tee %s",UME_BINARY,type_info,filename);
			break;
		case MESS_MODE:
			sprintf(filename,"%s%s%s",TMP_DIR,MESS_ROOT_NODE,type_info);
			sprintf(cmd,"%s %s | tee %s",MESS_BINARY,type_info,filename);
			break;
		default:
			printf("Unknow emu type\n");
			exit(-1);
	}

        if(stat(filename,&stat_info)==0 && !update ) {
                sprintf(cmd,"/bin/cat %s",filename);
        }

        softlist = LoadXML(cmd,&data,filter);
        return NULL;
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
                        case 'm':
                                emumode = MAME_MODE;
                                break;
                        case 'u':
                                emumode = MAME_MODE;
                                break;
                        case 'U':
                                update = 1;
                                break;
                        default:
                                printf("HELP:\n\n");
                                printf("-m : use MAME instead of MESS\n");
                                printf("-u : use UME instead of MESS\n");
                                printf("-U : update cache from binaries\n");
                                exit(0);
                }
        }

	switch(emumode) {
		case MAME_MODE:
			roms_dir = MAME_ROMS_DIR;
			binary = MAME_BINARY;
			entry_type = MAME_ENTRY_TYPE;
			break;
		case UME_MODE:
			roms_dir = UME_ROMS_DIR;
			binary = UME_BINARY;
			entry_type = UME_ENTRY_TYPE;
			break;
		case MESS_MODE:
			roms_dir = MESS_ROMS_DIR;
			binary = MESS_BINARY;
			entry_type = MESS_ENTRY_TYPE;
			break;
		default:
			printf("Unknown mode\n");
			exit(-1);
        }


	missing_roms = fopen(MISSING_ROMS,"w");
	if( missing_roms == NULL ) {
		printf("Error opening %s\n",MISSING_ROMS);
		exit(1);
	}
	missing_disks = fopen(MISSING_DISKS,"w");
	if( missing_disks == NULL ) {
		printf("Error opening %s\n",MISSING_DISKS);
		exit(1);
	}
	missing_softlists = fopen(MISSING_SOFTLISTS,"w");
	if( missing_softlists == NULL ) {
		printf("Error opening %s\n",MISSING_SOFTLISTS);
		exit(1);
	}
	missing_softs = fopen(MISSING_SOFTS,"w");
	if( missing_softs == NULL ) {
		printf("Error opening %s\n",MISSING_SOFTS);
		exit(1);
	}
	unneeded_files = fopen(UNNEEDED_FILES,"w");
	if( unneeded_files == NULL ) {
		printf("Error opening %s\n",UNNEEDED_FILES);
		exit(1);
	}
	unneeded_directories = fopen(UNNEEDED_DIRECTORIES,"w");
	if( unneeded_directories == NULL ) {
		printf("Error opening %s\n",UNNEEDED_DIRECTORIES);
		exit(1);
	}

	missing_roms_merged = fopen(MISSING_ROMS_MERGED,"w");
	if( missing_roms_merged == NULL ) {
		printf("Error opening %s\n",MISSING_ROMS_MERGED);
		exit(1);
	}
	missing_disks_merged = fopen(MISSING_DISKS_MERGED,"w");
	if( missing_disks_merged == NULL ) {
		printf("Error opening %s\n",MISSING_DISKS_MERGED);
		exit(1);
	}
	missing_softlists_merged = fopen(MISSING_SOFTLISTS_MERGED,"w");
	if( missing_softlists_merged == NULL ) {
		printf("Error opening %s\n",MISSING_SOFTLISTS_MERGED);
		exit(1);
	}
	missing_softs_merged = fopen(MISSING_SOFTS_MERGED,"w");
	if( missing_softs_merged == NULL ) {
		printf("Error opening %s\n",MISSING_SOFTS_MERGED);
		exit(1);
	}
	unneeded_files_merged = fopen(UNNEEDED_FILES_MERGED,"w");
	if( unneeded_files_merged == NULL ) {
		printf("Error opening %s\n",UNNEEDED_FILES_MERGED);
		exit(1);
	}
	unneeded_directories_merged = fopen(UNNEEDED_DIRECTORIES_MERGED,"w");
	if( unneeded_directories_merged == NULL ) {
		printf("Error opening %s\n",UNNEEDED_DIRECTORIES_MERGED);
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
	//sleep(1000000);
	return 0;
}
