/*  ----------------------------------- DSP/BIOS Headers            */
#include <std.h>
#include <gbl.h>
#include <log.h>
#include <swi.h>
#include <sys.h>
#include <tsk.h>
#include <pool.h>

/*  ----------------------------------- DSP/BIOS LINK Headers       */
#include <failure.h>
#include <dsplink.h>
#include <platform.h>
#include <notify.h>
#include <bcache.h>
/*  ----------------------------------- Sample Headers              */
#include <pool_notify_config.h>
#include <task.h>

#include <stdlib.h>

/* ------------------------------------ Custom Definition           */
#if !defined(FRACTION)
#define FRACTION 38
#endif

extern Uint16 MPCSXFER_BufferSize ;

int rows_neon; // number of rows process by neon/GPP side

static Void Task_notify (Uint32 eventNo, Ptr arg, Ptr info) ;
unsigned short int* gaussian_smooth(unsigned char *image, int rows, int cols);

Int Task_create (Task_TransferInfo ** infoPtr)
{
    Int status    = SYS_OK ;
    Task_TransferInfo * info = NULL ;

    /* Allocate Task_TransferInfo structure that will be initialized
     * and passed to other phases of the application */
    if (status == SYS_OK) 
	{
        *infoPtr = MEM_calloc (DSPLINK_SEGID,
                               sizeof (Task_TransferInfo),
                               0) ; /* No alignment restriction */
        if (*infoPtr == NULL) 
		{
            status = SYS_EALLOC ;
        }
        else 
		{
            info = *infoPtr ;
        }
    }

    /* Fill up the transfer info structure */
    if (status == SYS_OK) 
	{
        info->dataBuf       = NULL ; /* Set through notification callback. */
        info->bufferSize    = MPCSXFER_BufferSize ;
        SEM_new (&(info->notifySemObj), 0) ;
    }

    /*
     *  Register notification for the event callback to get control and data
     *  buffer pointers from the GPP-side.
     */
    if (status == SYS_OK) 
	{
        status = NOTIFY_register (ID_GPP,
                                  MPCSXFER_IPS_ID,
                                  MPCSXFER_IPS_EVENTNO,
                                  (FnNotifyCbck) Task_notify,
                                  info) ;
        if (status != SYS_OK) 
		{
            return status;
        }
    }

    /*
     *  Send notification to the GPP-side that the application has completed its
     *  setup and is ready for further execution.
     */
    if (status == SYS_OK) 
	{
        status = NOTIFY_notify (ID_GPP,
                                MPCSXFER_IPS_ID,
                                MPCSXFER_IPS_EVENTNO,
                                (Uint32) 0) ; /* No payload to be sent. */
        if (status != SYS_OK) 
		{
            return status;
        }
    }

    /*
     *  Wait for the event callback from the GPP-side to post the semaphore
     *  indicating receipt of the data buffer pointer and image width and height.
     */
    SEM_pend (&(info->notifySemObj), SYS_FOREVER) ; // buf address
    SEM_pend (&(info->notifySemObj), SYS_FOREVER) ; // buf size
    SEM_pend (&(info->notifySemObj), SYS_FOREVER) ; // rows and cols
    SEM_pend (&(info->notifySemObj), SYS_FOREVER) ; // rows_neon

    return status ;
}

unsigned char* buf; // address of buffer on shared memory 
int length;         // allocated buffer size
int rows;           // image rows
int cols;           // image cols
int imageSize;      // imagesize = rows*cols
unsigned short* DataBuf16;  // buffer address but in the 2 bytes format

Int Task_execute (Task_TransferInfo * info)
{
    //wait for semaphore
    SEM_pend (&(info->notifySemObj), SYS_FOREVER);

    //invalidate cache
    BCACHE_inv ((Ptr)buf, length, TRUE) ;
    
    gaussian_smooth(buf, rows, cols);
    
    //write back
    BCACHE_wbInv ((Ptr)buf, length, TRUE) ;

    //notify that we are done
    NOTIFY_notify(ID_GPP,MPCSXFER_IPS_ID,MPCSXFER_IPS_EVENTNO,(Uint32)1);    
    
    return SYS_OK;
}

Int Task_delete (Task_TransferInfo * info)
{
    Int    status     = SYS_OK ;
    /*
     *  Unregister notification for the event callback used to get control and
     *  data buffer pointers from the GPP-side.
     */
    status = NOTIFY_unregister (ID_GPP,
                                MPCSXFER_IPS_ID,
                                MPCSXFER_IPS_EVENTNO,
                                (FnNotifyCbck) Task_notify,
                                info) ;

    /* Free the info structure */
    MEM_free (DSPLINK_SEGID,
              info,
              sizeof (Task_TransferInfo)) ;
    info = NULL ;

    return status ;
}


