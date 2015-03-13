#include "bitonic_sort.h"


BitonicSort::BitonicSort()
{

}

BitonicSort::~BitonicSort()
{

}

void BitonicSort::InitlizeData()
{
	sort_flag = 1;
	length = 256;
	verification_input = (uint *) malloc(length * sizeof(uint));

	int input_size_bytes = length * sizeof(uint);
	input_array = (uint*)malloc(input_size_bytes); 
	FillRandom(input_array, length, 1, 0, 255, 16); 
	memcpy(verification_input, input_array, length * sizeof(uint));
}


int BitonicSort::FillRandom(uint * arrayPtr, const int width, const int height, const uint rangeMin, const uint rangeMax, unsigned int seed)
{
	if(!arrayPtr)
	{
		printf("Cannot fill array. NULL pointer.\n");
		return -1;
	}

	if(!seed)
		seed = (unsigned int)time(NULL);

	srand(seed);
	double range = double(rangeMax - rangeMin) + 1.0; 

	/* random initialisation of input */
	for(int i = 0; i < height; i++)
		for(int j = 0; j < width; j++)
		{
			int index = i*width + j;
			arrayPtr[index] = rangeMin + (uint)(range*rand()/(RAND_MAX + 1.0)); 
		}

	return 0;
}



int BitonicSort::VerifyResults()
{
	BitonicSortCPUReference(verification_input, length, sort_flag); 

#ifdef DEBUG
	for (int i=0; i<length; ++i)
	{
		printf("%d ", verification_input[i]);
	}

	printf("\n\n");
#endif

	// compare the results and see if they match
	if(memcmp(input_array, verification_input, length*sizeof(uint)) == 0)
	{
		cout<<"PASSED!\n" << endl;
		return 0;
	}
	else
	{
		cout<<"FAILED\n" << endl;
		return -1;
	}

	return 0;
}

void BitonicSort::BitonicSortCPUReference(uint * input, const uint length, const bool sortIncreasing) 
{
	const uint halfLength = length/2;

	uint i;
	for(i = 2; i <= length; i *= 2) 
	{
		uint j;
		for(j = i; j > 1; j /= 2) 
		{
			bool increasing = sortIncreasing;
			const uint half_j = j/2;

			uint k;
			for(k = 0; k < length; k += j) 
			{
				const uint k_plus_half_j = k + half_j;
				uint l;

				if(i < length) 
				{
					if((k == i) || ((k % i) == 0) && (k != halfLength)) 
						increasing = !increasing;
				}

				for(l = k; l < k_plus_half_j; ++l) 
				{
					if(increasing)
						SwapIfFirstIsGreater(&input[l], &input[l + half_j]);
					else
						SwapIfFirstIsGreater(&input[l + half_j], &input[l]);
				}
			}
		}
	}
}

void BitonicSort::SwapIfFirstIsGreater(uint *a, uint *b)
{
	if(*a > *b) 
	{
		uint temp = *a;
		*a = *b;
		*b = temp;
	}
}

void BitonicSort::RunKernels()
{	
	int num_of_stages = 0;

	for(uint temp = length; temp > 1; temp >>= 1)
		++num_of_stages;

	for(int stage = 0; stage < num_of_stages; ++stage) 
	{
		// Every stage has stage + 1 passes
		for(int pass_of_stage = 0; pass_of_stage < stage + 1; ++pass_of_stage) 
		{ 
			SetStages(stage, pass_of_stage);
		}
	}    
}

void BitonicSort::Clean()
{
	free(input_array);
	input_array = NULL;
	free(verification_input);
	verification_input = NULL;
}


void BitonicSort::SetStages(uint num_of_stage, uint pass_of_stage)
{
	struct __attribute__ ((aligned(HSA_ARGUMENT_ALIGN_BYTES))) args_t {
		uint* offset_0;
		uint* offset_1;
		uint* offset_2;
		uint* printf_buffer;
		uint* vqueue_buffer;
		uint* aqlwrap_pointer;			  

		uint* inpu_array; 
		uint stage;
		uint pass_of_stage;
		uint direction;
	} local_args;


	local_args.offset_0 = 0;
	local_args.offset_1 = 0;
	local_args.offset_2 = 0;
	local_args.printf_buffer = 0;
	local_args.vqueue_buffer = 0;
	local_args.aqlwrap_pointer = 0;    

	local_args.inpu_array = input_array;
	local_args.stage =  num_of_stage;
	local_args.pass_of_stage = pass_of_stage;
	local_args.direction = sort_flag;
	///////////////////////

	int group_x = GROUP_SIZE;
	int group_y = 1;
	int group_z = 1;
	int group_size = 0;
	int grid_x = length / 2 ;
	int grid_y = 1;
	int grid_z = 1;
	int kernel_size = sizeof(args_t);

	Run(1, group_x, group_y, group_z, group_size, grid_x, grid_y, grid_z, &local_args, kernel_size);
}

int main(int argc, char *argv[])
{
	//char file_name[128] = "bitonic_sort_kernel.brig";
	char file_name[128] = "bitonic_sort_kernel.hsail";
	char kernel_name[128] = "&__OpenCL_bitonicSort_kernel";
	BitonicSort bitonic;
	//bitonic.SetBrigFileAndKernelName(file_name, kernel_name);
	bitonic.GetHsailNameAndKernelName(file_name, kernel_name);
	bitonic.InitlizeData();
	bitonic.HsaInit();

	bitonic.RunKernels();
	bitonic.VerifyResults();
	bitonic.Clean();
	bitonic.Close(); 
       return 0;
}


