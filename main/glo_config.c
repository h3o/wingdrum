/*
 * glo_config.c
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Jan 31, 2019
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#include "glo_config.h"
#include "hw/gpio.h"
#include "hw/ui.h"
#include "hw/midi.h"
#include "hw/init.h"
#include "hw/codec.h"
#include "hw/Accelerometer.h"
#include <string.h>

//#define DEBUG_CONFIG_PARSING

spi_flash_mmap_handle_t mmap_handle_config;

//const char *binaural_beats_looping[BINAURAL_BEATS_AVAILABLE] = {"a:beta","b:delta","d:theta","t:alpha"}; //defines which wave follows next
//binaural_profile_t *binaural_profile;

//int channels_found, unlocked_channels_found;

void map_config_file(char **buffer) {
	//printf("map_config_file(): Mapping flash memory at %x (+%x length)\n", CONFIG_ADDRESS, CONFIG_SIZE);

	const void *ptr1;
	int esp_result = spi_flash_mmap(CONFIG_ADDRESS, (size_t) CONFIG_SIZE,
			SPI_FLASH_MMAP_DATA, &ptr1, &mmap_handle_config);
	//printf("map_config_file(): result = %d\n", esp_result);
	if (esp_result == ESP_OK) {
		//printf("map_config_file(): mapped destination = %x\n",(unsigned int)ptr1);
		//printf("map_config_file(): handle=%d ptr=%p\n", mmap_handle_config, ptr1);
		buffer[0] = (char*) ptr1;
	} else {
		printf("map_config_file(): could not map memory, result = %d\n",
				esp_result);
		while (1) {
			error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
		}; //halt
	}
}

void unmap_config_file() {
	//printf("unmap_config_file(): spi_flash_munmap(mmap_handle_config)... ");
	spi_flash_munmap(mmap_handle_config);
	//printf("unmapped!\n");
}

int read_line(char **line_ptr, char *line_buffer) {
	//read a line, look for newline or comment, whichever is found earlier
	//printf("read_line() starting from %p\n", line_ptr[0]);

	char *cr, *lf, *comment, *line_end;
	int line_length;

	cr = strstr(line_ptr[0], "\r"); //0x0D
	lf = strstr(line_ptr[0], "\n"); //0x0A
	comment = strstr(line_ptr[0], "//");

	//printf("read_line() cr=%p, lf=%p, comment=%p\n", cr, lf, comment);

	line_end = NULL;

	if (comment != NULL) {
		line_end = comment;
	} else if (cr != NULL) {
		line_end = cr;
	} else if (lf != NULL) {
		line_end = lf;
	}

	if (comment < line_end && comment != NULL) {
		line_end = comment;
	}
	if (cr < line_end && cr != NULL) {
		line_end = cr;
	}
	if (lf < line_end && lf != NULL) {
		line_end = lf;
	}

	//printf("read_line() line_end=%p\n", line_end);

	//printf("line %d: nearest cr = %p, lf = %p, comment = %p, line end at = %p\n", lines_parsed, cr, lf, comment, line_end);

	if (line_end == NULL) {
		printf("read_line() no line_end found, returning 0\n");
		return 0;
	}

	line_length = (int) (line_end - line_ptr[0]);
	//printf("read_line() line_end found, line_length=%d\n", line_length);

	if (line_length > CONFIG_LINE_MAX_LENGTH) {
		printf(
				"read_line(): detected line length (%d) longer than CONFIG_LINE_MAX_LENGTH (%d)\n",
				line_length, CONFIG_LINE_MAX_LENGTH);
		while (1) {
			error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
		}; //halt
	}
	strncpy(line_buffer, line_ptr[0], line_length);

	//roll back spaces and tabs
	int space_or_tab_found = 1;
	while (space_or_tab_found) {
		space_or_tab_found = 0;
		while (line_buffer[line_length - 1] == '\t') {
			line_length--;
			space_or_tab_found = 1;
		}
		while (line_buffer[line_length - 1] == ' ') {
			line_length--;
			space_or_tab_found = 1;
		}
	}

	line_buffer[line_length] = 0;
	//printf("read_line() string copied over\n");

	line_ptr[0] = lf + 1; //move the pointer to new line
	return line_length;
}

/*
expected format in config file:
sample_number: position,length //comment

sample_number starts at 1, because 0th element holds number of found samples and highest sample number found
both position and length are in bytes

if the format only contains two numbers, the second one is treated as sample length (in bytes):
sample_number: length //comment
*/

void get_samples_map(int *result) {
	char *config_buffer;
	map_config_file(&config_buffer);
	//printf("get_samples_map(): map_config_file returned config_buffer=%p\n", config_buffer);

	char *line_ptr = config_buffer;
	int line_length, lines_parsed = 0;
	char *line_buffer = (char*) malloc(CONFIG_LINE_MAX_LENGTH + 2);
	char *lptr;

	//parse data for samples map table
	int done = 0;
	int samples_map_found = 0;
	int sample_n;
	uint32_t sum_lengths = 0;

	result[0] = 0; //contains number of results found
	result[1] = 0; //contains highest sample number found

	while (!done) {
		//printf("get_samples_map(): reading line %d\n", lines_parsed);
		line_length = read_line(&line_ptr, line_buffer);
		//printf("get_samples_map(): line %d read, length = %d, line = \"%s\"\n", lines_parsed, line_length, line_buffer);

		if (line_length) {
			if (samples_map_found && line_length > 3) {
				if (line_buffer[1] == ':' || line_buffer[2] == ':') //looks like a valid line
						{
					result[0]++;
					//result[result[0]] = atoi(line_buffer+2);
					sample_n = atoi(line_buffer);
					if (sample_n > result[1]) {
						result[1] = sample_n;
					}
					//printf("get_samples_map(): found sample #%d\n", sample_n);

					lptr = line_buffer;
					do {
						lptr++;
					} while (lptr[0] != ':');
					lptr++;
					result[2 * sample_n] = atoi(lptr);
					do {
						lptr++;
					} while (lptr[0] != ',');
					lptr++;
					result[2 * sample_n + 1] = atoi(lptr);

					//if length is stored as zero, this means only lengths are stored and positions need to be calculated
					if(result[2 * sample_n + 1]==0)
					{
						result[2 * sample_n + 1] = result[2 * sample_n];
						result[2 * sample_n] = sum_lengths;
						sum_lengths += result[2 * sample_n + 1];
					}
				}
			}

			if (0 == strcmp(line_buffer, "[samples_map]")) {
				samples_map_found = 1;
				//printf("[samples map] found\n");
			} else if (samples_map_found && line_buffer[0] == '[') {
				done = 1;
				//printf("[samples map] block end\n");
			} else if (0 == strcmp(line_buffer, "[end]")) {
				printf("[samples map] block not found in config file!\n");
				while (1) {
					error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
				}; //halt
			}

		}

		if (result[0] == SAMPLES_MAX) {
			done = 1;
		}

		lines_parsed++;
	}

	//parse done, release the buffer and mapped memory
	free(line_buffer);
	unmap_config_file();
}

char *get_line_in_block(const char *block_name, char *prefix)
{
	//printf("get_line_in_block(): looking for record with prefix \"%s\" in block %s\n", prefix, block_name);

	char *line = NULL;

	char *config_buffer;
	map_config_file(&config_buffer);
	//printf("get_line_in_block(): map_config_file returned config_buffer=%p\n", config_buffer);

	char *line_ptr = config_buffer;
	int line_length, lines_parsed = 0;
	char *line_buffer = (char*) malloc(CONFIG_LINE_MAX_LENGTH + 2);

	int done = 0;
	int block_found = 0;
	//int prefix_found = 0;

	while (!done) {
		//printf("get_line_in_block(): reading line %d\n", lines_parsed);
		line_length = read_line(&line_ptr, line_buffer);
		//printf("get_line_in_block(): line %d read, length = %d, line = \"%s\"\n", lines_parsed, line_length, line_buffer);

		if (!done && line_length) {

			if (block_found && line_length > 2 && line_buffer[0] != '[') {

				if (!strncmp(line_buffer,prefix,strlen(prefix))) { //prefix found
					//printf("get_line_in_block(): prefix \"%s\" found\n", prefix);

					line = malloc(line_length+2);
					strcpy(line,line_buffer);
					done = 1;
					break;
				}
			}

			if (0 == strncmp(line_buffer, block_name, strlen(block_name))) {
				block_found = 1;
				//printf("get_line_in_block(): block %s found\n", block_name);
			} else if (block_found && line_buffer[0] == '[') {
				printf("get_line_in_block(): block %s ended\n", block_name);
				done = 1;
				break;
			} else if (0 == strcmp(line_buffer, "[end]")) {
				printf("get_line_in_block(): block %s not found in config file!\n", block_name);
				error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
			}
		}

		lines_parsed++;
	}
	//printf("get_line_in_block(): done parsing\n");

	//parse done, release the buffer and mapped memory
	free(line_buffer);
	unmap_config_file();

	return line;
}

#define MAX_LINES_IN_BLOCK 100

