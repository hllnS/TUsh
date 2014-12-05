/************************************************************************
 *                                                                      *
 *      TUsh - The Telnet User Shell            Simon Marsh 1992        *
 *                                                                      *
 ************************************************************************/


/* 
        This is the file socket.c

        Contains Telnet protocol I/O
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <arpa/telnet.h>

#include "config.h"

extern void print();
extern void *scratch;
/* extern char *sys_errlist[]; */
extern shell *current_shell;

/* the close command */

void close_com(sh,str)
shell *sh;
char *str;
{
  if (!(sh->flags&CONNECTED)) 
    shell_print(sh,"No connection made.");
  else {
    sh->flags &= ~(CONNECTED|EOR_ON);
    sh->flags |= COMMAND;
    close(sh->sock_desc);
    sh->sock_desc=0;
    shell_print(sh,"Connection Closed.");
    if (sh->flags&AUTODIE) kill_shell(sh,str);
  }
}

/* send a line to the connected server */

void send_out(sh,str)
shell *sh;
char *str;
{
  char *cpy;
  int len;
  if (!(sh->flags&CONNECTED)) return;  
  cpy=scratch;
  while(*str)  
    if (*str=='\n') {
      *cpy++='\r'; 
      *cpy++='\n';
      str++;
    }
    else
      *cpy++ = *str++;
  *cpy++='\r'; 
  *cpy++='\n';
  *cpy=0;
  len=strlen(scratch);
  if (!(sh->flags&SECRET) && 
      (sh->flags&ECHO_CHARS) || ((sh->flags&DO_BLANK) && 
				 (*(char *)scratch=='\n') && (len==2)))
    print(sh,scratch);
  if (write(sh->sock_desc,scratch,len) == -1) {
    shell_print("Error writing to socket ... closing.");
    close_com(sh,0);
  }
}

/* this bit acts on telnet control codes
   the code is a complete dogs breakfast
 */

int preprocess(sh)
shell *sh;
{
  unsigned char *from,*to;
  char pling[3];
  int len=0,i,test;
  from=(unsigned char *)scratch;
  to=(unsigned char *)scratch;
  while(*from) {
    switch(*from) {

/* got a telnet control char */

    case IAC:                             /* switch for telnet control */
      from++;
      switch(*from++) {

/* IP kill connection */

      case IP:                                        /* IP control */
	if (sh->flags&DIAGNOSTICS) {
	  print(sh,"RECEIVED - Interrupt Process\n");
	  print(sh,"RESPONSE - Close connection\n");
	}
	shell_print(sh,"Foreign host requested close.");
	close_com(sh,"");
	return 0;
	break;

/* prompt processing stuff */
	
      case GA:                                     /* Go ahead (prompt) */
	if (!(sh->flags&EOR_ON)) {
	  *to++=255;
	  len++;
	}
	break;
	
      case EOR:                                 /* EOR  (prompt)  */
	if (sh->flags&EOR_ON) {
	  *to++=255;
	  len++;
	}
	break;

/* WILL processing */

      case WILL:                                         /* WILL control */
	switch(*from++) {
	case TELOPT_ECHO:
	  if (sh->flags&SECRET) {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - WILL Echo\n");
	      print(sh,"RESPONSE - IGNORE, already in secret mode\n");
	    }
	  }
	  else {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - WILL Echo\n");
	      print(sh,"RESPONSE - Enter secrecy mode\n");
	    }
	    write(sh->sock_desc,"\377\375\001",3);
	    sh->flags |= SECRET;
	  }
	  break;
	case TELOPT_SGA:
          if (sh->flags&DIAGNOSTICS) {
            print(sh,"RECEIVED - WILL Single char mode\n");
            print(sh,"RESPONSE - Send DONT single char mode\n");
	  }
	  write(sh->sock_desc,"\377\376\003",3);
	  break;
	case TELOPT_EOR:
	  if (sh->flags&EOR_ON) {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - WILL EOR\n");
	      print(sh,"RESPONSE - IGNORE, already in EOR mode\n");
	    }
	  }
	  else {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - WILL EOR\n");
	      print(sh,"RESPONSE - Send DO EOR\n");
	    }
	    write(sh->sock_desc,"\377\375\031",3);
	    sh->flags |= EOR_ON;
	  }
	  break;
	}
	break;

