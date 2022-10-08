#include <stdio.h>
#include "aes_matrix.h"

#define AES_SERIAL  0
#define AES_OPENMP  1
#define AES_MPI     2
#define AES_MPICUDA 3

int main(int argc, char* argv[]) {
	if ((argc != 1) & (argc != 2)) {
		fprintf(stdout, "Usage:\nlogic.exe [MODE (default=serial]\nMODE <= {serial, openmp, mpi, mpicuda}\nExamples:\nlogic.exe\nlogic.exe openmp\n");
		exit(EXIT_FAILURE);
	}
	size_t recv_datasize, matrix_qty;
	SOCKET connect_socket;
	if (tcp_establish_conn_client(&connect_socket)) {
		fprintf(stderr, "ERROR in main(): general failure establishing connection with server.\n");
		exit(EXIT_FAILURE);
	}
	char* aes_matrices_flat = tcp_receive_matrices(&recv_datasize, connect_socket);
	if (!aes_matrices_flat) {
		fprintf(stderr, "ERROR in main(): general failure receiving matrices by TCP.\n");
		exit(EXIT_FAILURE);
	}
	struct AESMatrix* aes_matrices = aes_unflatten_matrices(aes_matrices_flat, recv_datasize, &matrix_qty);
	free(aes_matrices_flat);
	if (!aes_matrices) {
		fprintf(stderr, "ERROR in main(): general failure unflattening matrices.\n");
		exit(EXIT_FAILURE);
	}
	int aes_time;
	if (argc == 1) {
		fprintf(stdout, "INFO: using serial.\n");
		aes_time = aes_shiftrows_serial(aes_matrices, matrix_qty);
	}
	else if ((argc == 2) & (!strcmp(argv[1], "serial"))) {
		fprintf(stdout, "INFO: using serial.\n");
		aes_time = aes_shiftrows_serial(aes_matrices, matrix_qty);
	}
	else if ((argc == 2) & (!strcmp(argv[1], "openmp"))) {
		fprintf(stdout, "INFO: using OpenMP.\n");
		aes_time = aes_shiftrows_openmp(aes_matrices, matrix_qty);
	}
	if (aes_time < 0) {
		fprintf(stderr, "ERROR in main(): general failure shifting AES matrix array.\n");
		exit(EXIT_FAILURE);
	}
	char* tcp_data = aes_pack_matrices(aes_matrices, matrix_qty, aes_matrices[0].side_len * aes_matrices[0].side_len + 1);
	if (!tcp_data) {
		fprintf(stderr, "ERROR in main(): general failure flattening matrices.\n");
		exit(EXIT_FAILURE);
	}
	if (tcp_send_matrices(tcp_data, recv_datasize, connect_socket)) {
		fprintf(stderr, "ERROR in main(): general failure sending matrices over TCP.\n");
		exit(EXIT_FAILURE);
	}
	if (tcp_send_time(aes_time, connect_socket)) {
		fprintf(stderr, "ERROR in main(): general failure sending time info over TCP.\n");
		exit(EXIT_FAILURE);
	}
	fprintf(stdout, "INFO: all done!\n");
	exit(EXIT_SUCCESS);
}