#ifndef PE_UTIL_H
#define PE_UTIL_H

/* Read an entire file into a malloc'd NUL-terminated buffer. Returns NULL on failure. */
char *pe_read_file(const char *path);

#endif
