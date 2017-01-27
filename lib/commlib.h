int recv_variable_string(int cl_sock, char * buff);
int send_variable_string(int cl_sock, char * buff, int bytes_count);

void print_str_eos(char * mul_str, int length);
int split_eos(char * mul_str, int length, char ** string_pointers, int max_pointers);
int pack_eos(char * mul_str, char ** string_pointers, int n_str);

enum response_code {R_ERROR, R_HIT, R_NOTHIT};
int udp_send_coords(int sock_client_udp, struct sockaddr_in udp_srv_addr, char coord1, char coord2);
int udp_receive_coords(int sock_client_udp, struct sockaddr_in udp_srv_addr, char * coord1, char * coord2);
int udp_send_response_status(int sock_client_udp, struct sockaddr_in udp_srv_addr, enum response_code resp);
int udp_receive_response_status(int sock_client_udp, struct sockaddr_in udp_srv_addr, enum response_code * resp);

int check_port_str(char * str);