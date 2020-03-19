#include "veinfeature.h"
#include <stdint.h>
#include <cmath>

#define WHITE 65535
#define BLACK 0

//Set ROI dimensions, depth, scale, kernel size, minveinsize
veinfeature::veinfeature(int x1, int x2, int y1, int y2, unsigned short max_depth, unsigned short min_depth, double scale_x, double scale_y, int kernel_size, int min_vein_size, int connected)
{
	int roi_width = x2 - x1 + 1;
	int roi_height = y2 - y1 + 1;
	this->ref_roi = Rect(x1, y1, roi_width, roi_height);
	this->max_depth = max_depth;
	this->min_depth = min_depth;
	this->scale_x = scale_x;
	this->scale_y = scale_y;
	this->kernel_size = kernel_size;
	this->min_vein_size = min_vein_size;
	this->connected = connected;
}

void veinfeature::get_vein_pattern(Mat& depth, Mat& ir, struct package * output, int mode)
{
	Mat ref_ir = ir(this->ref_roi).clone();
	Mat ref_depth = depth(this->ref_roi).clone();	

	/**
	* Silhouette
	*/
	Mat silhouette, silhouetteRect, silhouette_scaled, silhouette_cleaned;
	veinfeature::get_silhouette(ref_depth, silhouette, veinfeature::max_unsigned, this->max_depth, this->min_depth);
	if (!silhouette.data) {
		printf("SILHOUETTE IS EMPTY. JESUS IS NEVER COMING BACK\n");
	}
	// overlay the outline if it exists
	veinfeature::displaySilhouette(silhouette.clone());

	
	veinfeature::shrink_silhouette(silhouette, silhouette_scaled, this->scale_x, this->scale_y);
	veinfeature::clean_silhouette(silhouette_scaled, silhouette_cleaned, veinfeature::max_unsigned, this->connected);

	/**
	* Veins
	*/



	Mat veins, veins_masked, veins_cleaned, veins_trimmed;
	veinfeature::get_veins(ref_ir, veins, veinfeature::max_unsigned, this->kernel_size);

	////cv::namedWindow("1", cv::WINDOW_NORMAL);
	//cv::imshow("1", veins);
	veinfeature::mask_veins(silhouette_cleaned, veins, veins_masked);



	veinfeature::clean_veins(veins_masked, veins_cleaned, this->connected, this->min_vein_size, veinfeature::max_unsigned);
	veinfeature::trim_veins(veins_cleaned, veins_trimmed);

	/*cv::namedWindow("2", cv::WINDOW_NORMAL);
	cv::imshow("2", veins_cleaned);*/

	Mat veins_thinned, veins_increased, veins_decreased, increased_thinned, decreased_thinned;

	veins_decreased = veinfeature::decreaseVeinWidth(veins_trimmed, 24);
	veins_increased = veinfeature::increaseVeinWidth(veins_trimmed, 24);

	decreased_thinned = veinfeature::my_thinning(veins_decreased);
	increased_thinned = veinfeature::my_thinning(veins_increased);

	//cv::namedWindow("Increased", cv::WINDOW_NORMAL);
	//cv::imshow("Increased", veins_increased);

	//cv::namedWindow("Decreased", cv::WINDOW_NORMAL);
	//cv::imshow("Decreased", veins_decreased);

	//cv::namedWindow("Increased Thinned", cv::WINDOW_NORMAL);
	//cv::imshow("Increased Thinned", increased_thinned);

	//cv::namedWindow("Decreased Thinned", cv::WINDOW_NORMAL);
	//cv::imshow("Decreased Thinned", decreased_thinned);

	cv::namedWindow("Normal", cv::WINDOW_NORMAL);
	cv::imshow("Normal", veins_trimmed);

	veins_thinned = veinfeature::my_thinning(veins_trimmed);
	//cv::namedWindow("Normal Thinned", cv::WINDOW_NORMAL);
	//cv::imshow("Normal Thinned", veins_thinned);

	//veinfeature::thinning(veins_trimmed, veins_thinned);
	//veins_thinned.convertTo(veins_thinned, CV_16UC1, 255);

	//cv::namedWindow("3", cv::WINDOW_NORMAL);
	//cv::imshow("3", veins_thinned);

	output->silhouette = silhouette;
	if (mode == DECREASED) {
		output->vein_pattern = decreased_thinned;
	}
	else if (mode == INCREASED) {
		output->vein_pattern = decreased_thinned;
	}
	else {
		output->vein_pattern = veins_thinned;
	}
	
}

