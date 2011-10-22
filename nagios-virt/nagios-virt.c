/* nagios-virt
 * Copyright (C) 2007 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * By Richard W.M. Jones <rjones@redhat.com>
 */

#include "config.h"

#define _GNU_SOURCE		/* getopt_long */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <errno.h>
#include <assert.h>

#include <libvirt/libvirt.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>

#define NV_LIBRARY  "virt-library.cfg"
#define NV_HOSTS    "virt-hosts.cfg"
//#define NV_SERVICES "virt-services.cfg"

#define STREQ(s1,s2) (strcmp ((s1),(s2)) == 0)
#define STRNEQ(s1,s2) (strcmp ((s1),(s2)) != 0)

static void usage (char *prog);
static void cmd_install (int argc, char **argv);
static void cmd_list (int argc, char **argv);
static void cmd_config (int argc, char **argv);

static int for_each_domain (int (*callback) (void *data, int i, int is_active, virDomainPtr, const char *xml, xmlDocPtr, const char *name), void *data);
static char *xpath_string (const char *xpath_expr, xmlXPathContextPtr ctx);
static int copy_fd_to_file (int fd, const char *pathname);
static int copy_fd_to_fd (int fd, int fd2);

static virConnectPtr conn = NULL;

static struct option base_options[] = {
  {"connect", 1, NULL, 'c' },
  { NULL, 0, NULL, 0 }
};
#define BASE_OPTIONS_SHORT "+c:"

int
main (int argc, char **argv)
{
  const char *cmd;
  const char *uri = NULL;

  /* Deal with help, version, config first. */
  if (argc < 2) {
    fprintf (stderr, "For help, use `%s --help'\n", argv[0]);
    exit (1);
  }

  cmd = argv[1];
  if (STREQ (cmd, "help") || STREQ (cmd, "--help")) {
    usage (argv[0]);
    exit (0);
  }
  else if (STREQ (cmd, "version") || STREQ (cmd, "--version")) {
    fprintf (stderr, "%s\n", PACKAGE_STRING);
    exit (0);
  }
  else if (STREQ (cmd, "config") || STREQ (cmd, "--config")) {
    cmd_config (argc-1, argv+1);
    exit (0);
  }

  /* Init libxml */   
  xmlInitParser();
  LIBXML_TEST_VERSION;

  /* At this point, parse the base options. */
  for (;;) {
    int c, index = 0;

    c = getopt_long (argc, argv,
		     BASE_OPTIONS_SHORT, base_options, &index);
    if (c == -1) break;

    switch (c) {
    case 'c':
      uri = optarg;
      break;
    case '?':
      usage (argv[0]);
      exit (0);
    default:
      fprintf (stderr,
	       "%s: ignored unknown return value from getopt_long: %d\n",
	       argv[0], c);
    }
  }

  if (optind >= argc) {
    fprintf (stderr, "%s: missing subcommand.  Use `--help' for help.\n",
	     argv[0]);
    exit (1);
  }

  /* Got a command, so connect to the hypervisor. */
  conn = virConnectOpenReadOnly (uri);
  if (conn == NULL) {
    fprintf (stderr, "%s: could not connect to the hypervisor.\n", argv[0]);
    if (uri == NULL && geteuid () != 0)
      fprintf (stderr, "Tip: To connect to Xen, you should run this program as root\n");
    exit (1);
  }

  cmd = argv[optind];

  if (STREQ (cmd, "install"))
    cmd_install (argc-optind, argv+optind);
  else if (STREQ (cmd, "list"))
    cmd_list (argc-optind, argv+optind);
  else {
    fprintf (stderr,
	     "%s: unknown subcommand.  Use `nagios-virt --help'\n",
	     cmd);
    exit (1);
  }

  /* Shutdown libxml */
  xmlCleanupParser();

  exit (0);
}

