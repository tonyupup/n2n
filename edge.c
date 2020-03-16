/**
 * (C) 2007-18 - ntop.org and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not see see <http://www.gnu.org/licenses/>
 *
 */

#include "n2n.h"
#ifdef WIN32
#include <sys/stat.h>
#else
#include <pwd.h>
#endif

#define N2N_NETMASK_STR_SIZE    16 /* dotted decimal 12 numbers + 3 dots */
#define N2N_MACNAMSIZ           18 /* AA:BB:CC:DD:EE:FF + NULL*/
#define N2N_IF_MODE_SIZE        16 /* static | dhcp */

/* *************************************************** */

/** maximum length of command line arguments */
#define MAX_CMDLINE_BUFFER_LENGTH    4096

/** maximum length of a line in the configuration file */
#define MAX_CONFFILE_LINE_LENGTH     1024

/* ***************************************************** */

typedef struct n2n_priv_config {
  char                tuntap_dev_name[N2N_IFNAMSIZ];
  char                ip_mode[N2N_IF_MODE_SIZE];
  char                ip_addr[N2N_NETMASK_STR_SIZE];
  char                netmask[N2N_NETMASK_STR_SIZE];
  char                device_mac[N2N_MACNAMSIZ];
  int                 mtu;
  uint8_t             got_s;
  uint8_t             daemon;
#ifndef WIN32
  uid_t               userid;
  gid_t               groupid;
#endif
} n2n_priv_config_t;

/* ***************************************************** */

/** Find the address and IP mode for the tuntap device.
 *
 *  s is one of these forms:
 *
 *  <host> := <hostname> | A.B.C.D
 *
 *  <host> | static:<host> | dhcp:<host>
 *
 *  If the mode is present (colon required) then fill ip_mode with that value
 *  otherwise do not change ip_mode. Fill ip_mode with everything after the
 *  colon if it is present; or s if colon is not present.
 *
 *  ip_add and ip_mode are NULL terminated if modified.
 *
 *  return 0 on success and -1 on error
 */
static int scan_address(char * ip_addr, size_t addr_size,
			char * ip_mode, size_t mode_size,
			const char * s) {
  int retval = -1;
  char * p;

  if((NULL == s) || (NULL == ip_addr))
    {
      return -1;
    }

  memset(ip_addr, 0, addr_size);

  p = strpbrk(s, ":");

  if(p)
    {
      /* colon is present */
      if(ip_mode)
        {
	  size_t end=0;

	  memset(ip_mode, 0, mode_size);
	  end = MIN(p-s, (ssize_t)(mode_size-1)); /* ensure NULL term */
	  strncpy(ip_mode, s, end);
	  strncpy(ip_addr, p+1, addr_size-1); /* ensure NULL term */
	  retval = 0;
        }
    }
  else
    {
      /* colon is not present */
      strncpy(ip_addr, s, addr_size-1);
      ip_addr[addr_size-1] = '\0';
    }

  return retval;
}

/* *************************************************** */