/* WONT processing ... */

      case WONT:                                       /* WONT control */
	switch(*from++) {
	case TELOPT_ECHO:
	  if (!(sh->flags&SECRET)) {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - WONT Echo\n");
	      print(sh,"RESPONSE - IGNORE, already doing echo\n");
	    }
	  }
	  else {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - WONT Echo\n");
	      print(sh,"RESPONSE - Stop secrecy mode\n");
	    }
	    write(sh->sock_desc,"\377\376\001",3);
	    sh->flags &= ~SECRET;
	  }
	  break;
	case TELOPT_SGA:
          if (sh->flags&DIAGNOSTICS) {
            print(sh,"RECEIVED - WONT Single char mode\n");
            print(sh,"RESPONSE - Ignore (TUsh is already in this mode)\n");
          }
	  break;
	case TELOPT_EOR:
	  if (!(sh->flags&EOR_ON)) {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - WONT EOR\n");
	      print(sh,"RESPONSE - IGNORE, not in EOR mode\n");
	    }
	  }
	  else {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - WONT EOR\n");
	      print(sh,"RESPONSE - Send DONT EOR\n");
	    }
	    write(sh->sock_desc,"\377\376\031",3);
	    sh->flags &= ~EOR_ON;
	  }
	  break;
	}
	break;

/* DO processing ... received request to do something ... */

      case DO:
        switch(*from) {
        case TELOPT_ECHO:
	  if (!(sh->flags&SECRET)) {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - DO Echo\n");
	      print(sh,"RESPONSE - IGNORE, already echoing\n");
	    }
	  }
	  else {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - DO Echo\n");
	      print(sh,"RESPONSE - Stop secrecy mode\n");
	    }
	    write(sh->sock_desc,"\377\373\001",3);
	    sh->flags &= ~SECRET;
	    from++;
	  }
          break;
        case TELOPT_SGA:
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DO Single char mode\n");
            print(sh,"RESPONSE - Send WONT Single char mode\n");
	  }
	  write(sh->sock_desc,"\377\374\003",3);
	  from++;
          break;
	case TELOPT_EOR:
	  if (sh->flags&EOR_ON) {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - DO EOR\n");
	      print(sh,"RESPONSE - IGNORE, already in mode\n");
	    }
	  }
	  else {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - DO EOR\n");
	      print(sh,"RESPONSE - Send WILL EOR\n");
	    }
	    write(sh->sock_desc,"\377\373\031",3);
	    sh->flags |= EOR_ON;
	  }
	  break;
	default:
	  if (sh->flags&DIAGNOSTICS) {
	    print(sh,"RECEIVED - DO Unsupported Option\n");
            print(sh,"RESPONSE - Send WONT <option>\n");
	  }
	  pling[0]='\377';
	  pling[1]='\374';
	  pling[2]=*from++;
	  write(sh->sock_desc,pling,3);
	  break;
        }
        break;


/* DONT processing ... received request not to do something ... */

      case DONT:
        switch(*from++) {
        case TELOPT_ECHO:
	  if (sh->flags&SECRET) {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - DONT Echo\n");
	      print(sh,"RESPONSE - IGNORE, already in secrecy mode\n");
	    }
	  }
	  else {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - DONT Echo\n");
	      print(sh,"RESPONSE - Enter secrecy mode\n");
	    }
	    write(sh->sock_desc,"\377\374\001",3);
	    sh->flags |= SECRET;
	  }
          break;
        case TELOPT_SGA:
	  if (sh->flags&DIAGNOSTICS) {
            print(sh,"RECEIVED - DONT Single char mode\n");
            print(sh,"RESPONSE - Ignore (TUsh is already in this mode)\n");
	  }
	  break;
	case TELOPT_EOR:
	  if (!(sh->flags&EOR_ON)) {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - DONT EOR\n");
	      print(sh,"RESPONSE - IGNORE, already in mode\n");
	    }
	  }
	  else {
	    if (sh->flags&DIAGNOSTICS) {
	      print(sh,"RECEIVED - DONT EOR\n");
	      print(sh,"RESPONSE - Send WONT EOR\n");
	    }
	    write(sh->sock_desc,"\377\374\031",3);
	    sh->flags &= ~EOR_ON;
	  }
	  break;
        }
        break;
      }
      break;
    case '\r':
      if (*(from+1)!='\n') {
	len++;
	*to++='\n';
      }
      from++;
      break;
    case '\n':
      if (*(from+1)=='\r') from++;
      len++;
      *to++='\n';
      from++;
      break;
    default:
      len++;
      *to++ = *from++;
      break;
    }
  }
  *to++=0;
  return len;
}


