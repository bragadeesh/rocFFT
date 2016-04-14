
#define __HIPCC__

#if defined(__NVCC__)
#include "helper_math.h"
#endif

#include <hip_runtime.h>
#include "rocfft.h"

struct rocfft_plan_t
{
	size_t rank;
	size_t lengths[3];
	size_t batch;
};

struct rocfft_buffer_t
{
	bool deviceAlloc;
	size_t elementSize;
	void *p;
};

rocfft_status rocfft_plan_create(	rocfft_plan *plan,
					rocfft_transform_type transform_type, rocfft_precision precision,
					size_t dimensions, const size_t *lengths, size_t number_of_transforms,
					const rocfft_description *description )
{
	rocfft_plan p = new rocfft_plan_t;
	p->rank = dimensions;
	
	for(size_t i=0; i<(p->rank); i++)
		p->lengths[i] = lengths[i];

	p->batch = number_of_transforms;

	*plan = p;

	return rocfft_status_success;
}

rocfft_status rocfft_plan_destroy( rocfft_plan plan )
{
	delete plan;

	return rocfft_status_success;	
}

rocfft_status rocfft_buffer_create_with_alloc( rocfft_buffer *buffer, rocfft_element_type element_type, size_t size_in_elements )
{
	rocfft_buffer b = new rocfft_buffer_t;

	switch(element_type)
	{
	case rocfft_element_type_complex_single: 	b->elementSize =  8; break;
	case rocfft_element_type_complex_double:	b->elementSize = 16; break;
	case rocfft_element_type_single:		b->elementSize =  4; break;
	case rocfft_element_type_double:		b->elementSize =  8; break;	
	default:					b->elementSize =  1; break;
	}

	hipMalloc(&(b->p), size_in_elements*(b->elementSize));
	b->deviceAlloc = true;
	*buffer = b;

	return rocfft_status_success;
}

rocfft_status rocfft_buffer_destroy( rocfft_buffer buffer )
{
	if(buffer->deviceAlloc)
		hipFree(buffer->p);

	delete buffer;

	return rocfft_status_success;
}

rocfft_status rocfft_buffer_create_with_ptr( rocfft_buffer *buffer, void *p )
{
	rocfft_buffer b = new rocfft_buffer_t;
	b->p = p;
	b->deviceAlloc = false;
	b->elementSize =  1;
	*buffer = b;

	return rocfft_status_success;	
}

rocfft_status rocfft_buffer_get_ptr( rocfft_buffer buffer, void **p )
{
	*p = buffer->p;
	return rocfft_status_success;
}

// ===============================================================

__device__ void FwdRad4B1(float2 *R0, float2 *R2, float2 *R1, float2 *R3)
{

	float2 T;

	(*R1) = (*R0) - (*R1);
	(*R0) = 2.0f * (*R0) - (*R1);
	(*R3) = (*R2) - (*R3);
	(*R2) = 2.0f * (*R2) - (*R3);

	(*R2) = (*R0) - (*R2);
	(*R0) = 2.0f * (*R0) - (*R2);

        float2 Temp;
        Temp.x = -(*R3).y;
        Temp.y = (*R3).x;
	(*R3) = (*R1) + Temp;
	(*R1) = 2.0f * (*R1) - (*R3);

	T = (*R1); (*R1) = (*R2); (*R2) = T;

}


__device__ void FwdPass0(uint me, uint inOffset, uint outOffset,
	float2 *bufIn, float *bufOutRe, float *bufOutIm,
	float2 *R0, float2 *R1, float2 *R2, float2 *R3)
{
	
	(*R0) = bufIn[inOffset + (0 + me * 1 + 0 + 0) * 1];
	(*R1) = bufIn[inOffset + (0 + me * 1 + 0 + 4) * 1];
	(*R2) = bufIn[inOffset + (0 + me * 1 + 0 + 8) * 1];
	(*R3) = bufIn[inOffset + (0 + me * 1 + 0 + 12) * 1];
	
	FwdRad4B1(R0, R1, R2, R3);

	__syncthreads();

	bufOutRe[outOffset + (((1 * me + 0) / 1) * 4 + (1 * me + 0) % 1 + 0) * 1] = (*R0).x;
	bufOutRe[outOffset + (((1 * me + 0) / 1) * 4 + (1 * me + 0) % 1 + 1) * 1] = (*R1).x;
	bufOutRe[outOffset + (((1 * me + 0) / 1) * 4 + (1 * me + 0) % 1 + 2) * 1] = (*R2).x;
	bufOutRe[outOffset + (((1 * me + 0) / 1) * 4 + (1 * me + 0) % 1 + 3) * 1] = (*R3).x;

	__syncthreads();

	(*R0).x = bufOutRe[outOffset + (0 + me * 1 + 0 + 0) * 1];
	(*R1).x = bufOutRe[outOffset + (0 + me * 1 + 0 + 4) * 1];
	(*R2).x = bufOutRe[outOffset + (0 + me * 1 + 0 + 8) * 1];
	(*R3).x = bufOutRe[outOffset + (0 + me * 1 + 0 + 12) * 1];

	__syncthreads();

	bufOutIm[outOffset + (((1 * me + 0) / 1) * 4 + (1 * me + 0) % 1 + 0) * 1] = (*R0).y;
	bufOutIm[outOffset + (((1 * me + 0) / 1) * 4 + (1 * me + 0) % 1 + 1) * 1] = (*R1).y;
	bufOutIm[outOffset + (((1 * me + 0) / 1) * 4 + (1 * me + 0) % 1 + 2) * 1] = (*R2).y;
	bufOutIm[outOffset + (((1 * me + 0) / 1) * 4 + (1 * me + 0) % 1 + 3) * 1] = (*R3).y;

	__syncthreads();

	(*R0).y = bufOutIm[outOffset + (0 + me * 1 + 0 + 0) * 1];
	(*R1).y = bufOutIm[outOffset + (0 + me * 1 + 0 + 4) * 1];
	(*R2).y = bufOutIm[outOffset + (0 + me * 1 + 0 + 8) * 1];
	(*R3).y = bufOutIm[outOffset + (0 + me * 1 + 0 + 12) * 1];

	__syncthreads();

}

