/*
 * Hacked for OS/2 by harald.kipp@egnite.de.
 *
 * $Log: os2user.c,v $
 * Revision 1.1  2000/04/25 19:52:10  harald
 * First OS/2 release
 *
 */
int getgid(void)
{
    return 1;
}

int setgid(int id)
{
    return 0;
}

int getuid(void)
{
    return 1;
}

int setuid(int id)
{
    return 0;
}

int chroot(char *dir)
{
    return 0;
}
