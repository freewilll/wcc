void assert_int(int expected, int actual, char *message);
void assert_long(long expected, long actual, char *message);
void assert_string(char *expected, char *actual, char *message);
void finalize();
void parse_args(int argc, char **argv, int *verbose);