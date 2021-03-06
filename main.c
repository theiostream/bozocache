/* 
This project is licensed under the terms of the
DO WHAT THE FUCK YOU WANT TO PUBLIC LICENSE, version 3,
as published by theiostream on March 2012, as it follows:

0. You just DO WHAT THE FUCK YOU WANT TO.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <limits.h>
#include <libgen.h>
#include <assert.h>

#import <CoreFoundation/CoreFoundation.h>
#import <MobileDevice.h>

static _Bool is_cwd = 0;
static _Bool gen_path = 0;
static char outputfile[PATH_MAX];

static void device_notification_callback(am_device_notification_callback_info *info, void *thing) {
	if (info->msg != ADNCI_MSG_CONNECTED) return;
	puts("Opened device connection.");
	
	am_device *dev = info->dev;
	AMDeviceConnect(dev);
	assert(AMDeviceIsPaired(dev));
	assert(AMDeviceValidatePairing(dev) == 0);
	assert(AMDeviceStartSession(dev) == 0);
		
	struct afc_connection *afc;
	service_conn_t afc_conn;
	assert(AMDeviceStartService(dev, CFSTR("com.apple.afc2"), &afc_conn, NULL) == 0);
	assert(AFCConnectionOpen(afc_conn, 0, &afc) == 0);
	
	char cachepath[63];
	const char *caches[3] = {"dyld_shared_cache_armv7s", "dyld_shared_cache_armv7", "dyld_shared_cache_armv6"};
	
	struct afc_dictionary *dict;
	unsigned int cache_index;
	char *fullpath;
	size_t cachesize;
	for (cache_index=0; cache_index<3; cache_index++) {
		strcpy(cachepath, "/System/Library/Caches/com.apple.dyld/");
		fullpath = strcat(cachepath, caches[cache_index]);
		
		if (AFCFileInfoOpen(afc, fullpath, &dict) == 0) {
			char *key, *value;
			while (1) {
				assert(AFCKeyValueRead(dict, &key, &value) == 0);
				if (key == NULL) break;
				
				if (strcmp(key, "st_size") == 0) {
					cachesize = strtol(value, NULL, 0);
					break;
				}
			}
			
			printf("Found cache %s with size %lu\n", fullpath, cachesize);
			
			assert(AFCKeyValueClose(dict) == 0);
			goto _label_hasfile;
		}
	}
	
	fprintf(stderr, "Could not find cache file.\n");
	exit(2);
	
	_label_hasfile:;
	afc_file_ref cache;
	assert(AFCFileRefOpen(afc, fullpath, 1, &cache) == 0);
	
	if (is_cwd) {
		strcat(outputfile, "/");
		strcat(outputfile, caches[cache_index]);
		
		gen_path = 1;
	}
	
	puts(outputfile);
	FILE *output = fopen(outputfile, "w");
	assert(output != NULL);
	printf("Writing cache to %s\n", outputfile);
	
	size_t total_bytes = 0;
	char buffer[65536];
	while (1) {
		unsigned int length = 65536;
		assert(AFCFileRefRead(afc, cache, buffer, &length) == 0);
		
		fwrite(buffer, sizeof(char), length, output);
		
		total_bytes += length;
		float progress = (float)total_bytes/cachesize*100;
		printf("Progress: %f%%\n\033[F\033[J", progress);
		
		if (length < sizeof(buffer)) break;
	}
	printf("Successfully wrote cache to %s\n", outputfile);
	
	assert(AFCFileRefClose(afc, cache) == 0);
	
	CFRunLoopStop(CFRunLoopGetCurrent());
}

static void sigint_handler(int sig) {
	if (gen_path) {
		puts("Deleting partially generated cache.");
		unlink(outputfile);
		
		puts("Ending.");
		exit(0);
	}
}

int main(int argc, char *argv[]) {
	if (argc > 1) {
		struct stat st;
		if (stat(dirname(argv[1]), &st)==-1 && errno==ENOENT) {
			fprintf(stderr, "Directory for writing does not exist.\n");
			exit(1);
		}
		
		realpath(argv[1], outputfile);
		gen_path = 1;
	}
	else {
		is_cwd = 1;
		getcwd(outputfile, PATH_MAX);
	}
	
	am_device_notification *notification;
	assert(AMDeviceNotificationSubscribe(device_notification_callback, 0, 0, NULL, &notification) == MDERR_OK);
	puts("Waiting for device.");
	
	signal(SIGINT, &sigint_handler);
	
	CFRunLoopRun();
	
	puts("Ending.");
	return 0;
}