//
//  openCLNR.cpp
//  OpenCL Example1
//
//  Created by Rasmusson, Jim on 18/03/13.
//
//  Copyright (c) 2013, Sony Mobile Communications AB
//  All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Sony Mobile Communications AB nor the
//       names of its contributors may be used to endorse or promote products
//       derived from this software without specific prior written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
//  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
//  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#define __CL_ENABLE_EXCEPTIONS

#include "openCLNR.h"
#include "hassproc.h"


inline std::string loadProgram(std::string input)
{
	std::ifstream stream(input.c_str());
	if (!stream.is_open()) {
		LOGE("Cannot open input file\n");
		exit(1);
	}
	return std::string( std::istreambuf_iterator<char>(stream),
						(std::istreambuf_iterator<char>()));
}

void openCLNR (unsigned char* bufIn, unsigned char* bufOut, int* info)
{

	LOGI("\n\nStart openCLNR (i.e., OpenCL on the GPU)");
    int xOffset=3;
    int yOffset=7;
    unsigned short blackLevel=60;


	int width = info[0];
	int height = info[1];
	int hass_iwidth=width;
    int hass_iheight=height;
	unsigned int imageSize = width * height * 4 * sizeof(cl_uchar);

	cl_int err = CL_SUCCESS;
	try {

		std::vector<cl::Platform> platforms;
		cl::Platform::get(&platforms);
		if (platforms.size() == 0) {
			std::cout << "Platform size 0\n";
			return;
		}

		cl_context_properties properties[] =
		{ CL_CONTEXT_PLATFORM, (cl_context_properties)(platforms[0])(), 0};
		cl::Context context(CL_DEVICE_TYPE_GPU, properties);

		std::vector<cl::Device> devices = context.getInfo<CL_CONTEXT_DEVICES>();
		cl::CommandQueue queue(context, devices[0], 0, &err);

		std::string kernelSource = loadProgram("/data/data/com.sony.openclexample1/app_execdir/bilateralKernel.cl");

		cl::Program::Sources source(1, std::make_pair(kernelSource.c_str(), kernelSource.length()+1));
		cl::Program program(context, source);
		const char *options = "-cl-fast-relaxed-math";
		program.build(devices, options);

        //PreKernel;
        size_t globalSize[2]  = {(size_t)(width), (size_t)height};
        size_t *localSize     = NULL;

		cl::Kernel prekernel(program, "PreKernel", &err);
		cl::Buffer bufferIn = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, imageSize, (void *) &bufIn[0], &err);
		cl::Buffer bufferOut = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, imageSize, (void *) &bufOut[0], &err);

		int hass_channel =16, hass_spectrum = 4;

        int d_recon_coef_sz = hass_channel * hass_spectrum * sizeof(float);
        float *hass_recon_coef = new float[hass_channel * hass_spectrum];
        cl::Buffer d_recon_coef = cl::Buffer(context, CL_MEM_READ_WRITE | CL_MEM_USE_HOST_PTR, d_recon_coef_sz, (void *)&hass_recon_coef[0], &err);
        cl::Buffer d_sat_map;

		prekernel.setArg(0,bufferIn);
		prekernel.setArg(1,bufferOut);
        prekernel.setArg(2,sizeof(xOffset),((void *)(&xOffset)));

        prekernel.setArg(3,sizeof(yOffset),((void *)(&yOffset)));

        prekernel.setArg(4,sizeof(blackLevel) ,((void *)(&blackLevel)));

        prekernel.setArg(5,sizeof(hass_iwidth),((void *)(&hass_iwidth)));

		prekernel.setArg(6,sizeof(hass_iheight),((void *)(&hass_iheight)));
		queue.enqueueNDRangeKernel(prekernel, cl::NullRange, cl::NDRange(
				width, height), cl::NullRange, 0, NULL);

        //Noise reduction
		cl::Kernel kernel(program, "bilateralFilterKernel", &err);

		kernel.setArg(0,bufferIn);
		kernel.setArg(1,bufferOut);
		kernel.setArg(2,width);
		kernel.setArg(3,height);

		cl::Event event;

		clock_t startTimer1, stopTimer1;
		startTimer1=clock();

// 		one time
		queue.enqueueNDRangeKernel(	kernel,
				cl::NullRange,
				cl::NDRange(width,height),
				cl::NullRange,
				NULL,
				&event);

//		swap in and out buffer pointers and run a 2nd time
		kernel.setArg(0,bufferOut);
		kernel.setArg(1,bufferIn);
		queue.enqueueNDRangeKernel(	kernel,
				cl::NullRange,
				cl::NDRange(width,height),
				cl::NullRange,
				NULL,
				&event);

//		swap in and out buffer pointers and run a 3rd time
		kernel.setArg(0,bufferIn);
		kernel.setArg(1,bufferOut);
		queue.enqueueNDRangeKernel(	kernel,
				cl::NullRange,
				cl::NDRange(width,height),
				cl::NullRange,
				NULL,
				&event);


        //Reduction
        int hass_raw_input_bit =10, hass_sat_th;
        hass_sat_th = (1 << hass_raw_input_bit) - 1 -blackLevel -1;
        float hass_sat_val = (float)hass_sat_th;

        cl::Kernel recon_kernel(program, "ReconKernel", &err);
        recon_kernel.setArg(0, bufferOut);
        recon_kernel.setArg(1, bufferIn);
        recon_kernel.setArg(2, d_recon_coef);
        recon_kernel.setArg(3, d_sat_map);
        recon_kernel.setArg(4, hass_sat_val);
        recon_kernel.setArg(5, width);
        recon_kernel.setArg(6, height);

		cl_int dimSiz   = 2;
        queue.enqueueNDRangeKernel(recon_kernel,dimSiz,cl::NDRange(width,height),cl::NDRange(128,8), NULL, &event);

        //		swap in and out buffer pointers and run a 3rd time
        kernel.setArg(0,bufferIn);
        kernel.setArg(1,bufferOut);
        queue.enqueueNDRangeKernel(	kernel,
                                       cl::NullRange,
                                       cl::NDRange(width,height),
                                       cl::NullRange,
                                       NULL,
                                       &event);

		queue.finish();

		stopTimer1 = clock();
		double elapse = 1000.0* (double)(stopTimer1 - startTimer1)/(double)CLOCKS_PER_SEC;
		info[2] = (int)elapse;
		LOGI("OpenCL code on the GPU took %g ms\n\n", 1000.0* (double)(stopTimer1 - startTimer1)/(double)CLOCKS_PER_SEC) ;

		queue.enqueueReadBuffer(bufferOut, CL_TRUE, 0, imageSize, bufOut);
	}
	catch (cl::Error err) {
		LOGE("ERROR: %s\n", err.what());
	}
	return;
}
