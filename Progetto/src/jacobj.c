#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
 * N               : dimensione della matrice da generare
 * flag            : di default 0 se settato ad 1 permette di visualizzare la matrice generata
 * generateMatrix  : genera la matrice prendendo in input il puntatore alla matrice,l'indice di righe di inizio e fine,
                     il rank del processo ed il lavoro ossia il numero di righe per ogni processo
 * printMatrix     : stampa a video la matrice prendendo in input il puntatore alla matrice,l'indice di righe e colonne di inizio e fine
 */
int N = 12;
int flag = 0;
void generateMatrix(double **xMatrix, int iStart, int iEnd, int rank, int work);
void printMatrix(double **xMatrix, int iStart, int iEnd, int jStart, int jEnd);

int main(int argc, char** argv) {
	int size, rank, iterator = 0;
	MPI_Init(NULL, NULL);
	MPI_Status status;
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);

// Verifico il numero di argomenti passati come linea di comando e di conseguenza inizializzo N e flag
	if (argc > 1)
		N = atoi(argv[1]);
        if (N % size !=0) 
                MPI_Abort(MPI_COMM_WORLD,1);
	if (argc > 2)
		flag = atoi(argv[2]);
	/*
	 * work            : numero di righe che ogni processo deve computare
	 * dim             : dimensione della matrice di ogni singolo processo
	 * indexStart      : indice inzio della computazione
	 * indexEnd        : indice fine della computazione
	 * diffNorm        : convergenza di ogni processo relativa quindi alla sottomatrice
	 * globalDiffNorm  : convergenza totale relativa a tutta la matrice
	 * xMatrix         : matrice in input 
	 * newxMatrix      : matrice temporanea 
	 */
	int work = N / size;
	int dim = work + 1, indexStart = 1, indexEnd = work + 1, i = 0, j = 0;
	double diffNorm, globalDiffNorm;

	if (rank == 0) {
		indexEnd -= 1;
		indexStart = 0;
	}
	if (rank != size - 1 && rank != 0)
		dim += 1;

//Allocazione delle matrici xMatrix,newxMatrix
	double **xMatrix, **newxMatrix;
	xMatrix = (double **) malloc(dim * sizeof(double *));
	newxMatrix = (double **) malloc(dim * sizeof(double *));

	int k = 0;
	for (k = 0; k < dim; k++) {
		xMatrix[k] = (double *) malloc(N * sizeof(double));
		newxMatrix[k] = (double *) malloc(N * sizeof(double));
	}

//Generazione matrice xMatrix
	generateMatrix(xMatrix, indexStart, indexEnd, rank, work);

	if (flag == 1)
		printMatrix(xMatrix, indexStart, indexEnd, 0, N);

