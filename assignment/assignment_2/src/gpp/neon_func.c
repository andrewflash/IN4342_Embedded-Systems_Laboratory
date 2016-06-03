#define VERBOSE 0

#define BOOSTBLURFACTOR 90.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <arm_neon.h>
#include "neon_func.h"

unsigned short int* gaussian_smooth_neon(unsigned char *image, int rows, int cols, float sigma, int total_rows)
{
    int windowsize, /* Dimension of the gaussian kernel. */
        center;     /* Half of the windowsize. */
    float *tempbuf,  /* Buffer for intermediate output of x-direction blur. */
          *kernel;//,        /* A one dimensional gaussian kernel. */

    unsigned short int* smoothed; // output of gaussian function

    // Blur in x-direction
    float *new_image;       // modified image for x-direction
    float *new_image_col;   // modified image for y-direction
    unsigned int new_cols;  // colomn number for the modified image  
    unsigned int new_rows;  // row number for the modified image
    unsigned int k;         // index for kernel basic summation
    unsigned int i, j;      // index for image modification
    unsigned int m, n;      // index for rows and cols iteration
    unsigned int acc;       // index for multiplication process

    int kernel_idx;         // index for modifying kernel
    float new_kernel[17];   // modified kernel 

    float32x4_t neon_image;     // neon register to store the processed image 
    float32x4_t neon_kernel;    // neon register to store the processed kernel
    float32x4_t temp_acc;       // neon register to store the temporary accumulation

    float32_t temp_out;         // variable to store the temporary final value
    
    float kernelBase = 0.0f;    // sum of kernel value for the half-side + center
    float kernelSum = 0.0f;     // to store kernel sum for one pixel blurring process

    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    make_gaussian_kernel_neon(sigma, &kernel, &windowsize);
    center = windowsize / 2;

    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/
    if((tempbuf = (float *) malloc(rows*cols* sizeof(float))) == NULL)
    {
        fprintf(stderr, "Error allocating the buffer image.\n");
        exit(1);
    }
    
    if(((smoothed) = (unsigned short int *) malloc(total_rows*cols*sizeof(short int))) == NULL)
    {
        fprintf(stderr, "Error allocating the smoothed image.\n");
        exit(1);
    }
 
    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/

    // modify the kernel, add two additional value of 0 as the first and last element
    for (kernel_idx = 0 ; kernel_idx < 17 ; kernel_idx++)
    {
        if(kernel_idx == 0 || kernel_idx == 16 )
            new_kernel[kernel_idx] = 0 ;
        else
            new_kernel [kernel_idx] = kernel[kernel_idx -1];  
    }

    // kernelBase = sum of kernel value for the half-side + center
    for( k=8; k<=16; k++){
        kernelBase += new_kernel[k];
    }

    // modify the image, add eight 0 in the left and right side (each side 8)
    new_cols=cols+16;
    new_image = (float*)malloc(new_cols*rows*sizeof(float));    
    for(i=0; i<rows; i++){
        memset(&new_image[i*new_cols],0,8*sizeof(float));
        
        for( j=0; j<cols;j++){
            new_image[i*new_cols+8+j] = (float)image[i*cols+j];
            
        }
        memset(&new_image[i*new_cols+8+cols],0,8*sizeof(float));
    }

    // iteration for each pixel in the new image
    for(m=0; m<rows; m++){
        for( n=0; n<cols; n++){
            unsigned int l; // index for extracting the value from neon register
        
            temp_acc = vdupq_n_f32(0); // initialize the neon register with zeros value
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                kernelSum = kernelBase;
            }
            else if(n <=8){
                kernelSum += new_kernel[8-n];
            }
            else if(n>=cols-8){
                kernelSum -=new_kernel[cols-n+8];
            }

            // multiply the neighbour kernel with the corresponding pixel value
            // skip is used to skip the respective blurred pixel (the center of windowsize)
            for( acc=0; acc<4; acc++)
            {
                int skip=0;
                if(acc>=2)
                {
                    skip=1;
                }
                // load 4 pixel value
                neon_image = vld1q_f32((float32_t const *)&new_image[m*new_cols+n+acc*4+skip]);
                
                // load 4 kernel value
                neon_kernel = vld1q_f32((float32_t const *)&new_kernel[acc*4+skip]);
                
                // multiplty accumulate, get the sum for 1 pixel (before average)
                temp_acc = vmlaq_f32(temp_acc,neon_image,neon_kernel);
            }
            
            
            for( l=0; l<=3; l++){   
                        
                // extract the vector and accumulate it to single float variable
                temp_out += vgetq_lane_f32(temp_acc,l ); 
            }
            // add the gaussian value of the center point
            temp_out += new_image[m*new_cols+n+8] * new_kernel[8];
            
            // normalize
            temp_out /= kernelSum;
            
            // gaussian blur value for the n pixel
            tempbuf[m*cols+n] = temp_out;
            
            temp_out=0; 
        }
    }

    
    // /****************************************************************************
    // * Blur in the x - direction.
    // ****************************************************************************/

    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/
    // kernelBase = sum of kernel value for the half-side + center
    kernelBase = 0.0; 
    for( k=8; k<=16; k++){
        kernelBase += new_kernel[k];
    }

    // modify the image, add eight 0 in the top and bottom side (each side 8)
    new_rows=rows+16;
    new_image_col = (float*)malloc(new_rows*cols*sizeof(float));
    for( i =0; i<cols; i++){
        memset(&new_image_col[i*new_rows],0,8*sizeof(float));  
        for( j=0; j<rows;j++){
            new_image_col[i*new_rows+8+j] = tempbuf[j*cols+i];
        }
        memset(&new_image_col[i*new_rows+8+rows],0,8*sizeof(float));
    }

    // iteration for each pixel in the new image
    for(m=0; m<cols; m++){
        for( n=0; n<rows; n++){
            unsigned int l;     // index for extracting the value from neon register      
            temp_acc = vdupq_n_f32(0); // initialize the neon register with zeros value
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                kernelSum = kernelBase;
            }
            else if(n <=8){
                kernelSum += new_kernel[8-n];
            }
            else if(n>=rows-8){
                kernelSum -=new_kernel[rows-n+8];
            }

            // multiply the neighbour kernel with the corresponding pixel value
            // skip is used to skip the respective blurred pixel (the center of windowsize)
            for( acc=0; acc<4; acc++)
            {
                int skip=0;
                if(acc>=2)
                {
                    skip=1;
                }
                // load 4 pixel value
                neon_image = vld1q_f32((float32_t const *)&new_image_col[m*new_rows+n+acc*4+skip]);
                // load 4 kernel value
                neon_kernel = vld1q_f32((float32_t const *)&new_kernel[acc*4+skip]);
                // multiplty accumulate, get the sum for 1 pixel (before normalization)
                temp_acc = vmlaq_f32(temp_acc,neon_image,neon_kernel);
            }
            
            // extract the vector and accumulate it to single float variable
            for( l=0; l<=3; l++){   
                temp_out += vgetq_lane_f32(temp_acc,l );
            }

            // add the gaussian value of the center point
            temp_out += new_image_col[m*new_rows+n+8] * new_kernel[8];
            
            // boost the blur factor and normalize
            temp_out = (temp_out * 90) / kernelSum + 0.5;
            
            // gaussian blur value for the n pixel
            smoothed[n*cols+m] = (unsigned short int )temp_out;
            temp_out=0; 
        }
    }

    free(new_image);
    free(new_image_col);
    free(tempbuf);
    free(kernel);
    return smoothed;
}