static void help() {
  print_n2n_version();

  printf("edge <config file> (see edge.conf)\n"
	 "or\n"
	 );
  printf("edge "
#if defined(N2N_CAN_NAME_IFACE)
	 "-d <tun device> "
#endif /* #if defined(N2N_CAN_NAME_IFACE) */
	 "-a [static:|dhcp:]<tun IP address> "
	 "-c <community> "
	 "[-k <encrypt key>]\n"
	 "    "
	 "[-s <netmask>] "
#ifndef WIN32
	 "[-u <uid> -g <gid>]"
#endif /* #ifndef WIN32 */

#ifndef WIN32
	 "[-f]"
#endif /* #ifndef WIN32 */
#ifdef __linux__
	 "[-T <tos>]"
#endif
	 "[-m <MAC address>] "
	 "-l <supernode host:port>\n"
	 "    "
	 "[-p <local port>] [-M <mtu>] "
#ifndef __APPLE__
	 "[-D] "
#endif
	 "[-r] [-E] [-v] [-i <reg_interval>] [-L <reg_ttl>] [-t <mgmt port>] [-A] [-h]\n\n");

#if defined(N2N_CAN_NAME_IFACE)
  printf("-d <tun device>          | tun device name\n");
#endif

  printf("-a <mode:address>        | Set interface address. For DHCP use '-r -a dhcp:0.0.0.0'\n");
  printf("-c <community>           | n2n community name the edge belongs to.\n");
  printf("-k <encrypt key>         | Encryption key (ASCII) - also N2N_KEY=<encrypt key>.\n");
  printf("-s <netmask>             | Edge interface netmask in dotted decimal notation (255.255.255.0).\n");
  printf("-l <supernode host:port> | Supernode IP:port\n");
  printf("-i <reg_interval>        | Registration interval, for NAT hole punching (default 20 seconds)\n");
  printf("-L <reg_ttl>             | TTL for registration packet when UDP NAT hole punching through supernode (default 0 for not set )\n");
  printf("-p <local port>          | Fixed local UDP port.\n");
#ifndef WIN32
  printf("-u <UID>                 | User ID (numeric) to use when privileges are dropped.\n");
  printf("-g <GID>                 | Group ID (numeric) to use when privileges are dropped.\n");
#endif /* ifndef WIN32 */
#ifndef WIN32
  printf("-f                       | Do not fork and run as a daemon; rather run in foreground.\n");
#endif /* #ifndef WIN32 */
  printf("-m <MAC address>         | Fix MAC address for the TAP interface (otherwise it may be random)\n"
         "                         | eg. -m 01:02:03:04:05:06\n");
  printf("-M <mtu>                 | Specify n2n MTU of edge interface (default %d).\n", DEFAULT_MTU);
#ifndef __APPLE__
  printf("-D                       | Enable PMTU discovery. PMTU discovery can reduce fragmentation but\n"
         "                         | causes connections stall when not properly supported.\n");
#endif
  printf("-r                       | Enable packet forwarding through n2n community.\n");
#ifdef N2N_HAVE_AES
  printf("-A                       | Use AES CBC for encryption (default=use twofish).\n");
#endif
  printf("-E                       | Accept multicast MAC addresses (default=drop).\n");
  printf("-S                       | Do not connect P2P. Always use the supernode.\n");
#ifdef __linux__
  printf("-T <tos>                 | TOS for packets (e.g. 0x48 for SSH like priority)\n");
#endif
  printf("-v                       | Make more verbose. Repeat as required.\n");
  printf("-t <port>                | Management UDP Port (for multiple edges on a machine).\n");

  printf("\nEnvironment variables:\n");
  printf("  N2N_KEY                | Encryption key (ASCII). Not with -k.\n");

#ifdef WIN32
  printf("\nAvailable TAP adapters:\n");
  win_print_available_adapters();
#endif

  exit(0);
}

/* *************************************************** */

