#include <stdlib.h>
#include <stdio.h>

#include <semaphore.h>
/*  ----------------------------------- DSP/BIOS Link                   */
#include <dsplink.h>

/*  ----------------------------------- DSP/BIOS LINK API               */
#include <proc.h>
#include <pool.h>
#include <mpcs.h>
#include <notify.h>
#if defined (DA8XXGEM)
#include <loaderdefs.h>
#endif

/*  ----------------------------------- Application Header              */
#include <pool_notify.h>

/* ------------------------------------ Custom header, canny edge app   */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// Canny edge
#include "canny_edge.h"
#include "timer.h"

// Verbose mode
#define VERBOSE 0

// Timing mode
#define TIMING

// Neon
#define NEON

// Do verification
#define VERIFY

// Fixed point
#define FIXED_POINT

#ifdef NEON
#include <arm_neon.h>
#include "neon_func.h"
#endif

#define BOOSTBLURFACTOR 90.0

#if !defined(FRACTION)
#define FRACTION 38     // Load sharing on GPP (Fraction should be 1 - 99)
#endif

// Notify to DSP
#define NOTIFY_DSP 1

// PRINTF SPACE, RIGHT PADDING
#define SPACE 20

/* ------------------------------- end custom header ------------------ */

#if defined (__cplusplus)
extern "C" {
#endif /* defined (__cplusplus) */


/*  ============================================================================
 *  @const   NUM_ARGS
 *
 *  @desc   Number of arguments specified to the DSP application.
 *  ============================================================================
 */
#define NUM_ARGS                       1

/** ============================================================================
 *  @name   SAMPLE_POOL_ID
 *
 *  @desc   ID of the POOL used for the sample.
 *  ============================================================================
 */
#define SAMPLE_POOL_ID                 0

/** ============================================================================
 *  @name   NUM_BUF_SIZES
 *
 *  @desc   Number of buffer pools to be configured for the allocator.
 *  ============================================================================
 */
#define NUM_BUF_SIZES                  1

/** ============================================================================
 *  @const  NUM_BUF_POOL0
 *
 *  @desc   Number of buffers in first buffer pool.
 *  ============================================================================
 */
#define NUM_BUF_POOL0                  1

/*  ============================================================================
 *  @const   pool_notify_INVALID_ID
 *
 *  @desc   Indicates invalid processor ID within the pool_notify_Ctrl structure.
 *  ============================================================================
 */
#define pool_notify_INVALID_ID            (Uint32) -1

/** ============================================================================
 *  @const  pool_notify_IPS_ID
 *
 *  @desc   The IPS ID to be used for sending notification events to the DSP.
 *  ============================================================================
 */
#define pool_notify_IPS_ID                0

/** ============================================================================
 *  @const  pool_notify_IPS_EVENTNO
 *
 *  @desc   The IPS event number to be used for sending notification events to
 *          the DSP.
 *  ============================================================================
 */
#define pool_notify_IPS_EVENTNO           5


/*  ============================================================================
 *  @name   pool_notify_BufferSize
 *
 *  @desc   Size of buffer to be used for data transfer.
 *  ============================================================================
 */
STATIC Uint32  pool_notify_BufferSize ;

/*  ============================================================================
 *  @name   pool_notify_NumIterations
 *
 *  @desc   Number of iterations of data transfer.
 *  ============================================================================
 */
#if (FRACTION != 100) && !defined(BASELINE)	
STATIC Uint32  pool_notify_NumIterations ;
#endif

/** ============================================================================
 *  @name   pool_notify_DataBuf
 *
 *  @desc   Pointer to the shared data buffer used by the pool_notify sample
 *          application.
 *  ============================================================================
 */
Uint8 * pool_notify_DataBuf = NULL;
Uint16 * DataBuf16 = NULL;


/** ============================================================================
 *  @func   pool_notify_Notify
 *
 *  @desc   This function implements the event callback registered with the
 *          NOTIFY component to receive notification indicating that the DSP-
 *          side application has completed its setup phase.
 *
 *  @arg    eventNo
 *              Event number associated with the callback being invoked.
 *  @arg    arg
 *              Fixed argument registered with the IPS component along with
 *              the callback function.
 *  @arg    info
 *              Run-time information provided to the upper layer by the NOTIFY
 *              component. This information is specific to the IPS being
 *              implemented.
 *
 *  @ret    None.
 *
 *  @enter  None.
 *
 *  @leave  None.
 *
 *  @see    None.
 *  ============================================================================
 */
STATIC Void pool_notify_Notify (Uint32 eventNo, Pvoid arg, Pvoid info) ;


/** ============================================================================
 *  Custom function prototype - canny edge
 *  ============================================================================ 
 */

void unit_init(void);

void canny_rest(float sigma, unsigned short *smoothedIm);

#ifdef VERIFY
void verify_results(unsigned char *image, int rows, int cols, float sigma,
           float tlow, float thigh, unsigned char *edge);
#endif

/*  ====================================== end custom prototype ================ 
*/

sem_t sem;

 // Add timing functions
#ifdef TIMING
long long startTotalTime;                        // Start execution time
long long startTotalTimeBaseline;                // Start execution time baseline

long long startDspTime;                          // DSP execution time
long long startNeonTime;                         // Neon execution time
long long startAllocTime;                        // Memory allocation time
long long startAlgoTime;						 // Canny algorithm time
long long startCreateTime;						 // Pool create time
long long stopDspTime;
long long stopNeonTime;
long long stopAllocTime;
long long stopAlgoTime;
long long stopCreateTime;

long long totalTime;                            // Total execution time
long long totalTimeBaseline;                     // Total execution time baseline

long long startGaussBaseTime, stopGaussBaseTime;

long long startPoolOpen, stopPoolOpen;
long long startAllocBuffer, stopAllocBuffer;
long long startRegisterNotify, stopRegisterNotify;
long long startDspSetup, stopDspSetup;

long long startPoolInvdTime, stopPoolInvdTime;
long long startDerrivativeTime, stopDerrivativeTime;
long long startMagnitudeTime, stopMagnitudeTime;
long long startNonMaxTime, stopNonMaxTime;
long long startHysteresisTime, stopHysteresisTime;
#endif

/** ============================================================================
 *  Canny edge global variables
 *  ============================================================================ 
 */
unsigned char *image;           /* The input image */
unsigned char *edge = NULL;     /* The output edge image */
int rows, cols;                 /* The dimensions of the image. */
int rows_neon;                  /* Rows - Load sharing with neon */
Uint32 imageSize;               /* The image size, rows*cols */

char filename[32] = "";

char outfilename[128];    /* Name of the output "edge" image */
float sigma=2.5,              /* Standard deviation of the gaussian kernel. */
    tlow=0.5,               /* Fraction of the high threshold in hysteresis. */
    thigh=0.5;              /* High hysteresis threshold control. The actual
                threshold is the (100 * thigh) percentage point
                in the histogram of the magnitude of the
                gradient image that passes non-maximal
                suppression. */
unsigned short *smoothedIm = NULL;      /* Intermediate process, output of 
                                            gaussian smooth process */

/*  ============================= end canny edge global variables ==============
*/

/** ============================================================================
 *  @func   pool_notify_Create
 *
 *  @desc   This function allocates and initializes resources used by
 *          this application.
 *
 *  @modif  None
 *  ============================================================================
 */
NORMAL_API DSP_STATUS pool_notify_Create (	IN Char8 * dspExecutable,
											IN Char8 * strBufferSize,
											IN Uint8   processorId)
{
    DSP_STATUS      status     = DSP_SOK  ;
    Uint32          numArgs    = NUM_ARGS ;
    Void *          dspDataBuf = NULL ;
    Uint32          numBufs [NUM_BUF_SIZES] = {NUM_BUF_POOL0, } ;
    Uint32          size    [NUM_BUF_SIZES] ;
    SMAPOOL_Attrs   poolAttrs ;
    Char8 *         args [NUM_ARGS] ;

	#ifdef DEBUG
    printf ("Entered pool_notify_Create ()\n") ;
	#endif
 
    #ifdef TIMING
    startPoolOpen = get_usec();
    #endif

    sem_init(&sem,0,0);

    /*
     *  Create and initialize the proc object.
     */
    status = PROC_setup (NULL) ;

    /*
     *  Attach the Dsp with which the transfers have to be done.
     */
    if (DSP_SUCCEEDED (status)) 
	{
        status = PROC_attach (processorId, NULL) ;
        if (DSP_FAILED (status)) 
		{
            printf ("PROC_attach () failed. Status = [0x%x]\n", (int)status );
        }
    }
    else 
	{
        printf ("PROC_setup () failed. Status = [0x%x]\n", (int)status) ;
    }

    /*
     *  Open the pool.
     */
    if (DSP_SUCCEEDED (status)) 
	{
        size [0] = pool_notify_BufferSize ;
        poolAttrs.bufSizes      = (Uint32 *) &size ;
        poolAttrs.numBuffers    = (Uint32 *) &numBufs ;
        poolAttrs.numBufPools   = NUM_BUF_SIZES ;
        poolAttrs.exactMatchReq = TRUE ;
        status = POOL_open (POOL_makePoolId(processorId, SAMPLE_POOL_ID), &poolAttrs) ;
        if (DSP_FAILED (status)) 
		{
            printf ("POOL_open () failed. Status = [0x%x]\n", (int)status );
        }
    }

    #ifdef TIMING
    stopPoolOpen = startAllocBuffer = get_usec();
    #endif

    /*
     *  Allocate the data buffer to be used for the application.
     */
    if (DSP_SUCCEEDED (status)) 
	{
        status = POOL_alloc (POOL_makePoolId(processorId, SAMPLE_POOL_ID),
                             (Void **) &pool_notify_DataBuf,
                             pool_notify_BufferSize) ;

        /* Get the translated DSP address to be sent to the DSP. */
        if (DSP_SUCCEEDED (status)) 
		{
            status = POOL_translateAddr (
                                   POOL_makePoolId(processorId, SAMPLE_POOL_ID),
                                         &dspDataBuf,
                                         AddrType_Dsp,
                                         (Void *) pool_notify_DataBuf,
                                         AddrType_Usr) ;

            if (DSP_FAILED (status)) 
			{
                printf ("POOL_translateAddr () DataBuf failed."
                                 " Status = [0x%x]\n",
                                 (int)status) ;
            }
        }
        else 
		{
            printf ("POOL_alloc() DataBuf failed. Status = [0x%x]\n",(int)status);
        }
    }

    #ifdef TIMING
    stopAllocBuffer = startRegisterNotify = get_usec();
    #endif

    /*
     *  Register for notification that the DSP-side application setup is
     *  complete.
     */
    if (DSP_SUCCEEDED (status)) 
	{
        status = NOTIFY_register (processorId,
                                  pool_notify_IPS_ID,
                                  pool_notify_IPS_EVENTNO,
                                  (FnNotifyCbck) pool_notify_Notify,
                                  0/* vladms XFER_SemPtr*/) ;
        if (DSP_FAILED (status)) 
		{
            printf ("NOTIFY_register () failed Status = [0x%x]\n",
                             (int)status) ;
        }
    }

    #ifdef TIMING
    stopRegisterNotify = startDspSetup = get_usec();
    #endif

    /*
     *  Load the executable on the DSP.
     */
    if (DSP_SUCCEEDED (status)) {
        args [0] = strBufferSize ;
        {
            status = PROC_load (processorId, dspExecutable, numArgs, args) ;
        }

        if (DSP_FAILED (status)) {
            printf ("PROC_load () failed. Status = [0x%x]\n", (int)status) ;
        }
    }

    /*
     *  Start execution on DSP.
     */
    if (DSP_SUCCEEDED (status)) {
        status = PROC_start (processorId) ;
        if (DSP_FAILED (status)) {
            printf ("PROC_start () failed. Status = [0x%x]\n",
                             (int)status) ;
        }
    }

    /*
     *  Wait for the DSP-side application to indicate that it has completed its
     *  setup. The DSP-side application sends notification of the IPS event
     *  when it is ready to proceed with further execution of the application.
     */
    if (DSP_SUCCEEDED (status)) {
        // wait for initialization 
        sem_wait(&sem);
    }

    /*
     *  Send notifications to the DSP with information about the address of the
     *  control structure and data buffer to be used by the application.
     *
     */
    // Send the address of the allocated buffer on the shared memory
    status = NOTIFY_notify (processorId,
                            pool_notify_IPS_ID,
                            pool_notify_IPS_EVENTNO,
                            (Uint32) dspDataBuf);
    if (DSP_FAILED (status)) 
	{
        printf ("NOTIFY_notify () DataBuf failed."
                " Status = [0x%x]\n",
                 (int)status) ;
    }

    // Send the size of the allocated buffer on the shared memory
    status = NOTIFY_notify (processorId,
                            pool_notify_IPS_ID,
                            pool_notify_IPS_EVENTNO,
                            (Uint32) pool_notify_BufferSize);
    if (DSP_FAILED (status)) 
    {
        printf ("NOTIFY_notify () DataBuf failed."
                " Status = [0x%x]\n",
                 (int)status) ;
    }

	// Send image size to DSP (rows and cols), packed into one single notify message (info)
    status = NOTIFY_notify (processorId,
                            pool_notify_IPS_ID,
                            pool_notify_IPS_EVENTNO,
                            (Uint32)rows << 16 | cols);

    if (DSP_FAILED (status)) 
    {
        printf ("NOTIFY_notify () rows failed."
                " Status = [0x%x]\n",
                 (int)status) ;
    }

    #ifdef DEBUG
    printf("Rows and cols value in one info: %#10lx\n", (Uint32)rows << 16 | cols);   // gives 0xFFFFFFFF
    #endif

    // Send rows_neon to DSP to indicate starting point of 
    //   the gaussian_smooth process in DSP
    status = NOTIFY_notify (processorId,
                            pool_notify_IPS_ID,
                            pool_notify_IPS_EVENTNO,
                            (Uint32)rows_neon);

    if (DSP_FAILED (status)) 
    {
        printf ("NOTIFY_notify () rows failed."
                " Status = [0x%x]\n",
                 (int)status) ;
    }

    #ifdef DEBUG
    printf("Rows neon: %lu\n", (Uint32)rows_neon);
    #endif
	
	#ifdef DEBUG
    printf ("Leaving pool_notify_Create ()\n") ;
	#endif

    #ifdef TIMING
    stopDspSetup = get_usec();
    #endif

    return status ;
}

/** ============================================================================
 *  @func   pool_notify_Execute
 *
 *  @desc   This function implements the execute phase for this application.
 *
 *  @modif  None
 *  ============================================================================
 */
NORMAL_API DSP_STATUS pool_notify_Execute (IN Uint32 numIterations, Uint8 processorId)
{
    DSP_STATUS  status    = DSP_SOK ;

	#ifdef DEBUG
    printf ("Entered pool_notify_Execute ()\n") ;
	#endif

    // Fill DSP buffer with image data
    unit_init();

    POOL_writeback (POOL_makePoolId(processorId, SAMPLE_POOL_ID),
                    pool_notify_DataBuf,
                    pool_notify_BufferSize);

    #if (FRACTION == 0)	// Allocate smoothedIm if we only use DSP
    if(((smoothedIm) = (unsigned short int *) malloc(rows*cols*sizeof(unsigned short int))) == NULL)
    {
        fprintf(stderr, "Error allocating the smoothed image.\n");
        exit(1);
    }
    #endif

    #ifdef TIMING
    stopAllocTime = startAlgoTime = get_usec();
    startDspTime = get_usec();
    #endif

    // Notify the DSP to start processing the gaussian smooth function
    NOTIFY_notify (processorId,pool_notify_IPS_ID,pool_notify_IPS_EVENTNO,NOTIFY_DSP);
    
    #if (FRACTION != 0)			// If we do not use DSP only
    // Neon - processing
    #ifdef TIMING
    startNeonTime = get_usec();
    #endif
    
    /* Do gaussian smooth processing using neon instruction.
     *   We have to add additional rows (in y-direction) in case of edge-problem if we split the image because
     *   the gaussian smooth process need neighbors to average for blurring the image.
     *   In this case, we add 8 additional rows which is compatible with the windows size used in Neon
     */ 
    #ifdef GAUSS_FLOAT    // Use floating point for calculating gaussian smooth
    smoothedIm = gaussian_smooth_neon(image,rows_neon+8,cols,sigma,rows);
    #elif GAUSS_U16       // Use uint16 for calculating gaussian smooth
    smoothedIm = gaussian_smooth_neon_u16(image,rows_neon+8,cols,sigma,rows);
    #elif GAUSS_U32       // Use uint32 for calculating gaussian smooth
    smoothedIm = gaussian_smooth_neon_u32(image,rows_neon+8,cols,sigma,rows);
    #else   // default use uint32 for calculating gaussian smooth
    smoothedIm = gaussian_smooth_neon_u32(image,rows_neon+8,cols,sigma,rows);
    #endif

    #ifdef TIMING
    stopNeonTime = get_usec();
    #endif

    #endif /* (FRACTION != 0) */

    // Wait for DSP to finish its calculation
    sem_wait(&sem);
    
    #ifdef TIMING
    startPoolInvdTime = get_usec();
    #endif

    // Invalidate the pool buffer because we have finish the gaussian smooth process in both DSP and GPP
	POOL_invalidate (POOL_makePoolId(0, SAMPLE_POOL_ID), pool_notify_DataBuf, pool_notify_BufferSize);  
    
    // Combine the results from DSP with the results from GPP and write it to the intermediate buffer
    #if defined(FULL_BUFFER) && (FRACTION != 0)
    memcpy(&smoothedIm[cols*rows_neon], &DataBuf16[cols*rows_neon], cols*(rows-rows_neon)*sizeof(short int));
    #elif (FRACTION != 0)
    memcpy(&smoothedIm[cols*rows_neon], DataBuf16, cols*(rows-rows_neon)*sizeof(short int));
    #else
    memcpy(smoothedIm, DataBuf16, cols*rows*sizeof(short int));
    #endif

    #ifdef TIMING
    stopPoolInvdTime = get_usec();
    #endif

    /*--- Continue canny edge detection algorithm after gaussian smooth process ---*/

    canny_rest(sigma, smoothedIm);

    #ifdef TIMING
    printf("--Total execution time:\t\t %*lld us.\n", SPACE, get_usec() - startAllocTime);
    totalTime += (stopAllocTime - startAllocTime)+(stopAlgoTime - stopAllocTime);;
    printf("--Calc total execution time:\t %*lld us.\n", SPACE, totalTime);
        
    /*********************************************************************************
    * Find maximum time between neon and dsp processing time for gaussian smooth func
    **********************************************************************************/ 
    printf("----Allocation execution time:\t %*lld us.\n", SPACE, stopAllocTime - startAllocTime);
    printf("------Main Init execution time:\t %*lld us.\n", SPACE, startCreateTime - startAllocTime);
    printf("------Create execution time:\t %*lld us.\n", SPACE, stopCreateTime - startCreateTime);
    printf("--------Pool Open time:\t\t %*lld us.\n", SPACE, stopPoolOpen - startPoolOpen);
    printf("--------Allocate Buffer time:\t %*lld us.\n", SPACE, stopAllocBuffer - startAllocBuffer);
    printf("--------Register Notify time:\t %*lld us.\n", SPACE, stopRegisterNotify - startRegisterNotify);
    printf("--------Setup DSP app:\t\t %*lld us.\n", SPACE, stopDspSetup - startDspSetup);
    printf("------Init Exe execution time:\t %*lld us.\n", SPACE, stopAllocTime - stopCreateTime);
    printf("----Algorithm execution time:\t %*lld us.\n", SPACE, stopAlgoTime - stopAllocTime);   
    printf("------(1 OR) Neon exec time:\t %*lld us.\n", SPACE, stopNeonTime - startNeonTime);
    printf("------(2 OR) DSP exec time:\t %*lld us.\n", SPACE, stopDspTime - startDspTime);
    if(stopDspTime > stopNeonTime)
        printf("------The rest execution time:\t %*lld us.\n", SPACE, stopAlgoTime - stopDspTime);
    else
        printf("------The rest execution time:\t %*lld us.\n", SPACE, stopAlgoTime - stopNeonTime);
    printf("--------Pool invld+memcpy time:\t %*lld us.\n", SPACE, stopPoolInvdTime - startPoolInvdTime);
    printf("--------Derrivative time:\t %*lld us.\n", SPACE, stopDerrivativeTime - startDerrivativeTime);
    printf("--------Magnitude calc time:\t %*lld us.\n", SPACE, stopMagnitudeTime - startMagnitudeTime);
    printf("--------Non-max-supp time:\t %*lld us.\n", SPACE, stopNonMaxTime - startNonMaxTime);
    printf("--------Hysteresis time:\t %*lld us.\n", SPACE, stopHysteresisTime - startHysteresisTime);
    
    #endif

    /****************************************************************************
    * Verify Results
    ****************************************************************************/
    verify_results(image, rows, cols, sigma, tlow, thigh, edge);

    /****************************************************************************
    * Write out the edge image to a file.
    ****************************************************************************/   
    sprintf(outfilename, "%s_s_%3.2f_l_%3.2f_h_%3.2f.pgm", filename, \
        tlow, thigh, sigma);

    if(write_pgm_image(outfilename, edge, rows, cols, "", 255) == 0)
    {
        fprintf(stderr, "Error writing the edge image, %s.\n", outfilename);
        exit(1);
    }

    // Free all
    free(image);
    free(edge);
    free(smoothedIm);
    
    return status ;
}


/** ============================================================================
 *  @func   pool_notify_Delete
 *
 *  @desc   This function releases resources allocated earlier by call to
 *          pool_notify_Create ().
 *          During cleanup, the allocated resources are being freed
 *          unconditionally. Actual applications may require stricter check
 *          against return values for robustness.
 *
 *  @modif  None
 *  ============================================================================
 */
NORMAL_API Void pool_notify_Delete (Uint8 processorId)
{
    DSP_STATUS status    = DSP_SOK ;
    DSP_STATUS tmpStatus = DSP_SOK ;

	#ifdef DEBUG
    printf ("Entered pool_notify_Delete ()\n") ;
	#endif

    /*
     *  Stop execution on DSP.
     */
    status = PROC_stop (processorId) ;
    if (DSP_FAILED (status)) {
        printf ("PROC_stop () failed. Status = [0x%x]\n", (int)status) ;
    }

    /*
     *  Unregister for notification of event registered earlier.
     */
    tmpStatus = NOTIFY_unregister (processorId,
                                   pool_notify_IPS_ID,
                                   pool_notify_IPS_EVENTNO,
                                   (FnNotifyCbck) pool_notify_Notify,
                                   0/* vladms pool_notify_SemPtr*/) ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("NOTIFY_unregister () failed Status = [0x%x]\n",
                         (int)status) ;
    }

    /*
     *  Free the memory allocated for the data buffer.
     */
    tmpStatus = POOL_free (POOL_makePoolId(processorId, SAMPLE_POOL_ID),
                           (Void *) pool_notify_DataBuf,
                           pool_notify_BufferSize) ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("POOL_free () DataBuf failed. Status = [0x%x]\n",
                         (int)status) ;
    }

    /*
     *  Close the pool
     */
    tmpStatus = POOL_close (POOL_makePoolId(processorId, SAMPLE_POOL_ID)) ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("POOL_close () failed. Status = [0x%x]\n", (int)status) ;
    }

    /*
     *  Detach from the processor
     */
    tmpStatus = PROC_detach  (processorId) ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("PROC_detach () failed. Status = [0x%x]\n", (int)status) ;
    }

    /*
     *  Destroy the PROC object.
     */
    tmpStatus = PROC_destroy () ;
    if (DSP_SUCCEEDED (status) && DSP_FAILED (tmpStatus)) {
        status = tmpStatus ;
        printf ("PROC_destroy () failed. Status = [0x%x]\n", (int)status) ;
    }

	#ifdef DEBUG
    printf ("Leaving pool_notify_Delete ()\n") ;
	#endif
}