char **get_all_lines_in_block(const char *block_name, char *prefix, int *lines_found)
{
	char *lines_collection[MAX_LINES_IN_BLOCK];
	lines_found[0] = 0;

	#ifdef DEBUG_CONFIG_PARSING
	printf("get_all_lines_in_block(): looking for records with prefix \"%s\" in block %s\n", prefix, block_name);
	#endif

	//char *line = NULL;

	char *config_buffer;
	map_config_file(&config_buffer);
	//printf("get_all_lines_in_block(): map_config_file returned config_buffer=%p\n", config_buffer);

	char *line_ptr = config_buffer;
	int line_length, lines_parsed = 0;
	char *line_buffer = (char*) malloc(CONFIG_LINE_MAX_LENGTH + 2);

	int done = 0;
	int block_found = 0;
	//int prefix_found = 0;

	while (!done) {
		//printf("get_line_in_block(): reading line %d\n", lines_parsed);
		line_length = read_line(&line_ptr, line_buffer);
		//printf("get_line_in_block(): line %d read, length = %d, line = \"%s\"\n", lines_parsed, line_length, line_buffer);

		if (!done && line_length) {

			if (block_found && line_length > 2 && line_buffer[0] != '[') {

				if (!strncmp(line_buffer,prefix,strlen(prefix))) { //prefix found
					//printf("get_all_lines_in_block(): prefix \"%s\" found, storing as line #%d\n", prefix, lines_found[0]);

					//line = malloc(line_length+2);
					lines_collection[lines_found[0]] = malloc(line_length+2);
					strcpy(lines_collection[lines_found[0]],line_buffer);
					//printf("get_all_lines_in_block(): line #%d stored at address %p\n", lines_found[0], lines_collection[lines_found[0]]);

					lines_found[0]++;
					//done = 1;
					//break;#
					continue;
				}
			}

			if (0 == strncmp(line_buffer, block_name, strlen(block_name))) {
				block_found = 1;
				//printf("get_all_lines_in_block(): block %s found\n", block_name);
			} else if (block_found && line_buffer[0] == '[') {
				//printf("get_all_lines_in_block(): block %s ended\n", block_name);
				done = 1;
				break;
			} else if (0 == strcmp(line_buffer, "[end]")) {
				printf("get_all_lines_in_block(): block %s not found in config file!\n", block_name);
				error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
			}
		}

		lines_parsed++;
	}
	//printf("get_all_lines_in_block(): done parsing\n");

	//parse done, release the buffer and mapped memory
	free(line_buffer);
	unmap_config_file();

	//printf("get_all_lines_in_block(): done parsing, total of %d lines found\n", lines_found[0]);

	//return line;

	//printf("get_all_lines_in_block(): allocating %d bytes for pointers\n", lines_found[0] * sizeof(char*));
	char **lines_collection_ret = malloc(lines_found[0] * sizeof(char*));
	//printf("get_all_lines_in_block(): allocated, pointer = %p\n", lines_collection_ret);

	//printf("get_all_lines_in_block(): copying lines_collection -> lines_collection_ret, %d bytes total\n", lines_found[0] * sizeof(char*));
	memcpy(lines_collection_ret, lines_collection, lines_found[0] * sizeof(char*));

	//printf("get_all_lines_in_block(): returning pointer %p\n", lines_collection_ret);
	return lines_collection_ret;
}

int get_scale(char *scale_name, char **buffer)
{
	if(strlen(scale_name)>SCALE_NAME_MAX_LENGTH)
	{
		printf("get_scale(): scale name (or list) too long: %d chars, maximum = %d\n", strlen(scale_name), SCALE_NAME_MAX_LENGTH);
	}

	char prefix[SCALE_NAME_MAX_LENGTH];
	strcpy(prefix,scale_name);
	//printf("get_scale(): prefix[1] = %s\n", prefix);

	char *p = strstr(prefix, ",");

	if(p)
	{
		//printf("get_scale(): strstr found: %s\n", p);
		p[0]=0;
	}
	//else
	//{
	//	printf("get_scale(): strstr returned NULL\n");
	//}

	//printf("get_scale(): prefix[2] = %s\n", prefix);

	sprintf(prefix+strlen(prefix),":");

	//printf("get_scale(): prefix[3] = %s\n", prefix);

	buffer[0] = get_line_in_block("[scales]",prefix); //this will allocate memory for buffer
	if(buffer[0]==NULL)
	{
		printf("get_scale(): [scales] record with prefix \"%s\" not found\n", prefix);
		return 0;
	}

	//number of notes is easy to tell by how many commas there are
	int notes_found = 0;
	char *pch=strchr(buffer[0],',');
	while (pch!=NULL)
	{
		notes_found++;
	    pch=strchr(pch+1,',');
	}

	notes_found++; //there is expected to be one more note than commas

	//printf("get_scale(): line with %d notes found in config: \"%s\"\n", notes_found, buffer[0]);

	return notes_found;
}

int get_tuning_coeffs(float **tuning_coeffs, int *coeffs_found)
{
	char **block_lines = get_all_lines_in_block("[samples_tuning]", "", coeffs_found); //this will allocate memory for buffer
	#ifdef DEBUG_CONFIG_PARSING
	printf("get_tuning_coeffs(): get_all_lines_in_block() returned %d coeffs, structure pointer = %p\n", coeffs_found[0], block_lines);
	#endif

	int highest_sample = 0, s_n;

	for(int i=0;i<coeffs_found[0];i++)
	{
		s_n = atoi(block_lines[i]);
		if(s_n>highest_sample)
		{
			highest_sample = s_n;
		}
	}
	#ifdef DEBUG_CONFIG_PARSING
	printf("get_tuning_coeffs(): highest sample # found = %d\n", highest_sample);
	#endif
	tuning_coeffs[0] = malloc((highest_sample+1)*sizeof(float));
	memset(tuning_coeffs[0], 0, (highest_sample+1)*sizeof(float));
	#ifdef DEBUG_CONFIG_PARSING
	printf("get_tuning_coeffs(): allocated %d bytes for tuning_coeffs, structure pointer = %p\n", (highest_sample+1)*sizeof(float), tuning_coeffs);
	#endif

	char *line_ptr;

	for(int i=0;i<coeffs_found[0];i++)
	{
		#ifdef DEBUG_CONFIG_PARSING
		printf("coeffs line %d at address %p: \"%s\"\n", i, block_lines[i], block_lines[i]);
		#endif

		line_ptr = block_lines[i];
		s_n = atoi(line_ptr);
		line_ptr = strstr(block_lines[i], ":");
		line_ptr++;
		tuning_coeffs[0][s_n] = atof(line_ptr);

		free(block_lines[i]);
	}
	free(block_lines);

	return highest_sample+1;
}

int get_patches(char *group_name, int **samples, /*float **tuning,*/ int ***notes)
{
	int patches_found;
	char prefix[20];
	sprintf(prefix,"%s:",group_name);
	/*buffer[0] =*/
	char **block_lines = get_all_lines_in_block("[default_order]", prefix, &patches_found); //this will allocate memory for buffer
	#ifdef DEBUG_CONFIG_PARSING
	printf("get_patches(): get_all_lines_in_block() returned %d patches, structure pointer = %p\n", patches_found, block_lines);
	#endif

	samples[0] = malloc(patches_found * sizeof(int));
	//tuning[0] = malloc(patches_found * sizeof(float));
	notes[0] = malloc(patches_found * sizeof(int*));

	char *line_ptr, *scale_notes;

	for(int i=0;i<patches_found;i++)
	{
		#ifdef DEBUG_CONFIG_PARSING
		printf("patch line %d at address %p: \"%s\"\n", i, block_lines[i], block_lines[i]);
		#endif
		//samples[0][i] = i+10;
		//tuning[0][i] = (float)i/10;
		notes[0][i] = malloc(NOTES_PER_SCALE * SCALES_PER_PATCH * sizeof(int));
		memset(notes[0][i], 0, NOTES_PER_SCALE * SCALES_PER_PATCH * sizeof(int));
		/*
		for(int n=0;n<9;n++)
		{
			notes[0][i][n] = i*n;
		}
		*/

		line_ptr = strstr(block_lines[i], ":");
		line_ptr++;
		samples[0][i] = atoi(line_ptr);
		//line_ptr = strstr(block_lines[i], ",");
		//line_ptr++;
		//tuning[0][i] = (float)atof(line_ptr);
		int scale_number = 0;

		do
		{
			//printf("do loop, scale_number = %d\n", scale_number);
			//printf("strlen(line_ptr) = %d\n", strlen(line_ptr));

			if(line_ptr==NULL)
			{
				//printf("line_ptr = NULL!\n");
				return 0;
			}

			line_ptr = strstr(line_ptr, ",");
			//printf("line_ptr = 0x%x\n", (uint32_t)line_ptr);
			//if(line_ptr==NULL)
			//{
			//	printf("comma not found\n");
			//}

			if(line_ptr!=NULL)
			{
				//printf("comma found: \"%s\"\n", line_ptr);
				line_ptr++;

				if(strlen(line_ptr) && strlen(line_ptr) < SCALE_NAME_MAX_LENGTH)
				{
					#ifdef DEBUG_CONFIG_PARSING
					printf("looking up scale by name: \"%s\"\n", line_ptr);
					#endif

					int notes_found = get_scale(line_ptr, &scale_notes);

					if(notes_found)
					{
						if(notes_found != NOTES_PER_SCALE)
						{
							printf("get_scale(): unexpected amount of notes: %d (should be always 9) in \"%s\"\n", notes_found, line_ptr);
							error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
							//halt

						}
						else
						{
							#ifdef DEBUG_CONFIG_PARSING
							printf("get_scale(): correct amount of notes found: %d\n", notes_found);
							printf("get_scale() returned notes: \"%s\"\n", scale_notes);
							#endif

							char *note_ptr = strstr(scale_notes, ":");
							note_ptr++;
							while(note_ptr[0]==' ' || note_ptr[0]=='\t')
							{
								note_ptr++;
							}
							#ifdef DEBUG_CONFIG_PARSING
							printf("get_scale() returned %d notes: \"%s\"\n", notes_found, note_ptr);
							#endif

							for(int p=0;p<strlen(note_ptr);p++)
							{
								note_ptr[p] = tolower(note_ptr[p]);
							}

							for(int n=0;n<9;n++)
							{
								//printf("storing a note to notes[0][%d][%d]\n", i, scale_number*NOTES_PER_SCALE+n);
								notes[0][i][scale_number*NOTES_PER_SCALE+n] = parse_note(note_ptr);
								note_ptr = strstr(note_ptr, ",");
								note_ptr++;
							}
							//printf("storing notes done\n");
						}
						free(scale_notes);
					}
					else
					{
						printf("scale \"%s\" not found\n", line_ptr);
						error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
						//halt
					}
				}
				else
				{
					printf("unexpected scale name, length = %d\n", strlen(line_ptr));
					error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
					//halt
				}

				//printf("scale #%d found, name = \"%s\"\n", scale_number, line_ptr);
				scale_number++;
			}
		}
		while(line_ptr!=NULL);

		free(block_lines[i]);
	}
	free(block_lines);

	/*
	if(buffer[0]==NULL)
	{
		printf("get_patches(): no [default_order] record with prefix \"%s\" found\n", prefix);
		return 0;
	}
	printf("get_patches(): total of %d [default_order] record(s) with prefix \"%s\" found\n", patches_found, prefix);
	*/

	/*
	//number of notes is easy to tell by how many commas there are
	int notes_found = 0;
	char *pch=strchr(buffer[0],',');
	while (pch!=NULL)
	{
		notes_found++;
	    pch=strchr(pch+1,',');
	}

	notes_found++; //there is expected to be one more note than commas

	printf("get_patches(): line with %d notes found in config: \"%s\"\n", notes_found, buffer[0]);

	return notes_found;
	*/
	return patches_found;
}

