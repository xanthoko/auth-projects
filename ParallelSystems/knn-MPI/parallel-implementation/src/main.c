#include "../includes/main.h"

#define FIRST_REP 1

#define OTHER_REP 0

//global variables
int N;
int D;
int K;

int PID;
int MAX;
int CHUNK;
MPI_Status Stat;

double **in_buffer;
double **out_buffer;
double **array;

double **k_dist;
int **k_id;

char CORPUS_FILENAME[]      = "../../corpus-files/corpus.txt";
char VALIDATION_FILENAME[]  = "../../corpus-files/validated.txt";

int main (int argc, char **argv) {
  
  if (argc != 4) {
    printf("==============================================\n");
    printf("Usage: Parallel implementation of knn algorithm.\n");
    printf("==============================================\n");
    printf("arg[1] = N ==> number of points\n");
    printf("arg[2] = D ==> point's dimensions\n");
    printf("arg[3] = K ==> k nearest points\n");
    printf("\n\nExiting program..\n");
    exit(1);
  }

  N = atoi(argv[1]);
  D = atoi(argv[2]);
  K = atoi(argv[3]);

  MPI_Init(NULL, NULL);
  MPI_Comm_rank (MPI_COMM_WORLD, &PID);
  MPI_Comm_size(MPI_COMM_WORLD, &MAX);
  
  check_args();
  init();

  //Time start
  knn();
  //Time end

  //Save results

  if(PID == 0){
    MPI_Finalize();
  }

  //start measure of time
  // print(array,N,D);
  // calc_knn();

  // printf( "MPI_FINALIZE = %d\n\n",MPI_Finalize() )  ;
/*  printf("===============================================\n");
  printf("\t\tDistances\n");
  printf("===============================================\n");
  calc_distances();
  print(dist_arr,N,N);
  printf("===============================================\n");  
  printf("\t\tknn distances\n");
  printf("===============================================\n");
  calc_knn();
  print(k_dist,N,K);
  printf("===============================================\n");  
  printf("\t\tknn id's\n");
  printf("===============================================\n");
  print_id();
  printf("===============================================\n");
  printf("===============================================\n");
  printf ("\n\nIs test PASSed? %s\n\n", test()?"YES":"NO");*/

  return(0);
}



void check_args() {
  if (N<=0 || K<=0 || D<=0){
    printf("Negative value for N, K or D.\n");
    printf("\n\nExiting program..\n");
    exit(1);
  }
  if (N<=K){
    printf("K value is larger than N.\n");
    printf("\n\nExiting program..\n");
    exit(1);
  }
  if (N%MAX){
    printf("Number of processes is false.\n");
    printf("\n\nExiting program..\n");
    exit(1);
  } 
  return;
}

//Dynamically allocate memory and create the 2D-array
void init(){
  CHUNK = N/MAX;
  memory_allocation();
  read_file();
  return;
}

//Memory allocation for buffer, array, k_dist and k_id
void memory_allocation(){
  int i;

  in_buffer   = (double **) malloc(CHUNK * sizeof(double *));
  out_buffer  = (double **) malloc(CHUNK * sizeof(double *));
  array       = (double **) malloc(CHUNK * sizeof(double *));
  k_dist      = (double **) malloc(CHUNK * sizeof(double *));
  k_id        = (int **) malloc(CHUNK * sizeof(int *));
  if(in_buffer == NULL || out_buffer == NULL || array == NULL || k_dist == NULL || k_id == NULL) { 
    printf("Memory allocation error 1!\n");
    exit(1);
  }
  for (i=0; i<D; i++){
    in_buffer[i]  = (double *) malloc(D * sizeof(double));
    out_buffer[i] = (double *) malloc(D * sizeof(double));
    array[i]      = (double *) malloc(D * sizeof(double));
    if(in_buffer[i] == NULL || out_buffer[i] == NULL || array[i] == NULL){ 
      printf("Memory allocation error 2!\n");
        exit(1);
    }
  }
  for (i=0; i<K; i++){
    k_dist[i] = (double *) malloc(D * sizeof(double));
    k_id[i]   = (int *) malloc(D * sizeof(int));
    if(k_dist[i] == NULL || k_id[i] == NULL){ 
      printf("Memory allocation error 3!\n");
        exit(1);
    }
  }
  return;
}

void read_file(){
  int i, j, source, dest;
 
  if (PID == 0){
    //MASTER process handles the reading and sends to the other processes
    int index_buff;
    dest =1;

    FILE * fp;
    fp = fopen(CORPUS_FILENAME,"r");
    
    for (i=0; i<N; i++){      
      index_buff = i % CHUNK;
      for (j=0; j<D; j++) 
        fscanf(fp, "%lf", &out_buffer[index_buff][j]);

      //The process with PID=0 keeps the last buffer for itself
      if (dest >= MAX) continue;

      MPI_Send(out_buffer[index_buff], D, MPI_DOUBLE, dest, 0, MPI_COMM_WORLD);

      //Check when buffer is full to change process
      if (index_buff == CHUNK-1) dest++;
    }
    fclose(fp);
    copy_2D_arrays(array, out_buffer);
  }
  else {
    //SLAVE processes receice the data
    source = 0;
    for (i=0; i<CHUNK; i++)
      MPI_Recv(array[i], D, MPI_DOUBLE, source, 0, MPI_COMM_WORLD, &Stat);

    printf("[Init] Process %d received array from process 0\n", PID); 
    // if (MPI_SUCCESS == MPI_Recv(&buffer, CHUNK*(D+2), MPI_DOUBLE, 0, 0, MPI_COMM_WORLD, &Stat)){
    //   printf("Process %d received buffer from process 0\n", PID); 
    // }
    copy_2D_arrays(out_buffer, array);
  }
  
  MPI_Barrier(MPI_COMM_WORLD);
}

