#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "timer.h"
#include "libcoro.h"

/**
 * You can compile and run this code using the commands:
 *
 * $> gcc solution.c libcoro.c
 * $> ./a.out
 */

static int *data_merge(int *arr1, int sz1, int *arr2, int sz2)
{
	int i = 0;
	int j = 0;
	int k = 0;
	int *rez = malloc(sizeof(int) * (sz1 + sz2));

	while (i < sz1 && j < sz2)
	{
		if (arr1[i] <= arr2[j])
			rez[k] = arr1[i++];
		else
			rez[k] = arr2[j++];
		++k;
	}

	while (i < sz1)
		rez[k++] = arr1[i++];

	while (j < sz2)
		rez[k++] = arr2[j++];

	return rez;
}

static void merge(int *arr, int l, int m, int r)
{
	int i, j, k;
	int n1 = m - l + 1;
	int n2 = r - m;

	// Create temp arrays
	int L[n1], R[n2];

	// Copy data to temp arrays L[] and R[]
	for (i = 0; i < n1; i++)
		L[i] = arr[l + i];
	for (j = 0; j < n2; j++)
		R[j] = arr[m + 1 + j];

	// Merge the temp arrays back into arr[l..r]
	i = 0;
	j = 0;
	k = l;
	while (i < n1 && j < n2)
	{
		if (L[i] <= R[j])
			arr[k] = L[i++];
		else
			arr[k] = R[j++];
		k++;

		coro_yield(); // yield after every cycle iteration
	}

	while (i < n1)
	{
		arr[k++] = L[i++];
		coro_yield(); // yield after every cycle iteration
	}

	while (j < n2)
	{
		arr[k++] = R[j++];
		coro_yield(); // yield after every cycle iteration
	}
}

static void mergeSort(int *arr, int l, int r)
{
	if (l < r)
	{
		int m = l + (r - l) / 2;

		// Sort first and second halves
		mergeSort(arr, l, m);
		mergeSort(arr, m + 1, r);

		merge(arr, l, m, r);
	}
}

/**
 * A function, called from inside of coroutines recursively. Just to demonstrate
 * the example. You can split your code into multiple functions, that usually
 * helps to keep the individual code blocks simple.
 */
// static void
// other_function(const char *name, int depth)
// {
// 	printf("%s: entered function, depth = %d\n", name, depth);
// 	coro_yield();
// 	if (depth < 3)
// 		other_function(name, depth + 1);
// }

/**
 * Coroutine body. This code is executed by all the coroutines. Here you
 * implement your solution, sort each individual file.
 */
static void *
coroutine_func_f(void *context)
{
	/* IMPLEMENT SORTING OF INDIVIDUAL FILES HERE. */

	struct coro *this = coro_this();


	char *name = ((struct Context *)context)->name;
	int l = ((struct Context *)context)->l;
	int r = ((struct Context *)context)->r;
	int *arr = ((struct Context *)context)->arr;

	//printf("Started coroutine %s\n", name);

	mergeSort(arr, l, r);

	// printf("%s: switch count %lld\n", name, coro_switch_count(this));
	// other_function(name, 1);
	// printf("%s: switch count after other function %lld\n", name,
	//        coro_switch_count(this));
	printf("%s finished: switch count %lld, ", name, coro_switch_count(this));

	free(name);
	/* This will be returned from coro_status(). */
	return context;
}

int main(int argc, char **argv)
{
	struct timespec start, stop;
	clock_gettime(CLOCK_MONOTONIC, &start);
	int n = argc - 2;
	double target_latency = (double)atoi(argv[1])/1000000.0;
	/* Data initialization */
	FILE **files = (FILE **)malloc((n) * sizeof(FILE *));
	int **data = (int **)malloc((n) * sizeof(int *));
	int *sizes = (int *)malloc((n) * sizeof(int));
	/* Reading data from files */
	for (int i = 0; i < n; ++i)
	{
		files[i] = fopen(argv[i + 2], "r");
		if (files[i] == NULL)
		{
			printf("Error: no such file exists");
			exit(-1);
		}

		data[i] = malloc(100000 * sizeof(int));
		sizes[i] = 0;
		while (fscanf(files[i], "%d", &data[i][sizes[i]++]) == 1)
		{
		}
		--sizes[i];

		fclose(files[i]);
	}
	free(files);
	/* Initialize our coroutine global cooperative scheduler. */
	coro_sched_init();
	/* Start several coroutines. */
	struct Context **conarr = malloc(n * sizeof(struct Context *));
	for (int i = 0; i < n; ++i)
	{
		/*
		 * The coroutines can take any 'void *' interpretation of which
		 * depends on what you want. Here as an example I give them
		 * some names.
		 */
		char name[16];
		sprintf(name, "coro_%d", i);
		/*
		 * I have to copy the name. Otherwise all the coroutines would
		 * have the same name when they finally start.
		 */
		conarr[i] = malloc(sizeof(struct Context));
		conarr[i]->arr = data[i];
		conarr[i]->name = strdup(name);
		conarr[i]->l = 0;
		conarr[i]->r = sizes[i] - 1;
		conarr[i]->coro_time_frame = target_latency/(double)n;
		coro_new(coroutine_func_f, conarr[i]);
	}
	/* Wait for all the coroutines to end. */
	struct coro *c;
	while ((c = coro_sched_wait()) != NULL)
	{
		/*
		 * Each 'wait' returns a finished coroutine with which you can
		 * do anything you want. Like check its exit status, for
		 * example. Don't forget to free the coroutine afterwards.
		 */
		free(coro_status(c));
		printf("work time: %lf\n", coro_work_time(c));
		coro_delete(c);
	}
	free(conarr);
	/* All coroutines have finished. */

	/* IMPLEMENT MERGING OF THE SORTED ARRAYS HERE. */
	for (int i = 0; i < n - 1; ++i)
	{
		int *tmp = data_merge(data[i], sizes[i], data[i + 1], sizes[i + 1]);
		free(data[i]);
		free(data[i + 1]);
		data[i + 1] = tmp;
		sizes[i + 1] += sizes[i];
	}
	FILE *output = fopen("output.txt", "w");
	for (int i = 0; i < sizes[n - 1]; ++i)
	{
		fprintf(output, "%d ", data[n - 1][i]);
	}
	free(data[n - 1]);
	free(data);
	free(sizes);
	clock_gettime(CLOCK_MONOTONIC, &stop);
	printf("Full work time: %lf\n", time_diff(&start, &stop));
	return 0;
}
