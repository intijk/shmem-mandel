#include <stdio.h>
#include <highgui.h>
double top1,top2,bottom1,bottom2,left1,left2,right1,right2,topM,bottomM,leftM,rightM;
int thickness=10;
CvScalar color={0,0,0};
int main(int argc, const char *argv[])
{
	///readimage

	IplImage *img;
	img=cvLoadImage(argv[1],CV_LOAD_IMAGE_COLOR );	

	//read current frame
	sscanf(argv[1],"output_%lf_%lf_%lf_%lf.png",
			top1,bottom1,left1,right1);
	//read next frame
	sscanf(argv[2],"output_%lf_%lf_%lf_%lf.png",
			top2,bottom2,left2,right2);
	//caculate lines;

	topM=4000-4000*(top2-bottom1)/(top1-bottom1);
	bottomM=4000-4000*(bottom2-bottom1)/(top1-bottom1);
	leftM=4000*(left2-right1)/(left1-right1);
	rightM=4000*(right2-right1)/(left1-right1);

	

	CvPoint pTopLeft=cvPoint(leftM,topM);
	CvPoint pTopRight=cvPoint(rightM,topM);
	CvPoint pBottomRight=cvPoint(rightM,bottomM);
	CvPoint pBottomLeft=cvPoint(leftM, bottomM);

	cvLine(img, pTopLeft, pTopRight, color, thickness, 8, 0);
	cvLine(img, pTopRight, pBottomRight, color, thickness, 8, 0);
	cvLine(img, pBottomRight, pBottomLeft, color, thickness, 8, 0);
	cvLine(img, pBottomLeft, pTopLeft, color, thickness, 8, 0);

	//draw lines;

	//saveimage;
	return 0;
}
