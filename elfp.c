/*
 * Walk a list of input files, printing them back out if they match a
 * particular elf class.  Think "ls | xargs file | grep ELF | cut -f1"
 * only with a lot less bullshit.
 *
 * Each file printed is null-terminated, not newline-terminated.  If you
 * you really want the other thing, "elfp ... | xargs -0 -n1 echo", but
 * I'm intending to feed this output to xargs -0 anyway.
 *
 * By default no elf clases are requested, so nothing will be printed.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

static int
has_dt_debug(Elf *elf)
{
    return 0; /* XXX */
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

    if (has_dt_debug(elf)) {
	if (flags & P_EXEC) {
	    goto out_print; /* treat PIEs as executables */
	}
    } else if (flags & P_DSO) {
	goto out_print;
    }

    return;

out_print:
    write(1, f, strlen(f) + 1);
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

int main(int argc, char **argv)
{
    int i, flags = 0;

    while ((i = getopt(argc, argv, "rdeo")) != -1) {
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
	}
    }

    if (!flags)
	return 0;

    elf_version(EV_CURRENT);

    for (i = optind; i < argc; i++)
	handle_one(argv[i], flags);

    return 0;
}