__device__ void FwdPass1(uint me, uint inOffset, uint outOffset,
	float2 *bufOut, float2 *twiddles,
	float2 *R0, float2 *R1, float2 *R2, float2 *R3)
{
	
	{
		float2 W = twiddles[3 + 3 * ((1 * me + 0) % 4) + 0];
		float TR, TI;
		TR = (W.x * (*R1).x) - (W.y * (*R1).y);
		TI = (W.y * (*R1).x) + (W.x * (*R1).y);
		(*R1).x = TR;
		(*R1).y = TI;
	}

	{
		float2 W = twiddles[3 + 3 * ((1 * me + 0) % 4) + 1];
		float TR, TI;
		TR = (W.x * (*R2).x) - (W.y * (*R2).y);
		TI = (W.y * (*R2).x) + (W.x * (*R2).y);
		(*R2).x = TR;
		(*R2).y = TI;
	}

	{
		float2 W = twiddles[3 + 3 * ((1 * me + 0) % 4) + 2];
		float TR, TI;
		TR = (W.x * (*R3).x) - (W.y * (*R3).y);
		TI = (W.y * (*R3).x) + (W.x * (*R3).y);
		(*R3).x = TR;
		(*R3).y = TI;
	}

	FwdRad4B1(R0, R1, R2, R3);

	__syncthreads();

	bufOut[outOffset + (1 * me + 0 + 0) * 1 ] = (*R0);
	bufOut[outOffset + (1 * me + 0 + 4) * 1 ] = (*R1);
	bufOut[outOffset + (1 * me + 0 + 8) * 1 ] = (*R2);
	bufOut[outOffset + (1 * me + 0 + 12) * 1] = (*R3);


}


__global__ void fft_fwd(hipLaunchParm lp, float2 *gb, float2 *twiddles)
{
	uint me = hipThreadIdx_x;

	__shared__ float lds[16];

	float2 R0, R1, R2, R3;

	FwdPass0(me, 0, 0, gb, lds, lds, &R0, &R1, &R2, &R3);
	FwdPass1(me, 0, 0, gb, twiddles, &R0, &R1, &R2, &R3);
}


rocfft_status rocfft_execute(	rocfft_plan plan,
				rocfft_buffer *in_buffer,
				rocfft_buffer *out_buffer,
				rocfft_execution_info info )
{

	assert(plan->lengths[0] == 16);

	size_t N = 16;
	float2 twiddles[] = {
		{1.0000000000000000000000000000000000e+00f, -0.0000000000000000000000000000000000e+00f},
		{1.0000000000000000000000000000000000e+00f, -0.0000000000000000000000000000000000e+00f},
		{1.0000000000000000000000000000000000e+00f, -0.0000000000000000000000000000000000e+00f},
		{1.0000000000000000000000000000000000e+00f, -0.0000000000000000000000000000000000e+00f},
		{1.0000000000000000000000000000000000e+00f, -0.0000000000000000000000000000000000e+00f},
		{1.0000000000000000000000000000000000e+00f, -0.0000000000000000000000000000000000e+00f},
		{9.2387953251128673848313610506011173e-01f, -3.8268343236508978177923268049198668e-01f},
		{7.0710678118654757273731092936941423e-01f, -7.0710678118654757273731092936941423e-01f},
		{3.8268343236508983729038391174981371e-01f, -9.2387953251128673848313610506011173e-01f},
		{7.0710678118654757273731092936941423e-01f, -7.0710678118654757273731092936941423e-01f},
		{6.1232339957367660358688201472919830e-17f, -1.0000000000000000000000000000000000e+00f},
		{-7.0710678118654746171500846685376018e-01f, -7.0710678118654757273731092936941423e-01f},
		{3.8268343236508983729038391174981371e-01f, -9.2387953251128673848313610506011173e-01f},
		{-7.0710678118654746171500846685376018e-01f, -7.0710678118654757273731092936941423e-01f},
		{-9.2387953251128684950543856757576577e-01f, 3.8268343236508967075693021797633264e-01f},
		{ 1.0000000000000000000000000000000000e+00f, -0.0000000000000000000000000000000000e+00f },
	};

	size_t Nbytes = 16 * sizeof(float2);

	float2 *tw;
	hipMalloc(&tw, Nbytes);
	hipMemcpy(tw, &twiddles[0], Nbytes, hipMemcpyHostToDevice);

	const unsigned blocks = 1;
	const unsigned threadsPerBlock = 4;

	// Launch HIP kernel
	hipLaunchKernel(HIP_KERNEL_NAME(fft_fwd), dim3(blocks), dim3(threadsPerBlock), 0, 0, (float2 *)(in_buffer[0]->p), tw);


	return rocfft_status_success;
}