/*******************************************************************************
* PROCEDURE: make_gaussian_kernel using neon
* PURPOSE: Create a one dimensional gaussian kernel using NEON
* NAME: Mike Heath & Andri Rahmadhani
* DATE: 2/15/96
*******************************************************************************/
void make_gaussian_kernel_neon(float sigma, float **kernel, int *windowsize)
{
    int i, center;
    float x, fx, sum=0.0;

    *windowsize = 1 + 2 * ceil(2.5 * sigma);
    center = (*windowsize) / 2;

    if((*kernel = (float *) malloc((*windowsize)* sizeof(float))) == NULL)
    {
        fprintf(stderr, "Error callocing the gaussian kernel array.\n");
        exit(1);
    }

    for(i=0; i<(*windowsize); i++)
    {
        x = (float)(i - center);
        fx = pow(2.71828, -0.5*x*x/(sigma*sigma)) / (sigma * sqrt(6.2831853));
        (*kernel)[i] = fx;
        sum += fx;
    }

    for(i=0; i<(*windowsize); i++) (*kernel)[i] /= sum;

    if(VERBOSE)
    {
        printf("The filter coefficients are:\n");
        for(i=0; i<(*windowsize); i++)
            printf("kernel[%d] = %f\n", i, (*kernel)[i]);
    }
}

