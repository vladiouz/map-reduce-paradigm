# MapReduce paradigm using pthreads

This project was an assigment in my third year of bachelors in CSE.
It's part of the **Parallel and Distributed Algorithms** course.

The checker can be ran using docker with the following command: `./run_with_docker.sh`.
I achieved the maximum grade for this project.

It is a simple implementation of the MapReduce paradigm using pthreads in C++.

## Explanation

To begin with, I created three structs: `input_file_for_mapper`, `mapper_args`, and `reducer_args`.

The `input_file_for_mapper` structure helped me sort the input files more easily while also keeping their ID. The sorting was done in descending order based on file size. This way, the mapper threads will first receive the larger files and will work using a queue, which also utilizes a mutex to prevent race conditions.

The `mapper_args` and `reducer_args` structures represent the arguments passed to the functions for the two types of threads.

After a mapper retrieves a file from the queue, it processes all the words in it, "cleans" them (removes any non-alphabetic characters), and adds them to `local_map`. When a mapper finishes processing a file, it locks and adds `local_map` to `mapper_output`, which is shared among all mappers. After that, it returns to the queue for a new file.

At the end of all mappers' execution and at the beginning of the reducers' execution, I placed a barrier. This ensures that no reducer starts execution until all mappers are finished. A mutex is used to control reducers' access to `mapper_output`. The reducer takes the first element from the queue, which is an `unordered_map<string, int>`. For each element in this `unordered_map`, we add it to `reducer_output`.

`reducer_output` is of type `unordered_map<int, unordered_map<string, vector<int>>>`, where the `int` key represents a number from 0 to 25 (corresponding to letters from a to z, where a is 0, b is 1, etc.), and the associated value represents a mapping from a word to a list of file IDs where it appears.

In `reducer_output`, words are organized by their first letter but are not yet sorted. So, I placed a barrier at the end of populating `reducer_output`, and after that, I started parallel processing for `inverted_index`. In this step, sorting is performed, with each reducer thread working on different sections of `reducer_output` (using `start` and `end`, calculated based on the number of letters in the alphabet, the reducer's ID, and the total number of reducers).

The data structure has reached its final form, but it still needs to be written to files. I placed a barrier at the end of inserting data into `inverted_index` and kept only one thread (the one with `leader_id`) responsible for writing to files.

## Explicatia in limba romana

Pentru inceput, am facut 3 `struct`s, anume `input_file_for_mapper`, `mapper_args` si `reducer_args`.

Structura `input_file_for_mapper` m-a ajutat sa sortez mai usor fisierele de input pastrandu-le si id-ul. Sortarea lor a fost facuta descrescator dupa dimensiune. Astfel, thread-urile de tip mapper vor primi mai intai fisierele mai mari si vor lucra pe baza unei cozi, coada pe care folosesc si un mutex pentru a nu avea race condition.

Structurile `mapper_args` si `reducer_args` sunt argumentele primite de functiile pentru cele doua tipuri de threaduri.

Dupa ce un mapper ia fisierul din coada, acesta ii parcurge toate cuvintele, le "curata" (elimina orice caracter non-alfabetic din cuvinte) si le adauga in `local_map`. Cand un mapper termina cu un fisier, pune un lock si adauga `local_map` in `mapper_output`, care este impartit intre toti mapperii. Dupa aceea se intoarce la coada pentru un nou fisier.

La finalul executiei tuturor mapperilor dar si la inceputul executiei reducerilor am pus o bariera. Astfel ma asigur ca niciun reducer nu incepe executia pana nu sunt gata mapperii. Pentru accesul reducerilor la `mapper_output` se utilizeaza un mutex. Reducerul ia primul element din coada, anume un `unordered_map<string, int>`. Pentru fiecare element din acest `unordered_map` vom adauga in `reducer_output`.

`reducer_output` are tipul `unordered_map<int, unordered_map<string, vector<int>>>`, unde cheia `int` reprezinta un numar de la 0 la 25 (acesta corespunde literelor de la a la z, unde a este 0, b este 1, etc.), iar valoarea asociata reprezinta o mapare de la un cuvant la o lista cu id-urile fisierelor in care apare.

In `reducer_output` avem cuvintele organizate dupa prima litera, dar inca nu sunt sortate. Astfel, am pus o bariera la finalul popularii `reducer_output`, iar dupa aceasta am inceput procesarea paralela pentru `inverted_index`. In aceasta parte se face sortarea, fiecare thread reducer lucrand pe zone diferite din `reducer_output` (folosind `start` si `end`, calculate pe baza numarului de litere din alfabet, a id-ului reducerului si a numarului de reduceri).

Structura de date a ajuns in forma finala, dar trebuie sa si scriem in fisiere. Am pus o bariera la finalul adaugarilor in `inverted_index` si am pastrat numai un thread (anume cel cu `leader_id`) pentru scrierea in fisiere.
