#ifndef __HSA_BITONIC_SORT__
#define __HSA_BITONIC_SORT__


#include "utilities.h"
#include <vector>

#include "hsa.h"
#include "hsa_ext_finalize.h"
#include "utilities.h"

#include <string.h>
#include<iostream>
using namespace std;

#include "hsa_base_test.h"


#define  GROUP_SIZE 256 


class BitonicSort : public HSA_TEST
{
	public:
		BitonicSort();
		~BitonicSort();

	public:
		void SetStages(uint num_of_stage, uint pass_of_stage);
		int FillRandom(uint * arrayPtr, const int width, const int height, const uint rangeMin, const uint rangeMax, unsigned int seed);
		void RunKernels();
		int VerifyResults();
		void BitonicSortCPUReference(uint * input, const uint length, const bool sortIncreasing) ;
		void SwapIfFirstIsGreater(uint *a, uint *b);
		void InitlizeData();
		void Clean();

	public:
		int length;
		int sort_flag;
		uint* verification_input;

		uint* input_array;
		uint* outBuffer_;
		unsigned int width_;
		unsigned int height_;
		unsigned int bufSize_;
		unsigned int blockSize_;
		//static const unsigned int MAX_ITERATIONS = 50;
};


#endif