/*******************************************************************************
* FUNCTION: gaussian_smooth_neon_u32
* PURPOSE: Gaussian smooth function using neon instrinsic, unsigned long integer format.
* NAME: Angga Irawan
* DATE: 
*******************************************************************************/
unsigned short int* gaussian_smooth_neon_u32(unsigned char *image, int rows, int cols, float sigma, int total_rows)
{
    int windowsize,        /* Dimension of the gaussian kernel. */
        center;            /* Half of the windowsize. */
    uint32_t  *tempbuf_fix;         /* Buffer for intermediate output of x-direction blur. */
    uint32_t  new_kernel_fix[17];   /* A one dimensional gaussian kernel. */
    
    unsigned short int* smoothed; // output of gaussian function
    
    uint32_t  *new_image_fix;       // modified image for x-direction
    uint32_t  *new_image_col_fix;   // modified image for y-direction
    unsigned int new_cols;          // colomn number for the modified image
    unsigned int new_rows;          // row number for the modified image
    unsigned int k;                 // index for kernel basic summation
    unsigned int i, j;              // index for image modification
    unsigned int m, n;              // index for rows and cols iteration
    unsigned int acc;               // index for multiplication process
    
    uint32x4_t neon_image_fix;      // neon register to store the processed image
    uint32x4_t neon_kernel_fix;     // neon register to store the processed kernel
    uint32x4_t temp_acc_fix;        // neon register to store the temporary accumulation
    
    uint32_t temp_out_fix;          // variable to store the temporary final value
    
    uint32_t  kernelBase_fix = 0;   // sum of kernel value for the half-side + center
    uint32_t  kernelSum_fix = 0;    // to store kernel sum for one pixel blurring process


    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    windowsize = 15;
    center = windowsize / 2;
    
    // kernel Q16
    new_kernel_fix[0] = 0;
    new_kernel_fix[1] = 208;
    new_kernel_fix[2] = 589;
    new_kernel_fix[3] = 1419;
    new_kernel_fix[4] = 2915;
    new_kernel_fix[5] = 5103;
    new_kernel_fix[6] = 7613;
    new_kernel_fix[7] = 9678;
    new_kernel_fix[8] = 10485;
    new_kernel_fix[9] = 9678;
    new_kernel_fix[10] = 7613;
    new_kernel_fix[11] = 5103;
    new_kernel_fix[12] = 2915;
    new_kernel_fix[13] = 1419;
    new_kernel_fix[14] = 589;
    new_kernel_fix[15] = 208;
    new_kernel_fix[16] = 0;

    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/
    if((tempbuf_fix = (uint32_t  *) malloc(rows*cols* sizeof(uint32_t ))) == NULL)
    {
        fprintf(stderr, "Error allocating the fix buffer image.\n");
        exit(1);
    }

    if(((smoothed) = (unsigned short int *) malloc(total_rows*cols*sizeof(short int))) == NULL)
    {
        fprintf(stderr, "Error allocating the smoothed image.\n");
        exit(1);
    }
 
    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/
    // modify the image, add eight 0 in the left and right side (each side 8)
    new_cols=cols+16;
    new_image_fix = (uint32_t *)malloc(new_cols*rows*sizeof(uint32_t ));
    for(i=0; i<rows; i++){
        
        memset(&new_image_fix[i*new_cols],0,8*sizeof(uint32_t ));

        for( j=0; j<cols;j++){
            new_image_fix[i*new_cols+8+j] = (uint32_t )image[i*cols+j];
        }
        memset(&new_image_fix[i*new_cols+8+cols],0,8*sizeof(uint32_t ));
    }

    // kernelBase = sum of kernel value for the first colomn of image
    for( k=8; k<=16; k++){
        kernelBase_fix += new_kernel_fix[k];
    }

    // modify the image, add eight 0 in the left and right side (each side 8)
    for(m=0; m<rows; m++){
        for( n=0; n<cols; n++){
            unsigned int l;
            temp_acc_fix = vdupq_n_u32(0);
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                kernelSum_fix = kernelBase_fix;
            }
            else if(n <=8){
                kernelSum_fix += new_kernel_fix[8-n];
            }
            else if(n>=cols-8){
                kernelSum_fix -=new_kernel_fix[cols-n+8];
            }

            // multiply the kernel with the pixel value
            // skip is used to skip the respective blurred pixel (the center of windowsize)
            for( acc=0; acc<4; acc++)
            {
                int skip=0;
                if(acc>=2)
                {
                    skip=1;
                }
                // load 4 pixel value
                neon_image_fix = vld1q_u32((uint32_t const *)&new_image_fix[m*new_cols+n+acc*4+skip]);
                
                // load 4 kernel value
                neon_kernel_fix = vld1q_u32((uint32_t const *)&new_kernel_fix[acc*4+skip]);
                
                // multiplty accumulate, get the sum for 1 pixel (before average)
                temp_acc_fix = vmlaq_u32(temp_acc_fix,neon_image_fix,neon_kernel_fix);
            }
    
            for( l=0; l<=3; l++){   
                        
                // extract the vector and accumulate it to single float variable
                temp_out_fix += vgetq_lane_u32(temp_acc_fix,l ); 
            }
            // add the gaussian value of the center point
            temp_out_fix += new_image_fix[m*new_cols+n+8] * new_kernel_fix[8];
            
            // normalize
            temp_out_fix /= kernelSum_fix;
            // gaussian blur value for the n pixel
            tempbuf_fix[m*cols+n] = temp_out_fix;
            
            temp_out_fix=0; 
        }
    }

    
    // /****************************************************************************
    // * Blur in the x - direction.
    // ****************************************************************************/

    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/

    // modify the image, add eight 0 in the top and bottom side (each side 8)
    new_rows=rows+16;
    new_image_col_fix = (uint32_t *)malloc(new_rows*cols*sizeof(uint32_t ));
    for( i =0; i<cols; i++){
        memset(&new_image_col_fix[i*new_rows],0,8*sizeof(uint32_t ));

        for( j=0; j<rows;j++){
            new_image_col_fix[i*new_rows+8+j] = tempbuf_fix[j*cols+i];
            
        }
        memset(&new_image_col_fix[i*new_rows+8+rows],0,8*sizeof(uint32_t ));
    }

    // kernelBase = sum of kernel value for the half-side + center
    kernelBase_fix = 0; 
    for( k=8; k<=16; k++){
        kernelBase_fix += new_kernel_fix[k];
    }
    
    // iteration for each pixel in the new image
    for(m=0; m<cols; m++){
        for( n=0; n<rows; n++){
            unsigned int l; // index for extracting the value from neon register
            temp_acc_fix = vdupq_n_u32(0); // initialize the neon register with zeros value
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                kernelSum_fix = kernelBase_fix;
            }
            else if(n <=8){
                kernelSum_fix += new_kernel_fix[8-n];
            }
            else if(n>=rows-8){
                kernelSum_fix -=new_kernel_fix[rows-n+8];
            }

            // multiply the neighbour kernel with the corresponding pixel value
            // skip is used to skip the respective blurred pixel (the center of windowsize)      
            for( acc=0; acc<4; acc++)
            {
                int skip=0;
                if(acc>=2)
                {
                    skip=1;
                }
                // load 4 pixel value
                neon_image_fix = vld1q_u32((uint32_t const *)&new_image_col_fix[m*new_rows+n+acc*4+skip]);
                // load 4 kernel value
                neon_kernel_fix = vld1q_u32((uint32_t const *)&new_kernel_fix[acc*4+skip]);
                // multiplty accumulate, get the sum for 1 pixel (before normalization)
                temp_acc_fix = vmlaq_u32(temp_acc_fix,neon_image_fix,neon_kernel_fix);
            }
            
            // extract the vector and accumulate it to single variable
            for( l=0; l<=3; l++){   
                temp_out_fix += vgetq_lane_u32(temp_acc_fix,l ); 

            }
            // add the gaussian value of the center point
            temp_out_fix += new_image_col_fix[m*new_rows+n+8] * new_kernel_fix[8];
            // boost the blur factor and normalize
            temp_out_fix = (temp_out_fix * 90) / kernelSum_fix + 1;
            // gaussian blur value for the n pixel
            smoothed[n*cols+m] = (unsigned short int )temp_out_fix;
            temp_out_fix=0; 
        }
    }

    free(new_image_fix);
    free(new_image_col_fix);
    free(tempbuf_fix);
    return smoothed;
}

