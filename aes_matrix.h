#ifndef AES_MATRIX_H
#define AES_MATRIX_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <omp.h>
#include <mpi.h>

#define DEF_BUFLEN 32
#define AES_PORT "21522"
#define MEGABYTE_SIZE 1024*1024

typedef uint8_t aesbyte_t;

struct AESMatrix {
	/* Матрица представлена массивом байт. */
	aesbyte_t* grid;
	aesbyte_t side_len;
};

/* Функция инициализации матрицы. */
void aes_matrix_init(struct AESMatrix* m, aesbyte_t side_len) {
	/* Проверить длину стороны на корректность. */
	if (!side_len) {
		fprintf(stderr, "ERROR in aes_matrix_init(): wrong side length specified \"%d\".\n", side_len);
		return;
	}
	/* Задать длину стороны матрицы, выделить память и проверить указатель. */
	m->side_len = side_len;
	m->grid = (aesbyte_t*)malloc(sizeof(aesbyte_t) * side_len * side_len);
	if (!m->grid) {
		fprintf(stderr, "ERROR in aes_matrix_init(): couldn't allocate memory for matrix grid pointer.\n");
		return;
	}
}

/* Функция удаления матрицы. */
void aes_matrix_free(struct AESMatrix* m) {
	/* Проверить указатели. */
	if (!m) {
		fprintf(stderr, "ERROR in aes_matrix_free(): got uninitialised matrix.\n");
		return;
	}
	if (!m->grid)
		fprintf(stderr, "WARNING in aes_matrix_free(): got null pointer.\n");
	/* Освободить и указатель внутри матрицы, и указатель на саму матрицу. */
	else
		free(m->grid);
}

/* Функция вывода матрицы в стандартный вывод. */
void aes_matrix_display(struct AESMatrix m) {
	/* Проверить указатель. */
	if (!m.grid) {
		fprintf(stderr, "ERROR in aes_matrix_display(): got uninitialised matrix.\n");
		return;
	}
	/* Проверить длину стороны матрицы. */
	if (!m.side_len) {
		fprintf(stderr, "ERROR in aes_matrix_display(): got matrix with null size.\n");
		return;
	}
	/* Вывести матрицу построчно в стандартный вывод. */
	for (aesbyte_t i = 0; i < m.side_len; i += 1) {
		for (aesbyte_t j = 0; j < m.side_len; j += 1)
			/* Не плодить лишних пробелов на случай, если вывод направлен в файл. */
			if (j != m.side_len - 1)
				fprintf(stdout, "%3d   ", m.grid[i * m.side_len + j]);
			else
				fprintf(stdout, "%3d", m.grid[i * m.side_len + j]);
		fprintf(stdout, "\n");
	}
}

/* Функция заполнения матрицы случайными байтами. */
void aes_matrix_randomise(struct AESMatrix m) {
	/* Проверить указатель. */
	if (!m.grid) {
		fprintf(stderr, "ERROR in aes_matrix_randomise(): got uninitialised matrix.\n");
		return;
	}
	/* Проверить длину стороны матрицы. */
	if (!m.side_len) {
		fprintf(stderr, "ERROR in aes_matrix_randomise(): got matrix with null size.\n");
		return;
	}
	/* Заполнить матрицу случайными байтами. */
	for (aesbyte_t i = 0; i < m.side_len * m.side_len; i += 1)
		m.grid[i] = (aesbyte_t)rand();
}