//Change chunks between processes using ring topology
//http://mpitutorial.com/tutorials/mpi-send-and-receive/
void ring_comm(int tag){
  int i, j, source, dest;

  source  = (PID - 1);
  dest    = (PID + 1);

  if (PID != 0){
    for (i=0; i<CHUNK; i++)
      MPI_Recv(in_buffer[i], D, MPI_DOUBLE, source, tag, MPI_COMM_WORLD, &Stat);
    printf("[Ring]: Process %d received buffer from process %d\n", PID, source);
  } 

  for (i=0; i<CHUNK; i++)
    MPI_Send(out_buffer[i], D, MPI_DOUBLE, dest % MAX, tag, MPI_COMM_WORLD);

  // Now process 0 can receive from the last process.
  if (PID == 0){
    for (i=0; i<CHUNK; i++)
      MPI_Recv(in_buffer[i], D, MPI_DOUBLE, MAX-1, tag, MPI_COMM_WORLD, &Stat);
    printf("[Ring]: Process %d received buffer from process %d\n", PID, MAX-1);
  }

  for (i=0; i<CHUNK; i++){
    for (j=0; j<D; j++){
      out_buffer[i][j] = in_buffer[i][j];
    }
  }
  copy_2D_arrays(out_buffer, in_buffer);
  MPI_Barrier(MPI_COMM_WORLD);
}

//Calculate knn
void knn(){
  int i;

  init_dist();

  //Initial comparison with its own data
  calc_knn(FIRST_REP);

  for (i=0; i<MAX-1; i++){
    ring_comm(i);
    //Debugging
    if(PID == 2){
      printf("-----------\n");       
      printf("[DEBUG]: The knn of process with PID = %d at ring state %d.\n", PID, i); 
      printf("-----------\n"); 
      print(k_dist,CHUNK,K);
      // print(out_buffer,CHUNK,D);
    }
    calc_knn(OTHER_REP);
  }

  if(PID == 2){
    printf("-----------\n");       
    printf("[DEBUG]: FINAL knn of process with PID = %d", PID); 
    printf("-----------\n"); 
    print(k_dist,CHUNK,K);
  }

  //Now every process has its overall knn
  return;
}

//Copy all arr2 elements to arr1
void copy_2D_arrays(double **arr1, double **arr2){
  int i,j;
  for (i=0; i<CHUNK; i++){
    for (j=0; j<D; j++){
      arr1[i][j] = arr2[i][j];
    }
  }
}

//Print 2D double arr array
void print(double **arr, int row, int col){
  int i, j;
  for (i=0; i<row; i++){
    for (j=0; j<col; j++){
      printf("%lf ", arr[i][j]);
    }
    printf("\n");
  }
  return;
}

//Print 2d int arr array
void print_id(){
  int i, j;
  for (i=0; i<N; i++){
    for (j=0; j<K; j++){
      printf("%d ", k_id[i][j]);
    }
    printf("\n");
  }
  return;
}



/*int test(){
  int i,j;
  //Change -1 with DBL_size 
  for (i=0;i<N;i++){
    for(j=0;j<N;j++){
      if (dist_arr[i][j] == -1){
        dist_arr[i][j] = DBL_MAX;
      }
    }
  }
  //Sort the dist_arr and do the testing
  for (i=0; i<N; i++){
    qsort(dist_arr[i], N, sizeof(double), cmp_func);
    for (j=0; j<K; j++){
      if (k_dist[i][j] != dist_arr[i][j]){
        printf("k_dist=%lf\n",k_dist[i][j]);
        printf("dist_arr=%lf\n",dist_arr[i][j]);
        printf("i=%d j=%d\n",i,j);
        return 0;
      }
    }
  }
  return 1;
}*/




double euclidean_distance(int first, int second){
  int j;
  double dist = 0;
  for (j=0; j<D; j++)
    dist += (array[first][j] - out_buffer[second][j]) * (array[first][j] - out_buffer[second][j]);
  return dist;
}

void init_dist(){
  int i, j;
  for (i=0; i<CHUNK; i++) {
    for (j=0; j<K; j++){
      k_dist[i][j] = DBL_MAX;
      k_id[i][j]   = -1;
    }
  }
}

void calc_knn(int rep){
  int i, j;
  double dist;

  //Calculate k nearest points
  for(i=0; i<CHUNK; i++){
    for (j=0; j<CHUNK; j++) {
      if (i==j && rep) continue;
        dist = euclidean_distance(i,j);
        if (dist < k_dist[i][K-1]){
          find_position(i,dist,j);  //Just found a new closer distance
        }
    }
  }
}

//Find the position of the new distance
void find_position(int i, double dist, int id){
  int j;
  for (j=0; j<K; j++){
    if (dist < k_dist[i][j]){
      // if (i==j) continue;
      move(i,j);
      k_dist[i][j] = dist;
      k_id[i][j]   = id;
      return;
    }
  }
  return;
}

//Shift k_dist[i] & k_id[i] values one position in order to insert the new distance
void move(int i, int pos){
  int j;
  for (j=(K-1); j>pos; j--){
    if(j==0) continue;
    k_dist[i][j] = k_dist[i][j-1];
    k_id[i][j]   = k_id[i][j-1];
  }
  return;
}