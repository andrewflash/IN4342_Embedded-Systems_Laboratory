-------------------------------------------------
IN4342 Embedded Systems Laboratory (2015-2016 Q4)
README FILE - ASSIGMENT 2 - GROUP 01
-------------------------------------------------

------------
1. Makefile
------------
In order to compile different scenarios, you have to change DSP and or GPP makefile.
For some scenarios, you are required to change both DSP and GPP makefile.
Here are the list of scenarios that can be changed through makefile.

------------------------------
--- Path to compiler
------------------------------
Before compiling your source code, please check path to your compiler.
If you use VM provided from the lab, use these line:

	In GPP makefile:
		BASE_TOOLCHAIN := /data/usr/local/share/codesourcery/
		DSPLINK := /data/usr/local/share/bbframework/platform/beagle-linux/tools/dsplink_linux_1_65_00_03

	In DSP makefile:
		BASE_TOOLCHAIN := /data/usr/local/share
		DSPLINK := /data/usr/local/share/bbframework/platform/beagle-linux/tools/dsplink_linux_1_65_00_03

If you use remote server connected to the lab, use these lines: 

	In GPP makefile:
		BASE_TOOLCHAIN := /data/bbToolChain/usr/local/share/codesourcery/
		DSPLINK := /data/bbToolChain/usr/local/share/bbframework/platform/beagle-linux/tools/dsplink_linux_1_65_00_03		

	In DSP makefile:
		BASE_TOOLCHAIN := /data/bbToolChain/usr/local/share
		DSPLINK := /data/bbToolChain/usr/local/share/bbframework/platform/beagle-linux/tools/dsplink_linux_1_65_00_03		

----------------------------------
--- Auto compile both DSP and GPP
----------------------------------
We provide a shell script, i.e. 'makecompile.sh' to easily compile both DSP and GPP.

-------------------------------------
1.0. Default Scenario (Optimal)
-------------------------------------
The default scenario uses configuration as follow:

	In GPP:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DFRACTION=38

	In DSP:
		CFLAGS := -I$(BASE_BSL)/dsp/include -DP__ROFILE -mw -DFRACTION=38

It means that we allow 38% of image to be processed using NEON and 62% using DSP in parallel. 
The rest of compiler options are required for NEON to run effectively.
Note that we split the process only for gaussian_smooth function. The rest are executed using GPP/NEON.

-------------------------
1.1. Using Baseline Only
-------------------------
Add "-DBASELINE" in your GPP makefile.
You can test baseline version by using the following configuration:

	In GPP:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DBASELINE

Note that it is not necessary to configure DSP makefile. Also, the BASELINE definition works regardless of FRACTION.
Therefore, it is not necessary to delete the FRACTION definition.

--------------------
1.2. Using DSP Only
--------------------
To execute gaussian_smooth using DSP only, use "-DFRACTION=0" both in DSP and GPP makefile.

	In GPP:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DFRACTION=0

	In DSP:
		CFLAGS := -I$(BASE_BSL)/dsp/include -DP__ROFILE -mw -DFRACTION=0

---------------------
1.3. Using NEON Only
---------------------
To execute gaussian_smooth using NEON only, use "-DFRACTION=100" in GPP.
It is not necessary to change FRACTION in DSP because the functions related to DSP will not be executed.
Thus, there will be no communication between GPP and DSP.

	In GPP:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DFRACTION=100

---------------------------
1.4. Using Custom Workload
---------------------------
To provide custom workload on GPP and DSP, use "-DFRACTION=xx", where 'xx' represents the percentage of workload
taken by NEON. The value should be between 1 - 99. You have to modify both GPP and DSP makefile to use this scenario.
Note that the value in DSP should be the same as in GPP. It is not a complement to each other.

	In GPP:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DFRACTION=40

	In DSP:
		CFLAGS := -I$(BASE_BSL)/dsp/include -DP__ROFILE -mw -DFRACTION=40

------------------------------
1.5. Shared Memory Allocation
------------------------------
This scenario is used to check the effect of shared memory (pool) allocation on the execution time.
Using '-DFULL_BUFFER' definition, we set the size of allocated memory equal to the full image size although we enabled
load sharing that is indicated by the value of FRACTION. 
If we do not define '-DFULL_BUFFER', the size of allocated memory is equal to the fraction of image size
sent to DSP, which depends on the FRACTION value. You only need to configure GPP makefile.

	In GPP:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DFRACTION=38 -DFULL_BUFFER

------------------------------------------------------------------
1.6. Using different data types for Gaussian smooth NEON function
------------------------------------------------------------------
There are 3 different data types that can be used to calculate gaussian_smooth function, namely:
	a. uint16
	b. uint32
	c. floating point

You can modify GPP makefile only. You do not need to configure DSP makefile.

	Using uint16:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DFRACTION=38 -DGAUSS_U16

	Using uint32:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DFRACTION=38 -DGAUSS_U32

	Using floating point:
		CFLAGS := -O3 -mfpu=neon -ftree-vectorize -mfloat-abi=softfp -funsafe-math-optimizations -DFRACTION=38 -DGAUSS_FLOAT

The default configuration uses uint32.

----------------------------------------
2. Executing the app on Beagleboard
----------------------------------------
After compiling the GPP and DSP, you can copy the 'pool_notify' file in GPP (Release) and 'pool_notify.out' file in DSP (Release)
to Beagleboard.

We provide a shell script, i.e. 'makesend.sh' to easily send the required files to Beagleboard. You have to configure the destination
path of the board in 'makesend.sh'. Note that this script will run only in VM, not in the server.

To run the application in Beagleboard, copy 'run.sh' to the application directory in the board. You need to configure the script if
you want to change the picture.

We also provide a shell script, i.e. 'run-loop.sh' to run the application several times. The results will be saved in folder 'results'.
The parameters of this script are:
	
	./run-loop.sh path_to_pic num_iterations

Example (process klomp.pgm image 10 times):

	./run-loop.sh pics/klomp.pgm 10

--------------------------------------------------------------------------------------------------------------------------------------