/* Функция операции ShiftRows из AES. */
void aes_matrix_shift_rows(struct AESMatrix m) {
	/* Проверить указатель. */
	if (!m.grid) {
		fprintf(stderr, "ERROR in aes_matrix_shift_rows(): got uninitialised matrix.\n");
		return;
	}
	/* Проверить длину стороны матрицы. */
	if (!m.side_len) {
		fprintf(stderr, "ERROR in aes_matrix_shift_rows(): got matrix with null size.\n");
		return;
	}
	/* Сдвигать матрицу влево построчно, на число элементов,
	   равное индексу строки, начиная с нуля. */
	aesbyte_t* temp_row = (aesbyte_t*)malloc(sizeof(aesbyte_t) * m.side_len);
	if (!temp_row) {
		fprintf(stderr, "ERROR in aes_matrix_shift_rows(): couldn't allocate memory for temp row.\n");
		return;
	}
	for (aesbyte_t i = 0; i < m.side_len; i += 1) {
		for (aesbyte_t j = 0; j < m.side_len; j += 1)
			temp_row[j] = m.grid[i * m.side_len + j];
		for (aesbyte_t j = 0; j < m.side_len; j += 1)
			m.grid[i * m.side_len + j] = temp_row[(i + j) % m.side_len];
	}
	free(temp_row);
}

int tcp_establish_conn_client(SOCKET* connect_socket) {
	/* Сервер пока на localhost. */
	const char* server_addr = "127.0.0.1";
	/* WSADATA содержит сведения о реализации сокетов в Windows. */
	WSADATA wsa_data;
	/* Структуры addrinfo содержат данные об адресах и протоколах. */
	struct addrinfo* result = NULL;
	struct addrinfo hints;
	int my_err;
	/* WSAStartup запрашивает версию WinSocket 2.2 и не даёт клиентам
	   использовать версию выше. */
	fprintf(stdout, "INFO: initialising WinSocket2 on client.\n");
	my_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (my_err) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): couldn't initialise WinSocket2, code %d.\n", my_err);
		WSACleanup();
		return -1;
	}
	/* Задать соединение по протоколу TCP через сокет. */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	/* Разрешить адрес сервера с портом. */
	fprintf(stdout, "INFO: resolving server address.\n");
	my_err = getaddrinfo(server_addr, AES_PORT, &hints, &result);
	if (my_err != 0) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): couldn't resolve server address, code %d.\n", my_err);
		WSACleanup();
		return -1;
	}
	*connect_socket = INVALID_SOCKET;
	/* Попытаться подключиться по адресу, полученному через getaddrinfo, создать сокет. */
	fprintf(stdout, "INFO: creating connect socket on client.\n");
	*connect_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (*connect_socket == INVALID_SOCKET) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): couldn't create connect socket, code %d.\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return -1;
	}
	/* Подключиться к серверу. */
	fprintf(stdout, "INFO: connecting to server.\n");
	my_err = connect(*connect_socket, result->ai_addr, (int)result->ai_addrlen);
	if (my_err == SOCKET_ERROR) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): couldn't connect to server, code %d.\n", WSAGetLastError());
		closesocket(*connect_socket);
		freeaddrinfo(result);
		WSACleanup();
		return -1;
	}
	freeaddrinfo(result);
	if (*connect_socket == INVALID_SOCKET) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): unable to connect to server, code %d.\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}
	return 0;
}