// Riassegnamento degli indici al processo con rank 0 e size-1 in quanto devono lavorare su work-1 righe 
	if (rank == 0) {
		indexStart = 1;
	}
	if (rank == size - 1) {
		indexEnd -= 1;
	}

	double start = MPI_Wtime();
	do {

		if (work != N) {
			/*
			 * Scambiano le righe :
			 * rank  0                  : invia la riga work-1 al processo con rank pari 1
			 * rank  i con 1<i<size-1   : invia la riga 1 e la riga work
			 * rank  size-1             : invia la riga 1 al processo con rank pari a size-2
			 *
			 */
			int indexInvio = work;

			if (rank == 0)
				indexInvio = work - 1;

			if (rank != size - 1)
				MPI_Send(xMatrix[indexInvio], N, MPI_DOUBLE, rank + 1, 0,
						MPI_COMM_WORLD);

			if (rank != 0)
				MPI_Recv(xMatrix[0], N, MPI_DOUBLE, rank - 1, 0, MPI_COMM_WORLD,
						&status);

			if (rank != 0)
				MPI_Send(xMatrix[1], N, MPI_DOUBLE, rank - 1, 99,
						MPI_COMM_WORLD);

			if (rank != size - 1)
				MPI_Recv(xMatrix[indexInvio + 1], N, MPI_DOUBLE, rank + 1, 99,
						MPI_COMM_WORLD, &status);
		} else {

			// Riassegnamento della dimensione se vi è un singolo processo
			dim = N;
		}
		iterator++;
		diffNorm = 0.0;
		// Viene effettuata la computazione utilizzando la matrice temporanea newxMatrix
		for (i = indexStart; i < indexEnd; i++)
			for (j = 1; j <= N - 2; j++) {
				newxMatrix[i][j] = (xMatrix[i + 1][j] + xMatrix[i - 1][j]
						+ xMatrix[i][j + 1] + xMatrix[i][j - 1]) / 4;
				diffNorm += (newxMatrix[i][j] - xMatrix[i][j])
						* (newxMatrix[i][j] - xMatrix[i][j]);

			}

		// Sostituisco i valori di xMatrix con quelli presenti nella matrice temporanea
		for (i = indexStart; i < indexEnd; i++)
			for (j = 1; j <= N - 2; j++)
				xMatrix[i][j] = newxMatrix[i][j];

		/*
		 * Ogni processo dopo aver calcolato il propio diffNorm 
		 * lo invia al master ossia al processo con rank pari a 0
		 * esso calcola la somma e la invia a tutti i processi
		 */
		if (size != 1) {
			globalDiffNorm = 0;
			if (rank != 0)
				MPI_Send(&diffNorm, 1, MPI_DOUBLE, 0, 22, MPI_COMM_WORLD);

			if (rank == 0) {
				double value = 0;
				int l = 1;
				for (l = 1; l < size; l++) {
					MPI_Recv(&value, 1, MPI_DOUBLE, l, 22, MPI_COMM_WORLD,
							&status);
					globalDiffNorm += value;
				}
				globalDiffNorm += diffNorm;
				for (l = 1; l < size; l++)
					MPI_Send(&globalDiffNorm, 1, MPI_DOUBLE, l, 23,
							MPI_COMM_WORLD);
			}

			if (rank != 0)
				MPI_Recv(&globalDiffNorm, 1, MPI_DOUBLE, 0, 23, MPI_COMM_WORLD,
						&status);
		} else {
			globalDiffNorm = diffNorm;
		}

		globalDiffNorm = sqrt(globalDiffNorm);

	} while (iterator < 100 && globalDiffNorm > 1.0e-2);
	double end = MPI_Wtime();

	/*
	 * Tutti gli mpi_job inviano un intero al processo con rank pari a 0
	 * Di modo che esso stampi a video numero di iterazioni,diffNorm e 
	 * il numero di secondi solo dopo che tutti hanno terminato
	 */
	int flags = 0;
	if (rank != 0)
		MPI_Send(&flags, 1, MPI_INT, 0, 78, MPI_COMM_WORLD);
	else {
		int l = 1, flagRec = 0;
		for (l = 1; l < size; l++)
			MPI_Recv(&flagRec, 1, MPI_INT, l, 78, MPI_COMM_WORLD, &status);

		printf("---------------------------------------------");
		printf("\n NUMERO ITERAZIONI :%d ", iterator);
		printf("\n DIFFNORM          :%f ", globalDiffNorm);
		printf("\n NUMERO SECONDI    :%f ", end - start);
		printf("\n---------------------------------------------\n");

	}

// Deallocazione delle matrice xMatrix,newxMatrix

	for (i = 0; i < dim; i++) {
		free(xMatrix[i]);
		free(newxMatrix[i]);
	}
	free(xMatrix);
	free(newxMatrix);

	MPI_Finalize();
	return 0;
}

/*
 * Generiamo la matrice utilizzando un piccolo "trucco".
 * Infatti utilizzando solamente la funzione rand() tutti i processi
 * genererebbero la stessa sottomatrice inoltre si deve garantire 
 * che la matrice sia a diagonale dominante.
 * Per ovviare al primo problema impostiamo il seed come (N*work*rank)+1
 * e lo incrementiamo ad ogni iterazione : chiaramente il suo valore per ogni rank
 * sara sempre compreso fra (N*work*rank)+1 <= seed <= (N*work*(rank+1))+1.
 * Per ovviare al secondo problema salviamo il max e lo assegnamo all'elemento 
 * sulla diagonale aiutandoci con la variabile tax in quanto non si puo semplicemente 
 * porre xMatrix[i][i]=max perchè la diagonale di xMatrix non è la diagonale della
 * matrice completa.
 */
void generateMatrix(double **xMatrix, int iStart, int iEnd, int rank, int work) {
	int i = 0, j = 0, max = 0, seed = (N * work * rank) + 1;
	for (i = iStart; i < iEnd; i++) {
		xMatrix[i] = (double *) malloc(N * sizeof(double));
		for (j = 0; j < N; j++) {
			srand(seed);
			xMatrix[i][j] = rand() % 10 + 0;
			seed++;
			max = (xMatrix[i][j] > max ? xMatrix[i][j] : max);
		}

		int tax = i + (rank * work);
		if (rank != 0)
			tax = tax - 1;
		xMatrix[i][tax] = max + 1;
		max = 0;
	}
}

void printMatrix(double **xMatrix, int iStart, int iEnd, int jStart, int jEnd) {
	int i, j;
	for (i = iStart; i < iEnd; i++) {
		for (j = jStart; j < jEnd; j++)
			printf(" %f ", xMatrix[i][j]);
		printf("\n");
	}
}