static int setOption(int optkey, char *optargument, n2n_priv_config_t *ec, n2n_edge_conf_t *conf) {
  /* traceEvent(TRACE_NORMAL, "Option %c = %s", optkey, optargument ? optargument : ""); */

  switch(optkey) {
  case 'a': /* IP address and mode of TUNTAP interface */
    {
      scan_address(ec->ip_addr, N2N_NETMASK_STR_SIZE,
		   ec->ip_mode, N2N_IF_MODE_SIZE,
		   optargument);
      break;
    }

  case 'c': /* community as a string */
    {
      memset(conf->community_name, 0, N2N_COMMUNITY_SIZE);
      strncpy((char *)conf->community_name, optargument, N2N_COMMUNITY_SIZE);
      conf->community_name[N2N_COMMUNITY_SIZE-1] = '\0';
      break;
    }

  case 'E': /* multicast ethernet addresses accepted. */
    {
      conf->drop_multicast=0;
      traceEvent(TRACE_DEBUG, "Enabling ethernet multicast traffic");
      break;
    }

#ifndef WIN32
  case 'u': /* unprivileged uid */
    {
      ec->userid = atoi(optargument);
      break;
    }

  case 'g': /* unprivileged uid */
    {
      ec->groupid = atoi(optargument);
      break;
    }
#endif

#ifndef WIN32
  case 'f' : /* do not fork as daemon */
    {
      ec->daemon=0;
      break;
    }
#endif /* #ifndef WIN32 */

  case 'm' : /* TUNTAP MAC address */
    {
      strncpy(ec->device_mac,optargument,N2N_MACNAMSIZ);
      ec->device_mac[N2N_MACNAMSIZ-1] = '\0';
      break;
    }

  case 'M' : /* TUNTAP MTU */
    {
      ec->mtu = atoi(optargument);
      break;
    }

#ifndef __APPLE__
  case 'D' : /* enable PMTU discovery */
    {
      conf->disable_pmtu_discovery = 0;
      break;
    }
#endif

  case 'k': /* encrypt key */
    {
      if(conf->encrypt_key) free(conf->encrypt_key);
      if(conf->transop_id == N2N_TRANSFORM_ID_NULL)
        conf->transop_id = N2N_TRANSFORM_ID_TWOFISH;

      conf->encrypt_key = strdup(optargument);
      traceEvent(TRACE_DEBUG, "encrypt_key = '%s'\n", conf->encrypt_key);
      break;
    }

  case 'r': /* enable packet routing across n2n endpoints */
    {
      conf->allow_routing = 1;
      break;
    }

#ifdef N2N_HAVE_AES
  case 'A':
    {
      conf->transop_id = N2N_TRANSFORM_ID_AESCBC;
      break;
    }
#endif

  case 'l': /* supernode-list */
    if(optargument) {
      if(edge_conf_add_supernode(conf, optargument) != 0) {
        traceEvent(TRACE_WARNING, "Too many supernodes!");
        exit(1);
      }
      break;
    }

  case 'i': /* supernode registration interval */
    conf->register_interval = atoi(optargument);
    break;

  case 'L': /* supernode registration interval */
    conf->register_ttl = atoi(optarg);
    break;

#if defined(N2N_CAN_NAME_IFACE)
  case 'd': /* TUNTAP name */
    {
      strncpy(ec->tuntap_dev_name, optargument, N2N_IFNAMSIZ);
      ec->tuntap_dev_name[N2N_IFNAMSIZ-1] = '\0';
      break;
    }
#endif

  case 'p':
    {
      conf->local_port = atoi(optargument);
      break;
    }

  case 't':
    {
      conf->mgmt_port = atoi(optargument);
      break;
    }

#ifdef __linux__
  case 'T':
    {
      if((optargument[0] == '0') && (optargument[1] == 'x'))
        conf->tos = strtol(&optargument[2], NULL, 16);
      else
        conf->tos = atoi(optargument);

      break;
    }
#endif

  case 's': /* Subnet Mask */
    {
      if(0 != ec->got_s) {
        traceEvent(TRACE_WARNING, "Multiple subnet masks supplied");
      }
      strncpy(ec->netmask, optargument, N2N_NETMASK_STR_SIZE);
      ec->netmask[N2N_NETMASK_STR_SIZE - 1] = '\0';
      ec->got_s = 1;
      break;
    }

  case 'S':
    {
      conf->allow_p2p = 0;
      break;
    }

  case 'h': /* help */
    {
      help();
      break;
    }

  case 'v': /* verbose */
    setTraceLevel(getTraceLevel() + 1);
    break;

  default:
    {
      traceEvent(TRACE_WARNING, "Unknown option -%c: Ignored", (char)optkey);
      return(-1);
    }
  }

  return(0);
}

/* *********************************************** */