char* tcp_receive_matrices(size_t* recv_datasize, SOCKET connect_socket) {
	/* Буферы для приёма метаинформации о размере данных,
	   а также для самих матриц. */
	char tcp_datasize_recvbuf[DEF_BUFLEN];
	char* tcp_aes_data_recvbuf;
	size_t tcp_datasize;
	int recv_res;
	/* Принять метаинформацию о размере пересылаемых данных с матрицами и выделить память. */
	memset(tcp_datasize_recvbuf, 0, DEF_BUFLEN);
	recv_res = recv(connect_socket, tcp_datasize_recvbuf, DEF_BUFLEN, 0);
	if (recv_res == 0) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): connection to server closed prematurely.\n");
		closesocket(connect_socket);
		WSACleanup();
		return (char*)NULL;
	}
	else if (recv_res < 0) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): reception failure, code %d.\n", WSAGetLastError());
		closesocket(connect_socket);
		WSACleanup();
		return (char*)NULL;
	}
	else if (recv_res == DEF_BUFLEN) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): buffer overflow while receiving %d bytes.\n", recv_res);
		closesocket(connect_socket);
		WSACleanup();
		return (char*)NULL;
	}
	fprintf(stdout, "INFO: client received metainfo: %d bytes, buffer is \"%s\".\n", recv_res, tcp_datasize_recvbuf);
	tcp_datasize = atoi(tcp_datasize_recvbuf);
	if (!tcp_datasize) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): received invalid data size \"%s\".\n", tcp_datasize_recvbuf);
		closesocket(connect_socket);
		WSACleanup();
		return (char*)NULL;
	}
	/* Принять матрицы от сервера. */
	tcp_aes_data_recvbuf = (char*)malloc(tcp_datasize);
	if (!tcp_aes_data_recvbuf) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): couldn't allocate reception buffer memory.\n");
		closesocket(connect_socket);
		WSACleanup();
		return (char*)NULL;
	}
	recv_res = recv(connect_socket, tcp_aes_data_recvbuf, (int)tcp_datasize, 0);
	if (recv_res == 0) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): connection to server closed prematurely.\n");
		free(tcp_aes_data_recvbuf);
		closesocket(connect_socket);
		WSACleanup();
		return (char*)NULL;
	}
	else if (recv_res < 0) {
		fprintf(stderr, "ERROR in tcp_receive_matrices(): reception failure, code %d.\n", WSAGetLastError());
		free(tcp_aes_data_recvbuf);
		closesocket(connect_socket);
		WSACleanup();
		return (char*)NULL;
	}
	else if (recv_res != tcp_datasize) {
		free(tcp_aes_data_recvbuf);
		fprintf(stderr, "ERROR in tcp_receive_matrices(): client received %d bytes when %llu were specified.\n", recv_res, tcp_datasize);
		closesocket(connect_socket);
		WSACleanup();
		return (char*)NULL;
	}
	*recv_datasize = recv_res;
	fprintf(stdout, "INFO: client received matrices: %d bytes.\n", recv_res);
	fprintf(stdout, "INFO: buffer overview -\n");
	fprintf(stdout, "%d %d %d %d ... %d %d %d %d\n", tcp_aes_data_recvbuf[0], tcp_aes_data_recvbuf[1], tcp_aes_data_recvbuf[2], tcp_aes_data_recvbuf[3], tcp_aes_data_recvbuf[tcp_datasize - 4], tcp_aes_data_recvbuf[tcp_datasize - 3], tcp_aes_data_recvbuf[tcp_datasize - 2], tcp_aes_data_recvbuf[tcp_datasize - 1]);
	return tcp_aes_data_recvbuf;
}

struct AESMatrix* aes_unflatten_matrices(char* aes_matrices_flat, size_t aes_datasize, size_t* matrix_qty) {
	fprintf(stdout, "INFO: unflattening matrices.\n");
	/* Проверить указатель. */
	if (!aes_matrices_flat) {
		fprintf(stderr, "ERROR in aes_unflatten_matrices(): got null pointer.\n");
		return (struct AESMatrix*)NULL;
	}
	aesbyte_t aes_side_len = (aesbyte_t)aes_matrices_flat[0];
	size_t aes_matrix_size = (size_t)aes_side_len * (size_t)aes_side_len + 1;
	size_t aes_matrix_qty = aes_datasize / aes_matrix_size;
	fprintf(stdout, "INFO: matrix size is %llu bytes, matrix side length is %d.\n", aes_matrix_size, aes_side_len);
	*matrix_qty = aes_matrix_qty;
	/* Рассчитать число матриц, в которое распакуется буфер. */
	fprintf(stdout, "INFO: allocating memory for %llu matrices.\n", aes_matrix_qty);
	struct AESMatrix* aes_matrices = (struct AESMatrix*)malloc(sizeof(struct AESMatrix) * aes_matrix_qty);
	if (!aes_matrices) {
		fprintf(stderr, "ERROR in aes_unflatten_matrices(): couldn't allocate memory for unflattened matrices.\n");
		return (struct AESMatrix*)NULL;
	}
	fprintf(stdout, "INFO: writing matrix structs.\n");
	for (size_t i = 0; i < aes_matrix_qty; i += 1) {
		aes_matrix_init(&(aes_matrices[i]), aes_side_len);
		for (size_t j = 0; j <= aes_matrix_size; j += 1)
			aes_matrices[i].grid[j] = aes_matrices_flat[i * aes_matrix_size + j + 1];
	}
	fprintf(stdout, "INFO: displaying first matrix -\n");
	aes_matrix_display(aes_matrices[0]);
	fprintf(stdout, "INFO: displaying last matrix -\n");
	aes_matrix_display(aes_matrices[aes_matrix_qty - 1]);
	return aes_matrices;
}

