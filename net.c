#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

// ret ptr and var 
uint16_t ret;
uint16_t *ret_ptr = &ret; 



/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/
static bool nread(int fd, int len, uint8_t *buf) {
  // fd cannot be -1 
  if (fd == -1) {
      return false; 
  }

  // readBytes is the bytes read from write function 
  int readBytes = 0; 
  int totalRead = 0; 
  int bytesToRead = 0;
  // while the total read does not equal len 
  while(len != totalRead) {
    // bytes read after each write iteration 
    bytesToRead = len - readBytes;
    readBytes = read(fd, &buf[totalRead], bytesToRead);
    
    // total counter
    totalRead = totalRead + readBytes;
    
    // this means readbytes overcomitted 
    if (len > totalRead) {
      return false;
    }
    if (readBytes == -1) {
      return false; 
    }
  }
  
  
  return true; 
  
}

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/
static bool nwrite(int fd, int len, uint8_t *buf) {
  // fd cannot be -1 
    if (fd == -1) {
      return false; 
  }

  // readBytes is the bytes read from write function 
  int readBytes = 0; 
  int totalRead = 0;  
  int bytesToRead = 0; 

  // while the total read does not equal len 
  while(len != totalRead) {
    bytesToRead = len - readBytes;
    // bytes read after each write iteration 
    readBytes = write(fd, &buf[totalRead], bytesToRead);

    // total counter
    totalRead = totalRead + readBytes;
    if (len > totalRead) {
      return false;
    }
    if (readBytes == -1) {
      return false; 
    }
  }
  return true;
}

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then 
**use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/

/* When receiving, you can retrieve the length from the packet to see if there is a block.
When sending, you should set the length depending on the operation and whether you are sending a packet.

There should be only 2 legal packet length, header len when there's no block, and header len + block size when there is a block.
*/ 
static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  // ntohs(len) in recv 
  // htons(len) before sending 
  if (sd == -1) {
      return false; 
  }


  // read header first 
  uint8_t *buf = malloc(HEADER_LEN);
  if (nread(sd, (HEADER_LEN), buf) == false) {
      return false; 
      }
 

  // temp vars for len, op, ret 
  uint16_t sd_len; 
  uint32_t op_temp; 
  uint16_t ret_temp; 


  // memcpy variables from buffer 
  memcpy(&sd_len, &buf[0], 2); 
  memcpy(&op_temp, &buf[2] , 4);
  memcpy(&ret_temp, &buf[6], 2); 
  
  // back to HBO 
  sd_len = ntohs(sd_len);
  *op = ntohl(op_temp);
  *ret = ntohs(ret_temp);

  // if there is a block 
  if (sd_len > HEADER_LEN) {
    return(nread(sd, JBOD_BLOCK_SIZE, block));
  }
  else {
    return true;
  }

}

/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/
static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  // allocate space for packet and read buf 
  
  if (sd == -1) {
    return false; 
  }
  
  // length is 264 if op is jbod write and 8 if op is not jbod write 
  uint16_t length = (op >> 26) == JBOD_WRITE_BLOCK ? (JBOD_BLOCK_SIZE + HEADER_LEN) : HEADER_LEN;


  // buf of size length 
  uint8_t *buf = (uint8_t *) malloc(length); // changed from (uint8_t *) 


  // nbo 
  uint16_t send_len = htons (length);
  memcpy (&buf[0], (uint8_t *)&send_len, 2); // uint8 

  uint32_t op_temp = htonl(op);

  // memcpy variables into buf 
  memcpy(&buf[2], (uint8_t *)&op_temp, 4); //uint 32 
  memset (&buf[6], 0, 2);

  // if length is greater than the packet header, there exists a block 
  if (length > HEADER_LEN) {

    // copy block into buf 
    memcpy (&buf[8], block, JBOD_BLOCK_SIZE); 
    return nwrite (sd, length, buf);
  }
  else {
    return nwrite(sd, HEADER_LEN, buf); 
  }
}





/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
 
  // socket struct 
  struct sockaddr_in caddr; 

  caddr.sin_family = AF_INET; 
  caddr.sin_port = htons(JBOD_PORT); 

  // connect ip 
  if ( inet_aton(ip, &(caddr.sin_addr)) == 0) {
    return false; 
  }
  // create socket 
  cli_sd = socket(PF_INET, SOCK_STREAM, 0); 
  if (cli_sd == -1) {
    return false; 
  }
  // connect to server with socket and struct 
  if (connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1 ) {

    return false; 
  }
  return true;
}


/* disconnects from the server and resets cli_sd */
void jbod_disconnect(void) {
  close(cli_sd); 
  cli_sd = -1; 

}



/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/
int jbod_client_operation(uint32_t op, uint8_t *block) {
  
  // send packet 
  if (send_packet(cli_sd, op, block) == false) {
    return -1; 
  }
  uint32_t *op_ptr = &op; 

  // recv packet 
  if (recv_packet(cli_sd, op_ptr, ret_ptr, block) == false) {
    return -1; 
  }
  // return ret 
  return *ret_ptr; 

}