void veinfeature::get_silhouette(Mat& depth, Mat& silhouette, unsigned short sub_value, unsigned short max_depth, unsigned short min_depth)
{
	// Get silhouette
	Mat depth_temp, silhouette_temp;
	depth.convertTo(depth_temp, CV_32FC1);
	// Remove values above upper bound
	threshold(depth_temp, silhouette_temp, double(max_depth), double(sub_value), THRESH_TOZERO_INV);
	// Remove values equal to or below lower bound
	threshold(silhouette_temp, silhouette_temp, double(min_depth), double(sub_value), THRESH_BINARY);
	silhouette_temp.convertTo(silhouette, CV_16UC1);
}

void veinfeature::clean_silhouette(Mat& silhouette, Mat& silhouette_cleaned, unsigned short sub_value, int connected)
{
	// Find connected components
	Mat silhouette_temp;
	silhouette.convertTo(silhouette_temp, CV_8UC1, double(1 / 256.0));
	Mat labels, stats, centroids;
	connectedComponentsWithStats(silhouette_temp, labels, stats, centroids, connected, CV_16UC1);

	// Detect largest non-zero segment
	int largest = 0, largest_index = 0;
	for (int i = 1; i < stats.rows; ++i)
	{
		int current_size = stats.at<int>(i, 4);
		if (current_size >= largest)
		{
			largest = current_size;
			largest_index = i;
		}
	}

	// Remove all but the largest non-zero segment
	silhouette_cleaned = Mat::zeros(silhouette.size(), CV_16UC1);
	for (int i = 0; i < labels.rows; ++i)
	{
		for (int j = 0; j < labels.cols; ++j)
		{
			unsigned short label_value = labels.at<unsigned short>(i, j);
			if (label_value == largest_index)
			{
				silhouette_cleaned.at<unsigned short>(i, j) = sub_value;
			}
		}
	}
}

void veinfeature::shrink_silhouette(Mat& silhouette, Mat& silhouette_scaled, double scaled_x, double scale_y)
{
	// Find silhouette centroid
	Moments m1 = moments(silhouette, false);
	Point p1 = Point(cvFloor(m1.m10 / m1.m00), cvFloor(m1.m01 / m1.m00));
	if (m1.m00 == 0) { p1 = Point(0, 0); }

	// Shrink silhouette
	Mat scale_down;
	Size scale1 = Size(cvFloor(silhouette.cols * scaled_x), cvFloor(silhouette.rows * scale_y));
	resize(silhouette, scale_down, scale1, 0, 0, INTER_NEAREST);

	// Find small silhouette centroid
	Moments m2 = moments(scale_down, false);
	Point p2 = Point(cvFloor(m2.m10 / m2.m00), cvFloor(m2.m01 / m2.m00));
	if (m2.m00 == 0) { p2 = Point(0, 0); }

	// Align small silhouette centroid to original silhouette centroid
	silhouette_scaled = Mat::zeros(silhouette.size(), CV_16UC1);
	int left = p1.x - p2.x;
	int top = p1.y - p2.y;

	Rect location = Rect(left, top, scale_down.cols, scale_down.rows);
	scale_down.copyTo(silhouette_scaled(location));
}

void veinfeature::get_veins(Mat& ir, Mat& veins, unsigned short sub_value, int kernel_size)
{
	// Filter veins
	Mat ir_temp;
	ir.convertTo(ir_temp, CV_8UC1, double(1 / 256.0));
	adaptiveThreshold(ir_temp, veins, double(sub_value), ADAPTIVE_THRESH_MEAN_C, THRESH_BINARY_INV, kernel_size, 0);
	veins.convertTo(veins, CV_16UC1, 256);
}

void veinfeature::mask_veins(Mat& silhouette, Mat& veins, Mat& veins_masked)
{
	// Crop out the veins using silhouette as mask
	Mat silhouette_temp;
	silhouette.convertTo(silhouette_temp, CV_8UC1, double(1 / 256.0));
	veins.copyTo(veins_masked, silhouette_temp);
}