int aes_shiftrows_mpi(char* aes_matrices_flat, size_t aes_datasize, size_t* matrix_qty) {
	fprintf(stdout, "INFO: unflattening matrices.\n");
	/* Проверить указатель. */
	if (!aes_matrices_flat) {
		fprintf(stderr, "ERROR in aes_unflatten_matrices(): got null pointer.\n");
		return -1;
	}
	aesbyte_t aes_side_len = (aesbyte_t)aes_matrices_flat[0];
	size_t aes_matrix_size = (size_t)aes_side_len * (size_t)aes_side_len + 1;
	size_t aes_matrix_qty = aes_datasize / aes_matrix_size;
	fprintf(stdout, "INFO: matrix size is %llu bytes, matrix side length is %d.\n", aes_matrix_size, aes_side_len);
	*matrix_qty = aes_matrix_qty;
	fprintf(stdout, "INFO: ID0 - creating a contiguous MPI data type for metadata.\n");
	clock_t start = clock();
	MPI_Datatype MPI_AES_META;
	MPI_Type_contiguous(2, MPI_UNSIGNED_LONG_LONG, &MPI_AES_META);
	MPI_Type_commit(&MPI_AES_META);
	size_t aes_meta[2];
	aes_meta[0] = aes_matrix_size;
	fprintf(stdout, "INFO: ID0 - async-broadcasting matrix size and quantity metadata.\n");
	MPI_Request req[7];
	int comm_size;
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);
	size_t portion = aes_matrix_qty / comm_size;
	size_t last_portion = aes_matrix_qty - portion * (comm_size - 1);
	for (int i = 1; i < comm_size; i += 1) {
		aes_meta[1] = (i == comm_size - 1) ? last_portion : portion;
		MPI_Isend(aes_meta, 1, MPI_AES_META, i, 0, MPI_COMM_WORLD, &(req[i - 1]));
	}
	MPI_Barrier(MPI_COMM_WORLD);
	fprintf(stdout, "INFO: ID0 - creating a contiguous MPI data type for AES matrix.\n");
	MPI_Datatype MPI_AES_MATRIX;
	MPI_Type_contiguous(aes_matrix_size, MPI_CHAR, &MPI_AES_MATRIX);
	MPI_Type_commit(&MPI_AES_MATRIX);
	fprintf(stdout, "INFO: ID0 - async-broadcasting matrix array portions.\n");
	for (int i = 1; i < comm_size; i += 1) {
		MPI_Isend(aes_matrices_flat + (portion * i * aes_matrix_size), (i == comm_size - 1) ? last_portion : portion, MPI_AES_MATRIX, i, 0, MPI_COMM_WORLD, &(req[i - 1]));
	}
	fprintf(stdout, "INFO: ID0 - performing AES shift on my portion.\n");
	aesbyte_t* temp_row = (aesbyte_t*)malloc(sizeof(aesbyte_t) * aes_matrices_flat[0]);
	for (size_t k = 0; k < matrix_qty; k += 1) {
		for (aesbyte_t i = 0; i < (aesbyte_t)aes_matrices_flat[0]; i += 1) {
			for (aesbyte_t j = 0; j < (aesbyte_t)aes_matrices_flat[0]; j += 1)
				temp_row[j] = aes_matrices_flat[(k * aes_matrix_size + 1) + i * aes_matrices_flat[0] + j];
			for (aesbyte_t j = 0; j < (aesbyte_t)aes_matrices_flat[0]; j += 1)
				aes_matrices_flat[(k * aes_matrix_size + 1) + i * aes_matrices_flat[0] + j] = temp_row[(i + j) % aes_matrices_flat[0]];
		}
	}
	for (int i = 1; i < comm_size; i += 1) {
		MPI_Irecv(aes_matrices_flat + (portion * i * aes_matrix_size), (i == comm_size - 1) ? last_portion : portion, MPI_AES_MATRIX, i, 0, MPI_COMM_WORLD, &(req[i - 1]));
	}
	MPI_Barrier(MPI_COMM_WORLD);
	clock_t end = (clock() - start) / (CLOCKS_PER_SEC / 1000);
	return (int)end;
}