int get_reverb_scale(char *patch_name, int patch_n, int ***notes_buffer)
{
	int patches_found;
	char prefix[20];
	sprintf(prefix,"%s:",patch_name);
	char **block_lines = get_all_lines_in_block("[default_order]", prefix, &patches_found); //this will allocate memory for buffer
	#ifdef DEBUG_CONFIG_PARSING
	printf("get_reverb_scale(): get_all_lines_in_block() returned %d patches, structure pointer = %p\n", patches_found, block_lines);
	#endif

	//samples[0] = malloc(patches_found * sizeof(int));
	//tuning[0] = malloc(patches_found * sizeof(float));
	notes_buffer[0] = malloc(patches_found * sizeof(int*));

	char *line_ptr, *scale_notes;

	for(int i=0;i<patches_found;i++)
	{
		#ifdef DEBUG_CONFIG_PARSING
		printf("patch line %d at address %p: \"%s\"\n", i, block_lines[i], block_lines[i]);
		#endif
		//samples[0][i] = i+10;
		//tuning[0][i] = (float)i/10;
		notes_buffer[0][i] = malloc(NOTES_PER_SCALE * SCALES_PER_PATCH * sizeof(int));
		memset(notes_buffer[0][i], 0, NOTES_PER_SCALE * SCALES_PER_PATCH * sizeof(int));
		/*
		for(int n=0;n<9;n++)
		{
			notes[0][i][n] = i*n;
		}
		*/

		line_ptr = strstr(block_lines[i], ":");
		line_ptr++;
		//samples[0][i] = atoi(line_ptr);
		//line_ptr = strstr(block_lines[i], ",");
		//line_ptr++;
		//tuning[0][i] = (float)atof(line_ptr);
		int scale_number = 0;

		do
		{
			//printf("do loop, scale_number = %d\n", scale_number);
			//printf("strlen(line_ptr) = %d\n", strlen(line_ptr));

			if(line_ptr==NULL)
			{
				//printf("line_ptr = NULL!\n");
				return 0;
			}

			line_ptr = strstr(line_ptr, ",");
			//printf("line_ptr = 0x%x\n", (uint32_t)line_ptr);
			//if(line_ptr==NULL)
			//{
			//	printf("comma not found\n");
			//}

			if(line_ptr!=NULL)
			{
				//printf("comma found: \"%s\"\n", line_ptr);
				line_ptr++;

				if(strlen(line_ptr) && strlen(line_ptr) < SCALE_NAME_MAX_LENGTH)
				{
					#ifdef DEBUG_CONFIG_PARSING
					printf("looking up scale by name: \"%s\"\n", line_ptr);
					#endif

					int notes_found = get_scale(line_ptr, &scale_notes);

					if(notes_found)
					{
						if(notes_found != NOTES_PER_SCALE)
						{
							printf("get_scale(): unexpected amount of notes: %d (should be always 9) in \"%s\"\n", notes_found, line_ptr);
							error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
							//halt

						}
						else
						{
							#ifdef DEBUG_CONFIG_PARSING
							printf("get_scale(): correct amount of notes found: %d\n", notes_found);
							printf("get_scale() returned notes: \"%s\"\n", scale_notes);
							#endif

							char *note_ptr = strstr(scale_notes, ":");
							note_ptr++;
							while(note_ptr[0]==' ' || note_ptr[0]=='\t')
							{
								note_ptr++;
							}
							#ifdef DEBUG_CONFIG_PARSING
							printf("get_scale() returned %d notes: \"%s\"\n", notes_found, note_ptr);
							#endif

							for(int p=0;p<strlen(note_ptr);p++)
							{
								note_ptr[p] = tolower(note_ptr[p]);
							}

							for(int n=0;n<9;n++)
							{
								//printf("storing a note to notes_buffer[0][%d][%d]\n", i, scale_number*NOTES_PER_SCALE+n);
								notes_buffer[0][i][scale_number*NOTES_PER_SCALE+n] = parse_note(note_ptr);
								note_ptr = strstr(note_ptr, ",");
								note_ptr++;
							}
							//printf("storing notes done\n");
						}
						free(scale_notes);
					}
					else
					{
						printf("scale \"%s\" not found\n", line_ptr);
						error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
						//halt
					}
				}
				else
				{
					printf("unexpected scale name, length = %d\n", strlen(line_ptr));
					error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
					//halt
				}

				//printf("scale #%d found, name = \"%s\"\n", scale_number, line_ptr);
				scale_number++;
			}
		}
		while(line_ptr!=NULL);

		free(block_lines[i]);
	}
	free(block_lines);

	return patches_found;
}