/*******************************************************************************
* FUNCTION: gaussian_smooth_neon_u16
* PURPOSE: Gaussian smooth function using neon instrinsic, unsigned short integer format.
* NAME: Angga Irawan
* DATE: 
*******************************************************************************/

unsigned short int* gaussian_smooth_neon_u16(unsigned char *image, int rows, int cols, float sigma, int total_rows)
{
    int windowsize,        /* Dimension of the gaussian kernel. */
        center;            /* Half of the windowsize. */
    
    uint16_t new_kernel_fix[17]; /* A one dimensional gaussian kernel. */
    uint16_t *tempbuf_fix; /* Buffer for intermediate output of x-direction blur. */
    
    unsigned short int* smoothed;   // output of gaussian function
    
    uint16_t *new_image_fix;        // modified image for x-direction
    uint16_t *new_image_col_fix;    // modified image for y-direction
    unsigned int new_rows;          // row number for the modified image
    unsigned int new_cols;          // colomn number for the modified image  
    unsigned int k;                 // index for kernel basic summation
    unsigned int i, j;              // index for image modification
    unsigned int m, n;              // index for rows and cols iteration
    unsigned int acc;               // index for multiplication process

    uint16x8_t neon_image_fix;      // neon register to store the processed image 
    uint16x8_t neon_kernel_fix;     // neon register to store the processed kernel
    uint16x8_t temp_acc_fix;        // neon register to store the temporary accumulation

    uint16_t temp_out_fix;          // variable to store the temporary final value in x-direction blurring
    uint32_t temp_out_fix_y;        // variable to store the temporary final value in y-direction blurring
    
    uint16_t kernelBase_fix = 0;    // sum of kernel value for the half-side + center
    uint16_t kernelSum_fix = 0;     // to store kernel sum for one pixel blurring process

    
    
    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    windowsize = 15;
    center = windowsize / 2;

    // kernel Q8
    new_kernel_fix[0] = 0;
    new_kernel_fix[1] = 1;
    new_kernel_fix[2] = 2;
    new_kernel_fix[3] = 6;
    new_kernel_fix[4] = 11;
    new_kernel_fix[5] = 20;
    new_kernel_fix[6] = 30;
    new_kernel_fix[7] = 38;
    new_kernel_fix[8] = 41;
    new_kernel_fix[9] = 38;
    new_kernel_fix[10] = 30;
    new_kernel_fix[11] = 20;
    new_kernel_fix[12] = 11;
    new_kernel_fix[13] = 6;
    new_kernel_fix[14] = 2;
    new_kernel_fix[15] = 1;
    new_kernel_fix[16] = 0;
    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/

    if((tempbuf_fix = (uint16_t *) malloc(rows*cols* sizeof(uint16_t))) == NULL)
    {
        fprintf(stderr, "Error allocating the fix buffer image.\n");
        exit(1);
    }

    if(((smoothed) = (unsigned short int *) malloc(total_rows*cols*sizeof(short int))) == NULL)
    {
        fprintf(stderr, "Error allocating the smoothed image.\n");
        exit(1);
    }
 
    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/

    // modify the image, add eight 0 in the left and right side (each side 8)
    new_cols=cols+16;
    new_image_fix = (uint16_t*)malloc(new_cols*rows*sizeof(uint16_t));
    for(i=0; i<rows; i++){
        memset(&new_image_fix[i*new_cols],0,8*sizeof(uint16_t));

        for( j=0; j<cols;j++){
            new_image_fix[i*new_cols+8+j] = (uint16_t)image[i*cols+j];
        }
        memset(&new_image_fix[i*new_cols+8+cols],0,8*sizeof(uint16_t));
    }

    // kernelBase = sum of kernel value for the first colomn of image
    for( k=8; k<=16; k++){
        kernelBase_fix += new_kernel_fix[k];
    }

    // modify the image, add eight 0 in the left and right side (each side 8)
    for(m=0; m<rows; m++){
        for( n=0; n<cols; n++){
            unsigned int l;     // index for extracting the value from neon register
            temp_acc_fix = vdupq_n_u16(0);  // initialize the neon register with zeros value
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                kernelSum_fix = kernelBase_fix;
            }
            else if(n <=8){
                kernelSum_fix += new_kernel_fix[8-n];
            }
            else if(n>=cols-8){
                kernelSum_fix -=new_kernel_fix[cols-n+8];
            }

            // multiply the neighbour kernel with the corresponding pixel value
            // skip is used to skip the respective blurred pixel (the center of windowsize)
            for( acc=0; acc<2; acc++)
            {
                int skip=0;
                if(acc>=1)
                {
                    skip=1;
                }
                // load 4 pixel value
                neon_image_fix = vld1q_u16((uint16_t const *)&new_image_fix[m*new_cols+n+acc*8+skip]);
                
                // load 4 kernel value
                neon_kernel_fix = vld1q_u16((uint16_t const *)&new_kernel_fix[acc*8+skip]);
                
                // multiplty accumulate, get the sum for 1 pixel (before normalization)
                temp_acc_fix = vmlaq_u16(temp_acc_fix,neon_image_fix,neon_kernel_fix);
            }
            
            for( l=0; l<=7; l++){   
                        
                // extract the vector and accumulate it to single variable
                temp_out_fix += vgetq_lane_u16(temp_acc_fix,l ); 
            }
            // add the gaussian value of the center point
            temp_out_fix += new_image_fix[m*new_cols+n+8] * new_kernel_fix[8];
            
            // normalize
            temp_out_fix /= kernelSum_fix;
            // gaussian blur value for the n pixel
            tempbuf_fix[m*cols+n] = temp_out_fix;
            
            temp_out_fix=0; 
        }
    }

    
    // /****************************************************************************
    // * Blur in the x - direction.
    // ****************************************************************************/

    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/

    // modify the image, add eight 0 in the top and bottom side (each side 8)
    new_rows=rows+16;
    new_image_col_fix = (uint16_t*)malloc(new_rows*cols*sizeof(uint16_t));
    for( i =0; i<cols; i++){
        memset(&new_image_col_fix[i*new_rows],0,8*sizeof(uint16_t));

        for( j=0; j<rows;j++){
            new_image_col_fix[i*new_rows+8+j] = tempbuf_fix[j*cols+i];
            
        }
        memset(&new_image_col_fix[i*new_rows+8+rows],0,8*sizeof(uint16_t));
    }

    // iteration for each pixel in the new image
    kernelBase_fix = 0; 
    for( k=8; k<=16; k++){
        kernelBase_fix += new_kernel_fix[k];
    }

    for(m=0; m<cols; m++){
        for( n=0; n<rows; n++){
            unsigned int l; // index for extracting the value from neon register
            temp_acc_fix = vdupq_n_u16(0);  // initialize the neon register with zeros value
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                kernelSum_fix = kernelBase_fix;
            }
            else if(n <=8){
                kernelSum_fix += new_kernel_fix[8-n];
            }
            else if(n>=rows-8){
                kernelSum_fix -=new_kernel_fix[rows-n+8];
            }

            // multiply the neighbour kernel with the corresponding pixel value
            // skip is used to skip the respective blurred pixel (the center of windowsize)
            for( acc=0; acc<2; acc++)
            {
                int skip=0;
                if(acc>=1)
                {
                    skip=1;
                }
                // load 8 pixel value
                neon_image_fix = vld1q_u16((uint16_t const *)&new_image_col_fix[m*new_rows+n+acc*8+skip]);
                // load 8 kernel value
                neon_kernel_fix = vld1q_u16((uint16_t const *)&new_kernel_fix[acc*8+skip]);
                // multiplty accumulate, get the sum for 1 pixel (before normalization)
                temp_acc_fix = vmlaq_u16(temp_acc_fix,neon_image_fix,neon_kernel_fix);
            }
            
            // extract the vector and accumulate it to single variable
            for( l=0; l<=7; l++){   
                temp_out_fix += vgetq_lane_u16(temp_acc_fix,l ); 

            }
            // add the gaussian value of the center point
            temp_out_fix += new_image_col_fix[m*new_rows+n+8] * new_kernel_fix[8];
            // use a bigger datatype to avoid overflow when boost blur process
            temp_out_fix_y = (uint32_t)temp_out_fix;
            // boost the blur factor and normalize 
            temp_out_fix_y = (temp_out_fix_y * 90) / (uint32_t)kernelSum_fix + 1;
            // gaussian blur value for the n pixel
            smoothed[n*cols+m] = (unsigned short int )temp_out_fix_y;
            temp_out_fix=0;
            temp_out_fix_y=0; 
        }
    }

    free(new_image_fix);
    free(new_image_col_fix);
    free(tempbuf_fix);
    return smoothed;
}

