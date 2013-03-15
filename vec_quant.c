/*Using hyperplanes(not sure if this is the proper term) to describle the boundary instead of caculate distance seprately*/
#include <stdlib.h>
#include <stdio.h>
#include "cv.h"
#include "highgui.h"
#include "time.h"

#define SAVE_IMAGES_ON_THE_FLY 0

#define PALETTE_L   128
#define THRESHOLD_D 0.001
#define COLORS_IN_CHANNEL (1<<(8*sizeof(uchar)))
//Macros
#define GET_CHANNEL(img, row, col, channel)	      ((uchar*)(img->imageData+row*img->widthStep))[col*img->nChannels+channel]
#define GET_PIXEL_POINTER(img, row, col)	      (((uchar*)(img->imageData+row*img->widthStep))+col*img->nChannels)   //Return a pointer of type uchar to the specified pixel 

enum{ CHANNEL_B, CHANNEL_G, CHANNEL_R, CHANNEL_NUM };

//Arrays
uchar palette[PALETTE_L][CHANNEL_NUM];

int   colors_counter [COLORS_IN_CHANNEL][COLORS_IN_CHANNEL][COLORS_IN_CHANNEL] = {0};  //R,G,B , Not so generalized
int   colors_divider [COLORS_IN_CHANNEL][COLORS_IN_CHANNEL][COLORS_IN_CHANNEL] = {0};  //Not so generalized, LUT (R,G,B)-> palette index
int   hyperplanes[PALETTE_L-1][PALETTE_L][CHANNEL_NUM+1] = {0};

//Functions
unsigned int  calc_distortion();
int	      calc_euclidean_dist(int*, uchar*);
void	      get_hyperplane(uchar*, uchar*, int*);
void	      init_codewords();
void	      init_colors_counter(IplImage*);
void	      update_hyperplanes(); 
void	      update_image(IplImage*, IplImage*);
void	      update_partitions();

unsigned int 
calc_distortion() //Not so generalized
{
  int color[CHANNEL_NUM];  //these 3 elements in color is actually r, g, b
  int i;
  unsigned int dist = 0;
  for(color[0] = 0; color[0] < COLORS_IN_CHANNEL; color[0]++)
    for(color[1] = 0; color[1] < COLORS_IN_CHANNEL; color[1]++)
      for(color[2] = 0; color[2] < COLORS_IN_CHANNEL; color[2]++)
	if(i=colors_counter[color[0]][color[1]][color[2]])
	  dist += i*calc_euclidean_dist(color, palette[colors_divider[color[0]][color[1]][color[2]]]);
  return(dist);
}

int
calc_euclidean_dist(int* Color1, uchar* Color2)
{
  int i, ret;
  ret = 0;
  for(i = 0; i < CHANNEL_NUM; i++)
    ret+=(Color1[i]-Color2[i])*(Color1[i]-Color2[i]);
  return(ret);
}

void 
get_hyperplane(uchar* Color0, uchar* Color1, int* A)
//Given two colors, calculate the boundary between them. NN based
{
  int i; 
  A[CHANNEL_NUM] = 0;
  for(i = 0; i<CHANNEL_NUM; i++)
  {
    A[i] = 2*(Color1[i]-Color0[i]);
    A[CHANNEL_NUM] += Color0[i]*Color0[i]-Color1[i]*Color1[i];
  }
}

void
init_codewords()
//Initialize the palette array with uniformly distributed(?) color values
{
  int i, j;
  int q = COLORS_IN_CHANNEL*COLORS_IN_CHANNEL*COLORS_IN_CHANNEL/PALETTE_L;
  uchar r, g, b;
  for(i = 0; i < PALETTE_L; i++)
  {
    j = (i*q + q>>2);//index*q+q/2 
    r = j/(COLORS_IN_CHANNEL*COLORS_IN_CHANNEL);
    g = j/COLORS_IN_CHANNEL%COLORS_IN_CHANNEL;
    b = j;

    palette[i][CHANNEL_R] = r;     
    palette[i][CHANNEL_G] = g;     
    palette[i][CHANNEL_B] = b;     
  }
}

void
init_colors_counter(IplImage* image)
/*Color Statistics*/
{
  int i, j;
  for(i = 0; i < image->height; i++)
    for(j = 0; j < image->width; j++)
      colors_counter[GET_CHANNEL(image, i, j, CHANNEL_R)]
		    [GET_CHANNEL(image, i, j, CHANNEL_G)]
		    [GET_CHANNEL(image, i, j, CHANNEL_B)]
		    ++;
}

