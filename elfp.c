/*
 * Walk a list of input files, printing them back out if they match a
 * particular elf class.  Think "ls | xargs file | grep ELF | cut -f1"
 * only with a lot less bullshit.
 *
 * By default no elf clases are requested, so nothing will be printed.
 *
 * This program is licensed under the GNU Public License version 2.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <libelf.h>
#include <gelf.h>

#define P_INVAL	    0
#define P_REL	    (1 << 0)
#define P_DSO	    (1 << 1)
#define	P_EXEC	    (1 << 2)
#define P_OTHER	    (1 << 3)

#define P_NEWLINE   (1 << 8)

static int
has_dt_debug(Elf *elf, GElf_Ehdr *ehdr)
{
    int i;

    for (i = 0; i < ehdr->e_phnum; i++) {
	GElf_Phdr phdr;
	GElf_Shdr shdr;
	Elf_Scn *scn;
	Elf_Data *data;
	unsigned int j;

	if (gelf_getphdr(elf, i, &phdr) == NULL)
	    continue;

	if (phdr.p_type != PT_DYNAMIC)
	    continue;

	scn = gelf_offscn(elf, phdr.p_offset);

	if (gelf_getshdr(scn, &shdr) == NULL)
	    continue;

	if (shdr.sh_type != SHT_DYNAMIC)
	    continue;

	if ((data = elf_getdata(scn, NULL)) == NULL)
	    continue;

	for (j = 0;
	     j < shdr.sh_size / gelf_fsize(elf, ELF_T_DYN, 1, EV_CURRENT);
	     j++) {
	    GElf_Dyn dyn;
	    if (gelf_getdyn(data, j, &dyn)) {
		if (dyn.d_tag == DT_DEBUG)
		    return 1;
	    }
	}
    }

    return 0;
}

static void
test_one(char *f, Elf *elf, int flags)
{
    GElf_Ehdr ehdr;

    if (elf_kind(elf) != ELF_K_ELF) {
	if (flags & P_OTHER)
	    goto out_print;
	return;
    }

    if (gelf_getehdr(elf, &ehdr) == NULL)
	return;

    if ((flags & P_REL) && (ehdr.e_type == ET_REL))
	goto out_print;

    if ((flags & P_EXEC) && (ehdr.e_type == ET_EXEC))
	goto out_print;

    /* arguably should print if P_OTHER, but, nah. */
    if (ehdr.e_type != ET_DYN)
	return;

    if (has_dt_debug(elf, &ehdr)) {
	if (flags & P_EXEC) {
	    goto out_print; /* treat PIEs as executables */
	}
    } else if (flags & P_DSO) {
	goto out_print;
    }

    return;

out_print:
    if (flags & P_NEWLINE) {
	write(1, f, strlen(f));
	write(1, "\n", 1);
    } else {
	write(1, f, strlen(f) + 1);
    }
}

static void
handle_one(char *f, int flags)
{
    int fd;
    Elf *elf;

    if ((fd = open(f, O_RDONLY)) >= 0) {
	if ((elf = elf_begin(fd, ELF_C_READ_MMAP, NULL)) != NULL) {
	    test_one(f, elf, flags);
	    elf_end(elf);
	}
	close(fd);
    }
}

static void
__attribute__((__noreturn__))
usage(int status)
{
    FILE *out = status ? stderr : stdout;

    fprintf(out, "Usage: elfp [flags] file0 [file1 [.. fileN]]\n");
    fprintf(out, "Flags:\n");
    fprintf(out, "       -r    Match relocatables\n");
    fprintf(out, "       -d    Match shared libraries\n");
    fprintf(out, "       -e    Match executables\n");
    fprintf(out, "       -o    Match other ELF types (.a, etc.)\n");
    fprintf(out, "       -n    Terminate output with newlines\n");
    fprintf(out, "       -h    Print this help text and exit\n");

    exit(status);
}

int main(int argc, char **argv)
{
    int i, flags = 0, newline = 0;
    struct option options[] = {
	    {.name = "help",
	     .val = '?',
	    },
	    {.name = "usage",
	     .val = '?',
	    },
	    {.name = ""}
    };
    int longindex = -1;

    while ((i = getopt_long(argc, argv, "hrdeon", options, &longindex)) != -1) {
	switch (i) {
	case 'r':
	    flags |= P_REL;
	    break;
	case 'd':
	    flags |= P_DSO;
	    break;
	case 'e':
	    flags |= P_EXEC;
	    break;
	case 'o':
	    flags |= P_OTHER;
	    break;
	case 'n':
	    newline = 1;
	    break;
	case 'h':
	case '?':
	    usage(longindex == -1 ? 1 : 0);
	    break;
	}
    }

    if (!flags)
	return 0;

    if (newline)
	flags |= P_NEWLINE;

    elf_version(EV_CURRENT);

    if (optind == argc)
	usage(1);

    for (i = optind; i < argc; i++)
	handle_one(argv[i], flags);

    return 0;
}