int load_settings(settings_t *settings, const char* block_name)
{
	char *config_buffer;
	map_config_file(&config_buffer);
	//printf("load_settings(): map_config_file returned config_buffer=%p\n", config_buffer);

	/*
	printf("load_settings(): config buffer first bytes = %02x %02x %02x %02x %02x %02x %02x %02x\n",
			config_buffer[0],
			config_buffer[1],
			config_buffer[2],
			config_buffer[3],
			config_buffer[4],
			config_buffer[5],
			config_buffer[6],
			config_buffer[7]);
	*/

	if(config_buffer[0]==0xff && config_buffer[1]==0xff && config_buffer[2]==0xff && config_buffer[3]==0xff &&
	   config_buffer[4]==0xff && config_buffer[5]==0xff && config_buffer[6]==0xff && config_buffer[7]==0xff)
	{
		return -1;
	}

	char *line_ptr = config_buffer;
	int line_length, lines_parsed = 0;
	char *line_buffer = (char*) malloc(CONFIG_LINE_MAX_LENGTH + 2);

	int done = 0;
	int block_found = 0;
	//float total_length = 0, correction = 0;

	while (!done) {
		//printf("load_settings(): reading line %d\n", lines_parsed);
		line_length = read_line(&line_ptr, line_buffer);
		//printf("load_settings(): line %d read, length = %d, line = \"%s\"\n", lines_parsed, line_length, line_buffer);

		if (line_length) {
			if (block_found && line_length > 2 && line_buffer[0] != '[') {

				int item_name_length = 0;
				while(line_buffer[item_name_length]!=' ' && line_buffer[item_name_length]!='\t') item_name_length++;

				//if(!strncmp(line_buffer,"AUTO_POWER_OFF_ONLY_IF_NO_MOTION",32)) { settings->AUTO_POWER_OFF_ONLY_IF_NO_MOTION = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"AUTO_POWER_OFF_TIMEOUT_ALL",26)) { settings->AUTO_POWER_OFF_TIMEOUT_ALL = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"AUTO_POWER_OFF_TIMEOUT_REVERB",29)) { settings->AUTO_POWER_OFF_TIMEOUT_REVERB = atoi(line_buffer+item_name_length); }
				//if(!strncmp(line_buffer,"DEFAULT_ACCESSIBLE_CHANNELS",27)) { settings->DEFAULT_ACCESSIBLE_CHANNELS = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TUNING_DEFAULT",14)) { settings->TUNING_DEFAULT = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TUNING_MIN",10)) { settings->TUNING_MIN = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TUNING_MAX",10)) { settings->TUNING_MAX = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"MICRO_TUNING_STEP",17)) { settings->MICRO_TUNING_STEP = atof(line_buffer+item_name_length); }
				/*
				if(!strncmp(line_buffer,"GRANULAR_DETUNE_COEFF_SET",25)) { settings->GRANULAR_DETUNE_COEFF_SET = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"GRANULAR_DETUNE_COEFF_MUL",25)) { settings->GRANULAR_DETUNE_COEFF_MUL = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"GRANULAR_DETUNE_COEFF_MAX",25)) { settings->GRANULAR_DETUNE_COEFF_MAX = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TEMPO_BPM_DEFAULT",17)) { settings->TEMPO_BPM_DEFAULT = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TEMPO_BPM_MIN",13)) { settings->TEMPO_BPM_MIN = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TEMPO_BPM_MAX",13)) { settings->TEMPO_BPM_MAX = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TEMPO_BPM_STEP",14)) { settings->TEMPO_BPM_STEP = atoi(line_buffer+item_name_length); }
				*/

				if(!strncmp(line_buffer,"AGC_ENABLED",11)) { settings->AGC_ENABLED = atoi(line_buffer+item_name_length); }
				else if(!strncmp(line_buffer,"AGC_MAX_GAIN_STEP",17)) { settings->AGC_MAX_GAIN_STEP = atoi(line_buffer+item_name_length); }
				else if(!strncmp(line_buffer,"AGC_MAX_GAIN_LIMIT",18)) { settings->AGC_MAX_GAIN_LIMIT = atoi(line_buffer+item_name_length); }
				else if(!strncmp(line_buffer,"AGC_MAX_GAIN",12)) { settings->AGC_MAX_GAIN = atoi(line_buffer+item_name_length); }
				else if(!strncmp(line_buffer,"AGC_TARGET_LEVEL",16)) { settings->AGC_TARGET_LEVEL = atoi(line_buffer+item_name_length); }
				else if(!strncmp(line_buffer,"MIC_BIAS",8)) { settings->MIC_BIAS = atoi(line_buffer+item_name_length); }

				if(!strncmp(line_buffer,"CODEC_ANALOG_VOLUME_DEFAULT",27)) { settings->CODEC_ANALOG_VOLUME_DEFAULT = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"CODEC_DIGITAL_VOLUME_DEFAULT",28)) { settings->CODEC_DIGITAL_VOLUME_DEFAULT = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"CLOUDS_HARD_LIMITER_POSITIVE",28)) { settings->CLOUDS_HARD_LIMITER_POSITIVE = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"CLOUDS_HARD_LIMITER_NEGATIVE",28)) { settings->CLOUDS_HARD_LIMITER_NEGATIVE = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"CLOUDS_HARD_LIMITER_MAX",23)) { settings->CLOUDS_HARD_LIMITER_MAX = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"CLOUDS_HARD_LIMITER_STEP",24)) { settings->CLOUDS_HARD_LIMITER_STEP = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"ACCELEROMETER_CALIBRATE_ON_POWERUP",34)) { settings->ACCELEROMETER_CALIBRATE_ON_POWERUP = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"SAMPLE_FORMAT_HEADER_LENGTH",27)) { settings->SAMPLE_FORMAT_HEADER_LENGTH = atoi(line_buffer+item_name_length); }
				/*
				if(!strncmp(line_buffer,"DRUM_THRESHOLD_ON",17)) { settings->DRUM_THRESHOLD_ON = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"DRUM_THRESHOLD_OFF",18)) { settings->DRUM_THRESHOLD_OFF = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"DRUM_LENGTH1",12)) { settings->DRUM_LENGTH1 = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"DRUM_LENGTH2",12)) { settings->DRUM_LENGTH2 = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"DRUM_LENGTH3",12)) { settings->DRUM_LENGTH3 = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"DRUM_LENGTH4",12)) { settings->DRUM_LENGTH4 = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"IDLE_SET_RST_SHORTCUT",21)) { settings->IDLE_SET_RST_SHORTCUT = atol(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"IDLE_RST_SET_SHORTCUT",21)) { settings->IDLE_RST_SET_SHORTCUT = atol(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"IDLE_LONG_SET_SHORTCUT",22)) { settings->IDLE_LONG_SET_SHORTCUT = atol(line_buffer+item_name_length); }
				*/
				if(!strncmp(line_buffer,"SCALE_PLAY_SPEED_MICROTUNING",28)) { settings->SCALE_PLAY_SPEED_MICROTUNING = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"SCALE_PLAY_SPEED_TRANSPOSE",26)) { settings->SCALE_PLAY_SPEED_TRANSPOSE = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"SCALE_PLAY_SPEED_INITIAL",24)) { settings->SCALE_PLAY_SPEED_INITIAL = atoi(line_buffer+item_name_length); }

				if(!strncmp(line_buffer,"TOUCHPAD_LED_THRESHOLD_A_MUL",28)) { settings->TOUCHPAD_LED_THRESHOLD_A_MUL = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TOUCHPAD_LED_THRESHOLD_A_DIV",28)) { settings->TOUCHPAD_LED_THRESHOLD_A_DIV = atoi(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"TOUCH_PAD_DEFAULT_MAX",21)) { settings->TOUCH_PAD_DEFAULT_MAX = atoi(line_buffer+item_name_length); }

				if(!strncmp(line_buffer,"SPLIT_TIMBRE_NOTE_THRESHOLD1_COEFF",34)) { settings->SPLIT_TIMBRE_NOTE_THRESHOLD1_COEFF = atof(line_buffer+item_name_length); }
				if(!strncmp(line_buffer,"SPLIT_TIMBRE_NOTE_THRESHOLD2_COEFF",34)) { settings->SPLIT_TIMBRE_NOTE_THRESHOLD2_COEFF = atof(line_buffer+item_name_length); }
			}

			if (0 == strcmp(line_buffer, block_name)) {
				block_found = 1;
				//printf("[voice_menu] found\n");
			} else if (block_found && line_buffer[0] == '[') {
				done = 1;
				//printf("[voice_menu] block end\n");
			} else if (0 == strcmp(line_buffer, "[end]")) {
				printf("%s block not found in config file!\n", block_name);
				while (1) {
					error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
				}; //halt
			}
		}

		lines_parsed++;
	}

	//parse done, release the buffer and mapped memory
	free(line_buffer);
	unmap_config_file();

	return block_found;
}

void load_persistent_settings(persistent_settings_t *settings)
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_settings", NVS_READONLY, &handle);
	if(res!=ESP_OK)
	{
		printf("load_persistent_settings(): problem with nvs_open() in ");
		printf("R/O mode, error = %d\n", res);

		//maybe it was not created yet, will try
		res = nvs_open("drum_settings", NVS_READWRITE, &handle);
		if(res!=ESP_OK)
		{
			printf("load_persistent_settings(): problem with nvs_open() in ");
			printf("R/W mode, error = %d\n", res);
			return;
		}
	}

	int8_t val_i8;
	int16_t val_i16;
	int32_t val_i32;
	//uint32_t val_u32;
	//uint64_t val_u64;
	float val_float;

	res = nvs_get_i16(handle, "AV", &val_i16);
	if(res!=ESP_OK) { val_i16 = global_settings.CODEC_ANALOG_VOLUME_DEFAULT; } //if the key does not exist, load default value
	settings->ANALOG_VOLUME = val_i16;

	res = nvs_get_i16(handle, "DV", &val_i16);
	if(res!=ESP_OK) { val_i16 = global_settings.CODEC_DIGITAL_VOLUME_DEFAULT; } //if the key does not exist, load default value
	settings->DIGITAL_VOLUME = val_i16;

	res = nvs_get_i8(handle, "EQ_BASS", &val_i8);
	if(res!=ESP_OK) { val_i8 = EQ_BASS_DEFAULT; }
	settings->EQ_BASS = val_i8;

	res = nvs_get_i8(handle, "EQ_TREBLE", &val_i8);
	if(res!=ESP_OK) { val_i8 = EQ_TREBLE_DEFAULT; }
	settings->EQ_TREBLE = val_i8;
	/*
	res = nvs_get_i16(handle, "TEMPO", &val_i16);
	if(res!=ESP_OK) {
		val_i16 = global_settings.TEMPO_BPM_DEFAULT;
		printf("load_persistent_settings(): TEMPO not found, loading default = %d\n", val_i16);
	}
	settings->TEMPO = val_i16;

	res = nvs_get_u64(handle, "TUNING", &val_u64);
	if(res!=ESP_OK) {
		settings->FINE_TUNING = global_settings.TUNING_DEFAULT;
		printf("load_persistent_settings(): FINE_TUNING(TUNING) not found, loading default = %f\n", global_settings.TUNING_DEFAULT);
	}
	else
	{
		settings->FINE_TUNING = ((double*)&val_u64)[0];
	}

	res = nvs_get_i8(handle, "TRN", &val_i8);
	if(res!=ESP_OK) {
		val_i8 = 0;
		printf("load_persistent_settings(): TRANSPOSE not found, loading default = %d\n", val_i8);
	}
	settings->TRANSPOSE = val_i8;

	res = nvs_get_i8(handle, "BEEPS", &val_i8);
	if(res!=ESP_OK) { val_i8 = 1; }
	settings->BEEPS = val_i8;

	res = nvs_get_i8(handle, "XTRA_CHNL", &val_i8);
	if(res!=ESP_OK) {
		val_i8 = 0;
		printf("load_persistent_settings(): ALL_CHANNELS_UNLOCKED(XTRA_CHNL) not found, loading default = %d\n", val_i8);
	}
	settings->ALL_CHANNELS_UNLOCKED = val_i8;

	res = nvs_get_i8(handle, "MIDI_SYNC", &val_i8);
	if(res!=ESP_OK) { val_i8 = MIDI_SYNC_MODE_DEFAULT; }
	settings->MIDI_SYNC_MODE = val_i8;

	res = nvs_get_i8(handle, "MIDI_POLY", &val_i8);
	if(res!=ESP_OK) { val_i8 = MIDI_POLYPHONY_DEFAULT; }
	settings->MIDI_POLYPHONY = val_i8;
	*/

	#ifdef BOARD_GECHO
	res = nvs_get_i8(handle, "SENSORS", &val_i8);
	if(res!=ESP_OK) { val_i8 = PARAMETER_CONTROL_SENSORS_DEFAULT; }
	settings->PARAMS_SENSORS = val_i8;
	#endif

	res = nvs_get_i8(handle, "AGC_PGA", &val_i8);
	if(res!=ESP_OK) { val_i8 = global_settings.AGC_ENABLED ? global_settings.AGC_TARGET_LEVEL : 0; }
	settings->AGC_ENABLED_OR_PGA = val_i8;

	res = nvs_get_i8(handle, "AGC_G", &val_i8);
	if(res!=ESP_OK) { val_i8 = global_settings.AGC_MAX_GAIN; }
	settings->AGC_MAX_GAIN = val_i8;

	res = nvs_get_i8(handle, "MIC_B", &val_i8);
	if(res!=ESP_OK) { val_i8 = global_settings.MIC_BIAS; }
	settings->MIC_BIAS = val_i8;

	/*
	res = nvs_get_i8(handle, "LEDS_OFF", &val_i8);
	if(res!=ESP_OK) { val_i8 = 0; }
	settings->ALL_LEDS_OFF = val_i8;

	res = nvs_get_i8(handle, "PWR_OFF", &val_i8);
	if(res!=ESP_OK) { val_i8 = 6; / * x10 minutes = 1 hour * / }
	settings->AUTO_POWER_OFF = val_i8;

	res = nvs_get_i16(handle, "FS", &val_i16);
	if(res!=ESP_OK) { val_i16 = SAMPLE_RATE_DEFAULT; } //if the key does not exist, load default value
	settings->SAMPLING_RATE = val_i16;

	res = nvs_get_i8(handle, "SD_CLK", &val_i8);
	if(res!=ESP_OK) { val_i8 = 1; / *high speed* / }
	settings->SD_CARD_SPEED = val_i8;

	res = nvs_get_i8(handle, "ACC_O", &val_i8);
	if(res!=ESP_OK) { val_i8 = ACC_ORIENTATION_DEFAULT; }
	settings->ACC_ORIENTATION = val_i8;

	res = nvs_get_i8(handle, "ACC_I", &val_i8);
	if(res!=ESP_OK) { val_i8 = ACC_INVERT_DEFAULT; }
	settings->ACC_INVERT = val_i8;
	*/

	res = nvs_get_u32(handle, "CAL_X", (uint32_t *)&val_float);
	if(res!=ESP_OK) { val_float = 0; }
	settings->ACC_CALIBRATION_X = val_float;

	res = nvs_get_u32(handle, "CAL_Y", (uint32_t *)&val_float);
	if(res!=ESP_OK) { val_float = 0; }
	settings->ACC_CALIBRATION_Y = val_float;

	res = nvs_get_i32(handle, "EL_S", &val_i32);
	if(res!=ESP_OK) { val_i32 = 0; }
	settings->ECHO_LOOP_LENGTH_STEP = val_i32;

	nvs_close(handle);
}

