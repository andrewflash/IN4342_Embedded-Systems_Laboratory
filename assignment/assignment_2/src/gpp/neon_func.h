#ifndef neon_func_H
#define neon_func_H

unsigned short int* gaussian_smooth_neon(unsigned char *image, int rows, int cols, float sigma, int complete_rows);
unsigned short int* gaussian_smooth_neon_u16(unsigned char *image, int rows, int cols, float sigma, int complete_rows);
unsigned short int* gaussian_smooth_neon_u32(unsigned char *image, int rows, int cols, float sigma, int complete_rows);

void make_gaussian_kernel_neon(float sigma, float **kernel, int *windowsize);
void make_gaussian_kernel_neon_fix(float sigma, float **kernel, int *windowsize, int qformat);

#endif /* !defined (neon_func_H) */