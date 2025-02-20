#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <queue>
#include "pthread.h"

using namespace std;

struct input_file_for_mapper
{
	string file_name;
	int file_id;
	long long size;
};

struct mapper_args
{
	int id;
	queue<input_file_for_mapper> *input_files_queue;
	pthread_mutex_t *input_files_mutex;
	queue<unordered_map<string, int>> *mapper_output;
	pthread_mutex_t *mapper_output_mutex;
	pthread_barrier_t *mappers_barrier;
};

struct reducer_args
{
	int id;
	int leader_id;
	int no_reducers;
	queue<unordered_map<string, int>> *mapper_output;
	pthread_mutex_t *mapper_output_mutex;
	unordered_map<int, unordered_map<string, vector<int>>> *reducer_output_non_sorted;
	pthread_mutex_t *reducer_output_mutex;
	unordered_map<int, vector<pair<string, vector<int>>>> *inverted_index;
	pthread_mutex_t *inverted_index_mutex;
	pthread_barrier_t *mappers_barrier;
	pthread_barrier_t *reducers_barrier;
};

void *mapper(void *args)
{
	struct mapper_args *mapper_args = (struct mapper_args *)args;
	int id = mapper_args->id;
	queue<input_file_for_mapper> *input_files_queue = mapper_args->input_files_queue;
	pthread_mutex_t *input_files_mutex = mapper_args->input_files_mutex;
	queue<unordered_map<string, int>> *mapper_output = mapper_args->mapper_output;
	pthread_mutex_t *mapper_output_mutex = mapper_args->mapper_output_mutex;
	pthread_barrier_t *mappers_barrier = mapper_args->mappers_barrier;

	while (true)
	{
		unordered_map<string, int> local_map;
		pthread_mutex_lock(input_files_mutex);
		if (input_files_queue->empty())
		{
			pthread_mutex_unlock(input_files_mutex);
			break;
		}

		input_file_for_mapper input_file = input_files_queue->front();
		input_files_queue->pop();
		pthread_mutex_unlock(input_files_mutex);

		ifstream fin;
		fin.open(input_file.file_name);

		if (!fin.is_open())
		{
			cout << "Error while opening the file" << endl;
			exit(-1);
		}

		string word;
		while (fin >> word)
		{
			word.erase(remove_if(word.begin(), word.end(), [](char c)
								 { return !isalpha(c); }),
					   word.end());
			transform(word.begin(), word.end(), word.begin(), ::tolower);

			local_map[word] = input_file.file_id + 1;
		}

		pthread_mutex_lock(mapper_output_mutex);
		mapper_output->push(local_map);
		pthread_mutex_unlock(mapper_output_mutex);
	}

	pthread_barrier_wait(mappers_barrier);

	pthread_exit(NULL);
}

