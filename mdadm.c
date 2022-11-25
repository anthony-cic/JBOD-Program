#include <stdio.h>                                                                                                                                                                                         
#include <string.h>                                                                                                                                                                                        
#include <assert.h>                                                                                                                                                                                        
#include <stdlib.h>                                                                                                                                                                                        
                                                                                                                                                                                                           
#include "mdadm.h"                                                                                                                                                                                         
#include "jbod.h"     
#include "net.h"                                                                                                                                                                                     
                                                                                                                                                                                                           
// function to return Block num from an address                                                                                                                                                            
int getBlockNum(uint32_t addr) {                                                                                                                                                                           
        int x = (addr % 65536) / JBOD_BLOCK_SIZE;                                                                                                                                                                      
        return x;                                                                                                                                                                                          
}                                                                                                                                                                                                          
                                                                                                                                                                                                           
// function to return disk Num from an address                                                                                                                                                             
int getDiskNum(uint32_t addr) {                                                                                                                                                                            
        int x = addr / 65536;                                                                                                                                                                              
        return x;                                                                                                                                                                                          
        }                                                                                                                                                                                                  

int static mounted = -1;                                                                                                                                                                                               
                                                                                                                                                                                                           
int mdadm_mount(void) {   
                                                                                                          
        mounted = jbod_client_operation(JBOD_MOUNT<<26, NULL);                                                                                                                                                         
                                                                                                                                                                                        
        // jbod successfully mounted                                                                                                                                                                       
        if (mounted == 0) {                                                                                                                                                                                
                return 1;                                                                                                                                                                                  
                }                                                                                                                                                                                          
        else {            
                                                                                                                                                                                               
        // if mounted is not 1, we can assume the system was already mounted, call fails                                                                                                                   
                return -1;                                                                                                                                                                                 
        }                                                                                                                                                                                                  
}                                                                                                                                                                                                          
                                                                                                                                                                                                           
int mdadm_unmount(void) {                                                                                                                                                                                  
                                                                                                                                                                                                           
        mounted = jbod_client_operation(JBOD_UNMOUNT<<26, NULL);                                                                                                                                                  
                                                                                                                                                                                                           
        // System already unmounted                                                                                                                                                                        
        if (mounted == -1) {                                                                                                                                                                               
                                                                                                                                                                                                    
                return -1;                                                                                                                                                                                 
                }                                                                                                                                                                                          
        // unmount succeeded, can return 1                                                                                                                                                                 
        else {      
                mounted = -1;                                                                                                                                                                                        
                return 1;                                                                                                                                                                                  
                }                                                                                                                                                                                          
}                                                                                                                                                                                                          
                                                                                                                                                                                                          
