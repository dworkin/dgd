# include "dgd.h"
# include "path.h"

char *path_ed_read(file)
char *file;
{
    return path_resolve(file);
}

char *path_ed_write(file)
char *file;
{
    return path_resolve(file);
}

char *path_object(from, file)
char *from, *file;
{
    return path_resolve(file);
}

char *path_inherit(from, file)
char *from, *file;
{
    return path_resolve(file);
}

char *path_include(from, file)
char *from, *file;
{
    return path_resolve(file);
}
