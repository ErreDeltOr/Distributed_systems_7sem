#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include "mpi.h"

#define MAX_SIZE_NAME 80


// Структура, которую будем посылать остальным процессам
// перед входом в критическую секцию
// (Временная метка, Номер процесса отправителя, Имя критической секции)
typedef struct {
    double current_time;
    int rank_of_sender;
    char name_of_critical[MAX_SIZE_NAME];
} Message;


int count = 2;
int array_of_blocklengths[] = { 1, 1, MAX_SIZE_NAME };
MPI_Aint array_of_displacements[] = { offsetof( Message, current_time ),
                                      offsetof( Message, rank_of_sender ),
                                      offsetof( Message, name_of_critical ) };

MPI_Datatype array_of_types[] = { MPI_DOUBLE, MPI_INT, MPI_CHAR };
MPI_Datatype tmp_type, my_mpi_type;
MPI_Aint lb, extent;



int main(int argc, char *argv[]) {
    int error;
    error = MPI_Init(&argc, &argv);  // Инициализируем процессы

    double start = MPI_Wtime();      // Регестрируем время запуска процесса

    srand(getpid());           // Для каждого процесса получаем случайный seed
                               // для того, чтобы в критической секции у всех было разное время
                               // для sleep()

    if (error != MPI_SUCCESS) {
        fprintf(stderr, "ERROR: CAN'T MPI INIT\n");
        return 1;
    }

    // Сообщаем MPI, как ему трактовать структуру данных Message при отправке сообщений
    MPI_Type_create_struct( count, array_of_blocklengths, array_of_displacements,
                            array_of_types, &tmp_type );
    MPI_Type_get_extent( tmp_type, &lb, &extent );
    MPI_Type_create_resized( tmp_type, lb, extent, &my_mpi_type );
    MPI_Type_commit( &my_mpi_type );


    int myrank, ranksize;

    // Получаем номер процесса
    MPI_Comm_rank(MPI_COMM_WORLD, &myrank);

    // Получаем количество процессов
    MPI_Comm_size(MPI_COMM_WORLD, &ranksize);

    MPI_Request req[ranksize - 1];    // массив для хранения запросов
    MPI_Status  status[ranksize - 1]; // массива для хранения статусов запросов

    Message messages[ranksize]; // массив для получения сообщений от других процессов
                                // на доступ в критическую секцию
    
    Message message_for_send; // сообщение которое будет посылать процесс для доступа
                              // в критическую секцию

    message_for_send.rank_of_sender = myrank;
    strcpy(message_for_send.name_of_critical, "critical");
    double timestamp = MPI_Wtime() - start;  //Получаем временную метку текущего процесса
    message_for_send.current_time = timestamp;

    char ok_send[3];
    char* ok_recv[3];
    strcpy(ok_send, "OK");

    int j = 0;
    // Отсылаем сообщения всем остальным процессам (при этом не блокируемся)
    for (int i = (myrank + 1) % ranksize; i != myrank; i = (i + 1) % ranksize)
        MPI_Isend(&message_for_send, 1, my_mpi_type, i, 1215, MPI_COMM_WORLD, &req[j++]);

    j = 0;
    // Принимаем запросы от всех остальных процессов БЛОКИРУЮЩИМ send
    // Deadlock не произойдет, так как все процессы гарантированно отправят свои
    // запросы всем остальным процессам
    for (int i = (myrank + 1) % ranksize; i != myrank; i = (i + 1) % ranksize)
        MPI_Recv(&messages[i], 1, my_mpi_type, i, 1215, MPI_COMM_WORLD, &status[j++]);

    // Ждём пока ВСЕ остальные процессы не получат запрос от текущего процесса
    if (ranksize > 1) MPI_Waitall(ranksize - 1, &req[0], &status[0]);
    MPI_Barrier(MPI_COMM_WORLD);

    int num_less = 0;
    for (int i = 0; i < myrank; ++i)
        // Если временная метка текущего процесса больше, чем метка
        // другого процесса, отправившего запрос на входв крит. секцию,
        // то по алгоритму мы разрешаем ему войти в крит. секцию и отсылаем "OK"
        // В противном случае мы отошлём ему "OK" уже после прохождения
        // крит. cекции
        if (messages[i].current_time <= timestamp)
            MPI_Isend(&ok_send[0], 3, MPI_CHAR, i, 1217, MPI_COMM_WORLD, &req[num_less++]);

    for (int i = myrank + 1; i < ranksize; ++i)
        if (messages[i].current_time < timestamp)
            MPI_Isend(&ok_send[0], 3, MPI_CHAR, i, 1217, MPI_COMM_WORLD, &req[num_less++]);

    j = 0;
    // Ожидаем, пока все остальные процессы не отправят разрешение на вход в крит. секцию
    for (int i = (myrank + 1) % ranksize; i != myrank; i = (i + 1) % ranksize)
        MPI_Recv(&ok_recv[0], 3, MPI_CHAR, MPI_ANY_SOURCE, 1217, MPI_COMM_WORLD, &status[j++]);


    /////////////////////////////////////////////
    // start critical section
    // проверка наличия файла “critical.txt”
    FILE *file;
    file = fopen("critical.txt", "r");
    if (file != NULL) {
        // Если программа работает по алгоритму, то ни один процесс ни разу не зайдёт
        // в эту ветку, так как это ьудет означать, что он получил доступ к критической секции,
        // пока какой-то другой процесс был внутри неё
        fprintf(stderr, "ERROR: FILE EXIST! PROCESS NUM = %d\n", myrank);
        MPI_Abort(MPI_COMM_WORLD, MPI_ERR_FILE_EXISTS);
    } else {
        fprintf(stdout, "process number %d entered critical section\n", myrank);
        file = fopen("critical.txt", "w"); //Создаём файл;
        sleep(rand() % 5 + 1); // Время сна - от 1 до 5 секунд
        fclose(file);
        remove("critical.txt"); // Удаляем файл
        fprintf(stdout, "process number %d left critical section\n", myrank);
    }
    // end critical section
    /////////////////////////////////////////////

    // Отсылаем "OK" тем процессам, чья временная метка оказалась больше
    // метки текущего процесса

    for (int i = 0; i < myrank; ++i)
        if (messages[i].current_time > timestamp)
            MPI_Isend(&ok_send[0], 3, MPI_CHAR, i, 1217, MPI_COMM_WORLD, &req[num_less++]);

    for (int i = myrank + 1; i < ranksize; ++i)
        if (messages[i].current_time >= timestamp)
            MPI_Isend(&ok_send[0], 3, MPI_CHAR, i, 1217, MPI_COMM_WORLD, &req[num_less++]);

    // Ждём, пока все остальные процессы не получат "OK" от нашего
    if (ranksize > 1) MPI_Waitall(ranksize - 1, &req[0], &status[0]);

    // Завершаем среду выполнения
    MPI_Type_free( &my_mpi_type );
    MPI_Finalize();
    return 0;
}