/** ============================================================================
 *  @func   pool_notify_Main
 *
 *  @desc   Entry point for the application
 *
 *  @modif  None
 *  ============================================================================
 */
NORMAL_API Void pool_notify_Main (IN Char8 * dspExecutable, IN Char8 * infilename)
{
    DSP_STATUS status       = DSP_SOK ;
    #if (FRACTION != 100) && !defined(BASELINE)	// if we use DSP only or DSP+NEON
    Uint8      processorId  = 0 ;
    #endif

    strcpy(filename, infilename);

	#ifdef DEBUG
    printf ("========== Sample Application : pool_notify ==========\n") ;
	#endif

    if (   (dspExecutable != NULL) && (infilename != NULL)   ) 
	{
		// Check if we use NEON and or DSP based on FRACTION
		#if (FRACTION != 100) && !defined(BASELINE)
        Uint32 buffSize;
        Char8 strBufferSize[10];    // maximum digit of buffer size, e.g. 100000000\0 (9 digits + 1 terminating char)
        #endif

        /****************************************************************************
        * Read in the image. This read function allocates memory for the image.
        ****************************************************************************/
        if(VERBOSE) printf("Reading the image %s.\n", infilename);
        if(read_pgm_image(infilename, &image, &rows, &cols) == 0)
        {
            fprintf(stderr, "Error reading the input image, %s.\n", infilename);
            exit(1);
        }

        // Initialize timing
        totalTime = 0;
        startAllocTime = get_usec();

   		// Check if we only test the BASELINE
		#if !defined(BASELINE)

        // number of rows that should be processed by neon (load sharing with DSP)
        #if (FRACTION != 100) && (FRACTION != 0)
        rows_neon = (rows*FRACTION)/100;
        #endif

        // Calculate real image size (in pixels)
        imageSize = rows*cols;
        
        #ifdef DEBUG
        printf("Image Size: Rows= %d, cols= %d, Area= %lu\n", rows,cols, imageSize);
        #endif

        // Initialize buffer size
        #if (defined(FULL_BUFFER) && (FRACTION != 100)) || (FRACTION == 0)
        buffSize = imageSize;
        #elif (FRACTION != 100) && (FRACTION != 0)
        buffSize = (rows-rows_neon+8)*cols;     
        #endif

        #if (FRACTION != 100)		// No need to run these lines below if we use NEON only

        // Initiate string with null char
        memset(strBufferSize,0x00,sizeof(strBufferSize));   
        
        /*
         *  Validate the buffer size and number of iterations specified.
         *  Automatic padding to 128 using DSPLINK_ALIGN.
         *  We assume that 1 pixel = 1 byte (char) and we know that the results
         *    from DSP is in the form of unsigned short which is 2 byte, twice as big as char,
         *    then we have to allocate the buffSize twice as large as the original one (char).
         */
        pool_notify_BufferSize = DSPLINK_ALIGN ( buffSize*sizeof(unsigned short),
                                             DSPLINK_BUF_ALIGN) ;
        sprintf(strBufferSize, "%lu", pool_notify_BufferSize);      // will be used for pool_notify_Create

		#ifdef DEBUG
        printf("Allocated a buffer of %d bytes\n",(int)pool_notify_BufferSize );
		#endif
				
        processorId = 0 ;
        if (pool_notify_BufferSize == 0) 
		{
            status = DSP_EINVALIDARG ;
            printf ("ERROR! Invalid arguments specified for  ");
            printf ("     Buffer size    = %d\n", (int)pool_notify_BufferSize) ;
        }

        /*
         *  Specify the dsp executable file name and the buffer size for
         *  pool_notify creation phase.
         */
        startCreateTime = get_usec();
        status = pool_notify_Create (dspExecutable,
                                     strBufferSize,
                                     0) ;
        stopCreateTime = get_usec();  

        if (DSP_SUCCEEDED (status)) 
		{
            status = pool_notify_Execute (pool_notify_NumIterations, 0) ;
        }

         pool_notify_Delete (processorId) ;

        #else   // if we use NEON only

        // Execute gaussian smooth neon
        #ifdef TIMING
        startNeonTime = get_usec();
        #endif

        #ifdef GAUSS_FLOAT    // Use floating point for calculating gaussian smooth
        smoothedIm = gaussian_smooth_neon(image,rows,cols,sigma,rows);
        #elif GAUSS_U16       // Use uint16 for calculating gaussian smooth
        smoothedIm = gaussian_smooth_neon_u16(image,rows,cols,sigma,rows);
        #elif GAUSS_U32       // Use uint32 for calculating gaussian smooth
        smoothedIm = gaussian_smooth_neon_u32(image,rows,cols,sigma,rows);
        #else   // default use uint32 for calculating gaussian smooth
        smoothedIm = gaussian_smooth_neon_u32(image,rows,cols,sigma,rows);
        #endif
        
        #ifdef TIMING
        stopNeonTime = get_usec();
        #endif

        canny_rest(sigma, smoothedIm);

        #ifdef TIMING
        stopAlgoTime = get_usec();
        #endif

        #ifdef TIMING
        totalTime = stopAlgoTime - startAllocTime;
        printf("--Total execution time:\t\t %*lld us.\n", SPACE, totalTime);

        /*********************************************************************************
        * Find maximum time between neon and dsp processing time for gaussian smooth func
        **********************************************************************************/ 
        printf("----Algorithm execution time:\t %*lld us.\n", SPACE, stopAlgoTime - startAllocTime);   
        printf("------Neon exec time:\t\t %*lld us.\n", SPACE, stopNeonTime - startNeonTime);
        printf("------The rest execution time:\t %*lld us.\n", SPACE, stopAlgoTime - stopNeonTime);
        printf("--------Derrivative time:\t %*lld us.\n", SPACE, stopDerrivativeTime - startDerrivativeTime);
        printf("--------Magnitude calc time:\t %*lld us.\n", SPACE, stopMagnitudeTime - startMagnitudeTime);
        printf("--------Non-max-supp time:\t %*lld us.\n", SPACE, stopNonMaxTime - startNonMaxTime);
        printf("--------Hysteresis time:\t %*lld us.\n", SPACE, stopHysteresisTime - startHysteresisTime);
        
        #endif	/* TIMING */

        /****************************************************************************
        * Verify Results
        ****************************************************************************/
        verify_results(image, rows, cols, sigma, tlow, thigh, edge);

        /****************************************************************************
        * Write out the edge image to a file.
        ****************************************************************************/   
        sprintf(outfilename, "%s_s_%3.2f_l_%3.2f_h_%3.2f.pgm", filename, \
            tlow, thigh, sigma);

        if(write_pgm_image(outfilename, edge, rows, cols, "", 255) == 0)
        {
            fprintf(stderr, "Error writing the edge image, %s.\n", outfilename);
            exit(1);
        }

        // Free all
        free(image);
        free(edge);
        free(smoothedIm);

        #endif /* (FRACTION != 100) */

        #else /* !BASELINE */ 
        
    	canny_base(image, rows, cols, sigma, tlow, thigh, &edge);

	    #ifdef TIMING
        stopAlgoTime = get_usec();
        totalTime = stopAlgoTime - startAllocTime;
        printf("--Total execution time:\t\t %*lld us.\n", SPACE, totalTime);
        #endif	/* TIMING */

        verify_results(image, rows, cols, sigma, tlow, thigh, edge);

        // Free all
        free(image);
        free(edge);

        #endif /* BASELINE */
    }
    else 
	{
        status = DSP_EINVALIDARG ;
        printf ("ERROR! Invalid arguments specified for  "
                         "pool_notify application\n") ;
    }

    printf ("====================================================\n") ;
}

