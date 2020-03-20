#include <windows.h>
#include <wchar.h>
#include "util_render.h"
#include <pxcsensemanager.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <opencv2\core\core.hpp>
#include <opencv2\highgui\highgui.hpp>
#include <thread>
#include "analysis.h"
#include "globals.h"
#include <time.h>

#define MAX_FRAMES 1
#define SIMILARITY_THRESHOLD 700
#define MIN_THRESHOLD 600

PXCSenseManager *pxcSenseManager;

int enumerateFormat(int format) {
	int allFormats[16] = {
		PXCImage::PIXEL_FORMAT_ANY, //0
		PXCImage::PIXEL_FORMAT_BGR, //1
		PXCImage::PIXEL_FORMAT_BGRA, //2
		PXCImage::PIXEL_FORMAT_DEPTH, //3
		PXCImage::PIXEL_FORMAT_DEPTH_CONFIDENCE, //4
		PXCImage::PIXEL_FORMAT_DEPTH_F32, //5
		PXCImage::PIXEL_FORMAT_DEPTH_RAW, //6
		PXCImage::PIXEL_FORMAT_NV12, //7
		PXCImage::PIXEL_FORMAT_RGB, //8
		PXCImage::PIXEL_FORMAT_RGB24, //9
		PXCImage::PIXEL_FORMAT_RGB32, //10
		PXCImage::PIXEL_FORMAT_RGBA, //11
		PXCImage::PIXEL_FORMAT_Y16, //12
		PXCImage::PIXEL_FORMAT_Y8, //13
		PXCImage::PIXEL_FORMAT_Y8_IR_RELATIVE, //14
		PXCImage::PIXEL_FORMAT_YUY2//15
	};
	for (int k = 0; k < 16; k++) {
		if (format == allFormats[k]) {
			printf("Found match! index = %d\n", k);
			return k;
		}
	}
	printf("no match found :'( \n");
	return -1;
}

//Converts PXCImage to Mat (used by OpenCV) type
//Only handles two pixel formats - Y16 & PIXEL_Format_Depth
cv::Mat PXCImage2CVMat(PXCImage *pxcImage, PXCImage::PixelFormat format)
{
    PXCImage::ImageData data;
    pxcImage->AcquireAccess(PXCImage::ACCESS_READ, format, &data);

    int width = pxcImage->QueryInfo().width;
    int height = pxcImage->QueryInfo().height;
	int colourFormat = pxcImage->QueryInfo().format;
    if(!format)
        format = pxcImage->QueryInfo().format;

    int type;
    if(format == PXCImage::PIXEL_FORMAT_Y16 || format == PXCImage::PIXEL_FORMAT_DEPTH) {
	    type = CV_16UC1;
	}

	cv::Mat ocvImage = cv::Mat(cv::Size(width, height), type, data.planes[0]);
    //cv::Mat ocvImage = cv::Mat(cv::Size(width, height), type,);
	memcpy(ocvImage.data, data.planes[0], height*width * 2 * sizeof(pxcBYTE));
	cv::Mat pic16bit;
	if (format == PXCImage::PIXEL_FORMAT_DEPTH) {
		ocvImage.convertTo(pic16bit, CV_16UC1, 90);
	}
	else {
		ocvImage.convertTo(pic16bit, CV_16UC1, 90);
	}
    pxcImage->ReleaseAccess(&data);
    return pic16bit;
}

void compare(cv::Mat& current) {
	printf("Reading in references:\n");
	cv::Mat ref1;
	ref1 = cv::imread("./1.png");
	printf("1->");
	cv::Mat ref2 = cv::imread("./2.png");
	printf("2->");
	cv::Mat ref3 = cv::imread("./3.png");
	printf("3->");
	cv::Mat ref4 = cv::imread("./4.png");
	printf("4->");
	cv::Mat ref5 = cv::imread("./5.png");

	printf("5->done\n");

	printf("converting references:");
	printf("1->");
	cvtColor(ref1, ref1, CV_RGB2GRAY);
	ref1.convertTo(ref1, CV_16UC1, 65535);
	printf("2->");
	cvtColor(ref2, ref2, CV_RGB2GRAY);
	ref2.convertTo(ref2, CV_16UC1, 65535);
	printf("3->");
	cvtColor(ref3, ref3, CV_RGB2GRAY);
	ref3.convertTo(ref3, CV_16UC1, 65535);
	printf("4->");
	cvtColor(ref4, ref4, CV_RGB2GRAY);
	ref4.convertTo(ref4, CV_16UC1, 65535);
	printf("5->");
	cvtColor(ref5, ref5, CV_RGB2GRAY);
	ref5.convertTo(ref5, CV_16UC1, 65535);
	printf("done\n");

	long double s1, s2, s3, s4, s5;
	
	s1 = analysePatterns(ref1, current);
	s2 = analysePatterns(ref2, current);
	s3 = analysePatterns(ref3, current);
	s4 = analysePatterns(ref4, current);
	s5 = analysePatterns(ref5, current);
	
	std::vector<long double> scores = { s1, s2, s3, s4, s5 };
	int sum = 0;
	long double smallest = s1;
	for (long double d : scores) {
		if (d < smallest) {
			smallest = d;
		}
		if (d <= SIMILARITY_THRESHOLD) {
			sum++;
		}
	}
	if (smallest < MIN_THRESHOLD || sum >= 2) {
		printf("The vein patterns match\n");
	} else {
		printf("The vein patterns don't match\n");
	}
}