void store_persistent_settings(persistent_settings_t *settings)
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_settings", NVS_READWRITE, &handle);
	if(res!=ESP_OK)
	{
		printf("store_persistent_settings(): problem with nvs_open(), error = %d\n", res);
		return;
	}
	//nvs_erase_all(handle); //testing

	if(settings->ANALOG_VOLUME_updated)
	{
		printf("store_persistent_settings(): ANALOG_VOLUME(AV) updated to %d\n", settings->ANALOG_VOLUME);
		settings->ANALOG_VOLUME_updated = 0;
		res = nvs_set_i16(handle, "AV", settings->ANALOG_VOLUME);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i16(), error = %d\n", res);
		}
	}
	if(settings->DIGITAL_VOLUME_updated)
	{
		printf("store_persistent_settings(): DIGITAL_VOLUME(DV) updated to %d\n", settings->DIGITAL_VOLUME);
		settings->DIGITAL_VOLUME_updated = 0;
		res = nvs_set_i16(handle, "DV", settings->DIGITAL_VOLUME);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i16(), error = %d\n", res);
		}
	}
	if(settings->EQ_BASS_updated)
	{
		printf("store_persistent_settings(): EQ_BASS updated to %d\n", settings->EQ_BASS);
		settings->EQ_BASS_updated = 0;
		res = nvs_set_i8(handle, "EQ_BASS", settings->EQ_BASS);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	if(settings->EQ_TREBLE_updated)
	{
		printf("store_persistent_settings(): EQ_TREBLE updated to %d\n", settings->EQ_TREBLE);
		settings->EQ_TREBLE_updated = 0;
		res = nvs_set_i8(handle, "EQ_TREBLE", settings->EQ_TREBLE);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	/*
	if(settings->TEMPO_updated)
	{
		printf("store_persistent_settings(): TEMPO updated to %d\n", settings->TEMPO);
		settings->TEMPO_updated = 0;
		res = nvs_set_i16(handle, "TEMPO", settings->TEMPO);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i16(), error = %d\n", res);
		}
	}
	if(settings->FINE_TUNING_updated)
	{
		printf("store_persistent_settings(): FINE_TUNING(TUNING) updated to %f\n", settings->FINE_TUNING);
		settings->FINE_TUNING_updated = 0;
		uint64_t *val_u64 = (uint64_t*)&settings->FINE_TUNING;
		res = nvs_set_u64(handle, "TUNING", val_u64[0]);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i16(), error = %d\n", res);
		}
	}
	if(settings->TRANSPOSE_updated)
	{
		printf("store_persistent_settings(): TRANSPOSE updated to %d\n", settings->TRANSPOSE);
		settings->TRANSPOSE_updated = 0;
		res = nvs_set_i8(handle, "TRN", settings->TRANSPOSE);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	if(settings->BEEPS_updated)
	{
		printf("store_persistent_settings(): BEEPS updated to %d\n", settings->BEEPS);
		settings->BEEPS_updated = 0;
		res = nvs_set_i8(handle, "BEEPS", settings->BEEPS);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	if(settings->ALL_CHANNELS_UNLOCKED_updated)
	{
		printf("store_persistent_settings(): ALL_CHANNELS_UNLOCKED(XTRA_CHNL) updated to %d\n", settings->ALL_CHANNELS_UNLOCKED);
		settings->ALL_CHANNELS_UNLOCKED_updated = 0;
		res = nvs_set_i8(handle, "XTRA_CHNL", settings->ALL_CHANNELS_UNLOCKED);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	if(settings->MIDI_SYNC_MODE_updated)
	{
		printf("store_persistent_settings(): MIDI_SYNC_MODE updated to %d\n", settings->MIDI_SYNC_MODE);
		settings->MIDI_SYNC_MODE_updated = 0;
		res = nvs_set_i8(handle, "MIDI_SYNC", settings->MIDI_SYNC_MODE);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	if(settings->MIDI_POLYPHONY_updated)
	{
		printf("store_persistent_settings(): MIDI_POLYPHONY_MODE updated to %d\n", settings->MIDI_POLYPHONY);
		settings->MIDI_POLYPHONY_updated = 0;
		res = nvs_set_i8(handle, "MIDI_POLY", settings->MIDI_POLYPHONY);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	if(settings->PARAMS_SENSORS_updated)
	{
		printf("store_persistent_settings(): PARAMS_SENSORS updated to %d\n", settings->PARAMS_SENSORS);
		settings->PARAMS_SENSORS_updated = 0;
		res = nvs_set_i8(handle, "SENSORS", settings->PARAMS_SENSORS);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	*/
	if(settings->AGC_ENABLED_OR_PGA_updated)
	{
		printf("store_persistent_settings(): AGC_ENABLED_OR_PGA updated to %d\n", settings->AGC_ENABLED_OR_PGA);
		settings->AGC_ENABLED_OR_PGA_updated = 0;
		res = nvs_set_i8(handle, "AGC_PGA", settings->AGC_ENABLED_OR_PGA);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}

	if(settings->AGC_MAX_GAIN_updated)
	{
		printf("store_persistent_settings(): AGC_MAX_GAIN updated to %d\n", settings->AGC_MAX_GAIN);
		settings->AGC_MAX_GAIN_updated = 0;
		res = nvs_set_i8(handle, "AGC_G", settings->AGC_MAX_GAIN);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}

	if(settings->MIC_BIAS_updated)
	{
		printf("store_persistent_settings(): MIC_BIAS updated to %d\n", settings->MIC_BIAS);
		settings->MIC_BIAS_updated = 0;
		res = nvs_set_i8(handle, "MIC_B", settings->MIC_BIAS);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	/*
	if(settings->ALL_LEDS_OFF_updated)
	{
		printf("store_persistent_settings(): ALL_LEDS_OFF updated to %d\n", settings->ALL_LEDS_OFF);
		settings->ALL_LEDS_OFF_updated = 0;
		res = nvs_set_i8(handle, "LEDS_OFF", settings->ALL_LEDS_OFF);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}

	if(settings->AUTO_POWER_OFF_updated)
	{
		printf("store_persistent_settings(): AUTO_POWER_OFF updated to %d\n", settings->AUTO_POWER_OFF);
		settings->AUTO_POWER_OFF_updated = 0;
		res = nvs_set_i8(handle, "PWR_OFF", settings->AUTO_POWER_OFF);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}

	if(settings->SAMPLING_RATE_updated)
	{
		printf("store_persistent_settings(): SAMPLING_RATE updated to %d\n", settings->SAMPLING_RATE);
		settings->SAMPLING_RATE_updated = 0;
		res = nvs_set_i16(handle, "FS", settings->SAMPLING_RATE);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i16(), error = %d\n", res);
		}
	}

	if(settings->SD_CARD_SPEED_updated)
	{
		printf("store_persistent_settings(): SD_CARD_SPEED updated to %d\n", settings->SD_CARD_SPEED);
		settings->SD_CARD_SPEED_updated = 0;
		res = nvs_set_i8(handle, "SD_CLK", settings->SD_CARD_SPEED);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}

	if(settings->ACC_ORIENTATION_updated)
	{
		printf("store_persistent_settings(): ACC_ORIENTATION updated to %d\n", settings->ACC_ORIENTATION);
		settings->ACC_ORIENTATION_updated = 0;
		res = nvs_set_i8(handle, "ACC_O", settings->ACC_ORIENTATION);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}

	if(settings->ACC_INVERT_updated)
	{
		printf("store_persistent_settings(): ACC_INVERT updated to %d\n", settings->ACC_INVERT);
		settings->ACC_INVERT_updated = 0;
		res = nvs_set_i8(handle, "ACC_I", settings->ACC_INVERT);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i8(), error = %d\n", res);
		}
	}
	*/

	if(settings->ACC_CALIBRATION_updated)
	{
		printf("store_persistent_settings(): ACC_CALIBRATION(CAL_X,CAL_Y) updated to (%f,%f)\n", settings->ACC_CALIBRATION_X, settings->ACC_CALIBRATION_Y);
		settings->ACC_CALIBRATION_updated = 0;

		uint32_t val;

		memcpy(&val, &settings->ACC_CALIBRATION_X, sizeof(uint32_t));
		//printf("store_persistent_setting(): ACC_CALIBRATION_X val = %x\n", val);
		res = nvs_set_u32(handle, "CAL_X", val);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_u32(), error = %d\n", res);
		}

		memcpy(&val, &settings->ACC_CALIBRATION_Y, sizeof(uint32_t));
		//printf("store_persistent_setting(): ACC_CALIBRATION_Y val = %x\n", val);
		res = nvs_set_u32(handle, "CAL_Y", val);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_u32(), error = %d\n", res);
		}
	}

	if(settings->ECHO_LOOP_LENGTH_STEP_updated)
	{
		printf("store_persistent_settings(): ECHO_LOOP_LENGTH_STEP(EL_S) updated to %d\n", settings->ECHO_LOOP_LENGTH_STEP);
		settings->ECHO_LOOP_LENGTH_STEP_updated = 0;
		res = nvs_set_i32(handle, "EL_S", settings->ECHO_LOOP_LENGTH_STEP);
		if(res!=ESP_OK)
		{
			printf("store_persistent_settings(): problem with nvs_set_i32(), error = %d\n", res);
		}
	}

	res = nvs_commit(handle);
	if(res!=ESP_OK) //problem writing data
	{
		printf("store_persistent_settings(): problem with nvs_commit(), error = %d\n", res);
	}
	nvs_close(handle);
}