int aes_shiftrows_serial(struct AESMatrix* m_arr, size_t matrix_qty) {
	fprintf(stdout, "INFO: performing serial AES shift rows on matrix array.\n");
	if (!m_arr) {
		fprintf(stderr, "ERROR in aes_shiftrows_serial(): got null pointer.\n");
		return -1;
	}
	clock_t start = clock();
	aesbyte_t* temp_row = (aesbyte_t*)malloc(sizeof(aesbyte_t) * m_arr[0].side_len);
	for (size_t k = 0; k < matrix_qty; k += 1) {
		for (aesbyte_t i = 0; i < m_arr[k].side_len; i += 1) {
			for (aesbyte_t j = 0; j < m_arr[k].side_len; j += 1)
				temp_row[j] = m_arr[k].grid[i * m_arr[k].side_len + j];
			for (aesbyte_t j = 0; j < m_arr[k].side_len; j += 1)
				m_arr[k].grid[i * m_arr[k].side_len + j] = temp_row[(i + j) % m_arr[k].side_len];
		}
	}
	free(temp_row);
	clock_t end = (clock() - start) / (CLOCKS_PER_SEC / 1000);
	fprintf(stdout, "INFO: AES shift rows on matrix array completed.\n");
	fprintf(stdout, "INFO: displaying first matrix -\n");
	aes_matrix_display(m_arr[0]);
	fprintf(stdout, "INFO: displaying last matrix -\n");
	aes_matrix_display(m_arr[matrix_qty - 1]);
	fprintf(stdout, "INFO: computations took %d ms.\n", end);
	return (int)end;
}

int aes_shiftrows_openmp(struct AESMatrix* m_arr, size_t matrix_qty) {
	fprintf(stdout, "INFO: performing AES shift rows on matrix array with OpenMP.\n");
	if (!m_arr) {
		fprintf(stderr, "ERROR in aes_shiftrows_serial(): got null pointer.\n");
		return -1;
	}
	clock_t start = clock();
	#pragma omp parallel
	{
		aesbyte_t* temp_row = (aesbyte_t*)malloc(sizeof(aesbyte_t) * m_arr[0].side_len);
		for (size_t k = omp_get_thread_num(); k < matrix_qty; k += omp_get_num_threads()) {
			for (aesbyte_t i = 0; i < m_arr[k].side_len; i += 1) {
				for (aesbyte_t j = 0; j < m_arr[k].side_len; j += 1)
					temp_row[j] = m_arr[k].grid[i * m_arr[k].side_len + j];
				for (aesbyte_t j = 0; j < m_arr[k].side_len; j += 1)
					m_arr[k].grid[i * m_arr[k].side_len + j] = temp_row[(i + j) % m_arr[k].side_len];
			}
		}
	free(temp_row);
	}
	clock_t end = (clock() - start) / (CLOCKS_PER_SEC / 1000);
	fprintf(stdout, "INFO: AES shift rows on matrix array completed.\n");
	fprintf(stdout, "INFO: displaying first matrix -\n");
	aes_matrix_display(m_arr[0]);
	fprintf(stdout, "INFO: displaying last matrix -\n");
	aes_matrix_display(m_arr[matrix_qty - 1]);
	fprintf(stdout, "INFO: computations took %d ms.\n", end);
	return (int)end;
}

