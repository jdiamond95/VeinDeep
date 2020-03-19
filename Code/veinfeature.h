#pragma once
#include <limits>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>
#include "globals.h"

using namespace cv;
using namespace std;

#ifndef VEINFEATURE_H
#define VEINFEATURE_H




class veinfeature
{
public:
	veinfeature(int x1, int x2, int y1, int y2, unsigned short max_depth, unsigned short min_depth, double scale_x, double scale_y, int kernel_size, int min_vein_size, int connected);
	void get_vein_pattern(Mat& depth, Mat& ir, struct package * output, int mode);

public:
	const static unsigned short max_unsigned = numeric_limits<unsigned short>::max();

private:
	Rect ref_roi;
	unsigned short min_depth, max_depth;
	double scale_x, scale_y;
	int kernel_size, min_vein_size, connected;

private:
	/**
	* Silhouette
	*/
	static void get_silhouette(Mat& depth, Mat& silhouette, unsigned short sub_value, unsigned short max_depth, unsigned short min_depth);
	static void clean_silhouette(Mat& silhouette, Mat& silhouette_cleaned, unsigned short sub_value, int connected);
	static void shrink_silhouette(Mat& silhouette, Mat& silhouette_scaled, double scaled_x, double scale_y);
	static void displaySilhouette(Mat& silhouette);

	/**
	* Veins
	*/
	static void get_veins(Mat& ir, Mat& veins, unsigned short sub_value, int kernel_size);
	static void mask_veins(Mat& silhouette, Mat& veins, Mat& veins_masked);
	static void clean_veins(Mat& veins, Mat& veins_cleaned, int connected, int min_vein_size, unsigned short sub_value);
	static void trim_veins(Mat& veins, Mat& veins_trimmed);
	static void gaps(Mat& veins_fill, Mat& kernel, Mat& veins_cleaned, float summed, unsigned short sub_value);
	static void vein_thinning(Mat& veins_cleaned);
	static cv::Mat veinfeature::my_thinning(cv::Mat& trimmed);
	static cv::Mat veinfeature::decreaseVeinWidth(Mat& pattern, int percentage);
	static cv::Mat veinfeature::increaseVeinWidth(Mat& pattern, int percentage);

	
	/*
	 extra thinning algos
	*/
	static void thinningIteration(cv::Mat& img, int iter);
	static void thinning(const cv::Mat& src, cv::Mat& dst);

};
#endif