static Void Task_notify (Uint32 eventNo, Ptr arg, Ptr info)
{
    static int count = 0;
    Task_TransferInfo * mpcsInfo = (Task_TransferInfo *) arg ;

    (Void) eventNo ; /* To avoid compiler warning. */

    count++;
    if (count==1) {
        buf =(unsigned char*)info ;
    }
    if (count==2) {
        length = (int)info;
    }
    if (count==3) {
        rows = (int)((Uint32)info>>16) & 0xFFFF;
        cols = (int)(info) & 0xFFFF;
    }
    if (count==4) {
        rows_neon = (int)(info) & 0xFFFF;
    }

    SEM_post(&(mpcsInfo->notifySemObj));
}

/************************************************************************************************/

/************************************************************
 * Canny Edge custom functions
 ***********************************************************/
 unsigned short int* gaussian_smooth(unsigned char *image, int rows, int cols)
{
    int r, c, rr, cc,     /* Counter variables. */
        windowsize,       /* Dimension of the gaussian kernel. */
        center;           /* Half of the windowsize. */
    unsigned int dot,     /* Dot product summing variable. */
          sum,            /* Sum of the kernel weights variable. */
          temp;

    unsigned short int* smoothedim;
    unsigned char * tmp;

    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    /* A one dimensional gaussian kernel, Q17 format. */
    static unsigned short int kernel[] = {
          416,  1177,  2837,  5830, 10206,
        15226, 19356, 20969, 19356, 15226,
        10206,  5830,  2837,  1177,  416
    };

    windowsize = 15;
    center = windowsize / 2;

    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/
    #if (FRACTION !=0) // allocated tmp as big as the part of the image for DSP part + 8 (neighbour for boundary case)
    if((tmp = (unsigned char *) malloc((rows+8-rows_neon)*cols* sizeof(unsigned char))) == NULL)
    #else // allocated full size image
    if((tmp = (unsigned char *) malloc(rows*cols*sizeof(unsigned char))) == NULL)
    #endif
    {
        NOTIFY_notify (ID_GPP,
                       MPCSXFER_IPS_ID,
                       MPCSXFER_IPS_EVENTNO,
                       2);
    }

    // assign output pointer to the shared memory
    smoothedim = (unsigned short *) image;

    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/
    #if defined(FULL_BUFFER) && (FRACTION !=0)  // full size image is allocated in shared memory + load sharing
    for(r=rows_neon-8; r<rows; r++)             // do the iteration as much as DSP image part + 8
    #elif (FRACTION != 0)                       // load sharing but minimized the size of allocated buffer
    for(r=0; r<rows+8-rows_neon; r++)           // do the iteration as much as DSP image part + 8
    #else                                       // full DSP, iterate without the offset
    for(r=0; r<rows; r++)                       // do the iteration for full image
    #endif
    {
        for(c=0; c<cols; c++)
        {
            dot = 0;
            sum = 0;
            for(cc=(-center); cc<=center; cc++)
            {
                if(((c+cc) >= 0) && ((c+cc) < cols))
                {
                    dot += image[r*cols+(c+cc)] * kernel[center+cc];
                    sum += kernel[center+cc];
                }
            }
            #if defined(FULL_BUFFER) && (FRACTION !=0)  
            tmp [(r+8-rows_neon)*cols+c] = dot/sum;     // adjust the index so that the value is assigned from the start of array
            #else                                       
            tmp [r*cols+c] = dot/sum;                   //
            #endif
        }
    }
    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/
    for(c=0; c<cols; c++)
    {
        #if (FRACTION != 0)                 // load sharing
        for(r=8; r<rows+8-rows_neon; r++)   // start from the necessary rows for DSP (first 8 rows only for x-direction)
        #else                               // full DSP 
        for(r=0; r<rows; r++)               // iterate for all the pixel
        #endif
        {
            sum = 0;
            dot = 0;
            for(rr=(-center); rr<=center; rr++)
            {
                if(((r+rr) >= 0) && ((r+rr) < rows))
                {
                    dot += tmp[(r+rr)*cols+c] * kernel[center+rr];
                    sum += kernel[center+rr];
                }
            }
            temp = ((dot*90/sum));
            #if defined(FULL_BUFFER) && (FRACTION != 0)
            smoothedim[(r-8+rows_neon)*cols+c] = temp;  // assign the value to the address exactly after neon rows
            #elif (FRACTION != 0)           
            smoothedim[(r-8)*cols+c] = temp;            // load sharing but buffer is only used for DSP part, start from zero
            #else
            smoothedim[r*cols+c] = temp;                // full DSP, start form zero
            #endif
        }
    }

    free(tmp);
    return smoothedim;
}