void veinfeature::clean_veins(Mat& veins, Mat& veins_cleaned, int connected, int min_vein_size, unsigned short sub_value)
{
	// Find connected components
	Mat veins_temp;
	veins.convertTo(veins_temp, CV_8UC1, double(1 / 256.0));
	Mat labels, stats, centroids;
	int nLabels = connectedComponentsWithStats(veins_temp, labels, stats, centroids, connected, CV_16UC1);

	// Remove every segment below limit
	veins_cleaned = Mat::zeros(veins.size(), CV_16UC1);
	for (int i = 0; i < labels.rows; ++i)
	{
		for (int j = 0; j < labels.cols; ++j)
		{
			int label_value = int(labels.at<unsigned short>(i, j));
			int label_occupancy = stats.at<int>(label_value, 4);
			if (label_occupancy > min_vein_size && label_value > 0)
			{
				veins_cleaned.at<unsigned short>(i, j) = sub_value;
			}
		}
	}
	
	// Fill in small gaps
	Mat veins_fill;
	veins_cleaned.convertTo(veins_fill, CV_32FC1, 1 / float(sub_value));
	
	// First filter
	float kernel_array1[] = { 1, 1, 1,
		1, 0, 1,
		1, 1, 1 };
	Mat kernel1 = Mat(Size(3, 3), CV_32FC1, kernel_array1);
	veinfeature::gaps(veins_fill, kernel1, veins_cleaned, 4, sub_value);
}

void veinfeature::trim_veins(Mat& veins, Mat& veins_trimmed)
{
	int left = veins.cols - 1, right = 0;
	int top = veins.rows - 1, bottom = 0;

	for (int i = 0; i < veins.rows; ++i)
	{
		for (int j = 0; j < veins.cols; ++j)
		{
			unsigned short current_value = veins.at<unsigned short>(i, j);
			if (current_value > 0)
			{
				if (j < left) { left = j; }
				if (j > right) { right = j; }
				if (i < top) { top = i; }
				if (i > bottom) { bottom = i; }
			}
		}
	}

	Rect region = Rect(left, top, right - left + 1, bottom - top + 1);
	if (region.width < 1 || region.height < 1)
	{
		veins_trimmed = Mat::zeros(1, 1, CV_16UC1);
	}
	else
	{
		veins_trimmed = veins(region).clone();
	}
}

void veinfeature::gaps(Mat& veins_fill, Mat& kernel, Mat& veins_cleaned, float summed, unsigned short sub_value)
{
	filter2D(veins_fill, veins_fill, CV_32FC1, kernel, Point(-1, -1), 0, BORDER_DEFAULT);
	for (int i = 0; i < veins_fill.rows; ++i)
	{
		for (int j = 0; j < veins_fill.cols; ++j)
		{
			float fill = veins_fill.at<float>(i, j);
			if (fill > summed)
			{
				veins_cleaned.at<unsigned short>(i, j) = sub_value;
			}
		}
	}
}

//Reduce vein width
void veinfeature::vein_thinning(Mat& veins_cleaned)
{
	//Store Image and temporary image
	cv::Mat reduced(veins_cleaned.size(), CV_16UC1, cv::Scalar(0));
	//cv::Mat reduced = veins_cleaned;	
	cv::Mat temp;// = cv::Mat::zeros(reduced.size(), CV_8UC1);
	cv::Mat eroded;

	cv::Mat element = cv::getStructuringElement(cv::MORPH_CROSS, cv::Size(2, 2));
	bool done;
	int count = 0;
	int numNonZero = cv::countNonZero(veins_cleaned);
	do {
		count++;
		cv::erode(veins_cleaned, eroded, element);
		cv::dilate(eroded, temp, element);
		cv::subtract(veins_cleaned, temp, temp);
		cv::Mat reduced_cloned = reduced.clone();
		cv::bitwise_or(reduced_cloned, temp, reduced);
		eroded.copyTo(veins_cleaned);
		/*if (count == 3) {
			break;
		}*/
		int nonZero = cv::countNonZero(veins_cleaned);
		done = (nonZero <= numNonZero/2);
	} while (!done);
}

