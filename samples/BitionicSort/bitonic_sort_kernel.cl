__kernel void bitonicSort(__global uint * theArray, const uint stage, const uint passOfStage, const uint direction)
{
	uint sortIncreasing = direction;
	uint threadId = get_global_id(0);

	uint pairDistance = 1 << (stage - passOfStage);
	uint blockWidth   = 2 * pairDistance;

	uint leftId = (threadId % pairDistance) + (threadId / pairDistance) * blockWidth;

	uint rightId = leftId + pairDistance;

	uint leftElement = theArray[leftId];
	uint rightElement = theArray[rightId];

	uint sameDirectionBlockWidth = 1 << stage;

	if((threadId/sameDirectionBlockWidth) % 2 == 1)
		sortIncreasing = 1 - sortIncreasing;

	uint greater;
	uint lesser;
	if(leftElement > rightElement)
	{
		greater = leftElement;
		lesser  = rightElement;
	}
	else
	{
		greater = rightElement;
		lesser  = leftElement;
	}

	if(sortIncreasing)
	{
		theArray[leftId]  = lesser;
		theArray[rightId] = greater;
	}
	else
	{
		theArray[leftId]  = greater;
		theArray[rightId] = lesser;
	}
}