/** ----------------------------------------------------------------------------
 *  @func   pool_notify_Notify
 *
 *  @desc   This function implements the event callback registered with the
 *          NOTIFY component to receive notification indicating that the DSP-
 *          side application has completed its setup phase.
 *
 *  @modif  None
 *  ----------------------------------------------------------------------------
 */
STATIC Void pool_notify_Notify (Uint32 eventNo, Pvoid arg, Pvoid info)
{
	#ifdef DEBUG
    static int count = 0;
    count++;
    printf("Notification %d %8d \n", count, (int)info);
	#endif

    /* Post the semaphore. */
    if((int)info==0) 
	{
        // DSP setup is finished
        sem_post(&sem);
    } 
    else if((int)info==1) 
	{
        // DSP calculation is finished
        stopDspTime = get_usec();
        sem_post(&sem);
    }
    else if((int)info==2) 
    {
        printf("Error on DSP Allocation Memory \n");
        sem_post(&sem);
    }
    else
    {
        #ifdef DEBUG
        printf("Notif debug from DSP: %d\n", (int)info);
        #endif
    }
}

/** ----------------------------------------------------------------------------
 *  Canny edge custom functions
 *  ----------------------------------------------------------------------------
 */

// Initialize pool_notify buffer with image data
void unit_init(void) 
{
    // Fill pool_notify buffer with 0x00
    memset(pool_notify_DataBuf, 0x00, pool_notify_BufferSize);
    // Copy image data to pool_notify buffer
    #if defined(FULL_BUFFER) && (FRACTION != 0)
    memcpy(pool_notify_DataBuf, image, imageSize);
    #elif (FRACTION != 0)
    memcpy(pool_notify_DataBuf, &image[(rows_neon-8)*cols], (rows-rows_neon+8)*cols);
    #else
    memcpy(pool_notify_DataBuf, image, rows*cols);    
    #endif
    // create a short int pointer
    DataBuf16 = (unsigned short*)(pool_notify_DataBuf); 
}