cv::Mat veinfeature::my_thinning(cv::Mat& trimmed) {

	cv::Mat temp = trimmed.clone();	
	int totalWidths = 0;
	int numWidths = 0;
	int averageWidth = 0;
	for (int numRow = 0; numRow < trimmed.rows; numRow++) {		
		for (int numColumn = 0; numColumn < trimmed.cols; numColumn++) {
			if (temp.at<unsigned short>(numRow, numColumn) == WHITE) {						
				// we've encountered a white pixel after a series of black pixels
				int index = numColumn;								
				while ((index < trimmed.cols) && (temp.at<unsigned short>(numRow, index) == WHITE)) {
					temp.at<unsigned short>(numRow, index) = BLACK;
					index++;
				}
				int middle = (index + numColumn) / 2;
				int width = index - numColumn;
				totalWidths += width;
				numWidths++;
				averageWidth = totalWidths / numWidths;
				if (width >= 5) {
					temp.at<unsigned short>(numRow, middle) = WHITE;					
				}					
				numColumn = index;
			}			
		}		
	}	
	return temp;
}

void veinfeature::displaySilhouette(Mat& silhouette) {
	// check and see if canny exists
	cv::Mat canny;	
	canny = cv::imread("./canny.png");	
	if (canny.data) {
		// convert 
		cv::cvtColor(canny, canny, CV_BGR2GRAY);
		//convert silhouette to RGB
		silhouette.convertTo(silhouette, CV_32FC1, 1.0 / 255.0);
		// overlay
		for (int numRow = 0; numRow < canny.rows; numRow++) {
			for (int numColumn = 0; numColumn < canny.cols; numColumn++) {
				if (canny.at<uchar>(numRow, numColumn) == 255) {
					silhouette.at<float>(numRow, numColumn) = 170;				
				}
			}
		}
	}	
	// display the silhouette
	cv::namedWindow("silhouette", cv::WINDOW_NORMAL);
	cv::Mat flipped = silhouette.clone();
	cv::flip(silhouette, flipped, 1);
	cv::imshow("silhouette", silhouette);
}

void veinfeature::thinningIteration(cv::Mat& img, int iter) {
	CV_Assert(img.channels() == 1);
	CV_Assert(img.depth() != sizeof(uchar));
	CV_Assert(img.rows > 3 && img.cols > 3);

	cv::Mat marker = cv::Mat::zeros(img.size(), CV_8UC1);

	int nRows = img.rows;
	int nCols = img.cols;

	if (img.isContinuous()) {
		nCols *= nRows;
		nRows = 1;
	}

	int x, y;
	uchar *pAbove;
	uchar *pCurr;
	uchar *pBelow;
	uchar *nw, *no, *ne;    // north (pAbove)
	uchar *we, *me, *ea;
	uchar *sw, *so, *se;    // south (pBelow)

	uchar *pDst;

	// initialize row pointers
	pAbove = NULL;
	pCurr = img.ptr<uchar>(0);
	pBelow = img.ptr<uchar>(1);

	for (y = 1; y < img.rows - 1; ++y) {
		// shift the rows up by one
		pAbove = pCurr;
		pCurr = pBelow;
		pBelow = img.ptr<uchar>(y + 1);

		pDst = marker.ptr<uchar>(y);

		// initialize col pointers
		no = &(pAbove[0]);
		ne = &(pAbove[1]);
		me = &(pCurr[0]);
		ea = &(pCurr[1]);
		so = &(pBelow[0]);
		se = &(pBelow[1]);

		for (x = 1; x < img.cols - 1; ++x) {
			// shift col pointers left by one (scan left to right)
			nw = no;
			no = ne;
			ne = &(pAbove[x + 1]);
			we = me;
			me = ea;
			ea = &(pCurr[x + 1]);
			sw = so;
			so = se;
			se = &(pBelow[x + 1]);

			int A = (*no == 0 && *ne == 1) + (*ne == 0 && *ea == 1) +
				(*ea == 0 && *se == 1) + (*se == 0 && *so == 1) +
				(*so == 0 && *sw == 1) + (*sw == 0 && *we == 1) +
				(*we == 0 && *nw == 1) + (*nw == 0 && *no == 1);
			int B = *no + *ne + *ea + *se + *so + *sw + *we + *nw;
			int m1 = iter == 0 ? (*no * *ea * *so) : (*no * *ea * *we);
			int m2 = iter == 0 ? (*ea * *so * *we) : (*no * *so * *we);

			if (A == 1 && (B >= 2 && B <= 6) && m1 == 0 && m2 == 0)
				pDst[x] = 1;
		}
	}

	img &= ~marker;
}
void veinfeature::thinning(const cv::Mat& src, cv::Mat& dst) {
	src.clone().convertTo(dst, CV_8UC1);
	dst /= 255;         // convert to binary image

	cv::Mat prev = cv::Mat::zeros(dst.size(), CV_8UC1);
	cv::Mat diff;

	do {
		thinningIteration(dst, 0);
		thinningIteration(dst, 1);
		cv::absdiff(dst, prev, diff);
		dst.copyTo(prev);
	} while (cv::countNonZero(diff) > 0);

	dst *= 255;
}


