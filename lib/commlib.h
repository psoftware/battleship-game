int recv_variable_string(int cl_sock, char * buff);
int send_variable_string(int cl_sock, char * buff, int bytes_count);

void print_str_eos(char * mul_str, int length);
int split_eos(char * mul_str, int length, char ** string_pointers, int max_pointers);
int pack_eos(char * mul_str, char ** string_pointers, int n_str);