/****************************************************************************
* Custom functions: Canny without gaussian smooth
****************************************************************************/
void canny_rest(float sigma, unsigned short *smoothedIm) {

    unsigned char *nms;         /* Non-max supp variable */
    short int *delta_x,*delta_y,*magnitude;     /* Magnitude = magnitude of the gradient 
                                                    delta_x = derivative of image in the x-direction 
                                                    delta_y = derivative of image in the y-direction */

    /****************************************************************************
    * Calculate derrivative of the image in both x and y directions
    ****************************************************************************/
    #ifdef TIMING
    startDerrivativeTime = get_usec();
    #endif
    derrivative_x_y((short int *)smoothedIm,rows,cols,&delta_x,&delta_y);
    #ifdef TIMING
    stopDerrivativeTime = get_usec();
    #endif

    /****************************************************************************
    * Calculate the radian direction, only if we define it.
    * In this case, we do not need it.
    ****************************************************************************/
    #ifdef RADIANS
    radian_direction(delta_x,delta_y,rows,cols,&dir_radians,-1,-1);
    #endif /* RADIANS */

    /****************************************************************************
    * Calculate magnitude of the gradient
    ****************************************************************************/
    #ifdef TIMING
    startMagnitudeTime = get_usec();
    #endif
    if((magnitude = (short *) malloc(rows*cols* sizeof(short))) == NULL)
    {
        fprintf(stderr, "Error allocating the magnitude image.\n");
    }
    magnitude_x_y(delta_x, delta_y, rows, cols, magnitude);
    #ifdef TIMING
    stopMagnitudeTime = get_usec();
    #endif

    /****************************************************************************
    * Calculate non_max_supp
    ****************************************************************************/
    #ifdef TIMING
    startNonMaxTime = get_usec();
    #endif
    if((nms = (unsigned char *) malloc(rows*cols*sizeof(unsigned char)))==NULL)
    {
        fprintf(stderr, "Error allocating the nms image.\n");
    }
    non_max_supp(magnitude, delta_x, delta_y, rows, cols, nms);
    #ifdef TIMING
    stopNonMaxTime = get_usec();
    #endif

    /****************************************************************************
    * Calculate hysteresis to mark the edge pixels
    ****************************************************************************/
    #ifdef TIMING
    startHysteresisTime = get_usec();
    #endif
    if( (edge=(unsigned char *)malloc(rows*cols*sizeof(unsigned char))) == NULL )
    {
        fprintf(stderr, "Error allocating the edge image.\n");
        exit(1);
    }
    apply_hysteresis(magnitude, nms, rows, cols, tlow, thigh, edge);
    #ifdef TIMING
    stopAlgoTime = stopHysteresisTime = get_usec();
    #endif

    free(delta_x);
    free(delta_y);
    free(magnitude);
    free(nms);
}