int load_all_settings()
{
    int settings_block_found = load_settings(&global_settings, "[global_settings]");
    printf("Global settings loaded: AUTO_POWER_OFF_TIMEOUT_ALL=%d, AUTO_POWER_OFF_TIMEOUT_REVERB=%d, "//AUTO_POWER_OFF_ONLY_IF_NO_MOTION=%d, "//DEFAULT_ACCESSIBLE_CHANNELS=%d, "
    		"TUNING_DEFAULT=%f, TUNING_MAX=%f, TUNING_MIN=%f, MICRO_TUNING_STEP=%f, "
    		//"GRANULAR_DETUNE_COEFF_SET=%f, GRANULAR_DETUNE_COEFF_MUL=%f, GRANULAR_DETUNE_COEFF_MAX=%f, "
    		//"TEMPO_BPM_DEFAULT=%d, TEMPO_BPM_MIN=%d, TEMPO_BPM_MAX=%d, TEMPO_BPM_STEP=%d, "
    		"AGC_ENABLED=%d, AGC_MAX_GAIN=%d, AGC_TARGET_LEVEL=%d, MIC_BIAS=%d, CODEC_ANALOG_VOLUME_DEFAULT=%d, CODEC_DIGITAL_VOLUME_DEFAULT=%d, "
    		"CLOUDS_HARD_LIMITER_POSITIVE=%d, CLOUDS_HARD_LIMITER_NEGATIVE=%d, CLOUDS_HARD_LIMITER_MAX=%d, CLOUDS_HARD_LIMITER_STEP=%d, "
    		"ACCELEROMETER_CALIBRATE_ON_POWERUP=%d, SAMPLE_FORMAT_HEADER_LENGTH=%d, "
    		//"DRUM_THRESHOLD_ON=%f, DRUM_THRESHOLD_OFF=%f, DRUM_LENGTH1=%d, DRUM_LENGTH2=%d, DRUM_LENGTH3=%d, DRUM_LENGTH4=%d, "
    		//"IDLE_SET_RST_SHORTCUT=%llu, IDLE_RST_SET_SHORTCUT=%llu, IDLE_LONG_SET_SHORTCUT=%llu
    		"SCALE_PLAY_SPEED_MICROTUNING=%d, SCALE_PLAY_SPEED_TRANSPOSE=%d, SCALE_PLAY_SPEED_INITIAL=%d, "
    		"TOUCHPAD_LED_THRESHOLD_A_MUL=%d, TOUCHPAD_LED_THRESHOLD_A_DIV=%d, TOUCH_PAD_DEFAULT_MAX=%d, "
    		"SPLIT_TIMBRE_NOTE_THRESHOLD1_COEFF=%f, SPLIT_TIMBRE_NOTE_THRESHOLD2_COEFF=%f"
    		"\n",
    		global_settings.AUTO_POWER_OFF_TIMEOUT_ALL,
    		global_settings.AUTO_POWER_OFF_TIMEOUT_REVERB,
			//global_settings.AUTO_POWER_OFF_ONLY_IF_NO_MOTION,
			//global_settings.DEFAULT_ACCESSIBLE_CHANNELS,
			global_settings.TUNING_DEFAULT,
			global_settings.TUNING_MAX,
			global_settings.TUNING_MIN,
			global_settings.MICRO_TUNING_STEP,
			/*
			global_settings.GRANULAR_DETUNE_COEFF_SET,
			global_settings.GRANULAR_DETUNE_COEFF_MUL,
			global_settings.GRANULAR_DETUNE_COEFF_MAX,
			global_settings.TEMPO_BPM_DEFAULT,
			global_settings.TEMPO_BPM_MIN,
			global_settings.TEMPO_BPM_MAX,
			global_settings.TEMPO_BPM_STEP,
			*/
			global_settings.AGC_ENABLED,
			global_settings.AGC_MAX_GAIN,
			global_settings.AGC_TARGET_LEVEL,
			global_settings.MIC_BIAS,
			global_settings.CODEC_ANALOG_VOLUME_DEFAULT,
			global_settings.CODEC_DIGITAL_VOLUME_DEFAULT,
			global_settings.CLOUDS_HARD_LIMITER_POSITIVE,
			global_settings.CLOUDS_HARD_LIMITER_NEGATIVE,
			global_settings.CLOUDS_HARD_LIMITER_MAX,
			global_settings.CLOUDS_HARD_LIMITER_STEP,

			global_settings.ACCELEROMETER_CALIBRATE_ON_POWERUP,
			global_settings.SAMPLE_FORMAT_HEADER_LENGTH,
			/*
			global_settings.DRUM_THRESHOLD_ON,
			global_settings.DRUM_THRESHOLD_OFF,
			global_settings.DRUM_LENGTH1,
			global_settings.DRUM_LENGTH2,
			global_settings.DRUM_LENGTH3,
			global_settings.DRUM_LENGTH4,
			global_settings.IDLE_SET_RST_SHORTCUT,
			global_settings.IDLE_RST_SET_SHORTCUT,
			global_settings.IDLE_LONG_SET_SHORTCUT
			*/
			global_settings.SCALE_PLAY_SPEED_MICROTUNING,
			global_settings.SCALE_PLAY_SPEED_TRANSPOSE,
			global_settings.SCALE_PLAY_SPEED_INITIAL,

			global_settings.TOUCHPAD_LED_THRESHOLD_A_MUL,
			global_settings.TOUCHPAD_LED_THRESHOLD_A_DIV,
			global_settings.TOUCH_PAD_DEFAULT_MAX,

			global_settings.SPLIT_TIMBRE_NOTE_THRESHOLD1_COEFF,
			global_settings.SPLIT_TIMBRE_NOTE_THRESHOLD2_COEFF
    );

    //tempo_bpm = global_settings.TEMPO_BPM_DEFAULT;
    //global_tuning = global_settings.TUNING_DEFAULT;

    //codec_analog_volume = global_settings.CODEC_ANALOG_VOLUME_DEFAULT;
    codec_digital_volume = global_settings.CODEC_DIGITAL_VOLUME_DEFAULT;
    //codec_volume_user = global_settings.CODEC_DIGITAL_VOLUME_DEFAULT;

    load_persistent_settings(&persistent_settings);
    printf("Persistent settings loaded: ANALOG_VOLUME=%d, DIGITAL_VOLUME=%d, EQ_BASS=%d, EQ_TREBLE=%d, "
    		//TEMPO=%d,
			//"TRANSPOSE=%d, FINE_TUNING=%f, "
    		//BEEPS=%d, "
    		//"ALL_CHANNELS_UNLOCKED=%d, MIDI_SYNC_MODE=%d, MIDI_POLYPHONY_MODE=%d, PARAMS_SENSORS=%d,
    		"AGC_ENABLED_OR_PGA=%d, AGC_MAX_GAIN=%d, "
    		//"ALL_LEDS_OFF=%d, "
    		//"AUTO_POWER_OFF=%d, SAMPLING_RATE=%u, SD_CARD_SPEED=%d, ACC_ORIENTATION=%d, ACC_INVERT=%d\n",
    		"ACC_CALIBRATION_X=%f, ACC_CALIBRATION_Y=%f, ECHO_LOOP_LENGTH_STEP=%d\n",
    		persistent_settings.ANALOG_VOLUME,
    		persistent_settings.DIGITAL_VOLUME,
			persistent_settings.EQ_BASS,
			persistent_settings.EQ_TREBLE,
			//persistent_settings.TEMPO,
			//persistent_settings.TRANSPOSE,
			//persistent_settings.FINE_TUNING,
			//persistent_settings.BEEPS,
			//persistent_settings.ALL_CHANNELS_UNLOCKED,
			//persistent_settings.MIDI_SYNC_MODE,
			//persistent_settings.MIDI_POLYPHONY,
			//persistent_settings.PARAMS_SENSORS,
			persistent_settings.AGC_ENABLED_OR_PGA,
			persistent_settings.AGC_MAX_GAIN,
			//persistent_settings.ALL_LEDS_OFF,
			//persistent_settings.AUTO_POWER_OFF,
			//persistent_settings.SAMPLING_RATE,
			/*persistent_settings.SD_CARD_SPEED,
			persistent_settings.ACC_ORIENTATION,
			persistent_settings.ACC_INVERT*/
			persistent_settings.ACC_CALIBRATION_X,
    		persistent_settings.ACC_CALIBRATION_Y,
			persistent_settings.ECHO_LOOP_LENGTH_STEP
    	);

    codec_analog_volume = persistent_settings.ANALOG_VOLUME;
    codec_volume_user = persistent_settings.DIGITAL_VOLUME;
    EQ_bass_setting = persistent_settings.EQ_BASS;
    EQ_treble_setting = persistent_settings.EQ_TREBLE;
    //tempo_bpm = persistent_settings.TEMPO;
    //global_tuning = persistent_settings.FINE_TUNING;
    //beeps_enabled = persistent_settings.BEEPS;

    /*
    //hide extra channels unless unlocked
    if(!persistent_settings.ALL_CHANNELS_UNLOCKED)
    {
    	channels_found = global_settings.DEFAULT_ACCESSIBLE_CHANNELS;
    }

    midi_sync_mode = persistent_settings.MIDI_SYNC_MODE;
    midi_polyphony = persistent_settings.MIDI_POLYPHONY;
	#ifdef BOARD_GECHO
    use_acc_or_ir_sensors = persistent_settings.PARAMS_SENSORS;
	#endif
	*/

    return settings_block_found;
}