static const struct option long_options[] = {
  { "community",       required_argument, NULL, 'c' },
  { "supernode-list",  required_argument, NULL, 'l' },
  { "tun-device",      required_argument, NULL, 'd' },
  { "euid",            required_argument, NULL, 'u' },
  { "egid",            required_argument, NULL, 'g' },
  { "help"   ,         no_argument,       NULL, 'h' },
  { "verbose",         no_argument,       NULL, 'v' },
  { NULL,              0,                 NULL,  0  }
};

/* *************************************************** */

/* read command line options */
static int loadFromCLI(int argc, char *argv[], n2n_edge_conf_t *conf, n2n_priv_config_t *ec) {
  u_char c;

  while((c = getopt_long(argc, argv,
			 "k:a:bc:Eu:g:m:M:s:d:l:p:fvhrt:i:SDL:"
#ifdef N2N_HAVE_AES
			 "A"
#endif
#ifdef __linux__
			 "T:"
#endif
			 ,
			 long_options, NULL)) != '?') {
    if(c == 255) break;
    setOption(c, optarg, ec, conf);
  }

  return 0;
}

/* *************************************************** */

static char *trim(char *s) {
  char *end;

  while(isspace(s[0]) || (s[0] == '"') || (s[0] == '\'')) s++;
  if(s[0] == 0) return s;

  end = &s[strlen(s) - 1];
  while(end > s
	&& (isspace(end[0])|| (end[0] == '"') || (end[0] == '\'')))
    end--;
  end[1] = 0;

  return s;
}

/* *************************************************** */

/* parse the configuration file */
static int loadFromFile(const char *path, n2n_edge_conf_t *conf, n2n_priv_config_t *ec) {
  char buffer[4096], *line, *key, *value;
  u_int line_len, opt_name_len;
  FILE *fd;
  const struct option *opt;

  fd = fopen(path, "r");

  if(fd == NULL) {
    traceEvent(TRACE_WARNING, "Config file %s not found", path);
    return -1;
  }

  while((line = fgets(buffer, sizeof(buffer), fd)) != NULL) {
    line = trim(line);
    value = NULL;

    if((line_len = strlen(line)) < 2 || line[0] == '#')
      continue;

    if(!strncmp(line, "--", 2)) { /* long opt */
      key = &line[2], line_len -= 2;

      opt = long_options;
      while(opt->name != NULL) {
	opt_name_len = strlen(opt->name);

	if(!strncmp(key, opt->name, opt_name_len)
	   && (line_len <= opt_name_len
	       || key[opt_name_len] == '\0'
	       || key[opt_name_len] == ' '
	       || key[opt_name_len] == '=')) {
	  if(line_len > opt_name_len)	  key[opt_name_len] = '\0';
	  if(line_len > opt_name_len + 1) value = trim(&key[opt_name_len + 1]);

	  // traceEvent(TRACE_NORMAL, "long key: %s value: %s", key, value);
	  setOption(opt->val, value, ec, conf);
	  break;
	}

	opt++;
      }
    } else if(line[0] == '-') { /* short opt */
      key = &line[1], line_len--;
      if(line_len > 1) key[1] = '\0';
      if(line_len > 2) value = trim(&key[2]);

      // traceEvent(TRACE_NORMAL, "key: %c value: %s", key[0], value);
      setOption(key[0], value, ec, conf);
    } else {
      traceEvent(TRACE_WARNING, "Skipping unrecognized line: %s", line);
      continue;
    }
  }

  fclose(fd);

  return 0;
}

/* ************************************** */

#if defined(DUMMY_ID_00001) /* Disabled waiting for config option to enable it */

static char gratuitous_arp[] = {
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, /* Dest mac */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Src mac */
  0x08, 0x06, /* ARP */
  0x00, 0x01, /* Ethernet */
  0x08, 0x00, /* IP */
  0x06, /* Hw Size */
  0x04, /* Protocol Size */
  0x00, 0x01, /* ARP Request */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Src mac */
  0x00, 0x00, 0x00, 0x00, /* Src IP */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Target mac */
  0x00, 0x00, 0x00, 0x00 /* Target IP */
};

/* ************************************** */