void
update_codewords()
{
  int palette_counter[PALETTE_L] = {0};
  int palette_value  [PALETTE_L][CHANNEL_NUM] = {0};
  int r,g,b;
  int i;
  for(r = 0; r < COLORS_IN_CHANNEL; r++)
    for(g = 0; g < COLORS_IN_CHANNEL; g++)
      for(b = 0; b < COLORS_IN_CHANNEL; b++)
	if(i = colors_counter[r][g][b])
	{
	  palette_counter[colors_divider[r][g][b]] += i;
	  palette_value  [colors_divider[r][g][b]][CHANNEL_R] += r*i;
	  palette_value  [colors_divider[r][g][b]][CHANNEL_G] += g*i;
	  palette_value  [colors_divider[r][g][b]][CHANNEL_B] += b*i;
	}
  for(i = 0; i < PALETTE_L; i++)
    if(r = palette_counter[i])
    {
      palette[i][CHANNEL_R] = palette_value[i][CHANNEL_R]/r;
      palette[i][CHANNEL_G] = palette_value[i][CHANNEL_G]/r;
      palette[i][CHANNEL_B] = palette_value[i][CHANNEL_B]/r;
    }
}

void
update_hyperplanes()
//calculate all hyperplanes and fill them into an array
{
  int i, j;
  for(i = 0; i < PALETTE_L-1; i++)
    for(j = i+1; j < PALETTE_L; j++)
      get_hyperplane(palette[i], palette[j], hyperplanes[i][j]);
}

void
update_image(IplImage* img, IplImage* des)
{
  int col,row;
  uchar *r, *b, *g;
  for(row = 0; row < img->height; row++)
    for(col = 0; col < img->width; col++)
    {
      r = GET_PIXEL_POINTER(img, row, col) + CHANNEL_R;
      g = GET_PIXEL_POINTER(img, row, col) + CHANNEL_G;
      b = GET_PIXEL_POINTER(img, row, col) + CHANNEL_B;
      GET_CHANNEL(des, row, col, CHANNEL_R) = palette[colors_divider[*r][*g][*b]][CHANNEL_R];
      GET_CHANNEL(des, row, col, CHANNEL_G) = palette[colors_divider[*r][*g][*b]][CHANNEL_G];
      GET_CHANNEL(des, row, col, CHANNEL_B) = palette[colors_divider[*r][*g][*b]][CHANNEL_B];
    }
}

void
update_partitions()  //Not so generalized....
{
  update_hyperplanes();
  int r,g,b;
  int i, j, k;
  for(r = 0; r < COLORS_IN_CHANNEL; r++)
    for(g = 0; g < COLORS_IN_CHANNEL; g++)
      for(b = 0; b < COLORS_IN_CHANNEL; b++)
	if(colors_counter[r][g][b])
	{
	  j = 0;
	  k = 1;
	  for(i=0; i < PALETTE_L - 1; i++) //PALETTE_L -1 Comparasions have to be made
	    (hyperplanes[j][k][CHANNEL_R]*r+hyperplanes[j][k][CHANNEL_G]*g+hyperplanes[j][k][CHANNEL_B]*b + hyperplanes[j][k][CHANNEL_NUM] <= 0) ? k++ : j++;
	  colors_divider[r][g][b] = j;
	}
}

int
main(int argc, char** argv)
{
  int ret_val = EXIT_FAILURE;
  unsigned int d0;
  unsigned int d = 0;
  /*Error Handling*/
  if(argc < 2) 
    goto NO_ENOUGH_ARG;
  IplImage* image	= cvLoadImage(argv[1], -1);  //Load the image as is
  IplImage* quant_image	= cvCloneImage(image);

  /*Error Handling*/
  if(!image|| image->nChannels != CHANNEL_NUM)
    goto UNKNOWN_ERROR;

  init_colors_counter(image);
  /*Initialize the PALETTE_L points & partitions*/
  init_codewords(); 
  update_partitions();
  d0 = calc_distortion();
  printf("d0 = %d\n", d0);

  int i = 0;
  time_t t = clock();
  while(abs(d-d0) >= THRESHOLD_D*d0)
  {
#if SAVE_IMAGES_ON_THE_FLY
    update_image(image,quant_image);
    char filename[11]; //8.3
    if(!sprintf(filename, "long%d.png", i) ||
    !cvSaveImage(filename, quant_image, 0))
      printf("Error occur while saving image.");
#endif
    if(++i > 80) break;
    update_codewords();
    update_partitions();
    d0 = d;
    //printf("the %d th iteration, %d \n", i, d = calc_distortion());
    d = calc_distortion();
  }
  t = clock()-t;
  printf("After %d iterations, d0 = %d, d1 = %d.\nTime elapsed: %d cycles.\n", i, d0, d, t);
  update_image(image,quant_image);
#if SAVE_IMAGES_ON_THE_FLY
  char filename[11]; //8.3
  if(!sprintf(filename, "long%d.png", i) ||
  !cvSaveImage(filename, quant_image, 0))
    printf("Error occur while saving image.");
#endif

  cvNamedWindow("Long cat is loooooooooooooooooooooooooong", CV_WINDOW_AUTOSIZE);
  cvShowImage  ("Long cat is loooooooooooooooooooooooooong", quant_image);
  cvWaitKey(0);
  ret_val = EXIT_SUCCESS;
UNKNOWN_ERROR:
  cvReleaseImage(&quant_image);
  cvReleaseImage(&image);
NO_ENOUGH_ARG:
  exit(ret_val);
}
