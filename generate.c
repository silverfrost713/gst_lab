#include <stdio.h>
#include "aes_matrix.h"

int main(int argc, char* argv[]) {
	/* Если генератор запущен с неверными параметрами, выйти с ошибкой. */
	if ((argc != 2) && (argc != 3)) {
		fprintf(stdout, "Usage:\ngenerate.exe [DATA SIZE, MB]\ngenerate.exe [DATA SIZE, MB] [AES MATRIX SIDE LENGTH (default=4)]\nExamples:\ngenerate.exe 30\ngenerate.exe 30 8\n");
		exit(EXIT_FAILURE);
	}
	size_t gen_size, gen_side_buf;
	aesbyte_t gen_matrix_side = 4;
	gen_size = atoi(argv[1]);
	if (argc == 3) {
		gen_side_buf = (size_t)atoi(argv[2]);
		if (gen_side_buf >= 256) {
			fprintf(stderr, "WARNING in main(): matrix side length exceeds 255, locking to 255.\n");
			gen_matrix_side = 255;
		}
		else
			gen_matrix_side = (aesbyte_t)gen_side_buf;
	}
	/* Если криво указан размер данных или матриц, выйти с ошибкой. */
	if (!gen_size) {
		fprintf(stderr, "ERROR in main(): passed invalid data size \"%s\".\n", argv[1]);
		fprintf(stdout, "Usage:\ngenerate.exe [DATA SIZE, MB]\ngenerate.exe [DATA SIZE, MB] [AES MATRIX SIDE LENGTH (default=4)]\nExamples:\ngenerate.exe 30\ngenerate.exe 30 8\n");
		exit(EXIT_FAILURE);
	}
	if (!gen_matrix_side) {
		fprintf(stderr, "ERROR in main(): passed invalid matrix side length \"%s\".\n", argv[2]);
		fprintf(stdout, "Usage:\ngenerate.exe [DATA SIZE, MB]\ngenerate.exe [DATA SIZE, MB] [AES MATRIX SIDE LENGTH (default=4)]\nExamples:\ngenerate.exe 30\ngenerate.exe 30 8\n");
		exit(EXIT_FAILURE);
	}
	size_t gen_len, gen_matrix_size;
	struct AESMatrix* gen_data = generate_matrices(gen_size, gen_matrix_side, &gen_len, &gen_matrix_size);
	char* tcp_data = aes_pack_matrices(gen_data, gen_len, gen_matrix_size);
	for(size_t i = 0; i < gen_len; i += 1)
		aes_matrix_free(&(gen_data[i]));
	free(gen_data);
	if (!tcp_data) {
		fprintf(stderr, "ERROR in main(): general failure flattening matrices.\n");
		exit(EXIT_FAILURE);
	}
	SOCKET cl_socket;
	if (tcp_establish_conn_server(&cl_socket)) {
		fprintf(stderr, "ERROR in main(): general failure establishing TCP connection.\n");
		exit(EXIT_FAILURE);
	}
	if (tcp_send_matrices(tcp_data, gen_len * gen_matrix_size, cl_socket)) {
		fprintf(stderr, "ERROR in main(): general failure sending matrices over TCP.\n");
		exit(EXIT_FAILURE);
	}
	size_t recv_size;
	char* aes_matrices_flat = tcp_receive_matrices(&recv_size, cl_socket);
	if (!aes_matrices_flat) {
		fprintf(stderr, "ERROR in main(): general failure receiving matrices by TCP.\n");
		exit(EXIT_FAILURE);
	}
	int aes_time = tcp_recv_time(cl_socket);
	if (aes_time < 0) {
		fprintf(stderr, "ERROR in main(): general failure receiving time info by TCP.\n");
		exit(EXIT_FAILURE);
	}
	struct AESMatrix* aes_matrices = aes_unflatten_matrices(aes_matrices_flat, gen_len * gen_matrix_size, &gen_len);
	if (!aes_matrices) {
		fprintf(stderr, "ERROR in main(): general failure unflattening matrices.\n");
		exit(EXIT_FAILURE);
	}
	if (tcp_close_conn_server(cl_socket)) {
		fprintf(stderr, "ERROR in main(): general failure closing connection by server.\n");
		exit(EXIT_FAILURE);
	}
}