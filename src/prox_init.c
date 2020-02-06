#include <vex.h>

void usage(const char *name) 
{
    fprintf(stderr, "%s < -f proxy_list > < -l addr:port > [ -t addr:port ] [ -u username ] [ -p password ] [ -5 ] [ -a ] [ -b ] [ -h ]\n\n",
            name);
    fprintf(stderr, "GENERAL OPTIONS:\n");
    fprintf(stderr, "  -f\tproxy_list\tPath to the proxy list (must be in\n");
    fprintf(stderr, "\t\t\taddress:port format, one per line)\n");
    fprintf(stderr, "  -l\taddr:port\tLocal address and port\n");
    fprintf(stderr, 
            "  -t\taddr:port\tTarget address [default: \"%s:%s\"]\n", 
            DEFAULT_TADDR, DEFAULT_TPORT);
    fprintf(stderr, 
            "  -u\tusername\tUsername for SOCKS5 authentication, or for\n");
    fprintf(stderr, 
            "\t\t\tthe SOCKS4 userid field [default: \"%s\"]\n", 
            DEFAULT_USER);
    fprintf(stderr, 
            "  -b\t\t\tBind at the local address, instead of connecting\n");
    fprintf(stderr, "  -x\t\t\tConnection timeout [default: %d]\n", 
            DEFAULT_TMOUT);
    fprintf(stderr, "  -h\t\t\tDisplay this message\n");
    fprintf(stderr, "\nSOCKS5 OPTIONS:\n");
    fprintf(stderr, "  -5\t\t\tUse SOCKS5 [default: SOCKS4]\n");
    fprintf(stderr, "  -a\t\t\tAdd User/Pass Authentication\n");
    fprintf(stderr, 
            "  -p\tpassword\tPassword for \'-a\'  [default: \"%s\"]\n", 
            DEFAULT_PASS);
    exit(EXIT_FAILURE);
}

struct proxy_config *init_proxy(int argc, char **argv)
{
    int opt;
    uint8_t socks_type = SOCKS4,
            targ_addr_t = IP_ADDRESS;
    uint32_t methods = 0;
    const char *userid = NULL, 
               *passwd = NULL;
    char *targ_addr = NULL,
         *targ_port = NULL;
    struct proxy_config *pc = NULL;
    
    if (!(pc = (struct proxy_config *) calloc(1, sizeof(struct proxy_config))))
    {
        fprintf(stderr, "calloc: %s\n", strerror(errno));
        return NULL;
    }

    while ((opt = getopt(argc, argv, "5ahbxf:l:t:u:p:")) != -1)
    {
        switch (opt)
        {
            case 'f':
                if (!(pc->socks_list = parse_socks_list(optarg))) goto fail;
                break;
            case 'l':
                if (!(parse_string_addr(optarg, &pc->loc_addr, 
                                &pc->loc_port))) goto fail;
                break;
            case 't':
                if (!(targ_addr_t = parse_string_addr(optarg, &targ_addr, 
                            &targ_port))) goto fail;
                break;
            case 'u':
                userid = optarg;
                break;
            case 'p':
                passwd = optarg;
                break;
            case 'x':
                pc->tmout = strtol(optarg, NULL, 10);
                if (pc->tmout == LONG_MIN || pc->tmout == LONG_MAX)
                {
                    fprintf(stderr, 
                            "[!] Invalid timeout value - using default\n");
                    pc->tmout = DEFAULT_TMOUT;
                }
                break;
            case 'b':
                pc->bind_local = 1;
                break;
            case 'a':
                methods |= USERPASS;
                break;
            case '5':
                socks_type = SOCKS5;
                break;
            case 'h':
            default:
                usage(argv[0]);
                break;
        }
    }

    if (!pc->loc_addr || !pc->loc_port) 
    {
        fprintf(stderr, "[!] Must specify local address and port\n");
        usage(argv[0]);
    }

    if (!pc->tmout) pc->tmout = DEFAULT_TMOUT;

    if (!pc->socks_list) 
    {
        fprintf(stderr, "[!] Must specify proxy list filepath\n");
        usage(argv[0]);
    }

    if (!(pc->socks_conf = init_socks(socks_type, methods))) goto fail;

    if (!targ_addr) 
    {
        if(!(targ_addr = strndup(DEFAULT_TADDR, INET_ADDRSTRLEN)))
        {
            fprintf(stderr, "strndup: %s\n", strerror(errno));
            goto fail;
        }
    }

    if (!targ_port) 
    {
       if (!(targ_port = strndup(DEFAULT_TPORT, MAX_PORT)))
       {
           fprintf(stderr, "strndup: %s\n", strerror(errno));
           goto fail;
       }
    }

    pc->socks_conf->userid = userid ? userid : DEFAULT_USER;
    pc->socks_conf->passwd = passwd ? passwd : DEFAULT_PASS;

    if (targ_addr_t == IP6_ADDRESS && socks_type == SOCKS4)
    {
        fprintf(stderr, "[!] IPv6 is not supported by SOCKS4\n");
        goto fail;
    }

    if (!(pc->target = init_target(targ_addr_t, targ_addr, targ_port)))
        goto fail;

    if ((pc->listen_fd = start_socket(pc->loc_addr, pc->loc_port,
                    pc->bind_local, pc->tmout)) == -1) 
    {
        fprintf(stderr, "start_socket: %s\n", strerror(errno));
        goto fail;
    }

    if (attempt_socks_connect(pc) == -1) goto fail;

    if (targ_addr) 
    {
        free(targ_addr);
        targ_addr = NULL;
    }

    if (targ_port)
    {
        free(targ_port);
        targ_port = NULL;
    }

    return pc;

fail:
    if (targ_addr) 
    {
        free(targ_addr);
        targ_addr = NULL;
    }

    if (targ_port)
    {
        free(targ_port);
        targ_port = NULL;
    }

    free_proxy_config(pc);
    pc = NULL;
    return NULL;
}

void free_proxy_config(struct proxy_config *pc)
{
    if (!pc) return;

    if (pc->loc_addr)
    {
        free(pc->loc_addr);
        pc->loc_addr = NULL;
    }

    if (pc->loc_port)
    {
        free(pc->loc_port);
        pc->loc_port = NULL;
    }

    if (pc->target)
    {
        if (pc->target->address)
        {
            free(pc->target->address);
            pc->target->address = NULL;
        }
        free(pc->target);
        pc->target = NULL;
    }

    if (pc->socks_list)
    {
        free_socks_list(pc->socks_list);
        pc->socks_list= NULL;
    }

    if (pc->socks_conf)
    {
        free(pc->socks_conf);
        pc->socks_conf = NULL;
    }

    free(pc);
}