/* recieves a line from the server */

void read_in(sh)
shell *sh;
{
  int chars_ready=0,sd,len,flags;
  char *m;

  flags=sh->flags&NO_UPDATE;
  if (sh==current_shell && !flags && !(sh->flags&HALF_LINE)) delete_input(sh);
  sh->flags |= NO_UPDATE;

  sd=sh->sock_desc;
  if (ioctl(sd,FIONREAD,&chars_ready) == -1) handle_error("Ioctl failed");
  if (!chars_ready) {
    shell_print(sh,"Connection closed by foreign host");
    close_com(sh,0);
    update(sh);
    return;
  }
  if (read(sd,scratch,chars_ready) == -1) handle_error("Reading from socket.");
  *((char *)scratch+chars_ready)=0;
  len=preprocess(sh);
  m=malloc(len+2);
  memcpy(m,scratch,len+1);
  input_processing(sh,m,len);
  free(m);
  
  sh->flags &= ~NO_UPDATE;
  sh->flags |= flags;
  if (sh==current_shell && !(sh->flags&NO_UPDATE) && !(sh->flags&HALF_LINE)) 
    draw_input(sh);
}

/* open up a connection to a server */

void open_socket(sh,host,port)
char *host;
int port;
shell *sh;
{
  struct in_addr inet_address;
  struct hostent *hp;
  struct sockaddr_in sock;
  int *address;

/* first convert 'host' into a recognisable address */

  if (isalpha(*host)) {
    hp=gethostbyname(host);
    if (!hp) {
      shell_print(sh,"Unknown hostname.");
      return;
    }
    bcopy(hp->h_addr,(char *)&inet_address,sizeof(struct in_addr));
  }
  else inet_address.s_addr=inet_addr(host);
  if (inet_address.s_addr == -1) {
    shell_print(sh,"Malformed number.");
    return;
  }
  sprintf(scratch,"%s Connecting to server : %s ...\n",SHELL_PRINT,host);
  print(sh,scratch);

  /* now grab a socket */  
  
  sh->sock_desc=socket(AF_INET,SOCK_STREAM,0);
  if (sh->sock_desc == -1) {
    shell_print(sh,"Unable to open socket.");
    return;
  }
  sock.sin_family=AF_INET;
  sock.sin_port=htons(port);
  sock.sin_addr=inet_address;
    
  /* and try to connect */
  
  if (connect(sh->sock_desc,&sock,sizeof(sock)) == -1) {
    shell_print(sh,"Couldn't connect to site.");
    shell_print(sh,sys_errlist[errno]);
    close(sh->sock_desc);
    return;
  }
  shell_print(sh,"Connected to server ...");
  sh->flags |= CONNECTED;
  sh->flags &= ~COMMAND;
}

/* the open command */

void open_com(sh,str)
shell *sh;
char *str;
{
  char *scan;
  int port=0;
  if (sh->flags&CONNECTED) {
    shell_print(sh,"Connection Already Established.");
    return;
  }
  if (!*str) {
    shell_print(sh,"Syntax - open <host> <port>");
    return;
  }
  scan=str;
  while(*scan && (*scan!=' ')) scan++;
  if (*scan) {
    *scan++=0;
    while(*scan && (*scan==' ')) scan++;
    port=atoi(scan);
  }
  if (!port) port=23;
  open_socket(sh,str,port);
}


