#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int MAX_STR_LEN = 512;

struct SBCP_Attribute {
  unsigned int                   Type   : 16;                     
  unsigned int                   Length : 16;
  char                           Payload [512];
};


struct SBCP_Message {
  unsigned int                   Version : 9;                     
  unsigned int                   Type    : 7;                     
  unsigned int                   Length  : 16;                    
  struct SBCP_Attribute   attribute;
};
