#define VERBOSE 0

#define BOOSTBLURFACTOR 90.0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <arm_neon.h>
#include "neon_func.h"

unsigned short int* gaussian_smooth_neon(unsigned char *image, int rows, int cols, float sigma, int complete_rows)
{
    int windowsize,/*r, c, rr, cc,*/     /* Counter variables. */
        center;/*windowsize,*/        /* Dimension of the gaussian kernel. */
        /*center;*/            /* Half of the windowsize. */
    float *tempim,//*tempim1,        /* Buffer for separable filter gaussian smoothing. */
          *kernel;//,        /* A one dimensional gaussian kernel. */
          //dot,            /* Dot product summing variable. */
          //sum;            /* Sum of the kernel weights variable. */
    
    #ifdef NEONTEST
    unsigned short int* smoothedim1;// just for the pupose of testing 
    #endif
    unsigned short int* smoothedim;

    // Blur in x-direction
    float *new_image;
    float *new_image_col;
    unsigned int new_cols;    
    unsigned int a; 
    unsigned int i, k; 
    unsigned int m; 
    unsigned int n, j;

    int floop;
    float new_kernel[17];

    float32x4_t neon_input;
    float32x4_t neon_filter;
    float32x4_t temp_sum;

    #ifdef NEONTEST
    float32x2_t tempUpper;
    float32x2_t tempLower; 
    float32_t zero = 0;
    #endif

    float32_t temp_output;
    
    float Basekernel = 0.0f;
    float kernelSum = 0.0f;

    unsigned int new_rows;
    
    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    if(VERBOSE) printf("   Computing the gaussian smoothing kernel.\n");
    make_gaussian_kernel_neon(sigma, &kernel, &windowsize);
    center = windowsize / 2;

    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/
    if((tempim = (float *) malloc(rows*cols* sizeof(float))) == NULL)
    {
        fprintf(stderr, "Error allocating the buffer image.\n");
        exit(1);
    }
    
    if(((smoothedim) = (unsigned short int *) malloc(complete_rows*cols*sizeof(short int))) == NULL)
    {
        fprintf(stderr, "Error allocating the smoothed image.\n");
        exit(1);
    }
 
    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/

    // modify the kernel, add two additional value of 0 as the first and last element
    for (floop = 0 ; floop < 17 ; floop++)
    {
        if(floop == 0 || floop == 16 )
            new_kernel[floop] = 0 ;
        else
            new_kernel [floop] = kernel[floop -1];  
    }

    // modify the image, add eight 0 in the left and right side (each side 8)
    new_cols=cols+16;
    
    new_image = (float*)malloc(new_cols*rows*sizeof(float));
    
    for(i=0; i<rows; i++){
        memset(&new_image[i*new_cols],0,8*sizeof(float));
        
        for( k=0; k<cols;k++){
            new_image[i*new_cols+8+k] = (float)image[i*cols+k];
            
        }
        memset(&new_image[i*new_cols+8+cols],0,8*sizeof(float));
    }

    //printf("\n the ouput image\n");
    // basekernel = sum of kernel value for the first colomn of image
    for( a=8; a<=16; a++){
        Basekernel += new_kernel[a];
    }


    for(m=0; m<rows; m++){
        for( n=0; n<cols; n++){
            unsigned int t;
        
            temp_sum = vdupq_n_f32(0);
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                kernelSum = Basekernel;
            }
            else if(n <=8){
                kernelSum += new_kernel[8-n];
            }
            else if(n>=cols-8){
                kernelSum -=new_kernel[cols-n+8];
            }

            // multiply the kernel with the pixel value
            // kk is used to skip the respective blurred pixel (the center of windowsize)
            for( j=0; j<4; j++)
            {
                int kk=0;
                if(j>=2)
                {
                    kk=1;
                }
                // load 4 pixel value
                neon_input = vld1q_f32((float32_t const *)&new_image[m*new_cols+n+j*4+kk]);
                
                // load 4 kernel value
                neon_filter = vld1q_f32((float32_t const *)&new_kernel[j*4+kk]);
                
                // multiplty accumulate, get the sum for 1 pixel (before average)
                temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
            }
            
            
            for( t=0; t<=3; t++){   
                        
                // extract the vector and accumulate it to single float variable
                temp_output += vgetq_lane_f32(temp_sum,t ); 
            }
            // add the gaussian value of the center point
            temp_output += new_image[m*new_cols+n+8] * new_kernel[8];
            
            // normalize
            temp_output /= kernelSum;
            
            // gaussian blur value for the n pixel
            tempim[m*cols+n] = temp_output;
            
            temp_output=0; 
        }
    }

    
    // /****************************************************************************
    // * Blur in the x - direction.
    // ****************************************************************************/

    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/

    new_rows=rows+16;
    new_image_col = (float*)malloc(new_rows*cols*sizeof(float));
    
    if(VERBOSE) printf("   Bluring the image in the Y-direction.\n");

    for( i =0; i<cols; i++){
    //actually nember of new rows are the number of columns here 
        memset(&new_image_col[i*new_rows],0,8*sizeof(float));
        
        for( k=0; k<rows;k++){
            //new_image_col[i*new_rows+8+k] = imagetest1[k*cols+i];
            new_image_col[i*new_rows+8+k] = tempim[k*cols+i];
        }
        memset(&new_image_col[i*new_rows+8+rows],0,8*sizeof(float));
    }

    Basekernel = 0.0; 
    for( a=8; a<=16; a++){
        Basekernel += new_kernel[a];
    }

    for(m=0; m<cols; m++){
        for( n=0; n<rows; n++){
            unsigned int t;       
            temp_sum = vdupq_n_f32(0);
            if(n==0){
                kernelSum = Basekernel;
            }
            else if(n <=8){
                kernelSum += new_kernel[8-n];
            }
            else if(n>=rows-8){
                kernelSum -=new_kernel[rows-n+8];
            }

            for( j=0; j<4; j++)
            {
                int kk=0;
                if(j>=2)
                {
                    kk=1;
                }
                neon_input = vld1q_f32((float32_t const *)&new_image_col[m*new_rows+n+j*4+kk]);
                neon_filter = vld1q_f32((float32_t const *)&new_kernel[j*4+kk]);
                temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
            }
            
            for( t=0; t<=3; t++){   
                temp_output += vgetq_lane_f32(temp_sum,t );
            }
            
            temp_output += new_image_col[m*new_rows+n+8] * new_kernel[8];
            temp_output = (temp_output * 90) / kernelSum + 0.5;
            
            smoothedim[n*cols+m] = (unsigned short int )temp_output;
            temp_output=0; 
        }
    }

    free(new_image);
    free(new_image_col);
    free(tempim);
    free(kernel);
    return smoothedim;
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
    for(i=0; i<(*windowsize); i++) (*kernel)[i] *=2^qformat;

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
unsigned short int* gaussian_smooth_neon_u32(unsigned char *image, int rows, int cols, float sigma, int complete_rows)
{
    int /*r, c, rr, cc,*/     /* Counter variables. */
        windowsize,        /* Dimension of the gaussian kernel. */
        center;            /* Half of the windowsize. */
    // float *tempim,//*tempim1,        /* Buffer for separable filter gaussian smoothing. */
    //       *kernel;//,        /* A one dimensional gaussian kernel. */
    //       //dot,            /* Dot product summing variable. */
    //       //sum;            /* Sum of the kernel weights variable. */
    float *kernel;
    uint32_t  new_kernel_fix[17];
    uint32_t  *tempim_fix;
    int qformat = 16;

    unsigned short int* smoothedim;
    
    // float *new_image;
    // float *new_image_col;
    uint32_t  *new_image_fix;
    uint32_t  *new_image_col_fix;
    
    unsigned int new_cols;
    unsigned int a; 
    unsigned int i, k; 
    unsigned int m; 
    unsigned int n, j;
    
    // float32x4_t neon_input;
    // float32x4_t neon_filter;
    // float32x4_t temp_sum;

    uint32x4_t neon_input_fix;
    uint32x4_t neon_filter_fix;
    uint32x4_t temp_sum_fix;
    
    // #ifdef NEONTEST
    // float32x2_t tempUpper;
    // float32x2_t tempLower; 
    // float32_t zero = 0;
    // #endif

    //float32_t temp_output;
    uint32_t temp_output_fix;
    
    // float Basekernel = 0.0f;
    // float kernelSum = 0.0f;
    uint32_t  Basekernel_fix = 0;
    uint32_t  kernelSum_fix = 0;

    unsigned int new_rows;
      
    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    if(VERBOSE) printf("   Computing the gaussian smoothing kernel.\n");
    make_gaussian_kernel_neon_fix(sigma, &kernel, &windowsize, qformat);
    center = windowsize / 2;

    // // Later we can change it to make_gaussian_kernel_fix
    // // // kernel Q10
    // // new_kernel_fix[0] = 0;
    // // new_kernel_fix[1] = 3;
    // // new_kernel_fix[2] = 9;
    // // new_kernel_fix[3] = 22;
    // // new_kernel_fix[4] = 46;
    // // new_kernel_fix[5] = 80;
    // // new_kernel_fix[6] = 119;
    // // new_kernel_fix[7] = 151;
    // // new_kernel_fix[8] = 164;
    // // new_kernel_fix[9] = 151;
    // // new_kernel_fix[10] = 119;
    // // new_kernel_fix[11] = 80;
    // // new_kernel_fix[12] = 46;
    // // new_kernel_fix[13] = 22;
    // // new_kernel_fix[14] = 9;
    // // new_kernel_fix[15] = 3;
    // // new_kernel_fix[16] = 0;

    // // kernel Q16
    // new_kernel_fix[0] = 0;
    // new_kernel_fix[1] = 208;
    // new_kernel_fix[2] = 589;
    // new_kernel_fix[3] = 1419;
    // new_kernel_fix[4] = 2915;
    // new_kernel_fix[5] = 5103;
    // new_kernel_fix[6] = 7613;
    // new_kernel_fix[7] = 9678;
    // new_kernel_fix[8] = 10485;
    // new_kernel_fix[9] = 9678;
    // new_kernel_fix[10] = 7613;
    // new_kernel_fix[11] = 5103;
    // new_kernel_fix[12] = 2915;
    // new_kernel_fix[13] = 1419;
    // new_kernel_fix[14] = 589;
    // new_kernel_fix[15] = 208;
    // new_kernel_fix[16] = 0;

    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/
    // if((tempim = (float *) malloc(rows*cols* sizeof(float))) == NULL)
    // {
    //     fprintf(stderr, "Error allocating the buffer image.\n");
    //     exit(1);
    // }

    if((tempim_fix = (uint32_t  *) malloc(rows*cols* sizeof(uint32_t ))) == NULL)
    {
        fprintf(stderr, "Error allocating the fix buffer image.\n");
        exit(1);
    }

    if(((smoothedim) = (unsigned short int *) malloc(complete_rows*cols*sizeof(short int))) == NULL)
    {
        fprintf(stderr, "Error allocating the smoothed image.\n");
        exit(1);
    }
 
    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/
    //int loop; 
    
    //int floop;
    //float new_kernel[17];

    // modify the kernel, add two additional value of 0 as the first and last element
    // for (floop = 0 ; floop < 17 ; floop++)
    // {
    //     if(floop == 0 || floop == 16 )
    //         new_kernel[floop] = 0 ;
    //     else
    //         new_kernel [floop] = kernel[floop -1];  
    // }

    // modify the image, add eight 0 in the left and right side (each side 8)
    new_cols=cols+16;
    
    //new_image = (float*)malloc(new_cols*rows*sizeof(float));
    new_image_fix = (uint32_t *)malloc(new_cols*rows*sizeof(uint32_t ));
    
    for(i=0; i<rows; i++){
        //memset(&new_image[i*new_cols],0,8*sizeof(float));
        memset(&new_image_fix[i*new_cols],0,8*sizeof(uint32_t ));

        for( k=0; k<cols;k++){
            //new_image[i*new_cols+8+k] = (float)image[i*cols+k];
            new_image_fix[i*new_cols+8+k] = (uint32_t )image[i*cols+k];
        }
        //memset(&new_image[i*new_cols+8+cols],0,8*sizeof(float));
        memset(&new_image_fix[i*new_cols+8+cols],0,8*sizeof(uint32_t ));
    }

    //printf("\n the ouput image\n");
    // basekernel = sum of kernel value for the first colomn of image
    for( a=8; a<=16; a++){
        //Basekernel += new_kernel[a];
        Basekernel_fix += new_kernel_fix[a];
    }


    for(m=0; m<rows; m++){
        for( n=0; n<cols; n++){
            unsigned int t;
            //temp_sum = vdupq_n_f32(0);
            temp_sum_fix = vdupq_n_u32(0);
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                //kernelSum = Basekernel;
                kernelSum_fix = Basekernel_fix;
            }
            else if(n <=8){
                //kernelSum += new_kernel[8-n];
                kernelSum_fix += new_kernel_fix[8-n];
            }
            else if(n>=cols-8){
                //kernelSum -=new_kernel[cols-n+8];
                kernelSum_fix -=new_kernel_fix[cols-n+8];
            }

            // multiply the kernel with the pixel value
            // kk is used to skip the respective blurred pixel (the center of windowsize)
            for( j=0; j<4; j++)
            {
                int kk=0;
                if(j>=2)
                {
                    kk=1;
                }
                // load 4 pixel value
                //neon_input = vld1q_f32((float32_t const *)&new_image[m*new_cols+n+j*4+kk]);
                neon_input_fix = vld1q_u32((uint32_t const *)&new_image_fix[m*new_cols+n+j*4+kk]);
                
                // load 4 kernel value
                //neon_filter = vld1q_f32((float32_t const *)&new_kernel[j*4+kk]);
                neon_filter_fix = vld1q_u32((uint32_t const *)&new_kernel_fix[j*4+kk]);
                
                // multiplty accumulate, get the sum for 1 pixel (before average)
                //temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
                temp_sum_fix = vmlaq_u32(temp_sum_fix,neon_input_fix,neon_filter_fix);
            }
    
            for( t=0; t<=3; t++){   
                        
                // extract the vector and accumulate it to single float variable
                //temp_output += vgetq_lane_f32(temp_sum,t ); 
                temp_output_fix += vgetq_lane_u32(temp_sum_fix,t ); 
            }
            // add the gaussian value of the center point
            //temp_output += new_image[m*new_cols+n+8] * new_kernel[8];
            temp_output_fix += new_image_fix[m*new_cols+n+8] * new_kernel_fix[8];
            
            // normalize
            //temp_output /= kernelSum;
            temp_output_fix /= kernelSum_fix;
            // gaussian blur value for the n pixel
            //tempim[m*cols+n] = temp_output;
            tempim_fix[m*cols+n] = temp_output_fix;
            
            //temp_output=0;
            temp_output_fix=0; 
        }
    }

    
    // /****************************************************************************
    // * Blur in the x - direction.
    // ****************************************************************************/

    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/

    new_rows=rows+16;
    //new_image_col = (float*)malloc(new_rows*cols*sizeof(float));
    new_image_col_fix = (uint32_t *)malloc(new_rows*cols*sizeof(uint32_t ));
    
    if(VERBOSE) printf("   Bluring the image in the Y-direction.\n");

    for( i =0; i<cols; i++){//actually nember of new rows are the number of columns here 
        //memset(&new_image_col[i*new_rows],0,8*sizeof(float));
        memset(&new_image_col_fix[i*new_rows],0,8*sizeof(uint32_t ));

        for( k=0; k<rows;k++){
            //new_image_col[i*new_rows+8+k] = imagetest1[k*cols+i];
            //new_image_col[i*new_rows+8+k] = tempim[k*cols+i];
            new_image_col_fix[i*new_rows+8+k] = tempim_fix[k*cols+i];
            
        }
        //memset(&new_image_col[i*new_rows+8+rows],0,8*sizeof(float));
        memset(&new_image_col_fix[i*new_rows+8+rows],0,8*sizeof(uint32_t ));
    }

    //Basekernel = 0.0; 
    Basekernel_fix = 0; 
    for( a=8; a<=16; a++){
        //Basekernel += new_kernel[a];
        Basekernel_fix += new_kernel_fix[a];
    }

    for(m=0; m<cols; m++){
        for( n=0; n<rows; n++){
            unsigned int t;
        
            //temp_sum = vdupq_n_f32(0);
            temp_sum_fix = vdupq_n_u32(0);
            if(n==0){
                //kernelSum = Basekernel;
                kernelSum_fix = Basekernel_fix;
            }
            else if(n <=8){
                //kernelSum += new_kernel[8-n];
                kernelSum_fix += new_kernel_fix[8-n];
            }
            else if(n>=rows-8){
                //kernelSum -=new_kernel[rows-n+8];
                kernelSum_fix -=new_kernel_fix[rows-n+8];
            }

            for( j=0; j<4; j++)
            {
                int kk=0;
                if(j>=2)
                {
                    kk=1;
                }
                //neon_input = vld1q_f32((float32_t const *)&new_image_col[m*new_rows+n+j*4+kk]);
                //neon_filter = vld1q_f32((float32_t const *)&new_kernel[j*4+kk]);
                //temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
                neon_input_fix = vld1q_u32((uint32_t const *)&new_image_col_fix[m*new_rows+n+j*4+kk]);
                neon_filter_fix = vld1q_u32((uint32_t const *)&new_kernel_fix[j*4+kk]);
                temp_sum_fix = vmlaq_u32(temp_sum_fix,neon_input_fix,neon_filter_fix);
            }
            
            
            for( t=0; t<=3; t++){   
                        
                //temp_output += vgetq_lane_f32(temp_sum,t );
                temp_output_fix += vgetq_lane_u32(temp_sum_fix,t ); 

            }
            //temp_output += new_image_col[m*new_rows+n+8] * new_kernel[8];
            //temp_output = (temp_output * 90) / kernelSum + 0.5;
            temp_output_fix += new_image_col_fix[m*new_rows+n+8] * new_kernel_fix[8];
            temp_output_fix = (temp_output_fix * 90) / kernelSum_fix + 1;
            
            //smoothedim[n*cols+m] = (unsigned short int )temp_output;
            //temp_output=0; 
            smoothedim[n*cols+m] = (unsigned short int )temp_output_fix;
            temp_output_fix=0; 
        }
    }

    //free(new_image);
    //free(new_image_col);
    //free(tempim);
    free(new_image_fix);
    free(new_image_col_fix);
    free(tempim_fix);
    return smoothedim;
}