//Main Function
int main (int argc, char *argv[]) {
	//Define some parameters for the camera
    cv::Size frameSize = cv::Size(640, 480);
    float frameRate = 60;

    //Create the OpenCV windows and image frames
    cv::namedWindow("IR", cv::WINDOW_AUTOSIZE);	
	cv::namedWindow("Depth", cv::WINDOW_AUTOSIZE);
    cv::Mat frameIR = cv::Mat::zeros(frameSize, CV_16UC1);
	cv::Mat frameDepth = cv::Mat::zeros(frameSize, CV_16UC1);

	//Initialize the RealSense Manager
    pxcSenseManager = PXCSenseManager::CreateInstance();

	//Enable the streams to be used
    pxcSenseManager->EnableStream(PXCCapture::STREAM_TYPE_IR, frameSize.width, frameSize.height, frameRate);
	pxcSenseManager->EnableStream(PXCCapture::STREAM_TYPE_DEPTH, frameSize.width, frameSize.height, frameRate);

	//Initialize the pipeline
    pxcSenseManager->Init();


  	bool keepRunning = true;
	unsigned short maxDepth, minDepth;
	minDepth = (unsigned short) 0;
	maxDepth = (unsigned short)100000;
	cv::Mat current;
	struct package result;
    while(keepRunning) {
		 //Acquire all the frames from the camera
		
        pxcSenseManager->AcquireFrame();
        PXCCapture::Sample *sample = pxcSenseManager->QuerySample();

        //Convert each frame into an OpenCV image
        frameIR = PXCImage2CVMat(sample->ir, PXCImage::PIXEL_FORMAT_Y16);
		frameDepth = PXCImage2CVMat(sample->depth, PXCImage::PIXEL_FORMAT_DEPTH);
		
		if (!result.vein_pattern.empty()) {
			// it's empty
			current = result.vein_pattern;
		}		
		cv::Mat canny;

        //Display the images
        cv::imshow("IR", frameIR);
		cv::imshow("IR", frameDepth);

        //Check for user input		
		clock_t t1, t2 = NULL;
		float diff = 0;
		int key = cv::waitKey(1);
		switch (key) {		
		case 27:
			// escape key
			keepRunning = false;
			break;
		case 'c':
			// view current image
			cv::namedWindow("vein", cv::WINDOW_KEEPRATIO);
			
			getVeinPattern(frameIR, frameDepth, minDepth, maxDepth, &result, NORMAL);
			if (!result.vein_pattern.empty()) {
				// it's empty
				current = result.vein_pattern;
			}			
			imshow("vein", current);
			break;
		case 'd':
			// view current image
			cv::namedWindow("vein", cv::WINDOW_KEEPRATIO);

			getVeinPattern(frameIR, frameDepth, minDepth, maxDepth, &result, DECREASED);
			if (!result.vein_pattern.empty()) {
				// it's empty
				current = result.vein_pattern;
			}
			imshow("vein", current);
			break;
		case 'i':
			// view current image
			cv::namedWindow("vein", cv::WINDOW_KEEPRATIO);

			getVeinPattern(frameIR, frameDepth, minDepth, maxDepth, &result, INCREASED);
			if (!result.vein_pattern.empty()) {
				// it's empty
				current = result.vein_pattern;
			}
			imshow("vein", current);
			break;
		case '1':

			// perform canny on silhoutte
			canny = result.silhouette.clone();
			canny.convertTo(canny, CV_8UC1);
			cv::Canny(canny,           // input image
				canny,                   // output image
				0,                         // low threshold
				255);                       // high threshold

			// save the reference and the outline
			cv::imwrite("./1.png", current);
			cv::imwrite("./canny.png", canny);
			
			break;				
		case '2':
			// save as second reference
			cv::imwrite("./2.png", current);
			break;
		case '3':
			// save as second reference
			cv::imwrite("./3.png", current);
			break;
		case '4':
			// save as second reference
			cv::imwrite("./4.png", current);
			break;
		case '5':
			// save as second reference
			cv::imwrite("./5.png", current);
			break;
		case 't':
			// t for test
			
			t1 = clock();

			compare(current);
			t2 = clock();
			diff = ((float)t2 - (float)t1);
			printf("Timing: %f\n", diff);
			
			break;
		default:			
			break;
		}		

		//Also render frame so we know that IR is working
		/*UtilRender *renderer = new UtilRender(L"IR STREAM");
		renderer->RenderFrame(sample->ir);*/

        //Release the memory from the frames
        pxcSenseManager->ReleaseFrame();
	}

	//Release the memory from the RealSense manager
    pxcSenseManager->Release();

	return EXIT_SUCCESS;
}