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

#define MAX_FRAMES 1

void generateBMP(PXCImage * image);
int SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile);
int enumerateFormat(int format);

int wmain(int argc, WCHAR* argv[]) {
	// initialise the UtilRender & UtilColor instances
	UtilRender * renderIR = new UtilRender(L"IR STREAM");
	// create the PXCSenseManager
	PXCSenseManager *psm = PXCSenseManager::CreateInstance();
	if (!psm) {
		wprintf_s(L"Unable to create PXCSenseManager");
		return 1;
	}

	// select the color stream of size 640x480 and depth stream of size 640x480	
	psm->EnableStream(PXCCapture::STREAM_TYPE_IR, 640, 480);

	// initialize the PXCSenseManager
	if (psm->Init() != PXC_STATUS_NO_ERROR) {
		wprintf_s(L"Unable to Init the PXCSenseManager\n");
		return 2;
	}

	PXCImage *infraRedIm;
	//cv::Mat * ocvImage = ;
	for (int i = 0; i<MAX_FRAMES; i++) {

		// This function blocks until all streams are ready (depth and color)
		// if false streams will be Unaligned
		if (psm->AcquireFrame(true)<PXC_STATUS_NO_ERROR) break;

		// retrieve all available image samples
		PXCCapture::Sample *sample = psm->QuerySample();

		// retrieve the image or frame by type from the sample
		infraRedIm = sample->ir;
		PXCImage::ImageData data;

		int cvIRDataType = CV_16U;
		int cvIRDataWidth = 2;		

		if (infraRedIm->AcquireAccess(PXCImage::ACCESS_READ, PXCImage::PIXEL_FORMAT_Y16, &data) >= PXC_STATUS_NO_ERROR) {
			/*Convert the image into the opencv Mat format*/			

			/*get the image details*/
			int format = infraRedIm->QueryInfo().format;
			int width = infraRedIm->QueryInfo().width;
			int height = infraRedIm->QueryInfo().height;

			cv::Mat ocvImage = cv::Mat::zeros(cv::Size(640, 480), CV_16U);// = cv::Mat(cv::Size(width, height), cvIRDataType, data.planes[0]);
			// save the openCV image
			cv::imshow("", ocvImage);

			ocvImage.create(height, data.pitches[0] / cvIRDataWidth, cvIRDataType);
			memcpy(ocvImage.data, data.planes[0], height*width*cvIRDataWidth * sizeof(pxcBYTE));

			std::cout << "finished copying data into Mat\n";

			

			infraRedIm->ReleaseAccess(&data);
		}



		// render the frame
		if (!renderIR->RenderFrame(infraRedIm)) break;

		// release or unlock the current frame to fetch the next frame
		psm->ReleaseFrame();
	}

	// delete the UtilRender instance
	delete renderIR;
	// close the last opened streams and release any session and processing module instances
	psm->Release();	

	// end message
	char willOverflow[22];
	std::cout << "Press enter to exit";
	std::cin >> willOverflow;
	std::cout << "Exiting";
	return 0;
}