void persistent_settings_store_eq()
{
	persistent_settings.EQ_BASS = EQ_bass_setting;
	persistent_settings.EQ_BASS_updated = 1;
	persistent_settings.EQ_TREBLE = EQ_treble_setting;
	persistent_settings.EQ_TREBLE_updated = 1;
	persistent_settings.update = PERSISTENT_SETTINGS_UPDATE_TIMER;
}
/*
void persistent_settings_store_tempo()
{
	persistent_settings.TEMPO = tempo_bpm;
	persistent_settings.TEMPO_updated = 1;
	persistent_settings.update = PERSISTENT_SETTINGS_UPDATE_TIMER;
}

void persistent_settings_store_tuning()
{
	persistent_settings.FINE_TUNING = global_tuning;
	persistent_settings.FINE_TUNING_updated = 1;
	persistent_settings.update = PERSISTENT_SETTINGS_UPDATE_TIMER;
}
*/

void persistent_settings_store_calibration()
{
	persistent_settings.ACC_CALIBRATION_X = acc_calibrated[0];
	persistent_settings.ACC_CALIBRATION_Y = acc_calibrated[1];
	persistent_settings.ACC_CALIBRATION_updated = 1;
	persistent_settings.update = PERSISTENT_SETTINGS_UPDATE_TIMER;
}

void persistent_settings_store_delay(int value)
{
	persistent_settings.ECHO_LOOP_LENGTH_STEP = value;
	persistent_settings.ECHO_LOOP_LENGTH_STEP_updated = 1;
	persistent_settings.update = PERSISTENT_SETTINGS_UPDATE_TIMER;
}

int nvs_erase_namespace(char *namespace)
{
	esp_err_t res;
	nvs_handle handle;

	res = nvs_open(namespace, NVS_READWRITE, &handle);
	if(res!=ESP_OK)
	{
		printf("nvs_erase_namespace(): problem with nvs_open(), error = %d\n", res);
		//indicate_error(0x0001, 10, 100);
		return 1;
	}
	res = nvs_erase_all(handle);
	if(res!=ESP_OK)
	{
		printf("nvs_erase_namespace(): problem with nvs_erase_all(), error = %d\n", res);
		//indicate_error(0x0003, 10, 100);
		return 2;
	}
	res = nvs_commit(handle);
	if(res!=ESP_OK)
	{
		printf("nvs_erase_namespace(): problem with nvs_commit(), error = %d\n", res);
		//indicate_error(0x0007, 10, 100);
		return 3;
	}
	nvs_close(handle);

	return 0;
}

void settings_reset()
{
	//LEDs_all_OFF();
	LEDS_ALL_OFF;

	if(!nvs_erase_namespace("drum_settings")) //if no error
	{
		//indicate_context_setting(SETTINGS_INDICATOR_ANIMATE_LEFT_8, 4, 50); //TODO: indication
		drum_restart();
	}
}

void store_patch_scales(int patch, int *src)
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_scales", NVS_READWRITE, &handle);
	if(res!=ESP_OK)
	{
		printf("store_patch_scales(): problem with nvs_open(), error = %d\n", res);
		return;
	}

	int patch_name_key_length = SCALES_PER_PATCH * NOTES_PER_SCALE * sizeof(int);

	if(patch==patch_reverb_n)
	{
		patch = PATCH_REVERB_NUMBER;
	}

	char patch_name_key[20];
	sprintf(patch_name_key,"P_%02d",patch);

	printf("store_patch_scales(): writing key %s, length = %d bytes\n", patch_name_key, patch_name_key_length);
	res = nvs_set_blob(handle, patch_name_key, src, patch_name_key_length);
	if(res!=ESP_OK)
	{
		printf("store_patch_scales(): problem with nvs_set_blob(), error = %d\n", res);
	}

	res = nvs_commit(handle);
	if(res!=ESP_OK) //problem writing data
	{
		printf("store_patch_scales(): problem with nvs_commit(), error = %d\n", res);
	}
	nvs_close(handle);
}

void load_patch_scales(int patch, int *dest)
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_scales", NVS_READONLY, &handle);
	if(res!=ESP_OK)
	{
		printf("load_patch_scales(): problem with nvs_open() in ");
		printf("R/O mode, error = %d\n", res);
		return;

		/*
		//maybe it was not created yet, will try
		res = nvs_open("drum_scales", NVS_READWRITE, &handle);
		if(res!=ESP_OK)
		{
			printf("load_patch_scales(): problem with nvs_open() in ");
			printf("R/W mode, error = %d\n", res);
			return;
		}
		*/
	}

	if(patch==patch_reverb_n)
	{
		patch = PATCH_REVERB_NUMBER;
	}

	char patch_name_key[20];
	sprintf(patch_name_key,"P_%02d",patch);

	size_t length = -1;
	int patch_name_key_expected_length = SCALES_PER_PATCH * NOTES_PER_SCALE * sizeof(int);

	//printf("load_patch_scales(): reading key %s, expected length = %d bytes\n", patch_name_key, patch_name_key_expected_length);
	res = nvs_get_blob(handle, patch_name_key, dest, &length);
	if(res!=ESP_OK)
	{
		printf("load_patch_scales(): problem with nvs_get_blob(), error = %d\n", res);
	}
	//printf("load_patch_scales(): nvs_get_blob() returned code = %d, length = %d\n", res, length);

	if(patch_name_key_expected_length != length)
	{
		printf("load_patch_scales(): unexpected key length: %d != %d\n", length, patch_name_key_expected_length);
	}

	nvs_close(handle);
}

void store_micro_tuning(int patch, float *src)
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_scales", NVS_READWRITE, &handle);
	if(res!=ESP_OK)
	{
		printf("store_micro_tuning(): problem with nvs_open(), error = %d\n", res);
		return;
	}

	int patch_name_key_length = SCALES_PER_PATCH * NOTES_PER_SCALE * sizeof(float);

	if(patch==patch_reverb_n)
	{
		patch = PATCH_REVERB_NUMBER;
	}

	char patch_name_key[20];
	sprintf(patch_name_key,"T_%02d",patch);

	printf("store_micro_tuning(): writing key %s, length = %d bytes\n", patch_name_key, patch_name_key_length);
	res = nvs_set_blob(handle, patch_name_key, src, patch_name_key_length);
	if(res!=ESP_OK)
	{
		printf("store_micro_tuning(): problem with nvs_set_blob(), error = %d\n", res);
	}

	res = nvs_commit(handle);
	if(res!=ESP_OK) //problem writing data
	{
		printf("store_micro_tuning(): problem with nvs_commit(), error = %d\n", res);
	}
	nvs_close(handle);
}

void load_micro_tuning(int patch, float* dest)
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_scales", NVS_READONLY, &handle);
	if(res!=ESP_OK)
	{
		printf("load_patch_scales(): problem with nvs_open() in ");
		printf("R/O mode, error = %d\n", res);
		return;
	}

	if(patch==patch_reverb_n)
	{
		patch = PATCH_REVERB_NUMBER;
	}

	char patch_name_key[20];
	sprintf(patch_name_key,"T_%02d",patch);

	size_t length = -1;
	int patch_name_key_expected_length = SCALES_PER_PATCH * NOTES_PER_SCALE * sizeof(float);

	//printf("load_micro_tuning(): reading key %s, expected length = %d bytes\n", patch_name_key, patch_name_key_expected_length);
	res = nvs_get_blob(handle, patch_name_key, dest, &length);
	if(res!=ESP_OK)
	{
		printf("load_micro_tuning(): problem with nvs_get_blob(), error = %d\n", res);
	}
	//printf("load_micro_tuning(): nvs_get_blob() returned code = %d, length = %d\n", res, length);

	if(patch_name_key_expected_length != length)
	{
		printf("load_micro_tuning(): unexpected key length: %d != %d\n", length, patch_name_key_expected_length);
	}

	nvs_close(handle);
}

void store_patch_scales_if_changed(int patch, int *src)
{
	int data_size = SCALES_PER_PATCH * NOTES_PER_SCALE * sizeof(int);
	int *stored = malloc(data_size);
	//printf("store_patch_scales_if_changed(): allocated %d bytes\n", data_size);

	load_patch_scales(patch, stored);
	if(memcmp(stored, src, data_size))
	{
		printf("store_patch_scales_if_changed(): data looks different, storing\n");
		store_patch_scales(patch, src);
	}
	else
	{
		printf("store_patch_scales_if_changed(): data looks the same, not storing\n");
	}
	free(stored);
}

