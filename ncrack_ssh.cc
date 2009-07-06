
/***************************************************************************
 * ncrack_ssh.cc -- ncrack module for the SSH protocol                     *
 *                                                                         *
 ***********************IMPORTANT NMAP LICENSE TERMS************************
 *                                                                         *
 * The Nmap Security Scanner is (C) 1996-2009 Insecure.Com LLC. Nmap is    *
 * also a registered trademark of Insecure.Com LLC.  This program is free  *
 * software; you may redistribute and/or modify it under the terms of the  *
 * GNU General Public License as published by the Free Software            *
 * Foundation; Version 2 with the clarifications and exceptions described  *
 * below.  This guarantees your right to use, modify, and redistribute     *
 * this software under certain conditions.  If you wish to embed Nmap      *
 * technology into proprietary software, we sell alternative licenses      *
 * (contact sales@insecure.com).  Dozens of software vendors already       *
 * license Nmap technology such as host discovery, port scanning, OS       *
 * detection, and version detection.                                       *
 *                                                                         *
 * Note that the GPL places important restrictions on "derived works", yet *
 * it does not provide a detailed definition of that term.  To avoid       *
 * misunderstandings, we consider an application to constitute a           *
 * "derivative work" for the purpose of this license if it does any of the *
 * following:                                                              *
 * o Integrates source code from Nmap                                      *
 * o Reads or includes Nmap copyrighted data files, such as                *
 *   nmap-os-db or nmap-service-probes.                                    *
 * o Executes Nmap and parses the results (as opposed to typical shell or  *
 *   execution-menu apps, which simply display raw Nmap output and so are  *
 *   not derivative works.)                                                * 
 * o Integrates/includes/aggregates Nmap into a proprietary executable     *
 *   installer, such as those produced by InstallShield.                   *
 * o Links to a library or executes a program that does any of the above   *
 *                                                                         *
 * The term "Nmap" should be taken to also include any portions or derived *
 * works of Nmap.  This list is not exclusive, but is meant to clarify our *
 * interpretation of derived works with some common examples.  Our         *
 * interpretation applies only to Nmap--we don't speak for other people's  *
 * GPL works.                                                              *
 *                                                                         *
 * If you have any questions about the GPL licensing restrictions on using *
 * Nmap in non-GPL works, we would be happy to help.  As mentioned above,  *
 * we also offer alternative license to integrate Nmap into proprietary    *
 * applications and appliances.  These contracts have been sold to dozens  *
 * of software vendors, and generally include a perpetual license as well  *
 * as providing for priority support and updates as well as helping to     *
 * fund the continued development of Nmap technology.  Please email        *
 * sales@insecure.com for further information.                             *
 *                                                                         *
 * As a special exception to the GPL terms, Insecure.Com LLC grants        *
 * permission to link the code of this program with any version of the     *
 * OpenSSL library which is distributed under a license identical to that  *
 * listed in the included COPYING.OpenSSL file, and distribute linked      *
 * combinations including the two. You must obey the GNU GPL in all        *
 * respects for all of the code used other than OpenSSL.  If you modify    *
 * this file, you may extend this exception to your version of the file,   *
 * but you are not obligated to do so.                                     *
 *                                                                         *
 * If you received these files with a written license agreement or         *
 * contract stating terms other than the terms above, then that            *
 * alternative license agreement takes precedence over these comments.     *
 *                                                                         *
 * Source is provided to this software because we believe users have a     *
 * right to know exactly what a program is going to do before they run it. *
 * This also allows you to audit the software for security holes (none     *
 * have been found so far).                                                *
 *                                                                         *
 * Source code also allows you to port Nmap to new platforms, fix bugs,    *
 * and add new features.  You are highly encouraged to send your changes   *
 * to nmap-dev@insecure.org for possible incorporation into the main       *
 * distribution.  By sending these changes to Fyodor or one of the         *
 * Insecure.Org development mailing lists, it is assumed that you are      *
 * offering the Nmap Project (Insecure.Com LLC) the unlimited,             *
 * non-exclusive right to reuse, modify, and relicense the code.  Nmap     *
 * will always be available Open Source, but this is important because the *
 * inability to relicense code has caused devastating problems for other   *
 * Free Software projects (such as KDE and NASM).  We also occasionally    *
 * relicense the code to third parties as discussed above.  If you wish to *
 * specify special license conditions of your contributions, just say so   *
 * when you send them.                                                     *
 *                                                                         *
 * This program is distributed in the hope that it will be useful, but     *
 * WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU       *
 * General Public License v2.0 for more details at                         *
 * http://www.gnu.org/licenses/gpl-2.0.html , or in the COPYING file       *
 * included with Nmap.                                                     *
 *                                                                         *
 ***************************************************************************/