void generateBMP(PXCImage * image) {
	PXCImage::ImageData data;

	pxcStatus sts = image->AcquireAccess(PXCImage::ACCESS_READ, &data);
	
	int width = image->QueryInfo().width;
	int height = image->QueryInfo().height;
	int format = image->QueryInfo().format;
	enumerateFormat(format);
	printf("res = %dx%d\n", width, height);
	if (sts >= PXC_STATUS_NO_ERROR)
	{		 

		BYTE* baseAddr = data.planes[0];// contains the pixel data 	
		
		// pitches = widthinpixels * bytesperpixel. pxcBYTE is of size 1.
		int stride = data.pitches[0] / sizeof(pxcBYTE);	 
		
		// initialise rgb data holder
		BYTE rgbData[480 * 640 * 3] = { 0 };
		memset(rgbData, 44, 480 * 640 * 3);

		// get rgb data from image and put it into the array
		// **** this section needs more research ****
		int pixels = 0;
		for (int y = 0; y < 480; y++)
		{
			for (int x = 0; x < 640; x++)
			{				
				// data in baseAddr array is made up of 2 byte pixels (Y16)
				pxcBYTE nBlue = (baseAddr + y * stride)[2 * x + 0];
				pxcBYTE nGreen = (baseAddr + y * stride)[2 * x + 1];				
				
				// this is weird to convert into rgb, so we just use 3 lots of the 
				// same colour (making our rgb grayscale)
				(rgbData + y * 640*3)[3 * x] = nBlue;
				(rgbData + y * 640*3)[3 * x + 1] = nGreen;
				(rgbData + y * 640*3)[3 * x + 2] = nGreen;
				pixels++;
			}
		}

		// write to BMP file
		LPCTSTR filename = (LPCTSTR)"bmpFile.bmp";		
		std::cout << "SaveBMP returns: " << SaveBMP(rgbData, 640, 480, 640 * 480 * 3, filename) << "\n";
	}

	image->ReleaseAccess(&data);	

}

int SaveBMP(BYTE* Buffer, int width, int height, long paddedsize, LPCTSTR bmpfile)
{
	// declare bmp structures 
	BITMAPFILEHEADER bmfh;
	BITMAPINFOHEADER info;

	// andinitialize them to zero
	memset(&bmfh, 0, sizeof(BITMAPFILEHEADER));
	memset(&info, 0, sizeof(BITMAPINFOHEADER));

	// fill the fileheader with data
	bmfh.bfType = 0x4d42;       // 0x4d42 = 'BM'
	bmfh.bfReserved1 = 0;
	bmfh.bfReserved2 = 0;
	bmfh.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + paddedsize;
	bmfh.bfOffBits = 0x36;		// number of bytes to start of bitmap bits

								// fill the infoheader

	info.biSize = sizeof(BITMAPINFOHEADER);
	info.biWidth = width;
	info.biHeight = height;
	info.biPlanes = 1;			// we only have one bitplane
	info.biBitCount = 24;		// RGB mode is 24 bits
	info.biCompression = BI_RGB;
	info.biSizeImage = 0;		// can be 0 for 24 bit images
	info.biXPelsPerMeter = 0x0ec4;     // paint and PSP use this values
	info.biYPelsPerMeter = 0x0ec4;
	info.biClrUsed = 0;			// we are in RGB mode and have no palette
	info.biClrImportant = 0;    // all colors are important
	std::cout << "BMP Header initialisation complete!\n";
								// now we open the file to write to
	HANDLE file = CreateFile(bmpfile, GENERIC_WRITE, FILE_SHARE_READ,
		NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == NULL)
	{
		CloseHandle(file);
		return 0;
	}
	std::cout << "Empty BMP File created!\n";
	// write file header
	unsigned long bwritten;
	if (WriteFile(file, &bmfh, sizeof(BITMAPFILEHEADER), &bwritten, NULL) == false)
	{
		CloseHandle(file);		
		return 1;
	}
	std::cout << "BMP File Header written!\n";
	// write infoheader
	if (WriteFile(file, &info, sizeof(BITMAPINFOHEADER), &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return 2;
	}
	std::cout << "BMP Info Header written!\n";
	// write image data
	if (WriteFile(file, Buffer, paddedsize, &bwritten, NULL) == false)
	{
		CloseHandle(file);
		return 3;
	}
	std::cout << "BMP Data written!\n";
	// and close file
	CloseHandle(file);

	return 4;
}

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
	char matched = 0;
	for (int k = 0; k < 16; k++) {
		if (format == allFormats[k]) {
			printf("Found match! index = %d\n", k);
			matched = 1;
			return k;
		}
	}
	if (!matched) {
		printf("no match found :'( \n");
		return -1;
	}	
}