void store_micro_tuning_if_changed(int patch, float *src)
{
	int data_size = SCALES_PER_PATCH * NOTES_PER_SCALE * sizeof(float);
	float *stored = malloc(data_size);
	//printf("store_micro_tuning_if_changed(): allocated %d bytes\n", data_size);

	load_micro_tuning(patch, stored);
	if(memcmp(stored, src, data_size))
	{
		printf("store_micro_tuning_if_changed(): data looks different, storing\n");
		store_micro_tuning(patch, src);
	}
	else
	{
		printf("store_micro_tuning_if_changed(): data looks the same, not storing\n");
	}
	free(stored);
}

void delete_all_custom_scales() //copy over of the default predefined scales
{
	printf("delete_all_custom_scales()\n");
	//if(!nvs_erase_namespace("drum_scales")) //if no error

	if(!backup_restore_nvs_data("nvs_bak", "nvs"))
	{
		Delay(500);
		drum_restart();
	}
}

#define NVS_PARTITION_COPY_BUFFER 0x10000

int backup_restore_nvs_data(const char *src, const char *dst)
{
	printf("backup_restore_nvs_data(%s -> %s)\n", src, dst);

	nvs_flash_deinit();

	const esp_partition_t *pt_src, *pt_dst;
	pt_src = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, src);
	pt_dst = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, dst);

	if(pt_src==NULL)
	{
		printf("backup_restore_nvs_data(): esp_partition_find_first[src] returned NULL\n");
		return -1;
	}
	printf("backup_restore_nvs_data(): esp_partition_find_first[src] found a partition with size = 0x%x, label = %s, addres=0x%x\n", pt_src->size, pt_src->label, pt_src->address);

	if(pt_dst==NULL)
	{
		printf("backup_restore_nvs_data(): esp_partition_find_first[dst] returned NULL\n");
		return -2;
	}
	printf("backup_restore_nvs_data(): esp_partition_find_first[dst] found a partition with size = 0x%x, label = %s, addres=0x%x\n", pt_dst->size, pt_dst->label, pt_dst->address);

	int free_mem = xPortGetFreeHeapSize();
	printf("backup_restore_nvs_data(): free mem = %d\n", free_mem);
	char *buf = malloc(NVS_PARTITION_COPY_BUFFER);

	if(buf==NULL)
	{
		printf("backup_restore_nvs_data(): malloc(%d) returned NULL\n", NVS_PARTITION_COPY_BUFFER);
		return -3;
	}

	free_mem = xPortGetFreeHeapSize();
	printf("backup_restore_nvs_data(): free mem after allocating buffer = %d\n", free_mem);

	printf("backup_restore_nvs_data(): nvs_flash_erase_partition: erasing dest partition \"%s\"\n", dst);
	esp_err_t res = nvs_flash_erase_partition(dst);
	if(res!=ESP_OK)
	{
		printf("backup_restore_nvs_data(): nvs_flash_erase_partition returned code = %d\n", res);
		free(buf);
		return res;
	}

	int steps = pt_src->size / NVS_PARTITION_COPY_BUFFER;
	int leftover = pt_src->size % NVS_PARTITION_COPY_BUFFER;
	printf("backup_restore_nvs_data(): steps=%d, leftover=%d\n", steps, leftover);

	for(int i=0;i<=steps;i++)
	{
		printf("backup_restore_nvs_data(): step=%d, address=%x, size=%d\n", i, i*NVS_PARTITION_COPY_BUFFER, i==steps?leftover:NVS_PARTITION_COPY_BUFFER);
		res = esp_partition_read(pt_src, i*NVS_PARTITION_COPY_BUFFER, buf, i==steps?leftover:NVS_PARTITION_COPY_BUFFER);
		if(res==ESP_ERR_INVALID_SIZE)
		{
			printf("backup_restore_nvs_data(): esp_partition_read returned code ESP_ERR_INVALID_SIZE\n");
			free(buf);
			return res;
		}
		else if(res!=ESP_OK)
		{
			printf("backup_restore_nvs_data(): esp_partition_read returned code = %d\n", res);
			free(buf);
			return res;
		}

		res = esp_partition_write(pt_dst, i*NVS_PARTITION_COPY_BUFFER, buf, i==steps?leftover:NVS_PARTITION_COPY_BUFFER);
		//if(res==ESP_ERR_INVALID_SIZE)
		if(res!=ESP_OK)
		{
			printf("backup_restore_nvs_data(): esp_partition_write returned code = %d\n", res);
			return res;
		}
	}

	free(buf);

	res = nvs_flash_init();
	if(res!=ESP_OK)
	{
		printf("backup_restore_nvs_data(): nvs_flash_init returned code = %d\n", res);
		return res;
	}

	return 0;
}

int self_tested()
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_settings", NVS_READONLY, &handle);
	if(res!=ESP_OK)
	{
		printf("self_test(): problem with nvs_open(), error = %d\n", res);
		return 0;
	}

	#ifdef DEBUG_OUTPUT
	printf("self_test(): checking for \"unit_tested\" key\n");
	#endif

	int8_t tested;
	res = nvs_get_i8(handle, "unit_tested", &tested);

	if(res!=ESP_OK) //problem reading data
	{
		printf("self_test(): problem with nvs_get_i8() while reading key \"unit_tested\", error = %d\n", res);
		nvs_close(handle);
		return 0;
	}

	nvs_close(handle);
	return tested;
}

void nvs_set_flag(const char *key, int8_t value)
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_settings", NVS_READWRITE, &handle);
	if(res!=ESP_OK)
	{
		printf("nvs_set_flag(): problem with nvs_open(), error = %d\n", res);
		return;
	}

	#ifdef DEBUG_OUTPUT
	printf("nvs_set_flag(): creating key \"%s\" with value %d\n", key, value);
	#endif

	res = nvs_set_i8(handle, key, value);
	if(res!=ESP_OK) //problem writing data
	{
		printf("nvs_set_flag(): problem with nvs_set_i8() while creating key \"%s\", error = %d\n", key, res);
		nvs_close(handle);
		return;
	}

	res = nvs_commit(handle);
	if(res!=ESP_OK) //problem writing data
	{
		printf("nvs_set_flag(): problem with nvs_commit() while creating key \"%s\", error = %d\n", key, res);
	}

	nvs_close(handle);
}

int nvs_get_flag(const char *key) //returns 0 not only when value is zero but also when key not found
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_settings", NVS_READONLY, &handle);
	if(res!=ESP_OK)
	{
		printf("nvs_get_flag(): problem with nvs_open(), error = %d\n", res);
		return 0;
	}

	#ifdef DEBUG_OUTPUT
	printf("nvs_get_flag(): checking for \"%s\" key\n", key);
	#endif

	int8_t val;
	res = nvs_get_i8(handle, key, &val);

	if(res!=ESP_OK) //problem reading data
	{
		printf("nvs_get_flag(): problem with nvs_get_i8() while reading key \"%s\", error = %d\n", key, res);
		nvs_close(handle);
		return 0;
	}

	nvs_close(handle);
	return val;
}

void store_selftest_pass(int value)
{
	nvs_set_flag("unit_tested", value);
}

void reset_selftest_pass()
{
	printf("reset_selftest_pass(): store_selftest_pass(0);\n");
	store_selftest_pass(0);
}

void nvs_set_text(const char *key, char *text)
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_settings", NVS_READWRITE, &handle);
	if(res!=ESP_OK)
	{
		printf("nvs_set_text(): problem with nvs_open(), error = %d\n", res);
		return;
	}

	#ifdef DEBUG_OUTPUT
	printf("nvs_set_text(): creating key \"%s\" with value of length %d\n", key, strlen(text));
	#endif

	res = nvs_set_str(handle, key, text);
	if(res!=ESP_OK) //problem writing data
	{
		printf("nvs_set_text(): problem with nvs_set_str() while creating key \"%s\", error = %d\n", key, res);
		nvs_close(handle);
		return;
	}

	res = nvs_commit(handle);
	if(res!=ESP_OK) //problem writing data
	{
		printf("nvs_set_text(): problem with nvs_commit() while creating key \"%s\", error = %d\n", key, res);
	}

	nvs_close(handle);
}

int nvs_get_text(const char *key, char **buffer) //returns 0 when key not found, otherwise returns string length
{
	esp_err_t res;
	nvs_handle handle;
	res = nvs_open("drum_settings", NVS_READONLY, &handle);
	if(res!=ESP_OK)
	{
		printf("nvs_get_text(): problem with nvs_open(), error = %d\n", res);
		return 0;
	}

	#ifdef DEBUG_OUTPUT
	printf("nvs_get_text(): checking for \"%s\" key\n", key);
	#endif

	size_t length = 0;
	res = nvs_get_str(handle, key, NULL, &length);
	if(res!=ESP_OK) //problem reading data
	{
		printf("nvs_get_text(): nvs_get_str() encountered problem while reading key \"%s\", error = %d\n", key, res);
		nvs_close(handle);
		return 0;
	}
	printf("nvs_get_text(): nvs_get_str(length=0) reports key \"%s\" has length of %d\n", key, length);

	buffer[0] = malloc(length+1);
	//length = 1;
	res = nvs_get_str(handle, key, buffer[0], &length);
	if(res!=ESP_OK) //problem reading data
	{
		printf("nvs_get_text(): nvs_get_str(length=%d) encountered problem while reading key \"%s\", error = %d\n", length, key, res);
		nvs_close(handle);
		return 0;
	}
	printf("nvs_get_text(): nvs_get_str(length=1) read %d bytes from key \"%s\"\n", length, key);

	nvs_close(handle);
	return length;
}