void *reducer(void *args)
{
	struct reducer_args *reducer_args = (struct reducer_args *)args;
	int id = reducer_args->id;
	int leader_id = reducer_args->leader_id;
	int no_reducers = reducer_args->no_reducers;
	queue<unordered_map<string, int>> *mapper_output = reducer_args->mapper_output;
	pthread_mutex_t *mapper_output_mutex = reducer_args->mapper_output_mutex;
	unordered_map<int, unordered_map<string, vector<int>>> *reducer_output = reducer_args->reducer_output_non_sorted;
	pthread_mutex_t *reducer_output_mutex = reducer_args->reducer_output_mutex;
	unordered_map<int, vector<pair<string, vector<int>>>> *inverted_index = reducer_args->inverted_index;
	pthread_mutex_t *inverted_index_mutex = reducer_args->inverted_index_mutex;
	pthread_barrier_t *mappers_barrier = reducer_args->mappers_barrier;
	pthread_barrier_t *reducers_barrier = reducer_args->reducers_barrier;

	pthread_barrier_wait(mappers_barrier);

	while (true)
	{
		unordered_map<string, int> local_map;
		pthread_mutex_lock(mapper_output_mutex);
		if (mapper_output->empty())
		{
			pthread_mutex_unlock(mapper_output_mutex);
			break;
		}

		local_map = mapper_output->front();
		mapper_output->pop();
		pthread_mutex_unlock(mapper_output_mutex);

		for (auto it = local_map.begin(); it != local_map.end(); it++)
		{
			string word = it->first;
			int first_letter = word[0] - 'a';

			pthread_mutex_lock(reducer_output_mutex);
			(*reducer_output)[first_letter][word].push_back(it->second);
			pthread_mutex_unlock(reducer_output_mutex);
		}
	}

	pthread_barrier_wait(reducers_barrier);

	int no_letters = 26;
	int start = id * (double)no_letters / no_reducers;
	int end;

	if ((id + 1) * (double)no_letters / no_reducers >= no_letters)
	{
		end = no_letters;
	}
	else
	{
		end = (id + 1) * (double)no_letters / no_reducers;
	}

	int i;
	for (i = start; i < end; i++)
	{
		vector<pair<string, vector<int>>> local_vector;
		for (auto it = (*reducer_output)[i].begin(); it != (*reducer_output)[i].end(); it++)
		{
			for (int j = 0; j < it->second.size(); j++)
			{
				sort(it->second.begin(), it->second.end());
			}
			local_vector.push_back({it->first, it->second});
		}
		sort(local_vector.begin(), local_vector.end(), [](pair<string, vector<int>> a, pair<string, vector<int>> b)
			 { if (a.second.size() == b.second.size())
				 {
					 return a.first < b.first;
				 }
				 return a.second.size() > b.second.size(); });

		pthread_mutex_lock(inverted_index_mutex);
		(*inverted_index)[i] = local_vector;
		pthread_mutex_unlock(inverted_index_mutex);
	}

	pthread_barrier_wait(reducers_barrier);

	if (id == leader_id)
	{
		for (i = 0; i < no_letters; i++)
		{
			char letter = 'a' + i;
			string letter_as_string(1, letter);
			string filename = letter_as_string + ".txt";
			ofstream fout(filename);

			if (!fout.is_open())
			{
				cout << "Error while opening the file" << endl;
				exit(-1);
			}

			for (auto it = (*inverted_index)[i].begin(); it != (*inverted_index)[i].end(); it++)
			{
				fout << it->first << ":[";

				for (int j = 0; j < it->second.size(); j++)
				{
					if (j == it->second.size() - 1)
					{
						fout << it->second[j] << "]";
					}
					else
					{
						fout << it->second[j] << " ";
					}
				}
				fout << endl;
			}

			fout.close();
		}
	}

	pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int no_mappers, no_reducers, id, r;
	void *status;
	string arg_file, input_file;
	vector<input_file_for_mapper> input_files;

	no_mappers = atoi(argv[1]);
	no_reducers = atoi(argv[2]);
	arg_file = argv[3];

	ifstream fin;
	fin.open(arg_file);

	if (!fin.is_open())
	{
		cout << "Error while opening the file" << endl;
		exit(-1);
	}

	int no_files, i;
	fin >> no_files;

	for (i = 0; i < no_files; i++)
	{
		fin >> input_file;
		long long file_size = filesystem::file_size(input_file);
		input_files.push_back({input_file, i, file_size});
	}

	std::sort(input_files.begin(), input_files.end(), [](input_file_for_mapper a, input_file_for_mapper b)
			  { return a.size > b.size; });

	queue<input_file_for_mapper> input_files_queue;
	for (i = 0; i < no_files; i++)
	{
		input_files_queue.push(input_files[i]);
	}

	int ids[no_mappers + no_reducers];
	pthread_t threads[no_mappers + no_reducers];

	queue<unordered_map<string, int>> mapper_output;

	pthread_mutex_t input_files_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t mapper_output_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t reducer_output_mutex = PTHREAD_MUTEX_INITIALIZER;
	pthread_mutex_t inverted_index_mutex = PTHREAD_MUTEX_INITIALIZER;

	pthread_barrier_t mappers_barrier;
	pthread_barrier_init(&mappers_barrier, NULL, no_mappers + no_reducers);

	pthread_barrier_t reducers_barrier;
	pthread_barrier_init(&reducers_barrier, NULL, no_reducers);

	mapper_args mapper_args[no_mappers];
	reducer_args reducer_args[no_reducers];

	unordered_map<int, unordered_map<string, vector<int>>> reducer_output_non_sorted;
	unordered_map<int, vector<pair<string, vector<int>>>> inverted_index;

	for (id = 0; id < no_mappers + no_reducers; id++)
	{
		ids[id] = id;
		if (ids[id] < no_mappers)
		{
			mapper_args[ids[id]].id = ids[id];
			mapper_args[ids[id]].input_files_queue = &input_files_queue;
			mapper_args[ids[id]].input_files_mutex = &input_files_mutex;
			mapper_args[ids[id]].mapper_output = &mapper_output;
			mapper_args[ids[id]].mapper_output_mutex = &mapper_output_mutex;
			mapper_args[ids[id]].mappers_barrier = &mappers_barrier;
			r = pthread_create(&threads[id], NULL, mapper, &mapper_args[ids[id]]);
		}
		else
		{
			reducer_args[ids[id] - no_mappers].id = ids[id] - no_mappers;
			reducer_args[ids[id] - no_mappers].leader_id = 0;
			reducer_args[ids[id] - no_mappers].no_reducers = no_reducers;
			reducer_args[ids[id] - no_mappers].mapper_output = &mapper_output;
			reducer_args[ids[id] - no_mappers].mapper_output_mutex = &mapper_output_mutex;
			reducer_args[ids[id] - no_mappers].reducer_output_non_sorted = &reducer_output_non_sorted;
			reducer_args[ids[id] - no_mappers].reducer_output_mutex = &reducer_output_mutex;
			reducer_args[ids[id] - no_mappers].inverted_index = &inverted_index;
			reducer_args[ids[id] - no_mappers].inverted_index_mutex = &inverted_index_mutex;
			reducer_args[ids[id] - no_mappers].mappers_barrier = &mappers_barrier;
			reducer_args[ids[id] - no_mappers].reducers_barrier = &reducers_barrier;
			r = pthread_create(&threads[id], NULL, reducer, &reducer_args[ids[id] - no_mappers]);
		}

		if (r)
		{
			cout << "Error while creating thread " << id << endl;
			exit(-1);
		}
	}

	for (id = 0; id < no_mappers + no_reducers; id++)
	{
		r = pthread_join(threads[id], &status);

		if (r)
		{
			cout << "Error while waiting for thread " << id << endl;
			exit(-1);
		}
	}
	pthread_barrier_destroy(&mappers_barrier);
	pthread_barrier_destroy(&reducers_barrier);
	pthread_mutex_destroy(&input_files_mutex);
	pthread_mutex_destroy(&mapper_output_mutex);
	pthread_mutex_destroy(&reducer_output_mutex);
	pthread_mutex_destroy(&inverted_index_mutex);

	return 0;
}