int tcp_close_conn_server(SOCKET client_socket) {
	int my_err;
	/* Закрыть соединение. */
	fprintf(stdout, "INFO: shutting down connection on server.\n");
	my_err = shutdown(client_socket, SD_SEND);
	if (my_err == SOCKET_ERROR) {
		fprintf(stderr, "ERROR in tcp_send_matrices(): shutting down connection on server failed with code %d.\n", WSAGetLastError());
		closesocket(client_socket);
		WSACleanup();
		return -1;
	}
	closesocket(client_socket);
	WSACleanup();
	return 0;
}


struct AESMatrix* generate_matrices(size_t gen_data_size, aesbyte_t gen_matrix_side, size_t* gen_len, size_t* gen_matrix_size) {
	*gen_matrix_size = ((size_t)gen_matrix_side * (size_t)gen_matrix_side + 1);
	fprintf(stdout, "INFO: required data size is %llu MB.\n", gen_data_size);
	fprintf(stdout, "INFO: AES matrix size is %llu bytes.\n", *gen_matrix_size);
	/* Рассчитать количество матриц для генерации. */
	*gen_len = (gen_data_size * MEGABYTE_SIZE) / *gen_matrix_size;
	fprintf(stdout, "INFO: will generate array of %llu matrices.\n", *gen_len);
	fprintf(stdout, "INFO: recalculated data size is %llu.\n", *gen_len * (*gen_matrix_size));
	/* Выделить память и проверить. */
	struct AESMatrix* gen_matrices = (struct AESMatrix*)malloc(sizeof(struct AESMatrix) * (*gen_len));
	if (!gen_matrices) {
		fprintf(stderr, "ERROR in main(): couldn't allocate memory for generated data.\n");
		return (struct AESMatrix*)NULL;
	}
	/* Получить текущее время, проверить его корректность и задать сидом ГСЧ. */
	time_t rand_seed = time(NULL);
	if (rand_seed == -1)
		fprintf(stderr, "WARNING in generate_matrices(): couldn't get current time for random seed.\n");
	srand((unsigned int)rand_seed);
	/* Инициализировать и генерировать матрицы AES из случайных байт. */
	for (size_t i = 0; i < *gen_len; i += 1) {
		aes_matrix_init(&(gen_matrices[i]), gen_matrix_side);
		aes_matrix_randomise(gen_matrices[i]);
		if (i == 0) {
			fprintf(stdout, "INFO: displaying first matrix -\n");
			aes_matrix_display(gen_matrices[i]);
		}
		else if (i == *gen_len - 1) {
			fprintf(stdout, "INFO: displaying last matrix -\n");
			aes_matrix_display(gen_matrices[i]);
		}
	}
	return gen_matrices;
}

char* aes_pack_matrices(struct AESMatrix* gen_matrices, size_t gen_len, size_t gen_matrix_size) {
	if (!gen_matrices) {
		fprintf(stderr, "ERROR in aes_pack_matrices(): got null pointer.\n");
		return (char*)NULL;
	}
	fprintf(stdout, "INFO: packing matrices into TCP buffer format.\n");
	size_t gen_real_size = gen_len * gen_matrix_size;
	char* gen_chars = (char*)malloc(gen_real_size * sizeof(char));
	if (!gen_chars) {
		fprintf(stderr, "ERROR in aes_pack_matrices(): couldn't allocate memory for flattened data.\n");
		return (char*)NULL;
	}
	/* Упаковать матрицу в массив чаров для передачи по TCP. */
	for (size_t i = 0; i < gen_len; i += 1) {
		gen_chars[i * gen_matrix_size] = gen_matrices[i].side_len;
		for (aesbyte_t j = 1; j <= gen_matrices[i].side_len * gen_matrices[i].side_len; j += 1) {
			gen_chars[(i * gen_matrix_size) + j] = gen_matrices[i].grid[j - 1];
		}
	}
	fprintf(stdout, "INFO: buffer overview -\n");
	fprintf(stdout, "%d %d %d %d ... %d %d %d %d\n", gen_chars[0], gen_chars[1], gen_chars[2], gen_chars[3], gen_chars[gen_real_size - 4], gen_chars[gen_real_size - 3], gen_chars[gen_real_size - 2], gen_chars[gen_real_size - 1]);
	return gen_chars;
}

