// Simplified versions of what is used from the standard library

extern void *stdout;

extern void exit(int __status);
extern void *fopen(char *__filename, char *__modes) ;
extern int fread(void *__ptr, int __size, int __n, void* __stream);
extern int fwrite(void * __ptr, int __size, int __n, void *__s);
extern int fclose(void *__stream);
extern int close(int __fd);
extern int printf(char *__format, ...);
extern int fprintf(void *stream, char * __format, ...);
extern void *malloc(long __size);
extern void free(void *__ptr);
extern void *memset(void *__s, int __c, int __n);
extern int memcmp(void *__s1, void *__s2, int __n);
extern int strcmp(char *__s1, char *__s2);
extern int strlen(char *__s);
extern char *strcpy(char * __dest, char * __src);
extern char *strrchr(char *__s, int __c);
extern int sprintf(char * __s, char * __format, ...);
extern int asprintf(char **strp, char *fmt, ...);
extern char *strdup(char *__s);
extern void *memcpy(void * __dest, void * __src, int __n);
extern int mkstemps(char *__template, int __suffixlen);
extern void perror(char *__s);
extern int system(char *__command);
extern char *getenv(char *__name);
extern long double strtold(char* str, char** endptr);
extern int atoi(char *str);