/** Build a gratuitous ARP packet for a /24 layer 3 (IP) network. */
static int build_gratuitous_arp(char *buffer, uint16_t buffer_len) {
  if(buffer_len < sizeof(gratuitous_arp)) return(-1);

  memcpy(buffer, gratuitous_arp, sizeof(gratuitous_arp));
  memcpy(&buffer[6], device.mac_addr, 6);
  memcpy(&buffer[22], device.mac_addr, 6);
  memcpy(&buffer[28], &device.ip_addr, 4);

  /* REVISIT: BbMaj7 - use a real netmask here. This is valid only by accident
   * for /24 IPv4 networks. */
  buffer[31] = 0xFF; /* Use a faked broadcast address */
  memcpy(&buffer[38], &device.ip_addr, 4);
  return(sizeof(gratuitous_arp));
}

/* ************************************** */

/** Called from update_supernode_reg to periodically send gratuitous ARP
 *  broadcasts. */
static void send_grat_arps(n2n_edge_t * eee,) {
  char buffer[48];
  size_t len;

  traceEvent(TRACE_NORMAL, "Sending gratuitous ARP...");
  len = build_gratuitous_arp(buffer, sizeof(buffer));
  send_packet2net(eee, buffer, len);
  send_packet2net(eee, buffer, len); /* Two is better than one :-) */
}

#endif /* #if defined(DUMMY_ID_00001) */

/* ************************************** */