int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {                                                                                                                                                
                                                                                                                                                                                                           
        // ****** EDGE CASES *******                                                                                                                                                                       
                                                                                                                                                                                                           
        // mount before read                                                                                                                                                                               
        if (mounted == -1) {                                                                                                                                                                               
                return -1;                                                                                                                                                                                 
        }                                                                                                                                                                                                  
        if ((buf == NULL) && (len != 0)) { // can have a null pointer if length is 0, in this case it is not                                                                                               
                return -1;                                                                                                                                                                                 
                }                                                                                                                                                                                          
        if ((buf == NULL) && (len == 0)) { // proper null pointer format                                                                                                                                   
                return 0;                                                                                                                                                                                  
                }                                                                                                                                                                                          
        if ((len > 1024) || (len < 0)) { // 1024 length constraint, no negative length                                                                                                                     
                return -1;                                                                                                                                                                                 
                }                                                                                                                                                                                          
        if (addr < 0) { // cannot have negative addr                                                                                                                                                       
                return -1;                                                                                                                                                                                 
                }                                                                                                                                                                                          
        if (addr+len > 1048576) { // if addr + len exceed the maximum amount of space on all disks, fail                                                                                                   
                return -1;                                                                                                                                                                                 
                }                                                                                                                                                                                          
        // *********END OF EDGE CASES *****************                                                                                                                                                    
                                                                                                                                                                                                           
                                                                                                                                                                                                           
        // buffer to pass into read jbod_client_operation                                                                                                                                                         
        uint8_t *blockBuffer = malloc(JBOD_BLOCK_SIZE);                                                                                                                                                                
                                                                                                                                                                                                           
        // address to keep track of the current position                                                                                                                                                   
        uint32_t currentAddr= addr;                                                                                                                                                                        
                                                                                                                                                                                                           
        // remaining total bytes to be read, (for my understanding: opposite of old currentByte variable)                                                                                                  
        int remainingBytes = 0;                                                                                                                                                                            
                                                                                                                                                                                                           
        // while the final point in the memory to be read (addr + len) is greater than the current point in memory                                                                                         
        // think of current address as a count variable and addr + len is like total length of an array                                                                                                    
        while(addr +len > currentAddr) {                                                                                                                                                                   
                                                                                                                                                                                                           
                                                                                                                                                                                                           
                int offset = (currentAddr%65536)%JBOD_BLOCK_SIZE; // offset is the number of extra bytes                                                                                                               
                int remainingBlock = JBOD_BLOCK_SIZE - offset; // remainingBlock is the number of bytes LEFT in a block                                                                                                
                                                                                                                                                                                                           
                // ******** JBOD OPERATIONS ********                                                                                                                                                       
                                                                                                                                                                                                           
                jbod_client_operation(JBOD_SEEK_TO_DISK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), NULL); // command after shift 26 bits, disk num to fill 22-26 and blockNum doesnt need a
                jbod_client_operation(JBOD_SEEK_TO_BLOCK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), NULL);                                                                                 
                jbod_client_operation(JBOD_READ_BLOCK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), blockBuffer);                                                                             
                                                                                                                                                                                                           
                // ******** END OF JBOD OPERATION *******                                                                                                                                                  
                                                                                                                                                                                                           
                // if the current address is at address loop has just started and need to make check if there is space in the block                                                                        
                // or if the current address plus another block is less than or equal to addr + len, there is more bytes to check                                                                          
                if((currentAddr == addr) || (addr + len >= currentAddr + JBOD_BLOCK_SIZE)) {                                                                                                                           
                        if(remainingBlock >= len) { // we have enough space in the remaining block to complete the read and copy to buf                                                                    
                                // ** for own use can use '+' instead of splicing the address **                                                                                                           
                                memcpy (buf+remainingBytes, offset + blockBuffer, len);                                                                                                                    
                                break; // read done                                                                                                                                                        
                                }                                                                                                                                                                          
                                                                                                                                                                                                           
                        // copy the rest into buf and continue the while loop                                                                                                                              
                        memcpy( buf+remainingBytes, offset + blockBuffer, remainingBlock);                                                                                                                 
                                                                                                                                                                                                           
                                                                                                                                                                                                           
                        remainingBytes = remainingBytes + (remainingBlock);                                                                                                                                
                        currentAddr = currentAddr + (remainingBlock);                                                                                                                                      
                                }                                                                                                                                                                          
                else { // then addr + len < currentAddr + JBOD_BLOCK_SIZE or currentAddr !=addr                                                                                                                        
                        memcpy(buf + remainingBytes, blockBuffer, len - remainingBytes);                                                                                                                   
                        break;                                                                                                                                                                             
                                }                                                                                                                                                                          
                                                                                                                                                                                                           
                }                                                                                                                                                                                          
                                                                                                                                                                                                           
        //mdadm_unmount();                                                                                                                                                                                   
        free(blockBuffer);                                                                                                                                                                                 
                                                                                                                                                                                                           
        return len;                                                                                                                                                                                        
}                                                                                                                                                                                                          
                                                                                                                                                                                                           