#include "ncrack.h"
#include "nsock.h"
#include "NcrackOps.h"
#include "Service.h"
#include "modules.h"
#include <list>

/* OpenSSH include-files */
//#include "openssh.h"
#include "buffer.h"
#include "kex.h"
#include "sshconnect.h"
#include "packet.h"


#define SSH_TIMEOUT 20000


extern NcrackOps o;

extern void ncrack_read_handler(nsock_pool nsp, nsock_event nse, void *mydata);
extern void ncrack_write_handler(nsock_pool nsp, nsock_event nse, void *mydata);
extern void ncrack_connect_handler(nsock_pool nsp, nsock_event nse, void *mydata);
extern void ncrack_module_end(nsock_pool nsp, void *mydata);


typedef struct ssh_info {
  Kex *kex;

} ssh_info;


enum states { SSH_INIT, SSH_ID_EX, SSH_KEY, SSH_KEY2, SSH_KEY3, SSH_KEY4, SSH_FINI };

void
ncrack_ssh(nsock_pool nsp, Connection *con)
{
  char lbuf[BUFSIZE]; /* local buffer */
  nsock_iod nsi = con->niod;
  Service *serv = con->service;
  const char *hostinfo = serv->HostInfo();
  void *ioptr;
  u_int buflen;
  Buffer ncrack_buf; /* this is OpenSSH's buffer, not Ncrack's class */
  ssh_info *info;
  int type;

  if (con->misc_info)
    info = (ssh_info *) con->misc_info;

  switch (con->state)
  {
    case SSH_INIT:
      con->state = SSH_ID_EX;
      con->misc_info = (ssh_info *)safe_zalloc(sizeof(ssh_info));  
      nsock_read(nsp, nsi, ncrack_read_handler, SSH_TIMEOUT, con);
      break;

    case SSH_ID_EX:

      buflen = con->iobuf->get_len();
      ioptr = con->iobuf->get_dataptr();
      if (!memsearch((const char *)ioptr, "\n", buflen)) {
        con->state = SSH_ID_EX;
        nsock_read(nsp, nsi, ncrack_read_handler, SSH_TIMEOUT, con);
        break;
      }
      if (strncmp((const char *)ioptr, "SSH-", 4)) {
        con->iobuf->clear();
        con->state = SSH_ID_EX;
        nsock_read(nsp, nsi, ncrack_read_handler, SSH_TIMEOUT, con);
        break;
      }
      printf("%s ssh id: %s\n", hostinfo, (char *)ioptr);
      /* NEVER forget to free allocated memory and also NULL-assign ptr */
      delete con->iobuf;
      con->iobuf = NULL;

      con->state = SSH_KEY;
      strncpy(lbuf, "SSH-2.0-OpenSSH 5.2\n", sizeof(lbuf) - 1); 
      nsock_write(nsp, nsi, ncrack_write_handler, SSH_TIMEOUT, con, lbuf, -1);
      break;

    case SSH_KEY:

      con->state = SSH_KEY2;
      buffer_init(&ncrack_buf); 
      info->kex = ssh_kex2(&ncrack_buf);

      nsock_write(nsp, nsi, ncrack_write_handler, SSH_TIMEOUT, con, 
          (const char *)buffer_ptr(&ncrack_buf), buffer_len(&ncrack_buf));
      buffer_free(&ncrack_buf);
      break;

    case SSH_KEY2:

      con->state = SSH_KEY3;
      nsock_read(nsp, nsi, ncrack_read_handler, SSH_TIMEOUT, con);
      break;

    case SSH_KEY3:

      con->state = SSH_KEY4;

      /* convert Ncrack's Buf to ssh's Buffer */
      buffer_init(&ncrack_buf);
      buffer_append(&ncrack_buf, con->iobuf->get_dataptr(), con->iobuf->get_len());
      //buffer_dump(&ncrack_buf);

      type = ssh_packet_read(&ncrack_buf);

      buffer_clear(&ncrack_buf);
      ssh_kex_input_kexinit(type, 0, info->kex, &ncrack_buf);
      
      nsock_write(nsp, nsi, ncrack_write_handler, SSH_TIMEOUT, con, 
          (const char *)buffer_ptr(&ncrack_buf), buffer_len(&ncrack_buf));

      break;


    case SSH_KEY4:

      printf("SSH KEY 4\n");
      break;


    case SSH_FINI:

      return ncrack_module_end(nsp, con);
  }
}