#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

bool cache_flag = false; 


int cache_create(int num_entries) {
  // edge cases, cannot be less than 2 or more than 4096 
  if (cache_flag == true) {
      return -1; 
  }
  if (num_entries > 4096) {
    return -1; 
  } 
  if (num_entries < 2) {
      return -1; 
  }
 
  // end of edge cases 

  cache_flag = true; 
  cache_size = num_entries; 
  // calloc over malloc assigns 0's 
  cache = calloc(cache_size, sizeof(cache_entry_t)); 
  return 1;
}

int cache_destroy(void) {
  // cache must be enabled 
  if (cache_flag == false) {
      return -1; 
  }
  

  cache_size = 0; 
  cache = NULL; 
  cache_flag = false; 
  // free will set the memory free and destroy cache 
  free(cache); 
  return 1;
}

  



int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  // edge cases, cache must be enabled 
  if (cache_flag == false) {
    return -1; 
  }
  // buf cannot be null 
  if (buf == NULL) {
    return -1; 
  }
  if ((block_num > 256) || (block_num < 0)) {
    return -1; 
  }
  if ((disk_num > 16) || (disk_num < 0)) {
    return -1; 
  }
  if (cache_size == 0) {
    return -1; 
  }
  if (clock == 0) {
    return -1; 
  }
  // END OF EDGE CASES


  int i; 
  // loop through entire cache 
  for(i=0; i < cache_size; i++) {
    num_queries += 1; 
    int cache_disk = cache[i].disk_num;
    int cache_block = cache[i].block_num; 
    // if current cache and disk match our lookup: 
    if ((cache_disk == disk_num) && (cache_block == block_num)) {
      // copy memory into buf 
      memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE); 
     
     // increment for hit detection 
    
      clock += 1; 
      cache[i].access_time = clock; 
      num_hits += 1; 


      return 1;
    }
  }
  // not found 
  return -1; 
}


void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  // edge cases 
  if (cache_flag == false) {
    return; 
  }
  if ((block_num > 256) || (block_num < 0)) {
      return; 
  }
  if ((disk_num > 16) || (disk_num < 0)) {
    return; 
  }
  if (buf == NULL) {
    return; 
  }

  // end of edge cases 

  int i; 
  // loop through cache 
  for(i=0; i < cache_size; ++i) {
    // if cache disk and block already exist 
    if ((cache[i].disk_num == disk_num) && (cache[i].block_num == block_num)) {

      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE); 
      clock += 1; 
      cache[i].access_time = clock;
      
      return; 
    }
  }
  // not found 


}

void least_recently_used(int disk_num, int block_num, const uint8_t *buf) {
  int i; 
  int minimum = cache[0].access_time;  
   
   // loop through cache 
  for(i = 0; i < cache_size; i++) { 

    // make sure not end of loop 
    if(i + 1 == cache_size) {
      break;
    }
    // compare current element to next element 
    if(cache[i].access_time > cache[i+1].access_time) { 
      minimum = cache[i+1].access_time;
    }

  }

  //printf("MIN: %d, %d \n", cache[i].disk_num, cache[i].block_num);
  // loop through cache 
  for (i = 0; i < cache_size; i++) {
    // find min value and update it 

    if(cache[i].access_time == minimum) {

      // update block 
      cache[i].disk_num = disk_num;
      cache[i].block_num = block_num; 

      // copy mem 
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      cache[i].valid = false; 
      cache[i].access_time = clock;
      return; 
    }
  }

}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  // edge cases   printf("HERE \n"); 
  if (cache_flag == false) {
    return -1; 
  }
  if ((block_num > 256) || (block_num < 0)) {
      return -1; 
  }
  if ((disk_num > 16) || (disk_num < 0)) {
    return -1; 
  }
  if (buf == NULL) {
    return -1; 
  }

  // make sure entry not already in cache 
  uint8_t *temp_buf = malloc(JBOD_BLOCK_SIZE);
  memcpy(temp_buf, buf, JBOD_BLOCK_SIZE); 
  int lookup_flag = cache_lookup(disk_num, block_num, temp_buf);
  if (lookup_flag == 1) {
 
    return -1; 
  }
  free(temp_buf); 
  //   printf("disk: %d block: %d flag: %d \n", disk_num, block_num, lookup_flag);  
  // end of edge cases 

  // index variable 
  int i;
  

  // loop through cache 
  for(i = 0; i < cache_size; i++ ) {
    
    //printf("i: %d \n", i); 
    if(cache[i].valid == false) { // false means there is no entry, free space 
          
      clock += 1; 
      // updates block and disk 
      cache[i].block_num = block_num;
      cache[i].disk_num = disk_num; 
      
      // update block 
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE);
      cache[i].access_time = clock;
      cache[i].valid = true; 
      return 1; 
    }

    // if block and disk already exist 
    if((cache[i].disk_num == disk_num) && (cache[i].block_num == block_num)) {
      clock += 1; 
      
      if(cache[i].valid == true) { // valid returns true if space is available,
         
          // copy memory into block 
          memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE); 
          cache[i].access_time = clock; 
          cache[i].valid = false;
          return 1; 
          }
      else { // no space, entry exists already. fails 
        return -1; 
        }
      }


  }
  // if loops through entire loop and no matches, we must make space 

  least_recently_used(disk_num, block_num, buf);
  return 1; 
}


bool cache_enabled(void) {
  return false;
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
}