cv::Mat veinfeature::increaseVeinWidth(Mat& pattern, int percentage) {
	cv::Mat increased = pattern.clone();
	bool started;
	int start = 0;
	int finish = 0;
	int width = 0;
	int increase = 0;
	for (int numRow = 0; numRow < increased.rows; numRow++) {
		started = false;
		for (int numColumn = 0; numColumn < increased.cols; numColumn++) {
			//Found the first white bit
			if (!started && increased.at<unsigned short>(numRow, numColumn) == WHITE) {
				start = numColumn;
				started = true;
			}

			//Found the last white bit
			//Record finish and make the width increase
			if (started && increased.at<unsigned short>(numRow, numColumn) == BLACK) {
				finish = numColumn;
				//Make increase
				width = finish - start;
				increase = (width * percentage) / 100;
				increase = increase / 2;

				//Check you're not accessing row < 0 && after numCols
				if (start - increase >= 0 && finish + increase < increased.cols) {
					for (int i = 0; i < increase; i++) {
						increased.at<unsigned short>(numRow, start - i) = WHITE;
						increased.at<unsigned short>(numRow, finish + i) = WHITE;
					}

				//Goes over on left side
				//Only increase right side
				//Increase left to the edge
				} else if ((start - increase) < 0 && (finish + increase) < increased.cols) {
					for (int i = 0; i < increase; i++) {
						increased.at<unsigned short>(numRow, finish + i) = WHITE;
					}

					for (int i = 0; i <= start; i++) {
						increased.at<unsigned short>(numRow, i) = WHITE;
					}

				//Goes over right side
				//Only increase left side
				} else if ((finish + increase) >= increased.cols && (start - increase >= 0)) {
					for (int i = 0; i < increase; i++) {
						increased.at<unsigned short>(numRow, start - i) = WHITE;
					}

					for (int i = finish; i < increased.cols; i++) {
						increased.at<unsigned short>(numRow, i) = WHITE;
					}
				}
				started	= false;
			}
		}
	}
	return increased;
}

cv::Mat veinfeature::decreaseVeinWidth(Mat& pattern, int percentage) {
	cv::Mat decreased = pattern.clone();
	bool started;
	int start = 0;
	int finish = 0;
	int width = 0;
	int decrease = 0;
	for (int numRow = 0; numRow < decreased.rows; numRow++) {
		started = false;
		for (int numColumn = 0; numColumn < decreased.cols; numColumn++) {
			//Found the first white bit
			if (started == false && decreased.at<unsigned short>(numRow, numColumn) == WHITE) {
				start = numColumn;
				started = true;
				//printf("Found the start of one!\n");
			}

			//If its a middle white do nothing

			//Found the last white bit
			//Record finish and make the width increase
			else if (started == true && (decreased.at<unsigned short>(numRow, numColumn) == BLACK || numColumn == decreased.cols)) {
				//printf("Found the end of one!\n");
				finish = numColumn;
				//Make decrease
				width = finish - start;
				decrease = (width * percentage) / 100;
				decrease = decrease / 2;
		
				for (int i = 0; i <= decrease; i++) {
					decreased.at<unsigned short>(numRow, start + i) = BLACK;
					decreased.at<unsigned short>(numRow, finish - i) = BLACK;
				}
				started = false;
			}
		}
	}
	return decreased;
}