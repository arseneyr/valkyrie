/*
	 Copyright (c) 2023 arseneyr

	 Updated to only target datagram sockets and to resolve
	 hostnames.

	 Copyright (C) 2000  Daniel Ryde

	 This library is free software; you can redistribute it and/or
	 modify it under the terms of the GNU Lesser General Public
	 License as published by the Free Software Foundation; either
	 version 2.1 of the License, or (at your option) any later version.

	 This library is distributed in the hope that it will be useful,
	 but WITHOUT ANY WARRANTY; without even the implied warranty of
	 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	 Lesser General Public License for more details.
*/

/*
	 LD_PRELOAD library to make bind and connect to use a virtual
	 IP address as localaddress. Specified via the enviroment
	 variable BIND_ADDR.

	 Compile on Linux with:
	 gcc -nostartfiles -fpic -shared bind.c -o bind.so -ldl -D_GNU_SOURCE


	 Example in bash to make inetd only listen to the localhost
	 lo interface, thus disabling remote connections and only
	 enable to/from localhost:

	 BIND_ADDR="127.0.0.1" LD_PRELOAD=./bind.so /sbin/inetd


	 Example in bash to use your virtual IP as your outgoing
	 sourceaddress for ircII:

	 BIND_ADDR="your-virt-ip" LD_PRELOAD=./bind.so ircII

	 Note that you have to set up your servers virtual IP first.


	 This program was made by Daniel Ryde
	 email: daniel@ryde.net
	 web:   http://www.ryde.net/

	 TODO: I would like to extend it to the accept calls too, like a
	 general tcp-wrapper. Also like an junkbuster for web-banners.
	 For libc5 you need to replace socklen_t with int.
*/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <errno.h>

int (*real_bind)(int, const struct sockaddr *, socklen_t);
int (*real_connect)(int, const struct sockaddr *, socklen_t);

char *bind_addr_env;
struct in_addr bind_addr_inaddr;
struct sockaddr_in local_sockaddr_in;

void __attribute__((constructor)) init(void)
{
	const char *err;

	real_bind = dlsym(RTLD_NEXT, "bind");
	if ((err = dlerror()) != NULL)
	{
		fprintf(stderr, "dlsym (bind): %s\n", err);
	}

	real_connect = dlsym(RTLD_NEXT, "connect");
	if ((err = dlerror()) != NULL)
	{
		fprintf(stderr, "dlsym (connect): %s\n", err);
	}

	if (bind_addr_env = getenv("BIND_ADDR"))
	{
		struct addrinfo in_ai = {
				.ai_family = AF_INET,
				.ai_socktype = SOCK_DGRAM,
				.ai_flags = AI_ADDRCONFIG};
		struct addrinfo *out_ai;
		if (getaddrinfo(bind_addr_env, NULL, &in_ai, &out_ai) == 0 && out_ai->ai_addr->sa_family == AF_INET)
		{
			struct sockaddr_in *sin = (struct sockaddr_in *)out_ai->ai_addr;
			printf("redirecting udp bind to %s\n", inet_ntoa(sin->sin_addr));

			bind_addr_inaddr = sin->sin_addr;
			local_sockaddr_in = *(struct sockaddr_in *)out_ai->ai_addr;
			freeaddrinfo(out_ai);
		}
		else
		{
			fprintf(stderr, "could not find %s\n", bind_addr_env);
			bind_addr_env = NULL;
		}
	}
}

int get_socket_type(int fd)
{
	int type = 0;
	int size = sizeof(type);

	getsockopt(fd, SOL_SOCKET, SO_TYPE, &type, &size);
	return type;
}

int bind(int fd, const struct sockaddr *sk, socklen_t sl)
{
	// struct in_addr i = {lsk_in->sin_addr.s_addr};
	// printf("bind: %d %s:%d\n", fd, inet_ntoa(i),
	// 				ntohs(lsk_in->sin_port));
	if (bind_addr_env && (get_socket_type(fd) == SOCK_DGRAM) && (sk->sa_family == AF_INET))
	{
		((struct sockaddr_in *)sk)->sin_addr = bind_addr_inaddr;
	}
	return real_bind(fd, sk, sl);
}

int connect(int fd, const struct sockaddr *sk, socklen_t sl)
{
	if (bind_addr_env && (get_socket_type(fd) == SOCK_DGRAM) && (sk->sa_family == AF_INET))
	{
		int err;
		// struct in_addr i = {rsk_in->sin_addr.s_addr};
		// printf("connect: %d %s:%d\n", fd, inet_ntoa(i),
		// 			 ntohs(rsk_in->sin_port));
		err = real_bind(fd, (struct sockaddr *)&local_sockaddr_in, sizeof(struct sockaddr));
		if (err)
		{
			return err;
		}
	}
	return real_connect(fd, sk, sl);
}