static void daemonize() {
#ifndef WIN32
  int childpid;

  traceEvent(TRACE_NORMAL, "Parent process is exiting (this is normal)");

  signal(SIGPIPE, SIG_IGN);
  signal(SIGHUP,  SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  if((childpid = fork()) < 0)
    traceEvent(TRACE_ERROR, "Occurred while daemonizing (errno=%d)",
	       errno);
  else {
    if(!childpid) { /* child */
      int rc;

      //traceEvent(TRACE_NORMAL, "Bye bye: I'm becoming a daemon...");
      rc = chdir("/");
      if(rc != 0)
	traceEvent(TRACE_ERROR, "Error while moving to / directory");

      setsid();  /* detach from the terminal */

      fclose(stdin);
      fclose(stdout);
      /* fclose(stderr); */

      /*
       * clear any inherited file mode creation mask
       */
      //umask(0);

      /*
       * Use line buffered stdout
       */
      /* setlinebuf (stdout); */
      setvbuf(stdout, (char *)NULL, _IOLBF, 0);
    } else /* father */
      exit(0);
  }
#endif
}

/* *************************************************** */

static int keep_on_running;

#ifdef WIN32
BOOL WINAPI term_handler(DWORD sig)
#else
static void term_handler(int sig)
#endif
{
  static int called = 0;

  if(called) {
    traceEvent(TRACE_NORMAL, "Ok I am leaving now");
    _exit(0);
  } else {
    traceEvent(TRACE_NORMAL, "Shutting down...");
    called = 1;
  }

  keep_on_running = 0;
#ifdef WIN32
  return(TRUE);
#endif
}

/* *************************************************** */

/** Entry point to program from kernel. */
int main(int argc, char* argv[]) {
  int rc;
  tuntap_dev tuntap;    /* a tuntap device */
  n2n_edge_t *eee;      /* single instance for this program */
  n2n_edge_conf_t conf; /* generic N2N edge config */
  n2n_priv_config_t ec; /* config used for standalone program execution */
#ifndef WIN32
  struct passwd *pw = NULL;
#endif

  /* Defaults */
  edge_init_conf_defaults(&conf);
  memset(&ec, 0, sizeof(ec));
  ec.mtu = DEFAULT_MTU;
  ec.daemon = 1;    /* By default run in daemon mode. */

#ifndef WIN32
  if(((pw = getpwnam("n2n")) != NULL) ||
     ((pw = getpwnam("nobody")) != NULL)) {
    ec.userid = pw->pw_uid;
    ec.groupid = pw->pw_gid;
  }
#endif

#ifdef WIN32
  ec.tuntap_dev_name[0] = '\0';
#else
  snprintf(ec.tuntap_dev_name, sizeof(ec.tuntap_dev_name), "edge0");
#endif
  snprintf(ec.ip_mode, sizeof(ec.ip_mode), "static");
  snprintf(ec.netmask, sizeof(ec.netmask), "255.255.255.0");

  if((argc >= 2) && (argv[1][0] != '-')) {
    rc = loadFromFile(argv[1], &conf, &ec);
    if(argc > 2)
      rc = loadFromCLI(argc, argv, &conf, &ec);
  } else if(argc > 1)
    rc = loadFromCLI(argc, argv, &conf, &ec);
  else
#ifdef WIN32
    /* Load from current directory */
    rc = loadFromFile("edge.conf", &conf, &ec);
#else
    rc = -1;
#endif

  if(rc < 0)
    help();

  if(edge_verify_conf(&conf) != 0)
    help();

  traceEvent(TRACE_NORMAL, "Starting n2n edge %s %s", PACKAGE_VERSION, PACKAGE_BUILDDATE);

  /* Random seed */
  srand(time(NULL));

  if(0 == strcmp("dhcp", ec.ip_mode)) {
    traceEvent(TRACE_NORMAL, "Dynamic IP address assignment enabled.");

    conf.dyn_ip_mode = 1;
  } else
    traceEvent(TRACE_NORMAL, "ip_mode='%s'", ec.ip_mode);

  if(!(
#ifdef __linux__
       (ec.tuntap_dev_name[0] != 0) &&
#endif
       (ec.ip_addr[0] != 0)
       ))
    help();

#ifndef WIN32
  /* If running suid root then we need to setuid before using the force. */
  setuid(0);
  /* setgid(0); */
#endif

  if(tuntap_open(&tuntap, ec.tuntap_dev_name, ec.ip_mode, ec.ip_addr, ec.netmask, ec.device_mac, ec.mtu) < 0)
    return(-1);

  if(conf.encrypt_key && !strcmp((char*)conf.community_name, conf.encrypt_key))
    traceEvent(TRACE_WARNING, "Community and encryption key must differ, otherwise security will be compromised");

  if((eee = edge_init(&tuntap, &conf, &rc)) == NULL) {
    traceEvent(TRACE_ERROR, "Failed in edge_init");
    exit(1);
  }

#ifndef WIN32
  if(ec.daemon) {
    setUseSyslog(1); /* traceEvent output now goes to syslog. */
    daemonize();
  }
#endif /* #ifndef WIN32 */

#ifndef WIN32
  if((ec.userid != 0) || (ec.groupid != 0)) {
    traceEvent(TRACE_NORMAL, "Dropping privileges to uid=%d, gid=%d",
	       (signed int)ec.userid, (signed int)ec.groupid);

    /* Finished with the need for root privileges. Drop to unprivileged user. */
    if((setgid(ec.groupid) != 0)
       || (setuid(ec.userid) != 0)) {
      traceEvent(TRACE_ERROR, "Unable to drop privileges [%u/%s]", errno, strerror(errno));
      exit(1);
    }
  }

  if((getuid() == 0) || (getgid() == 0))
    traceEvent(TRACE_WARNING, "Running as root is discouraged, check out the -u/-g options");
#endif

#ifdef __linux__
  signal(SIGTERM, term_handler);
  signal(SIGINT,  term_handler);
#endif
#ifdef WIN32
  SetConsoleCtrlHandler(term_handler, TRUE);
#endif

  keep_on_running = 1;
  traceEvent(TRACE_NORMAL, "edge started");
  rc = run_edge_loop(eee, &keep_on_running);
  print_edge_stats(eee);

  /* Cleanup */
  edge_term(eee);
  tuntap_close(&tuntap);

  if(conf.encrypt_key) free(conf.encrypt_key);

  return(rc);
}

/* ************************************** */
