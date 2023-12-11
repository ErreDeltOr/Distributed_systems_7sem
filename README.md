# Distributed_systems_7sem
CMC MSU 7 sem distributed systems task. First task - decentralized algorithm of entreing critical section. Second task - optimization of 3mm

To run any of the programms type:
1)          : mpicc task1.c :           or            : mpicc task2.c :
2)                       : mpirun -np PROCESS_NUM ./a.out :

For some unknown and absolutely stupid reason second prog can't print anything in stdout and stderr after it reaches 
the section where process 2 is killed, so be aware of it and if you want to check if the program
computes result correctly (without any process crashing) make sure you first deleted that exact section from
the code.

MPI moment :-)

----------------------------------------------------------------------------------------------------------------------------------------------

ВМК МГУ 7 семестр распределённые системы. Первое задание - децентрализованный алгоритм входа в критическую секцию.
Второе задание - оптимизация 3mm.

Чтобы запустить любую из программ напишите:
1)          : mpicc task1.c :           или            : mpicc task2.c :
2)                       : mpirun -np PROCESS_NUM ./a.out :

По какой-то непонятной и совершенно идиотской причине вторая программа ничего не печатает
в поток вывода и ошибок после того как достигла секции, где убивается второй процесс, так что будьте в курсе этого
и если вам хочется проверить, правильно ли программа вычисляет результ (когда процессы не крашатся, 
сначала удалите ту самую секцию из кода.

MPI момент :-)