#ifdef VERIFY

/****************************************************************************
* Custom functions: Verify results, compare with baseline
****************************************************************************/
void verify_results(unsigned char *image, int rows, int cols, float sigma,
       float tlow, float thigh, unsigned char *edge)
{
    char outfilename[128];    /* Name of the output "edge" image */
    unsigned char *edge_base = NULL;      /* The output baseline edge of image */
    unsigned int i;                /* Index row for verification */
    unsigned int j;                 /* Index column for verification */
    unsigned long false_alarm = 0;
    unsigned long misdetection = 0;
    unsigned long total_error = 0;
    unsigned char *edge_error = NULL;      /* Output of the edge error */

    canny_base(image, rows, cols, sigma, tlow, thigh, &edge_base); /* Flushing memory, gives garbage if we don't call it */

    #ifdef TIMING
    startTotalTimeBaseline = get_usec();
    #endif

    canny_base(image, rows, cols, sigma, tlow, thigh, &edge_base);  // Execute canny baseline algorithm

    #ifdef TIMING
    totalTimeBaseline = get_usec()-startTotalTimeBaseline;
    printf("\n");
    printf("--Sum exec time baseline:\t %*lld us.\n", SPACE, totalTimeBaseline);
    
    #ifndef BASELINE
    printf("--Gaussian time baseline:\t %*lld us.\n", SPACE, stopGaussBaseTime- startGaussBaseTime);
    #if (FRACTION != 0)
    printf("--Gaussian Neon Speedup:\t %.4f\n",(float)(stopGaussBaseTime- startGaussBaseTime)/(stopNeonTime-startNeonTime));
    #endif
    #if (FRACTION != 100)
    printf("--Gaussian Dsp Speedup:\t\t %.4f\n",(float)(stopGaussBaseTime- startGaussBaseTime)/(stopDspTime-startDspTime));
    printf("--Total Speedup w Overhead:\t\t %.4f\n",(float)totalTimeBaseline/totalTime);
    printf("--Total Speedup w/o Overhead:\t\t %.4f\n",(float)totalTimeBaseline/(stopAlgoTime-startAlgoTime));
	#else
	printf("--Total Speedup:\t\t %.4f\n",(float)totalTimeBaseline/totalTime);
	#endif /* (FRACTION != 100) */
    
    #else
	printf("--Total Speedup:\t\t %.4f\n",(float)totalTimeBaseline/totalTime);
    #endif /* BASELINE */

    #endif /* TIMING */
    
    if(((edge_error) = (unsigned char *) malloc(rows*cols*sizeof(unsigned char))) == NULL)
    {
         fprintf(stderr, "Error allocating the edge_error image.\n");
         exit(1);
    }

    for(i=0; i<rows; i++){        
        for(j=0; j<cols; j++){
        	//printf("Baseline: %d, Actual: %d\n", edge_base[i*cols+j], edge[i*cols+j]);
            if(edge_base[i*cols+j] != edge[i*cols+j]) {
                if(edge_base[i*cols+j] == 255)
                    false_alarm++;
                else
                    misdetection++;
                edge_error[i*cols+j] = 0;   // output the error as black pixel
            } else {
                edge_error[i*cols+j] = 255; // output the edge (no error) as white pixel
            }
        }
    }

    // Write error edge to file
    sprintf(outfilename, "%s_error.pgm", filename);

    if(write_pgm_image(outfilename, edge_error, rows, cols, "", 255) == 0)
    {
        fprintf(stderr, "Error writing the edge error image, %s.\n", outfilename);
        exit(1);
    }

    total_error = false_alarm + misdetection;
    printf("\nTotal error (%%):\t %f\n", (float)total_error/(rows*cols)*100);
    printf("False alarm (%%):\t %f\n", (float)false_alarm/(rows*cols)*100);
    printf("Misdetection (%%):\t %f\n\n", (float)misdetection/(rows*cols)*100);

    #ifndef BASELINE
    printf("Load sharing (NEON PART): %d%%\n", FRACTION);
    #endif
}
#endif

/*------------------------------- end custom canny edge implementations ---------------*/

#if defined (__cplusplus)
}
#endif /* defined (__cplusplus) */