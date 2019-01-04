/***************************************************************************
 * main.cc for NetBeans                                                    *
 *                                                                         *
 * Purpose:   *
 * Author:  Slavik Ivantsiv, 2018                                          *
 ***************************************************************************/

/* Headers */
#include <asm/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <stdio.h>

/* Function prototypes */
int getType(unsigned int type_code, char* type_str, int max_len);
int read_message(int sockint);

/*if_arp.h
 *  main() is entry point of the application
 *  Initialise NetLink socket and send a request to the Kernel.
 *  Wait for the response using select() statement.
 */
int main(int argc, char**argv) 
{
  fd_set rfds, wfds;
  struct timeval tv;
  int retval;
  struct sockaddr_nl addr;
  static int sequence_num = 0;
  
  struct {  struct nlmsghdr nlh;
            struct rtgenmsg g;  } req;  

  int nl_socket = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
  if (nl_socket < 0)
  {
    printf("Socket Open Error!");
    exit(1);
  }

  memset((void *) &addr, 0, sizeof (addr));

  addr.nl_family = AF_NETLINK;
  addr.nl_pid = getpid();
  addr.nl_groups = 0;

  if (bind(nl_socket, (struct sockaddr *)&addr, sizeof (addr)) < 0)
  {
    printf("Socket bind failed!");
    exit(1);
  }
  
  memset(&req, 0, sizeof(req));  
  req.nlh.nlmsg_len = sizeof(req);
  req.nlh.nlmsg_type = RTM_GETLINK;
  req.nlh.nlmsg_flags = NLM_F_ROOT | NLM_F_MATCH | NLM_F_REQUEST;
  req.nlh.nlmsg_pid = 0;
  req.nlh.nlmsg_seq =  ++sequence_num;
  req.g.rtgen_family = AF_NETLINK;
  
  send(nl_socket, (void*)&req, sizeof(req), 0);
  
  FD_ZERO(&rfds);
  FD_CLR (nl_socket, &rfds);
  FD_SET (nl_socket, &rfds);

  /* 500 ms response timeout */
  tv.tv_sec  = 0;
  tv.tv_usec = 500;

  retval = select(FD_SETSIZE, &rfds, NULL, NULL, &tv);
  if (retval == -1)
  {
    printf("Error select() \n");
  }
  else if(retval)
  {      
    read_message(nl_socket);
  }    
    
 return 0;
}

/*
 *  Decode network interface type based on if_arp.h encoding.
 *  Return result in the string argument.
 *  
 */
int getType(unsigned int type_code, char* type_str, int max_len)
{
    int rc = -1;
    
    if (type_str != NULL)
    {
        /* Interface identifiers based on if_arp.h */
        switch(type_code)
        {
            case 0:   strncpy(type_str, "NET/ROM", max_len);            
                      break;                    
            case 1:   strncpy(type_str, "Ethernet", max_len);            
                      break;                     
            case 2:   strncpy(type_str, "Experimental", max_len);            
                      break;
            case 3:   strncpy(type_str, "AX.25", max_len);            
                      break;
            case 4:   strncpy(type_str, "PROnet", max_len);            
                      break;
            case 5:   strncpy(type_str, "Chaosnet", max_len);            
                      break;
            case 6:   strncpy(type_str, "IEEE 802.2", max_len);            
                      break;
            case 7:   strncpy(type_str, "ARCnet", max_len);            
                      break;
            case 8:   strncpy(type_str, "APPLEtalk", max_len);            
                      break;
            case 15:  strncpy(type_str, "Frame Relay", max_len);            
                      break;        
            case 19:  strncpy(type_str, "ATM", max_len);            
                      break;
            case 23:  strncpy(type_str, "Mericom", max_len);            
                      break;
            case 24:  strncpy(type_str, "IEEE 1394", max_len);            
                      break;
            case 27:  strncpy(type_str, "EUI-64", max_len);            
                      break;
            case 32:  strncpy(type_str, "InfiniBand", max_len);            
                      break;
            case 772: strncpy(type_str, "Loopback", max_len);            
                      break;
            default:  strncpy(type_str, "Unknown", max_len);            
                      break;                                                
        }
        rc = 0;
    }
        
    return rc;
}

// Read response message from Kernel
/*
 *  Read response message and iterate through linked list fields.
 *  Find relevant network interfaces and print their statuses and
 *  close NetLink socket.
 */
int read_message(int sockint)
{
  int status;
  int ret = 0;
  char buf[4096];    
  struct nlmsghdr *h;
  struct ifinfomsg *ifi;
  char type[IFNAMSIZ];
  struct rtattr *rta_ptr;
  int attr_len;

  status = recv(sockint, &buf, sizeof(buf), 0);

  if (status < 0)
  {      
      close(sockint);
      return status;
  }

  for (h = (struct nlmsghdr *)buf; NLMSG_OK(h, (unsigned int)status); h = NLMSG_NEXT(h, status))
  {
    /* Finish reading */
    if (h->nlmsg_type == NLMSG_DONE)
    {
        close(sockint);
        return ret;
    }   
    else if (h->nlmsg_type == NLMSG_ERROR)
    {
        close(sockint);
        return -1;        
    }
   
    ifi = (ifinfomsg*)NLMSG_DATA(h);
    getType(ifi->ifi_type, type, sizeof(type));    
    rta_ptr = (struct rtattr *)IFLA_RTA(ifi);
    attr_len = IFLA_PAYLOAD(h);
    for (; RTA_OK(rta_ptr, attr_len); rta_ptr = RTA_NEXT(rta_ptr, attr_len)) 
    {
     if  (rta_ptr->rta_type == IFLA_IFNAME)
     {        
        break;
     }
    }
    printf("%s Interface %s is: %s\n", type, (char*)RTA_DATA(rta_ptr), (ifi->ifi_flags & IFF_RUNNING) ? "Connected " : "Disconnected");    
  }

  close(sockint);
  return ret;
}
