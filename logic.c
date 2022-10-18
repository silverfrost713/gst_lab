#include <stdio.h>
#include "aes_matrix.h"
#include <mpi.h>

enum AES_MODE {AES_SERIAL, AES_OPENMP, AES_MPI, AES_MPICUDA};

int main(int argc, char* argv[]) {
	int comm_rank, comm_size;
	enum AES_MODE mode = AES_SERIAL;
	MPI_Init(&argc, &argv);
	MPI_Comm_rank(MPI_COMM_WORLD, &comm_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	if (comm_rank == 0) {
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
		struct AESMatrix* aes_matrices;
		int aes_time;
		aes_matrices = aes_unflatten_matrices(aes_matrices_flat, recv_datasize, &matrix_qty);
		if (!aes_matrices) {
			fprintf(stderr, "ERROR in main(): general failure unflattening matrices.\n");
			exit(EXIT_FAILURE);
		}
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
			mode = AES_OPENMP;
		}
		else if ((argc == 2) & (!strcmp(argv[1], "mpi"))) {
			fprintf(stdout, "INFO: using MPI.\n");
			aes_time = aes_shiftrows_mpi(aes_matrices_flat, recv_datasize, &matrix_qty);
			mode = AES_MPI;
		}
		if (aes_time < 0) {
			fprintf(stderr, "ERROR in main(): general failure shifting AES matrix array.\n");
			exit(EXIT_FAILURE);
		}
		char* tcp_data;
		if (mode != AES_MPI)
			tcp_data = aes_pack_matrices(aes_matrices, matrix_qty, aes_matrices[0].side_len * aes_matrices[0].side_len + 1);
		else
			tcp_data = aes_matrices_flat;
		if (!tcp_data) {
			fprintf(stderr, "ERROR in main(): general failure flattening matrices.\n");
			exit(EXIT_FAILURE);
		}
		if (tcp_send_matrices(tcp_data, recv_datasize, connect_socket)) {
			fprintf(stderr, "ERROR in main(): general failure sending matrices over TCP.\n");
			exit(EXIT_FAILURE);
		}
		free(tcp_data);
		if (tcp_send_time(aes_time, connect_socket)) {
			fprintf(stderr, "ERROR in main(): general failure sending time info over TCP.\n");
			exit(EXIT_FAILURE);
		}
		fprintf(stdout, "INFO: all done!\n");
	}
	else {
		size_t aes_meta[2];
		MPI_Status mpi_stat;
		fprintf(stdout, "INFO: ID%d - creating a contiguous MPI data type for metadata.\n", comm_rank);
		MPI_Datatype MPI_AES_META;
		MPI_Type_contiguous(2, MPI_UNSIGNED_LONG_LONG, &MPI_AES_META);
		MPI_Type_commit(&MPI_AES_META);
		fprintf(stdout, "INFO: ID%d - receiving matrix size and quantity metadata from ID0.\n", comm_rank);
		MPI_Recv(&aes_meta, 1, MPI_AES_META, 0, 0, MPI_COMM_WORLD, &mpi_stat);
		MPI_Barrier(MPI_COMM_WORLD);
		fprintf(stdout, "INFO: ID%d - received matrix size = %llu, matrix quantity = %llu from ID0.\n", comm_rank, aes_meta[0], aes_meta[1]);
		fprintf(stdout, "INFO: ID%d - creating a contiguous MPI data type for AES matrix.\n", comm_rank);
		MPI_Datatype MPI_AES_MATRIX;
		MPI_Type_contiguous(aes_meta[0], MPI_CHAR, &MPI_AES_MATRIX);
		MPI_Type_commit(&MPI_AES_MATRIX);
		fprintf(stdout, "INFO: ID%d - allocating %llu bytes for %llu matrices.\n", comm_rank, aes_meta[0]*aes_meta[1], aes_meta[1]);
		char* portion_buf = (char*)malloc(sizeof(char) * aes_meta[0] * aes_meta[1]);
		fprintf(stdout, "INFO: ID%d - receiving matrix array portion from ID0.\n", comm_rank);
		MPI_Recv(portion_buf, aes_meta[1], MPI_AES_MATRIX, 0, 0, MPI_COMM_WORLD, &mpi_stat);
		fprintf(stdout, "INFO: ID%d - received matrix array portion from ID0, %llu matrices.\n", comm_rank, aes_meta[1]);
		fprintf(stdout, "INFO: ID%d - performing AES shift on my portion.\n", comm_rank);
		aesbyte_t* temp_row = (aesbyte_t*)malloc(sizeof(aesbyte_t) * portion_buf[0]);
		for (size_t k = 0; k < aes_meta[1]; k += 1) {
			for (aesbyte_t i = 0; i < (aesbyte_t)portion_buf[0]; i += 1) {
				for (aesbyte_t j = 0; j < (aesbyte_t)portion_buf[0]; j += 1)
					temp_row[j] = portion_buf[(k * aes_meta[0] + 1) + i * portion_buf[0] + j];
				for (aesbyte_t j = 0; j < (aesbyte_t)portion_buf[0]; j += 1)
					portion_buf[(k * aes_meta[0] + 1) + i * portion_buf[0] + j] = temp_row[(i + j) % portion_buf[0]];
			}
		}
		fprintf(stdout, "INFO: ID%d - sending processed matrix array portion back to ID0.\n", comm_rank);
		MPI_Send(portion_buf, aes_meta[1], MPI_AES_MATRIX, 0, 0, MPI_COMM_WORLD);
		MPI_Barrier(MPI_COMM_WORLD);
		free(temp_row);
		free(portion_buf);
	}
	MPI_Finalize();
	exit(EXIT_SUCCESS);
}