int tcp_establish_conn_server(SOCKET* cl_socket) {
	/* WSADATA содержит сведения о реализации сокетов в Windows. */
	WSADATA wsa_data;
	int my_err;
	/* Структуры addrinfo содержат данные об адресах и протоколах. */
	struct addrinfo* result = NULL;
	struct addrinfo hints;
	/* WSAStartup запрашивает версию WinSocket 2.2 и не даёт клиентам
	   использовать версию выше. */
	fprintf(stdout, "INFO: initialising WinSocket2 on server.\n");
	my_err = WSAStartup(MAKEWORD(2, 2), &wsa_data);
	if (my_err) {
		fprintf(stderr, "ERROR in tcp_send_matrices(): couldn't initialise WinSocket2, code %d.\n", my_err);
		WSACleanup();
		return -1;
	}
	memset(&hints, 0, sizeof(hints));
	/* Задать соединение по протоколу TCP через сокет. */
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;
	/* Разрешить собственный адрес и порт сервера. */
	fprintf(stdout, "INFO: resolving local address and port on server.\n");
	my_err = getaddrinfo(NULL, AES_PORT, &hints, &result);
	if (my_err) {
		fprintf(stderr, "ERROR in tcp_send_matrices(): resolving local address and port on server failed with code %d.\n", my_err);
		WSACleanup();
		return -1;
	}
	/* Создать сокет для прослушивания. */
	fprintf(stdout, "INFO: opening listening socket on server.\n");
	SOCKET listening_socket = INVALID_SOCKET;
	listening_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (listening_socket == INVALID_SOCKET) {
		fprintf(stderr, "ERROR in tcp_send_matrices(): opening listening socket on server failed with code %d.\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return -1;
	}
	/* Привязать сокет. */
	fprintf(stdout, "INFO: binding listening socket.\n");
	my_err = bind(listening_socket, result->ai_addr, (int)result->ai_addrlen);
	if (my_err == SOCKET_ERROR) {
		fprintf(stderr, "ERROR in tcp_send_matrices(): binding socket failed with code %d.\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(listening_socket);
		WSACleanup();
		return -1;
	}
	freeaddrinfo(result);
	/* Ждать, пока не подключится клиент. */
	fprintf(stdout, "INFO: server listening socket.\n");
	if (listen(listening_socket, SOMAXCONN) == SOCKET_ERROR) {
		printf("ERROR in tcp_send_matrices(): listening socket on server failed with code %d.\n", WSAGetLastError());
		closesocket(listening_socket);
		WSACleanup();
		return -1;
	}
	SOCKET client_socket;
	client_socket = INVALID_SOCKET;
	/* Принять подключение клиента при его появлении. */
	client_socket = accept(listening_socket, NULL, NULL);
	if (client_socket == INVALID_SOCKET) {
		printf("ERROR in tcp_send_matrices(): accepting client connection failed with code %d.\n", WSAGetLastError());
		closesocket(listening_socket);
		WSACleanup();
		return -1;
	}
	*cl_socket = client_socket;
	return 0;
}

int tcp_send_matrices(char* aes_data, size_t aes_datasize, SOCKET client_socket) {
	/* Буферы для приёма метаинформации о размере данных,
		а также для самих матриц. */
	char tcp_datasize_buf[DEF_BUFLEN];
	char* tcp_aes_data_sendbuf = aes_data;
	int my_err;
	if (!_itoa((int)aes_datasize, tcp_datasize_buf, 10)) {
		fprintf(stderr, "ERROR in tcp_send_matrices(): conversion of data size %llu failed.\n", aes_datasize);
		closesocket(client_socket);
		WSACleanup();
		return -1;
	}
	/* Отправить клиенту метаинформацию о размере пересылаемых данных с матрицами. */
	my_err = send(client_socket, tcp_datasize_buf, (int)strlen(tcp_datasize_buf), 0);
	if (my_err != SOCKET_ERROR)
		fprintf(stdout, "INFO: server sent metainfo to client: %d bytes, buffer is \"%s\".\n", my_err, tcp_datasize_buf);
	else {
		fprintf(stderr, "ERROR in tcp_send_matrices(): send failed with code %d.\n", WSAGetLastError());
		closesocket(client_socket);
		WSACleanup();
		return -1;
	}
	/* Отправить клиенту матрицы. */
	my_err = send(client_socket, tcp_aes_data_sendbuf, (int)aes_datasize, 0);
	if (my_err != SOCKET_ERROR)
		fprintf(stdout, "INFO: server sent matrices to client: %d bytes.\n", my_err);
	else {
		fprintf(stderr, "ERROR in tcp_send_matrices(): send failed with code %d.\n", WSAGetLastError());
		closesocket(client_socket);
		WSACleanup();
		return -1;
	}
	return 0;
}

int tcp_send_time(int aes_time, SOCKET client_socket) {
	/* Буфер для отправки информации о времени вычислений. */
	char tcp_time_buf[DEF_BUFLEN];
	int my_err;
	if (!_itoa(aes_time, tcp_time_buf, 10)) {
		fprintf(stderr, "ERROR in tcp_send_matrices(): conversion of time %d failed.\n", aes_time);
		closesocket(client_socket);
		WSACleanup();
		return -1;
	}
	/* Отправить клиенту метаинформацию о времени вычислений. */
	my_err = send(client_socket, tcp_time_buf, (int)strlen(tcp_time_buf), 0);
	if (my_err != SOCKET_ERROR)
		fprintf(stdout, "INFO: sent time info: %d bytes, buffer is \"%s\".\n", my_err, tcp_time_buf);
	else {
		fprintf(stderr, "ERROR in tcp_send_time(): send failed with code %d.\n", WSAGetLastError());
		closesocket(client_socket);
		WSACleanup();
		return -1;
	}
	return 0;
}

int tcp_recv_time(SOCKET connect_socket) {
	char tcp_time_recvbuf[DEF_BUFLEN];
	int recv_res;
	int tcp_time;
	/* Принять метаинформацию о времени вычислений. */
	memset(tcp_time_recvbuf, 0, DEF_BUFLEN);
	recv_res = recv(connect_socket, tcp_time_recvbuf, DEF_BUFLEN, 0);
	if (recv_res == 0) {
		fprintf(stderr, "ERROR in tcp_recv_time(): connection to server closed prematurely.\n");
		closesocket(connect_socket);
		WSACleanup();
		return -1;
	}
	else if (recv_res < 0) {
		fprintf(stderr, "ERROR in tcp_recv_time(): reception failure, code %d.\n", WSAGetLastError());
		closesocket(connect_socket);
		WSACleanup();
		return -1;
	}
	else if (recv_res == DEF_BUFLEN) {
		fprintf(stderr, "ERROR in tcp_recv_time(): buffer overflow while receiving %d bytes.\n", recv_res);
		closesocket(connect_socket);
		WSACleanup();
		return -1;
	}
	fprintf(stdout, "INFO: received time info: %d bytes, buffer is \"%s\".\n", recv_res, tcp_time_recvbuf);
	tcp_time = atoi(tcp_time_recvbuf);
	if (!tcp_time) {
		fprintf(stderr, "ERROR in tcp_recv_time(): received invalid time \"%s\".\n", tcp_time_recvbuf);
		closesocket(connect_socket);
		WSACleanup();
		return -1;
	}
	return tcp_time;
}

#endif