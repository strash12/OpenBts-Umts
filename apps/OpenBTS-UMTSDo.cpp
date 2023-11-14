/*
 * OpenBTS provides an open source alternative to legacy telco protocols and
 * traditionally complex, proprietary hardware systems.
 *
 * Copyright 2011, 2012, 2013, 2014 Range Networks, Inc.
 *
 * This software is distributed under the terms of the GNU Affero General 
 * See the COPYING and NOTICE files in the current or main directory for
 * directory for licensing information.
 *
 * This use of this software may be subject to additional restrictions.
 * See the LEGAL file in the main directory for details.
 */

// KEEP THIS FILE CLEAN FOR PUBLIC RELEASE.

#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>


#define DEFAULT_CMD_PATH "/var/run/OpenBTS-UMTS-command"

int main(int argc, char *argv[])
{
	const char* cmdPath = DEFAULT_CMD_PATH;
	if (argc!=1) {
		cmdPath = argv[1];
	}

	char rspPath[200];
	sprintf(rspPath,"/tmp/OpenBTS-UMTS.do.%d",getpid());

	// the socket
	int sock = socket(AF_UNIX,SOCK_DGRAM,0);
	if (sock<0) {
		perror("opening datagram socket");
		exit(1);
	}

	// destination address
	struct sockaddr_un cmdSockName;
	cmdSockName.sun_family = AF_UNIX;
	strcpy(cmdSockName.sun_path,cmdPath);

	// locally bound address
	struct sockaddr_un rspSockName;
	rspSockName.sun_family = AF_UNIX;
	char rmcmd[strlen(rspPath)+10];
	sprintf(rmcmd,"rm -f %s",rspPath);
	system(rmcmd);
	strcpy(rspSockName.sun_path,rspPath);
	if (bind(sock, (struct sockaddr *) &rspSockName, sizeof(struct sockaddr_un))) {
		perror("binding name to datagram socket");
		exit(1);
	}


	char *inbuf = (char*)malloc(200);
	char *cmd = fgets(inbuf,199,stdin);
	if (!cmd) exit(0);
	cmd[strlen(cmd)-1] = '\0';

	if (sendto(sock,cmd,strlen(cmd)+1,0,(struct sockaddr*)&cmdSockName,sizeof(cmdSockName))<0) {
		perror("sending datagram");
		exit(1);
	}

	// buffer to be sized as necessary to accomodate config data length
	const int bufsz = 8500;
	char resbuf[bufsz];
	int nread = recv(sock,resbuf,bufsz-1,0);
	if (nread<0) {
		perror("receiving response");
		exit(1);
	}
	resbuf[nread] = '\0';
	printf("%s\n",resbuf);

	close(sock);

	// Delete the path to limit clutter in /tmp.
	sprintf(rmcmd,"rm -f %s",rspPath);
	system(rmcmd);
}