int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {  
        if (mounted == -1) {      
                                                                                                                                                                           
                return -1;                                                                                                                                                                                 
        }                                                                                                                                                                                                  
        if ((buf == NULL) && (len != 0)) { // can have a null pointer if length  
                                                                                                                                         
                return -1;                                                                                                                                                                                 
                }                                                                                                                                                                                          
        if ((buf == NULL) && (len == 0)) { // proper null pointer format  
                                                                                                                                                  
                return 0;                                                                                                                                                                                  
                }                                                                                                                                                                                          
        if ((len > 1024) || (len < 0)) { // 1024 length constraint, no negative 
                                                                                                                                            
                return -1;                                                                                                                                                                                 
                }                                                                                                                                                                                          
        if (addr < 0) { // cannot have negative addr                                                                                                                                                        
                return -1;                                                                                                                                                                                 
                } 

        // 1048576   
        //printf("addr + len: %d ", (addr+len));                                                                                                                                                                                       
        if (addr+len  > 1048576) { // if addr + len exceed the maximum amount of       
                //printf("FAILED ADDR + LEN \n\n");                                                                                                                     
                return -1;                                                                                                                                                                                 
                }
	//printf("addr: %d PASSED EDGE CASES \n\n", addr);
	// create a temp buf buf 2 and allocate JBOD_BLOCK_SIZE space                                                                                                                                                                                           
        uint8_t *buf2 = malloc(JBOD_BLOCK_SIZE);                                                                                                                                                                       
        memcpy(buf2, buf, JBOD_BLOCK_SIZE); // changed from len                                                                                                                                                                            
                                                                                                                                                                                                           
                                                                                                                                                                                                           
        uint32_t currentAddr = addr;                                                                                                                                                                       
        int remainingBytes = 0;                                                                                                                                                                            
         
	// while addr and len are not at the currentAddr                                                                                                                                                                                                   
        while (addr + len > currentAddr) {                                                                                                                                                                 
                int offset = (currentAddr%65536)%JBOD_BLOCK_SIZE;                                                                                                                                                      
                int remainingBlock = JBOD_BLOCK_SIZE - offset;                                                                                                                                                         
                                                     
		// JBOD OPERATIONS 
	
          	// READ RESETS IO, SEEK AGAIN AFTER                                                                                                                                             
                jbod_client_operation(JBOD_SEEK_TO_DISK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), NULL);                                                                                  
                jbod_client_operation(JBOD_SEEK_TO_BLOCK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), NULL);                                                                                 
                jbod_client_operation(JBOD_READ_BLOCK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), buf2);                                                                                    
                jbod_client_operation(JBOD_SEEK_TO_DISK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), NULL);                                                                                
                jbod_client_operation(JBOD_SEEK_TO_BLOCK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), NULL);                                                                               
                                                                                                                                                                                                           
                                                                                                                                                                                                           
                if((currentAddr == addr) || (addr + len >= currentAddr + JBOD_BLOCK_SIZE)) {                                                                                                                           
                        if(remainingBlock >= len) { // we have enough space in t                                                                                                                           
                                // ** for own use can use '+' instead of splicin                                                                                                                           
                                // copy memory from buffer                                                                                                                                                                         
                                memcpy (buf2+offset, buf+remainingBytes, len);                                                                                                                           
                                                                                                                                                                                                                                                                                                                 
                                jbod_client_operation(JBOD_WRITE_BLOCK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), buf2);   
                                //printf("addr: %d ENOUGH BYTES TO WRITE AND FINISH \n\n", addr);                                                                  
                                return len; // read done                                                                                                                                                        
                                }                                                                                                                                                                          
                                                                                                                                                                                                           
                        // copy the rest into buf and continue the while loop

			                                                                                                                                                                                                                                         
                        memcpy(buf2+offset, buf+remainingBytes, remainingBlock);                                                                                                                                                                                                 
                                                                                       
                        jbod_client_operation(JBOD_WRITE_BLOCK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), buf2);                                                                           
                                                                                                                                                                                                           
                        remainingBytes = remainingBytes + (remainingBlock);                                                                                                                                
                        currentAddr = currentAddr + (remainingBlock);                                                                                                                                      
                                }                                                                                                                                                                          
                else { // then addr + len < currentAddr + JBOD_BLOCK_SIZE or currentAddr !=addr                                                                                                                           
                                                                                                                                                                                          
                        memcpy(buf2, buf+remainingBytes, len- remainingBytes);                                                                                                                          
                                                                                              
                        jbod_client_operation(JBOD_WRITE_BLOCK << 26 | getDiskNum(currentAddr) << 22 | getBlockNum(currentAddr), buf2);                                                                           
                        return len;                                                                                                                                                                             
                               }                                                                                                                                                                           
                 }                                                                                                                                                                                         
                                                                                                                                                                                                           
        // unmount and free temp buff                                                                                         
        //mdadm_unmount();                                                                                                                                                                                   
        free(buf2);                                                                                                                                                                                        
                                                                                                                                                                                                           
        return len;                                                                                                                                                                                        
                                                                                                                                                                                                           
}                                                 