/*******************************************************************************
* PROCEDURE: make_gaussian_kernel using neon
* PURPOSE: Create a one dimensional gaussian kernel using NEON
* NAME: Mike Heath & Angga Irawan
* DATE: 2/15/96
*******************************************************************************/

void make_gaussian_kernel_neon_fix(float sigma, float **kernel, int *windowsize, int qformat)
{
    int i, center;
    float x, fx, sum=0.0;

    *windowsize = 1 + 2 * ceil(2.5 * sigma);
    center = (*windowsize) / 2;

    if((*kernel = (float *) malloc((*windowsize)* sizeof(float))) == NULL)
    {
        fprintf(stderr, "Error callocing the gaussian kernel array.\n");
        exit(1);
    }

    for(i=0; i<(*windowsize); i++)
    {
        x = (float)(i - center);
        fx = pow(2.71828, -0.5*x*x/(sigma*sigma)) / (sigma * sqrt(6.2831853));
        (*kernel)[i] = fx;
        sum += fx;
    }

    for(i=0; i<(*windowsize); i++) (*kernel)[i] /= sum;
    for(i=0; i<(*windowsize); i++) (*kernel)[i] *=qformat;

    if(VERBOSE)
    {
        printf("The filter coefficients are:\n");
        for(i=0; i<(*windowsize); i++)
            printf("kernel[%d] = %f\n", i, (*kernel)[i]);
    }
}