/*******************************************************************************
* FUNCTION: gaussian_smooth_neon_u16
* PURPOSE: Gaussian smooth function using neon instrinsic, unsigned short integer format.
* NAME: Angga Irawan
* DATE: 
*******************************************************************************/

unsigned short int* gaussian_smooth_neon_u16(unsigned char *image, int rows, int cols, float sigma, int complete_rows)
{
    int /*r, c, rr, cc,*/     /* Counter variables. */
        windowsize,        /* Dimension of the gaussian kernel. */
        center;            /* Half of the windowsize. */
    // float *tempim,//*tempim1,        /* Buffer for separable filter gaussian smoothing. */
    //       *kernel;//,        /* A one dimensional gaussian kernel. */
    //       //dot,            /* Dot product summing variable. */
    //       //sum;            /* Sum of the kernel weights variable. */
    float *kernel;
    uint16_t new_kernel_fix[17];
    uint16_t *tempim_fix;
    int qformat = 8;
    
    unsigned short int* smoothedim;
    
    // float *new_image;
    // float *new_image_col;
    uint16_t *new_image_fix;
    uint16_t *new_image_col_fix;

    unsigned int new_cols;
    unsigned int a; 
    unsigned int i, k; 
    unsigned int m; 
    unsigned int n, j;
    
    // float32x4_t neon_input;
    // float32x4_t neon_filter;
    // float32x4_t temp_sum;

    uint16x8_t neon_input_fix;
    uint16x8_t neon_filter_fix;
    uint16x8_t temp_sum_fix;
    
    // #ifdef NEONTEST
    // float32x2_t tempUpper;
    // float32x2_t tempLower; 
    // float32_t zero = 0;
    // #endif

    //float32_t temp_output;
    uint16_t temp_output_fix;
    uint32_t temp_output_fix_y;
    
    // float Basekernel = 0.0f;
    // float kernelSum = 0.0f;
    uint16_t Basekernel_fix = 0;
    uint16_t kernelSum_fix = 0;

    unsigned int new_rows;
    
    /****************************************************************************
    * Create a 1-dimensional gaussian smoothing kernel.
    ****************************************************************************/
    if(VERBOSE) printf("   Computing the gaussian smoothing kernel.\n");
    make_gaussian_kernel_neon_fix(sigma, &kernel, &windowsize, qformat);
    //windowsize = 1 + 2 * ceil(2.5 * sigma);
    center = windowsize / 2;

    // // kernel Q8
    // new_kernel_fix[0] = 0;
    // new_kernel_fix[1] = 1;
    // new_kernel_fix[2] = 2;
    // new_kernel_fix[3] = 6;
    // new_kernel_fix[4] = 11;
    // new_kernel_fix[5] = 20;
    // new_kernel_fix[6] = 30;
    // new_kernel_fix[7] = 38;
    // new_kernel_fix[8] = 41;
    // new_kernel_fix[9] = 38;
    // new_kernel_fix[10] = 30;
    // new_kernel_fix[11] = 20;
    // new_kernel_fix[12] = 11;
    // new_kernel_fix[13] = 6;
    // new_kernel_fix[14] = 2;
    // new_kernel_fix[15] = 1;
    // new_kernel_fix[16] = 0;

    /****************************************************************************
    * Allocate a temporary buffer image and the smoothed image.
    ****************************************************************************/
    // if((tempim = (float *) malloc(rows*cols* sizeof(float))) == NULL)
    // {
    //     fprintf(stderr, "Error allocating the buffer image.\n");
    //     exit(1);
    // }

    if((tempim_fix = (uint16_t *) malloc(rows*cols* sizeof(uint16_t))) == NULL)
    {
        fprintf(stderr, "Error allocating the fix buffer image.\n");
        exit(1);
    }

    if(((smoothedim) = (unsigned short int *) malloc(complete_rows*cols*sizeof(short int))) == NULL)
    {
        fprintf(stderr, "Error allocating the smoothed image.\n");
        exit(1);
    }
 
    /****************************************************************************
    * Blur in the x - direction.
    ****************************************************************************/
    //int loop; 
    
    //int floop;
    //float new_kernel[17];

    // modify the kernel, add two additional value of 0 as the first and last element
    // for (floop = 0 ; floop < 17 ; floop++)
    // {
    //     if(floop == 0 || floop == 16 )
    //         new_kernel[floop] = 0 ;
    //     else
    //         new_kernel [floop] = kernel[floop -1];  
    // }

    // modify the image, add eight 0 in the left and right side (each side 8)
    new_cols=cols+16;
    //new_image = (float*)malloc(new_cols*rows*sizeof(float));
    new_image_fix = (uint16_t*)malloc(new_cols*rows*sizeof(uint16_t));
    
    for(i=0; i<rows; i++){
        //memset(&new_image[i*new_cols],0,8*sizeof(float));
        memset(&new_image_fix[i*new_cols],0,8*sizeof(uint16_t));

        for( k=0; k<cols;k++){
            //new_image[i*new_cols+8+k] = (float)image[i*cols+k];
            new_image_fix[i*new_cols+8+k] = (uint16_t)image[i*cols+k];
        }
        //memset(&new_image[i*new_cols+8+cols],0,8*sizeof(float));
        memset(&new_image_fix[i*new_cols+8+cols],0,8*sizeof(uint16_t));
    }

    //printf("\n the ouput image\n");
    // basekernel = sum of kernel value for the first colomn of image
    for( a=8; a<=16; a++){
        //Basekernel += new_kernel[a];
        Basekernel_fix += new_kernel_fix[a];
    }


    for(m=0; m<rows; m++){
        for( n=0; n<cols; n++){
            unsigned int t;
            //temp_sum = vdupq_n_f32(0);
            temp_sum_fix = vdupq_n_u16(0);
            
            // determine sum of kernel based on number of kernel that still lie inside the picture frame
            if(n==0){
                //kernelSum = Basekernel;
                kernelSum_fix = Basekernel_fix;
            }
            else if(n <=8){
                //kernelSum += new_kernel[8-n];
                kernelSum_fix += new_kernel_fix[8-n];
            }
            else if(n>=cols-8){
                //kernelSum -=new_kernel[cols-n+8];
                kernelSum_fix -=new_kernel_fix[cols-n+8];
            }

            // multiply the kernel with the pixel value
            // kk is used to skip the respective blurred pixel (the center of windowsize)
            for( j=0; j<2; j++)
            {
                int kk=0;
                if(j>=1)
                {
                    kk=1;
                }
                // load 4 pixel value
                //neon_input = vld1q_f32((float32_t const *)&new_image[m*new_cols+n+j*4+kk]);
                neon_input_fix = vld1q_u16((uint16_t const *)&new_image_fix[m*new_cols+n+j*8+kk]);
                
                // load 4 kernel value
                //neon_filter = vld1q_f32((float32_t const *)&new_kernel[j*4+kk]);
                neon_filter_fix = vld1q_u16((uint16_t const *)&new_kernel_fix[j*8+kk]);
                
                // multiplty accumulate, get the sum for 1 pixel (before average)
                //temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
                temp_sum_fix = vmlaq_u16(temp_sum_fix,neon_input_fix,neon_filter_fix);
            }
            
            for( t=0; t<=7; t++){   
                        
                // extract the vector and accumulate it to single float variable
                //temp_output += vgetq_lane_f32(temp_sum,t ); 
                temp_output_fix += vgetq_lane_u16(temp_sum_fix,t ); 
            }
            // add the gaussian value of the center point
            //temp_output += new_image[m*new_cols+n+8] * new_kernel[8];
            temp_output_fix += new_image_fix[m*new_cols+n+8] * new_kernel_fix[8];
            
            // normalize
            //temp_output /= kernelSum;
            temp_output_fix /= kernelSum_fix;
            // gaussian blur value for the n pixel
            //tempim[m*cols+n] = temp_output;
            tempim_fix[m*cols+n] = temp_output_fix;
            
            //temp_output=0;
            temp_output_fix=0; 
        }
    }

    
    // /****************************************************************************
    // * Blur in the x - direction.
    // ****************************************************************************/

    /****************************************************************************
    * Blur in the y - direction.
    ****************************************************************************/

    new_rows=rows+16;
    //new_image_col = (float*)malloc(new_rows*cols*sizeof(float));
    new_image_col_fix = (uint16_t*)malloc(new_rows*cols*sizeof(uint16_t));
    
    if(VERBOSE) printf("   Bluring the image in the Y-direction.\n");

    for( i =0; i<cols; i++){//actually nember of new rows are the number of columns here 
        //memset(&new_image_col[i*new_rows],0,8*sizeof(float));
        memset(&new_image_col_fix[i*new_rows],0,8*sizeof(uint16_t));

        for( k=0; k<rows;k++){
            //new_image_col[i*new_rows+8+k] = imagetest1[k*cols+i];
            //new_image_col[i*new_rows+8+k] = tempim[k*cols+i];
            new_image_col_fix[i*new_rows+8+k] = tempim_fix[k*cols+i];
            
        }
        //memset(&new_image_col[i*new_rows+8+rows],0,8*sizeof(float));
        memset(&new_image_col_fix[i*new_rows+8+rows],0,8*sizeof(uint16_t));
    }

    //Basekernel = 0.0; 
    Basekernel_fix = 0; 
    for( a=8; a<=16; a++){
        //Basekernel += new_kernel[a];
        Basekernel_fix += new_kernel_fix[a];
    }

    for(m=0; m<cols; m++){
        for( n=0; n<rows; n++){
            unsigned int t;
            //temp_sum = vdupq_n_f32(0);
            temp_sum_fix = vdupq_n_u16(0);
            if(n==0){
                //kernelSum = Basekernel;
                kernelSum_fix = Basekernel_fix;
            }
            else if(n <=8){
                //kernelSum += new_kernel[8-n];
                kernelSum_fix += new_kernel_fix[8-n];
            }
            else if(n>=rows-8){
                //kernelSum -=new_kernel[rows-n+8];
                kernelSum_fix -=new_kernel_fix[rows-n+8];
            }

            for( j=0; j<2; j++)
            {
                int kk=0;
                if(j>=1)
                {
                    kk=1;
                }
                //neon_input = vld1q_f32((float32_t const *)&new_image_col[m*new_rows+n+j*4+kk]);
                //neon_filter = vld1q_f32((float32_t const *)&new_kernel[j*4+kk]);
                //temp_sum = vmlaq_f32(temp_sum,neon_input,neon_filter);
                neon_input_fix = vld1q_u16((uint16_t const *)&new_image_col_fix[m*new_rows+n+j*8+kk]);
                neon_filter_fix = vld1q_u16((uint16_t const *)&new_kernel_fix[j*8+kk]);
                temp_sum_fix = vmlaq_u16(temp_sum_fix,neon_input_fix,neon_filter_fix);
            }
            
            for( t=0; t<=7; t++){   
                        
                //temp_output += vgetq_lane_f32(temp_sum,t );
                temp_output_fix += vgetq_lane_u16(temp_sum_fix,t ); 

            }
            //temp_output += new_image_col[m*new_rows+n+8] * new_kernel[8];
            //temp_output = (temp_output * 90) / kernelSum + 0.5;
            temp_output_fix += new_image_col_fix[m*new_rows+n+8] * new_kernel_fix[8];
            temp_output_fix_y = (uint32_t)temp_output_fix;
            temp_output_fix_y = (temp_output_fix_y * 90) / (uint32_t)kernelSum_fix + 1;
            
            //smoothedim[n*cols+m] = (unsigned short int )temp_output;
            //temp_output=0; 
            smoothedim[n*cols+m] = (unsigned short int )temp_output_fix_y;
            temp_output_fix=0;
            temp_output_fix_y=0; 
        }
    }

    //free(new_image);
    //free(new_image_col);
    //free(tempim);
    //free(kernel);
    free(new_image_fix);
    free(new_image_col_fix);
    free(tempim_fix);
    return smoothedim;
}