static void
usage (char *prog)
{
  prog = basename (prog);

  fprintf (stderr, "%s: configure Nagios for virtualization\n\
\n\
Usage:\n\
  %s [OPTIONS] install [INSTALL-OPTIONS]\n\
    Autodetect guests and install or update Nagios configuration.\n\
\n\
  %s [OPTIONS] list\n\
    Lists guests, and information about them.  Does not change\n\
    the Nagios configuration.\n\
\n\
  %s config\n\
  %s --config\n\
    Display nagios-virt configuration.  Does not change the Nagios\n\
    configuration.\n\
\n\
  %s version\n\
  %s --version\n\
    Display package name and version, then exit.\n\
\n\
  %s help\n\
  %s --help\n\
    Displays this help text.\n\
\n",
	   prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

static int write_host (void *data, int i, int is_active, virDomainPtr dom, const char *xml, xmlDocPtr doc, const char *name);
static int write_hostgroup_member (void *data, int i, int is_active, virDomainPtr dom, const char *xml, xmlDocPtr doc, const char *name);

static struct option install_options[] = {
  { "quiet", 0, NULL, 'q' },
  { NULL, 0, NULL, 0 }
};
#define INSTALL_OPTIONS_SHORT "q"

static void
cmd_install (int argc, char **argv)
{
  int quiet = 0;
  int fd;
  int r;
  FILE *fp;
  char tmpdir[] = "/tmp/nagiosvirtXXXXXX";
  char tmpfile[256];
  char cmd[256];
  char *rs;

  /* Parse install options. */
  optind = 1;

  for (;;) {
    int c, index = 0;

    c = getopt_long (argc, argv,
		     INSTALL_OPTIONS_SHORT, install_options, &index);
    if (c == -1) break;

    switch (c) {
    case 'q':
      quiet = 1;
      break;
    case '?':
      usage (argv[0]);
      exit (0);
    default:
      fprintf (stderr,
	       "%s: ignored unknown return value from getopt_long: %d\n",
	       argv[0], c);
    }
  }

  if (!quiet)
    fprintf (stderr, "Installing %s in %s ...\n",
	     NV_LIBRARY, NAGIOS_CONFIGDIR);

  /* Try to find the Nagios-virt configuration file and reinstall it. */
  fd = open (NV_DATADIR "/" NV_LIBRARY, O_RDONLY);
  if (fd == -1) {
    fd = open (NV_LIBRARY, O_RDONLY);
    if (fd == -1) {
      perror (NV_DATADIR "/" NV_LIBRARY);
      exit (1);
    }
  }

  if (copy_fd_to_file (fd, NAGIOS_CONFIGDIR "/" NV_LIBRARY) == -1) {
    perror (NAGIOS_CONFIGDIR "/" NV_LIBRARY ": installation failed");
    exit (1);
  }
  close (fd);

  /* Check that /etc/nagios/commands.cfg exists.  If not copy it from
   * /etc/nagios/commands.cfg-sample.  If that doesn't exist, give an
   * error but continue.
   */
  if (access (NAGIOS_CONFIGDIR "/commands.cfg", R_OK) == -1) {
    if (!quiet)
      fprintf (stderr, "Installing %s/commands.cfg ...\n", NAGIOS_CONFIGDIR);

    /* Try to copy. */
    fd = open (NAGIOS_CONFIGDIR "/commands.cfg-sample", O_RDONLY);
    if (fd >= 0) {
      if (copy_fd_to_file (fd, NAGIOS_CONFIGDIR "/commands.cfg") == -1) {
	perror (NAGIOS_CONFIGDIR "/commands.cfg: installation failed");
	exit (1);
      }
      close (fd);
    }
    /* Otherwise warn. */
    else
      fprintf (stderr,
	       "Warning: could not install %s/commands.cfg  "
	       "This file from the standard Nagios distribution is "
	       "required in order for nagios-virt to work correctly.\n",
	       NAGIOS_CONFIGDIR);
  }

  if (!quiet)
    fprintf (stderr, "Installing %s in %s ...\n", NV_HOSTS, NAGIOS_CONFIGDIR);

  /* Now create new /etc/nagios/virt-hosts.cfg */
  fp = fopen (NAGIOS_CONFIGDIR "/" NV_HOSTS ".nvnew", "w");
  if (fp == NULL) {
    perror (NAGIOS_CONFIGDIR "/" NV_HOSTS ".nvnew");
    exit (1);
  }

  fprintf (fp, "\
# List of virtual hosts\n\
#\n\
# You can edit this file but if you rerun `nagios-virt install' then this\n\
# file will be recreated, with the old file saved as %s.nvold\n\
\n",
	   NV_HOSTS);

  for_each_domain (write_host, fp);

  fprintf (fp, "define hostgroup {\n");
  fprintf (fp, "  hostgroup_name virt-hostgroup\n");
  fprintf (fp, "  alias          Virtual hosts\n");
  fprintf (fp, "  members        ");

  for_each_domain (write_hostgroup_member, fp);

  fprintf (fp, "\n");
  fprintf (fp, "}\n");
  fprintf (fp, "\n");

  fprintf (fp, "# End of file\n");

  if (ferror (fp)) {
    perror (NAGIOS_CONFIGDIR "/" NV_HOSTS ".nvnew");
    unlink (NAGIOS_CONFIGDIR "/" NV_HOSTS ".nvnew");
    exit (1);
  }
  fclose (fp);

  if (access (NAGIOS_CONFIGDIR "/" NV_HOSTS, R_OK) == 0) {
    if (!quiet)
      fprintf (stderr, "  (Renaming old %s to %s.nvold)\n",
	       NV_HOSTS, NV_HOSTS);

    if (rename (NAGIOS_CONFIGDIR "/" NV_HOSTS,
		NAGIOS_CONFIGDIR "/" NV_HOSTS ".nvold") == -1) {
      perror ("rename");
      exit (1);
    }
  }

  if (rename (NAGIOS_CONFIGDIR "/" NV_HOSTS ".nvnew",
	      NAGIOS_CONFIGDIR "/" NV_HOSTS) == -1) {
    perror ("rename");
    exit (1);
  }

  /* Now create a dummy file for testing the configuration. */
  rs = mkdtemp (tmpdir);
  if (rs == NULL) {
    perror ("mkdtemp");
    exit (1);
  }

  chmod (tmpdir, 0755);		/* Otherwise nagios can't read it. */

  snprintf (tmpfile, sizeof tmpfile, "%s/nagios.cfg", tmpdir);
  fp = fopen (tmpfile, "w");
  if (fp == NULL) {
    perror (tmpfile);
    exit (1);
  }

  fprintf (fp, "cfg_file=%s/%s\n", NAGIOS_CONFIGDIR, NV_LIBRARY);
  fprintf (fp, "cfg_file=%s/%s\n", NAGIOS_CONFIGDIR, NV_HOSTS);

  fclose (fp);

  /* Test the configuration (nagios -v /tmp/...) */
  if (!quiet)
    fprintf (stderr, "Testing the nagios configuration ...\n");

  snprintf (cmd, sizeof cmd, "%s -v %s%s",
	    NAGIOS, tmpfile,
	    quiet ? " >/dev/null 2>&1" : "");
  if (!quiet)
    fprintf (stderr, "%s\n", cmd);

  r = system (cmd);
  unlink (tmpfile);
  rmdir (tmpdir);

  if (r == -1) {
    perror (cmd);
    exit (1);
  }
  if (!WIFEXITED (r)) {
    fprintf (stderr, "%s: command failed -- see messages above\n", cmd);
    exit (1);
  }
  if (WEXITSTATUS (r) != 0) {
    fprintf (stderr,
	     "\n\n***** Error: *****\n\n"
	     "Nagios could not validate the configuration files.\n"
	     "This is likely to be a bug in nagios-virt or may be because\n"
	     "this version is incompatible with a later or earlier version\n"
	     "of Nagios.  Please refer to the nagios-virt site for help\n"
	     "and to report bugs.  You will also need the full messages\n"
	     "printed above.\n\n");
    exit (1);
  }

  if (!quiet) {
    fprintf (stderr, "\n----------------------------------------\n\n");
  }

  printf ("Make sure that 'nagios' user can use sudo by adding the\n");
  printf ("following line to /etc/sudoers:\n");
  printf ("\n");
  printf ("\tnagios ALL = NOPASSWD: /usr/bin/virsh\n");
  printf ("\n");
  printf ("Make sure that 'Defaults requiretty' is commented out in\n");
  printf ("/etc/sudoers.\n");
  printf ("\n");
  printf ("Add the following lines to %s/nagios.cfg:\n", NAGIOS_CONFIGDIR);
  printf ("\n");
  printf ("\tcfg_file=%s/%s\n", NAGIOS_CONFIGDIR, NV_LIBRARY);
  printf ("\tcfg_file=%s/%s\n", NAGIOS_CONFIGDIR, NV_HOSTS);
  printf ("\n");
  printf ("Then verify Nagios configuration by doing:\n");
  printf ("\n");
  printf ("\t%s -v %s/nagios.cfg\n", NAGIOS, NAGIOS_CONFIGDIR);
  printf ("\n");
  printf ("If verification is successful, restart Nagios.\n");
  printf ("\n");
  printf ("You may want to edit %s/%s to monitor extra services\non your virtual machines.\n",
	  NAGIOS_CONFIGDIR, NV_HOSTS);
  exit (0);
}

static int write_host (void *data,
		       int i, int is_active, virDomainPtr dom,
		       const char *xml, xmlDocPtr doc, const char *name)
{
  FILE *fp = (FILE *) data;

  if (i == 0) return 0;		/* Ignore dom0. */

  fprintf (fp, "\
# ---- %s\n\
\n\
# This uses ping to check the guest\n\
define host {\n\
  use virt-host\n\
  host_name %s\n\
  alias     Virtual host %s\n\
  address   1.0.0.0            ; XXX Set the correct address here.\n\
}\n\
\n\
# This uses hypervisor calls to check the guest\n\
define service {\n\
  use virt-service-up\n\
  host_name %s\n\
}\n\
\n\
# Uncomment this to test SSH on the guest\n\
#define service {\n\
#  use virt-service-ssh\n\
#  host_name %s\n\
#}\n\
\n\
# Uncomment this to test HTTP on the guest\n\
#define service {\n\
#  use virt-service-http\n\
#  host_name %s\n\
#}\n\
\n",
	   name, name, name, name, name, name);

  return 0;
}

static int write_hostgroup_member (void *data,
				   int i, int is_active, virDomainPtr dom,
				   const char *xml, xmlDocPtr doc,
				   const char *name)
{
  FILE *fp = (FILE *) data;

  if (i == 0) return 0;		/* Ignore dom0. */

  fprintf (fp, "%s,", name);

  return 0;
}

static int print_name (void *data, int i, int is_active, virDomainPtr dom, const char *xml, xmlDocPtr doc, const char *name);

static void
cmd_list (int argc, char **argv)
{
  for_each_domain (print_name, NULL);
}

static int
print_name (void *data,
	    int i, int is_active, virDomainPtr dom, const char *xml,
	    xmlDocPtr doc, const char *name)
{
  printf ("%s", name);
  if (!is_active) printf (" (inactive)");
  printf ("\n");
  return 0;
}

static void
cmd_config (int argc, char **argv)
{
  printf ("PACKAGE_NAME " PACKAGE_NAME "\n"
	  "PACKAGE_VERSION " PACKAGE_VERSION "\n"
	  "NAGIOS_CONFIGDIR " NAGIOS_CONFIGDIR "\n"
	  "NV_DATADIR " NV_DATADIR "\n");
}

static int
for_each_domain (int (*callback) (void *data, int i, int is_active,
				  virDomainPtr, const char *xml, xmlDocPtr,
				  const char *name),
		 void *data)
{
  int nr_ids, *ids, i, nr_names;
  virDomainPtr *doms;
  char **names;
  char **xmls;
  xmlDocPtr *docs;
  xmlXPathContextPtr xpath_ctx;
  char *name;

  /* Active domains. */

  nr_ids = virConnectNumOfDomains (conn);
  if (nr_ids == -1) {
    fprintf (stderr, "virConnectNumOfDomains: failed\n");
    exit (1);
  }

  ids = malloc (nr_ids * sizeof (int));
  if (ids == NULL) { perror ("malloc: ids"); exit (1); }

  if (virConnectListDomains (conn, ids, nr_ids) == -1) {
    fprintf (stderr, "virConnectListDomains: failed\n");
    exit (1);
  }

  doms = malloc (nr_ids * sizeof (virDomainPtr));
  if (doms == NULL) { perror ("malloc: doms"); exit (1); }

  for (i = 0; i < nr_ids; ++i) {
    doms[i] = virDomainLookupByID (conn, ids[i]);
    if (doms[i] == NULL) {
      fprintf (stderr, "virDomainLookupByID: failed on domain ID: %d\n",
	       ids[i]);
      exit (1);
    }
  }

  /* Inactive domains. */

  nr_names = virConnectNumOfDefinedDomains (conn);
  if (nr_names == -1) {
    fprintf (stderr, "virConnectNumOfDefinedDomains: failed\n");
    exit (1);
  }

  names = malloc (nr_names * sizeof (const char *));
  if (names == NULL) { perror ("malloc: names"); exit (1); }

  nr_names = virConnectListDefinedDomains (conn, names, nr_names);
  if (nr_names == -1) {
    fprintf (stderr, "virConnectListDefinedDomains: failed\n");
    exit (1);
  }

  doms = realloc (doms, (nr_ids + nr_names) * sizeof (virDomainPtr));
  if (doms == NULL) { perror ("realloc: doms"); exit (1); }

  for (i = 0; i < nr_names; ++i) {
    doms[nr_ids+i] = virDomainLookupByName (conn, names[i]);
    if (doms[nr_ids+i] == NULL) {
      fprintf (stderr, "virDomainLookupByName: failed on domain: %s\n",
	       names[i]);
      exit (1);
    }
  }

  /* Get the XML for each domain. */

  xmls = malloc ((nr_ids + nr_names) * sizeof (char *));
  if (xmls == NULL) { perror ("malloc: xmls"); exit (1); }

  for (i = 0; i < nr_ids + nr_names; ++i) {
    xmls[i] = virDomainGetXMLDesc (doms[i], 0);
    if (xmls[i] == NULL) {
      if (i < nr_ids)
	fprintf (stderr, "virDomainGetXMLDesc: failed on domain ID: %d\n",
		 ids[i]);
      else
	fprintf (stderr, "virDomainGetXMLDesc: failed on domain: %s\n",
		 names[i-nr_ids]);
      exit (1);
    }
  }

  /* Parse the XML. */

  docs = malloc ((nr_ids + nr_names) * sizeof (char *));
  if (docs == NULL) { perror ("malloc: docs"); exit (1); }

  for (i = 0; i < nr_ids + nr_names; ++i) {
    docs[i] = xmlReadDoc ((xmlChar *) xmls[i], NULL, NULL, XML_PARSE_NONET);
    if (docs[i] == NULL) {
      fprintf (stderr, "xmlReadDoc: failed on index %d\n", i);
      exit (1);
    }
  }

  /* Callback. */

  for (i = 0; i < nr_ids + nr_names; ++i) {
    /* Extract the name which is about all that we need normally. */
    xpath_ctx = xmlXPathNewContext (docs[i]);
    name = xpath_string ("string(/domain/name[1])", xpath_ctx);

    if (callback (data, i, i < nr_ids, doms[i], xmls[i], docs[i], name) == -1)
      return -1;

    xmlXPathFreeContext (xpath_ctx);
    free (name);
  }

  return 0;
}

static char *
xpath_string (const char *xpath_expr, xmlXPathContextPtr ctx)
{
  xmlXPathObjectPtr obj;
  char *ret;

  obj = xmlXPathEval ((xmlChar *) xpath_expr, ctx);
  if (obj == NULL) return NULL;
  if (obj->type != XPATH_STRING || obj->stringval == NULL ||
      obj->stringval[0] == '\0') {
    xmlXPathFreeObject (obj);
    return NULL;
  }

  ret = strdup ((char *) obj->stringval);
  if (!ret) { perror ("strdup"); exit (1); }
  xmlXPathFreeObject (obj);

  return ret;
}

/* Copy a file descriptor to a file.  If it fails, return -1. */
static int
copy_fd_to_file (int fd, const char *pathname)
{
  int fd2, r;

  fd2 = open (pathname, O_WRONLY|O_CREAT|O_TRUNC, 0644);
  if (fd2 == -1) {
    perror (pathname);
    return -1;
  }

  r = copy_fd_to_fd (fd, fd2);
  if (close (fd2) == -1) r = -1;
  return r;
}

/* Copy a file descriptor to a file descriptor.  If it fails, return -1. */
static int
copy_fd_to_fd (int fd, int fd2)
{
  char buffer[16384], *p;
  int r, n;

 again:
  while ((n = read (fd, buffer, sizeof buffer)) > 0) {
    p = buffer;
    do {
    again2:
      r = write (fd2, p, n);
      if (r == -1) {
	if (errno == EINTR || errno == EAGAIN) goto again2;
	return r;
      }
      p += r;
      n -= r;
    } while (n > 0);
  }

  if (n == -1 && (errno == EINTR || errno == EAGAIN)) goto again;

  return n;
}
